#include "mylite_regexp.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_sqlite_bootstrap.h"
#include "mylite_sqlite_registration.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
    mysql_error_parse = 1064,
    mysql_error_regexp_illegal_argument = 3685,
    mysql_error_regular_expression = 3696,
    mysql_error_regular_expression_character_range = 3697,
    regexp_pattern_length_max = 256,
    regexp_value_length_max = 4096,
    regexp_string_buffer_initial_capacity = 64,
    ascii_max = 0x7f,
    ascii_upper_a = 'A',
    ascii_upper_z = 'Z',
    ascii_lower_delta = 'a' - 'A',
    decimal_radix = 10,
};

enum regexp_token_kind {
    REGEXP_TOKEN_LITERAL = 0,
    REGEXP_TOKEN_ANY = 1,
    REGEXP_TOKEN_CLASS = 2,
    REGEXP_TOKEN_END_ANCHOR = 3,
    REGEXP_TOKEN_OPTIONAL_LITERAL_SEQUENCE = 4,
};

enum regexp_quantifier {
    REGEXP_QUANTIFIER_ONE = 0,
    REGEXP_QUANTIFIER_ZERO_OR_ONE = 1,
    REGEXP_QUANTIFIER_ZERO_OR_MORE = 2,
    REGEXP_QUANTIFIER_ONE_OR_MORE = 3,
};

struct regexp_class_range {
    unsigned char first;
    unsigned char last;
};

struct regexp_token {
    enum regexp_token_kind kind;
    enum regexp_quantifier quantifier;
    unsigned char literal;
    bool class_is_negated;
    size_t range_start;
    size_t range_count;
    size_t literal_sequence_start;
    size_t literal_sequence_length;
};

struct mylite_regexp_program {
    bool anchored_start;
    bool case_sensitive;
    struct mylite_regexp_program **alternatives;
    size_t alternative_count;
    struct regexp_token *tokens;
    size_t token_count;
    struct regexp_class_range *ranges;
    size_t range_count;
    unsigned char *literal_sequence_bytes;
    size_t literal_sequence_length;
};

enum regexp_sqlite_function_kind {
    REGEXP_SQLITE_FUNCTION_MATCH = 1,
    REGEXP_SQLITE_FUNCTION_INSTR = 2,
    REGEXP_SQLITE_FUNCTION_SUBSTR = 3,
    REGEXP_SQLITE_FUNCTION_REPLACE = 4,
};

struct regexp_sqlite_function_config {
    enum regexp_sqlite_function_kind kind;
    bool case_sensitive;
};

struct regexp_string_buffer {
    char *data;
    size_t length;
    size_t capacity;
};

struct regexp_sqlite_match_arguments {
    sqlite3_value *pattern_value;
    sqlite3_value *value_value;
};

struct regexp_sqlite_compiled_program {
    struct mylite_regexp_program *program;
    struct mylite_regexp_program *compiled_program;
};

struct regexp_sqlite_replace_arguments {
    const unsigned char *value;
    const unsigned char *replacement;
    size_t value_length;
    size_t replacement_length;
};

struct regexp_match_span_context {
    const struct mylite_regexp_program *program;
    const unsigned char *value;
    size_t value_length;
    const unsigned char *matches;
};

struct regexp_match_span_position {
    size_t token_index;
    size_t value_index;
};

struct regexp_parse_cursor {
    const char *pattern;
    size_t pattern_length;
    size_t index;
};

struct regexp_alternation_scan_state {
    bool in_class;
    bool escaped;
};

struct regexp_find_input {
    const char *value;
    size_t value_length;
    size_t start_offset;
};

static void regexp_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void regexp_sqlite_match_callback(
    sqlite3_context *context,
    const struct regexp_sqlite_function_config *config,
    int argc,
    sqlite3_value **argv
);
static void regexp_sqlite_instr_callback(
    sqlite3_context *context,
    const struct regexp_sqlite_function_config *config,
    int argc,
    sqlite3_value **argv
);
static void regexp_sqlite_substr_callback(
    sqlite3_context *context,
    const struct regexp_sqlite_function_config *config,
    int argc,
    sqlite3_value **argv
);
static void regexp_sqlite_replace_callback(
    sqlite3_context *context,
    const struct regexp_sqlite_function_config *config,
    int argc,
    sqlite3_value **argv
);
static bool regexp_sqlite_replace_prepare_arguments(
    sqlite3_context *context,
    sqlite3_value **argv,
    struct regexp_sqlite_replace_arguments *out_arguments
);
static bool regexp_sqlite_replace_all(
    sqlite3_context *context,
    const struct regexp_sqlite_function_config *config,
    const struct regexp_sqlite_compiled_program *programs,
    const struct regexp_sqlite_replace_arguments *arguments,
    struct regexp_string_buffer *buffer
);
static bool regexp_sqlite_replace_find_match(
    sqlite3_context *context,
    const struct regexp_sqlite_function_config *config,
    const struct regexp_sqlite_compiled_program *programs,
    const struct regexp_sqlite_replace_arguments *arguments,
    size_t search_offset,
    struct mylite_regexp_match *out_match
);
static bool regexp_sqlite_get_program(
    sqlite3_context *context,
    sqlite3_value *pattern_value,
    bool case_sensitive,
    struct mylite_regexp_program **out_program,
    struct mylite_regexp_program **out_compiled_program
);
static bool regexp_sqlite_pattern_is_empty(
    sqlite3_context *context,
    sqlite3_value *pattern_value,
    bool *out_is_empty
);
static bool regexp_sqlite_any_argument_is_null(int argc, sqlite3_value **argv);
static void regexp_sqlite_result_illegal_argument(sqlite3_context *context);
static void regexp_sqlite_result_compile_error(
    sqlite3_context *context,
    enum mylite_regexp_compile_status status
);
static void regexp_sqlite_result_match_error(
    sqlite3_context *context,
    enum mylite_regexp_match_status status
);
static void regexp_sqlite_set_compile_diagnostic(
    sqlite3_context *context,
    enum mylite_regexp_compile_status status
);
static void regexp_sqlite_set_match_diagnostic(
    sqlite3_context *context,
    enum mylite_regexp_match_status status
);
static void regexp_sqlite_set_diagnostic(
    sqlite3_context *context,
    int error_code,
    const char *sqlstate,
    const char *message
);
static void regexp_sqlite_cache_program(
    sqlite3_context *context,
    struct mylite_regexp_program *compiled_program
);
static void regexp_sqlite_free_compiled_program(struct mylite_regexp_program *compiled_program);
static const char *regexp_compile_status_message(enum mylite_regexp_compile_status status);
static const char *regexp_match_status_message(enum mylite_regexp_match_status status);
static bool regexp_sqlite_find_match(
    sqlite3_context *context,
    const struct regexp_sqlite_function_config *config,
    const struct regexp_sqlite_match_arguments *arguments,
    struct mylite_regexp_match *out_match,
    const unsigned char **out_value,
    int *out_value_length
);
static bool regexp_sqlite_match_value_span(
    sqlite3_context *context,
    const struct regexp_sqlite_function_config *config,
    const struct regexp_sqlite_compiled_program *programs,
    sqlite3_value *value_value,
    size_t start_offset,
    struct mylite_regexp_match *out_match,
    const unsigned char **out_value,
    int *out_value_length
);
static void regexp_string_buffer_deinit(struct regexp_string_buffer *buffer);
static bool regexp_string_buffer_append(
    struct regexp_string_buffer *buffer,
    const char *text,
    size_t length
);
static enum mylite_regexp_compile_status regexp_compile_ascii(
    const char *pattern,
    size_t pattern_length,
    bool case_sensitive,
    struct mylite_regexp_program **out_program
);
static enum mylite_regexp_compile_status regexp_compile_ascii_alternatives(
    const char *pattern,
    size_t pattern_length,
    bool case_sensitive,
    size_t alternative_count,
    struct mylite_regexp_program **out_program
);
static bool regexp_alternation_split_at(
    const char *pattern,
    size_t pattern_length,
    size_t index,
    struct regexp_alternation_scan_state *state
);
static enum mylite_regexp_compile_status regexp_compile_ascii_alternative_branch(
    const char *pattern,
    size_t branch_start,
    size_t branch_end,
    bool case_sensitive,
    struct mylite_regexp_program **out_branch
);
static enum mylite_regexp_compile_status regexp_compile_ascii_sequence(
    const char *pattern,
    size_t pattern_length,
    bool case_sensitive,
    struct mylite_regexp_program **out_program
);
static bool regexp_pattern_has_top_level_alternation(
    const char *pattern,
    size_t pattern_length,
    size_t *out_alternative_count
);
static enum mylite_regexp_compile_status compile_next_token(
    struct mylite_regexp_program *program,
    const char *pattern,
    size_t pattern_length,
    size_t *index
);
static enum mylite_regexp_compile_status compile_atom_token(
    struct mylite_regexp_program *program,
    const char *pattern,
    size_t pattern_length,
    size_t *index,
    struct regexp_token *out_token
);
static enum mylite_regexp_compile_status compile_optional_literal_group_token(
    struct mylite_regexp_program *program,
    const char *pattern,
    size_t pattern_length,
    size_t *index,
    struct regexp_token *out_token
);
static enum mylite_regexp_compile_status compile_class_token(
    struct mylite_regexp_program *program,
    const char *pattern,
    size_t pattern_length,
    size_t *index,
    struct regexp_token *out_token
);
static enum mylite_regexp_compile_status parse_class_byte(
    const char *pattern,
    size_t pattern_length,
    size_t *index,
    unsigned char *out_byte
);
static enum mylite_regexp_compile_status parse_literal_group_byte(
    const char *pattern,
    size_t pattern_length,
    size_t *index,
    unsigned char *out_byte
);
static enum mylite_regexp_compile_status parse_escaped_literal(
    const char *pattern,
    size_t pattern_length,
    size_t *index,
    unsigned char *out_byte
);
static enum mylite_regexp_compile_status parse_fixed_repetition_count(
    struct regexp_parse_cursor *cursor,
    size_t *out_count
);
static bool is_supported_literal_escape(unsigned char byte);
static bool is_supported_class_escape(unsigned char byte);
static bool is_quantifier_byte(unsigned char byte);
static bool is_unsupported_regex_metacharacter(unsigned char byte);
static enum mylite_regexp_match_status regexp_program_match_ascii(
    const struct mylite_regexp_program *program,
    const char *value,
    size_t value_length,
    bool *out_matches
);
static enum mylite_regexp_match_status regexp_program_find_ascii(
    const struct mylite_regexp_program *program,
    const char *value,
    size_t value_length,
    size_t start_offset,
    struct mylite_regexp_match *out_match
);
static enum mylite_regexp_match_status regexp_program_find_sequence_ascii(
    const struct mylite_regexp_program *program,
    const struct regexp_find_input *input,
    struct mylite_regexp_match *out_match
);
static enum mylite_regexp_match_status regexp_program_find_alternatives_ascii(
    const struct mylite_regexp_program *program,
    const struct regexp_find_input *input,
    struct mylite_regexp_match *out_match
);
static void regexp_program_find_anchored_sequence_ascii(
    const struct regexp_match_span_context *span_context,
    size_t start_offset,
    struct mylite_regexp_match *out_match
);
static void regexp_program_find_unanchored_sequence_ascii(
    const struct regexp_match_span_context *span_context,
    size_t start_offset,
    struct mylite_regexp_match *out_match
);
static void regexp_program_free_shallow(struct mylite_regexp_program *program);
static enum mylite_regexp_compile_status append_token(
    struct mylite_regexp_program *program,
    struct regexp_token token
);
static enum mylite_regexp_compile_status append_repeated_token(
    struct mylite_regexp_program *program,
    struct regexp_token token,
    size_t count
);
static enum mylite_regexp_compile_status append_class_range(
    struct mylite_regexp_program *program,
    unsigned char first,
    unsigned char last
);
static enum mylite_regexp_compile_status append_literal_sequence_byte(
    struct mylite_regexp_program *program,
    unsigned char byte
);
static unsigned char normalize_ascii_byte(
    const struct mylite_regexp_program *program,
    unsigned char byte
);
static unsigned char fold_ascii(unsigned char byte);
static bool value_length_is_supported(size_t value_length);
static bool regexp_value_is_supported_ascii(const char *value, size_t value_length);
static void fill_match_table(
    const struct mylite_regexp_program *program,
    const unsigned char *value,
    size_t value_length,
    unsigned char *matches
);
static bool match_span_from(
    const struct regexp_match_span_context *context,
    struct regexp_match_span_position position,
    size_t *out_end
);
static bool match_span_advance(
    const struct regexp_match_span_context *context,
    struct regexp_match_span_position *position
);
static bool match_span_advance_optional_literal_sequence(
    const struct regexp_match_span_context *context,
    const struct regexp_token *token,
    struct regexp_match_span_position *position
);
static bool match_span_advance_one(
    const struct regexp_match_span_context *context,
    bool current_byte_matches,
    struct regexp_match_span_position *position
);
static bool match_span_advance_zero_or_one(
    const struct regexp_match_span_context *context,
    bool current_byte_matches,
    struct regexp_match_span_position *position
);
static bool match_span_advance_repeated(
    const struct regexp_match_span_context *context,
    const struct regexp_token *token,
    bool require_one,
    struct regexp_match_span_position *position
);
static size_t token_match_run_length(
    const struct mylite_regexp_program *program,
    const struct regexp_token *token,
    const unsigned char *value,
    size_t value_length,
    size_t value_index
);
static bool match_table_get(
    const unsigned char *matches,
    size_t value_length,
    size_t token_index,
    size_t value_index
);
static void match_table_set(
    unsigned char *matches,
    size_t value_length,
    size_t token_index,
    size_t value_index,
    bool value
);
static size_t match_table_index(size_t value_length, size_t token_index, size_t value_index);
static bool match_token_suffix_from(
    const struct mylite_regexp_program *program,
    const struct regexp_token *token,
    const unsigned char *value,
    size_t value_length,
    size_t token_index,
    size_t value_index,
    const unsigned char *matches
);
static bool match_optional_literal_sequence_suffix_from(
    const struct mylite_regexp_program *program,
    const struct regexp_token *token,
    const unsigned char *value,
    size_t value_length,
    size_t token_index,
    size_t value_index,
    const unsigned char *matches
);
static bool token_matches_byte(
    const struct mylite_regexp_program *program,
    const struct regexp_token *token,
    unsigned char byte
);
static bool literal_sequence_matches_at(
    const struct mylite_regexp_program *program,
    const struct regexp_token *token,
    const unsigned char *value,
    size_t value_length,
    size_t value_index
);
static bool regexp_byte_is_line_terminator(unsigned char byte);
static bool class_contains_byte(
    const struct mylite_regexp_program *program,
    const struct regexp_token *token,
    unsigned char byte
);

int mylite_sqlite_register_regexp_functions(sqlite3 *sqlite) {
    static const struct regexp_sqlite_function_config match_ci = {
        .kind = REGEXP_SQLITE_FUNCTION_MATCH,
        .case_sensitive = false,
    };
    static const struct regexp_sqlite_function_config match_cs = {
        .kind = REGEXP_SQLITE_FUNCTION_MATCH,
        .case_sensitive = true,
    };
    static const struct regexp_sqlite_function_config instr_ci = {
        .kind = REGEXP_SQLITE_FUNCTION_INSTR,
        .case_sensitive = false,
    };
    static const struct regexp_sqlite_function_config instr_cs = {
        .kind = REGEXP_SQLITE_FUNCTION_INSTR,
        .case_sensitive = true,
    };
    static const struct regexp_sqlite_function_config substr_ci = {
        .kind = REGEXP_SQLITE_FUNCTION_SUBSTR,
        .case_sensitive = false,
    };
    static const struct regexp_sqlite_function_config substr_cs = {
        .kind = REGEXP_SQLITE_FUNCTION_SUBSTR,
        .case_sensitive = true,
    };
    static const struct regexp_sqlite_function_config replace_ci = {
        .kind = REGEXP_SQLITE_FUNCTION_REPLACE,
        .case_sensitive = false,
    };
    static const struct regexp_sqlite_function_config replace_cs = {
        .kind = REGEXP_SQLITE_FUNCTION_REPLACE,
        .case_sensitive = true,
    };
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_regexp_ci_ascii",
            .argument_count = 2,
            .text_representation =
                SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS | SQLITE_DETERMINISTIC,
            .application_data = (void *)&match_ci,
            .scalar_callback = regexp_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_regexp_cs_ascii",
            .argument_count = 2,
            .text_representation =
                SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS | SQLITE_DETERMINISTIC,
            .application_data = (void *)&match_cs,
            .scalar_callback = regexp_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_regexp_instr_ci_ascii",
            .argument_count = 2,
            .text_representation =
                SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS | SQLITE_DETERMINISTIC,
            .application_data = (void *)&instr_ci,
            .scalar_callback = regexp_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_regexp_instr_cs_ascii",
            .argument_count = 2,
            .text_representation =
                SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS | SQLITE_DETERMINISTIC,
            .application_data = (void *)&instr_cs,
            .scalar_callback = regexp_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_regexp_substr_ci_ascii",
            .argument_count = 2,
            .text_representation =
                SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS | SQLITE_DETERMINISTIC,
            .application_data = (void *)&substr_ci,
            .scalar_callback = regexp_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_regexp_substr_cs_ascii",
            .argument_count = 2,
            .text_representation =
                SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS | SQLITE_DETERMINISTIC,
            .application_data = (void *)&substr_cs,
            .scalar_callback = regexp_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_regexp_replace_ci_ascii",
            .argument_count = 3,
            .text_representation =
                SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS | SQLITE_DETERMINISTIC,
            .application_data = (void *)&replace_ci,
            .scalar_callback = regexp_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_regexp_replace_cs_ascii",
            .argument_count = 3,
            .text_representation =
                SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS | SQLITE_DETERMINISTIC,
            .application_data = (void *)&replace_cs,
            .scalar_callback = regexp_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
    };

    return mylite_sqlite_register_functions(
        sqlite,
        registrations,
        sizeof(registrations) / sizeof(registrations[0])
    );
}

enum mylite_regexp_compile_status mylite_regexp_compile_ascii_ci(
    const char *pattern,
    size_t pattern_length,
    struct mylite_regexp_program **out_program
) {
    return regexp_compile_ascii(pattern, pattern_length, false, out_program);
}

enum mylite_regexp_compile_status mylite_regexp_compile_ascii_cs(
    const char *pattern,
    size_t pattern_length,
    struct mylite_regexp_program **out_program
) {
    return regexp_compile_ascii(pattern, pattern_length, true, out_program);
}

void mylite_regexp_program_free(void *program) {
    struct mylite_regexp_program *regexp_program = (struct mylite_regexp_program *)program;

    if (regexp_program == NULL) {
        return;
    }

    for (size_t index = 0U; index < regexp_program->alternative_count; ++index) {
        regexp_program_free_shallow(regexp_program->alternatives[index]);
    }
    free((void *)regexp_program->alternatives);
    regexp_program_free_shallow(regexp_program);
}

static void regexp_program_free_shallow(struct mylite_regexp_program *program) {
    if (program == NULL) {
        return;
    }
    free(program->tokens);
    free(program->ranges);
    free(program->literal_sequence_bytes);
    free(program);
}

enum mylite_regexp_match_status mylite_regexp_program_match_ascii_ci(
    const struct mylite_regexp_program *program,
    const char *value,
    size_t value_length,
    bool *out_matches
) {
    return regexp_program_match_ascii(program, value, value_length, out_matches);
}

enum mylite_regexp_match_status mylite_regexp_program_match_ascii_cs(
    const struct mylite_regexp_program *program,
    const char *value,
    size_t value_length,
    bool *out_matches
) {
    return regexp_program_match_ascii(program, value, value_length, out_matches);
}

enum mylite_regexp_match_status mylite_regexp_program_find_ascii_ci(
    const struct mylite_regexp_program *program,
    const char *value,
    size_t value_length,
    size_t start_offset,
    struct mylite_regexp_match *out_match
) {
    return regexp_program_find_ascii(program, value, value_length, start_offset, out_match);
}

enum mylite_regexp_match_status mylite_regexp_program_find_ascii_cs(
    const struct mylite_regexp_program *program,
    const char *value,
    size_t value_length,
    size_t start_offset,
    struct mylite_regexp_match *out_match
) {
    return regexp_program_find_ascii(program, value, value_length, start_offset, out_match);
}

static enum mylite_regexp_compile_status regexp_compile_ascii(
    const char *pattern,
    size_t pattern_length,
    bool case_sensitive,
    struct mylite_regexp_program **out_program
) {
    size_t alternative_count = 0U;

    if (pattern == NULL || out_program == NULL) {
        return MYLITE_REGEXP_COMPILE_UNSUPPORTED;
    }
    *out_program = NULL;
    if (pattern_length > regexp_pattern_length_max) {
        return MYLITE_REGEXP_COMPILE_TOO_LARGE;
    }
    if (regexp_pattern_has_top_level_alternation(pattern, pattern_length, &alternative_count)) {
        return regexp_compile_ascii_alternatives(
            pattern,
            pattern_length,
            case_sensitive,
            alternative_count,
            out_program
        );
    }
    return regexp_compile_ascii_sequence(pattern, pattern_length, case_sensitive, out_program);
}

static enum mylite_regexp_compile_status regexp_compile_ascii_alternatives(
    const char *pattern,
    size_t pattern_length,
    bool case_sensitive,
    size_t alternative_count,
    struct mylite_regexp_program **out_program
) {
    struct mylite_regexp_program *program = NULL;
    size_t branch_start = 0U;
    size_t branch_index = 0U;
    struct regexp_alternation_scan_state scan_state = {
        .in_class = false,
        .escaped = false,
    };

    program = (struct mylite_regexp_program *)calloc(1U, sizeof(*program));
    if (program == NULL) {
        return MYLITE_REGEXP_COMPILE_NOMEM;
    }
    program->case_sensitive = case_sensitive;
    program->alternative_count = alternative_count;
    program->alternatives =
        (struct mylite_regexp_program **)calloc(alternative_count, sizeof(*program->alternatives));
    if (program->alternatives == NULL) {
        mylite_regexp_program_free(program);
        return MYLITE_REGEXP_COMPILE_NOMEM;
    }

    for (size_t index = 0U; index <= pattern_length; ++index) {
        enum mylite_regexp_compile_status status = MYLITE_REGEXP_COMPILE_OK;

        if (!regexp_alternation_split_at(pattern, pattern_length, index, &scan_state)) {
            continue;
        }
        if (branch_index >= alternative_count) {
            mylite_regexp_program_free(program);
            return MYLITE_REGEXP_COMPILE_UNSUPPORTED;
        }
        status = regexp_compile_ascii_alternative_branch(
            pattern,
            branch_start,
            index,
            case_sensitive,
            &program->alternatives[branch_index]
        );
        if (status != MYLITE_REGEXP_COMPILE_OK) {
            mylite_regexp_program_free(program);
            return status;
        }
        ++branch_index;
        branch_start = index + 1U;
    }

    if (branch_index != alternative_count) {
        mylite_regexp_program_free(program);
        return MYLITE_REGEXP_COMPILE_UNSUPPORTED;
    }

    *out_program = program;
    return MYLITE_REGEXP_COMPILE_OK;
}

static bool regexp_alternation_split_at(
    const char *pattern,
    size_t pattern_length,
    size_t index,
    struct regexp_alternation_scan_state *state
) {
    const unsigned char byte = index < pattern_length ? (unsigned char)pattern[index] : 0U;

    if (index == pattern_length) {
        return true;
    }
    if (state->escaped) {
        state->escaped = false;
        return false;
    }
    if (byte == '\\') {
        state->escaped = true;
        return false;
    }
    if (state->in_class) {
        if (byte == ']') {
            state->in_class = false;
        }
        return false;
    }
    if (byte == '[') {
        state->in_class = true;
        return false;
    }
    return byte == '|';
}

static enum mylite_regexp_compile_status regexp_compile_ascii_alternative_branch(
    const char *pattern,
    size_t branch_start,
    size_t branch_end,
    bool case_sensitive,
    struct mylite_regexp_program **out_branch
) {
    const size_t branch_length = branch_end - branch_start;

    if (branch_length == 0U) {
        return MYLITE_REGEXP_COMPILE_UNSUPPORTED;
    }
    return regexp_compile_ascii_sequence(
        pattern + branch_start,
        branch_length,
        case_sensitive,
        out_branch
    );
}

static enum mylite_regexp_compile_status regexp_compile_ascii_sequence(
    const char *pattern,
    size_t pattern_length,
    bool case_sensitive,
    struct mylite_regexp_program **out_program
) {
    struct mylite_regexp_program *program = NULL;
    size_t index = 0U;

    if (pattern == NULL || out_program == NULL) {
        return MYLITE_REGEXP_COMPILE_UNSUPPORTED;
    }
    *out_program = NULL;
    if (pattern_length > regexp_pattern_length_max) {
        return MYLITE_REGEXP_COMPILE_TOO_LARGE;
    }

    program = (struct mylite_regexp_program *)calloc(1U, sizeof(*program));
    if (program == NULL) {
        return MYLITE_REGEXP_COMPILE_NOMEM;
    }
    program->case_sensitive = case_sensitive;

    while (index < pattern_length) {
        enum mylite_regexp_compile_status status =
            compile_next_token(program, pattern, pattern_length, &index);

        if (status != MYLITE_REGEXP_COMPILE_OK) {
            mylite_regexp_program_free(program);
            return status;
        }
    }

    *out_program = program;
    return MYLITE_REGEXP_COMPILE_OK;
}

static bool regexp_pattern_has_top_level_alternation(
    const char *pattern,
    size_t pattern_length,
    size_t *out_alternative_count
) {
    size_t alternative_count = 1U;
    bool in_class = false;
    bool escaped = false;

    for (size_t index = 0U; index < pattern_length; ++index) {
        const unsigned char byte = (unsigned char)pattern[index];

        if (escaped) {
            escaped = false;
            continue;
        }
        if (byte == '\\') {
            escaped = true;
            continue;
        }
        if (in_class) {
            if (byte == ']') {
                in_class = false;
            }
            continue;
        }
        if (byte == '[') {
            in_class = true;
            continue;
        }
        if (byte == '|') {
            ++alternative_count;
        }
    }

    *out_alternative_count = alternative_count;
    return alternative_count > 1U;
}

static enum mylite_regexp_match_status regexp_program_match_ascii(
    const struct mylite_regexp_program *program,
    const char *value,
    size_t value_length,
    bool *out_matches
) {
    struct mylite_regexp_match match = {
        .matched = false,
        .start = 0U,
        .end = 0U,
    };
    enum mylite_regexp_match_status status = MYLITE_REGEXP_MATCH_OK;

    if (out_matches == NULL) {
        return MYLITE_REGEXP_MATCH_VALUE_TOO_LARGE;
    }
    *out_matches = false;
    status = regexp_program_find_ascii(program, value, value_length, 0U, &match);
    if (status == MYLITE_REGEXP_MATCH_OK) {
        *out_matches = match.matched;
    }
    return status;
}

static enum mylite_regexp_match_status regexp_program_find_ascii(
    const struct mylite_regexp_program *program,
    const char *value,
    size_t value_length,
    size_t start_offset,
    struct mylite_regexp_match *out_match
) {
    const struct regexp_find_input input = {
        .value = value,
        .value_length = value_length,
        .start_offset = start_offset,
    };

    if (program == NULL || value == NULL || out_match == NULL) {
        return MYLITE_REGEXP_MATCH_VALUE_TOO_LARGE;
    }
    *out_match = (struct mylite_regexp_match){
        .matched = false,
        .start = 0U,
        .end = 0U,
    };
    if (!value_length_is_supported(value_length)) {
        return MYLITE_REGEXP_MATCH_VALUE_TOO_LARGE;
    }
    if (!regexp_value_is_supported_ascii(value, value_length)) {
        return MYLITE_REGEXP_MATCH_UNSUPPORTED_VALUE;
    }
    if (program->alternative_count > 0U) {
        return regexp_program_find_alternatives_ascii(program, &input, out_match);
    }
    return regexp_program_find_sequence_ascii(program, &input, out_match);
}

static enum mylite_regexp_match_status regexp_program_find_sequence_ascii(
    const struct mylite_regexp_program *program,
    const struct regexp_find_input *input,
    struct mylite_regexp_match *out_match
) {
    unsigned char *matches = NULL;
    size_t cell_count = 0U;
    struct regexp_match_span_context span_context = {
        .program = program,
        .value = (const unsigned char *)input->value,
        .value_length = input->value_length,
        .matches = NULL,
    };

    if ((program->token_count + 1U) > SIZE_MAX / (input->value_length + 1U)) {
        return MYLITE_REGEXP_MATCH_VALUE_TOO_LARGE;
    }

    cell_count = (program->token_count + 1U) * (input->value_length + 1U);
    matches = (unsigned char *)calloc(cell_count, sizeof(*matches));
    if (matches == NULL) {
        return MYLITE_REGEXP_MATCH_NOMEM;
    }

    fill_match_table(program, (const unsigned char *)input->value, input->value_length, matches);
    span_context.matches = matches;
    if (program->anchored_start) {
        regexp_program_find_anchored_sequence_ascii(&span_context, input->start_offset, out_match);
    } else {
        regexp_program_find_unanchored_sequence_ascii(
            &span_context,
            input->start_offset,
            out_match
        );
    }

    free(matches);
    return MYLITE_REGEXP_MATCH_OK;
}

static enum mylite_regexp_match_status regexp_program_find_alternatives_ascii(
    const struct mylite_regexp_program *program,
    const struct regexp_find_input *input,
    struct mylite_regexp_match *out_match
) {
    bool matched = false;

    for (size_t index = 0U; index < program->alternative_count; ++index) {
        struct mylite_regexp_match alternative_match = {
            .matched = false,
            .start = 0U,
            .end = 0U,
        };
        enum mylite_regexp_match_status status = regexp_program_find_sequence_ascii(
            program->alternatives[index],
            input,
            &alternative_match
        );

        if (status != MYLITE_REGEXP_MATCH_OK) {
            return status;
        }
        if (!alternative_match.matched) {
            continue;
        }
        if (!matched || alternative_match.start < out_match->start) {
            *out_match = alternative_match;
            matched = true;
        }
    }

    return MYLITE_REGEXP_MATCH_OK;
}

static void regexp_program_find_anchored_sequence_ascii(
    const struct regexp_match_span_context *span_context,
    size_t start_offset,
    struct mylite_regexp_match *out_match
) {
    const struct regexp_match_span_position position = {
        .token_index = 0U,
        .value_index = 0U,
    };

    if (start_offset != 0U ||
        !match_table_get(span_context->matches, span_context->value_length, 0U, 0U)) {
        return;
    }

    out_match->matched = match_span_from(span_context, position, &out_match->end);
    out_match->start = 0U;
}

static void regexp_program_find_unanchored_sequence_ascii(
    const struct regexp_match_span_context *span_context,
    size_t start_offset,
    struct mylite_regexp_match *out_match
) {
    if (start_offset > span_context->value_length) {
        return;
    }
    for (size_t start = start_offset; start <= span_context->value_length; ++start) {
        const struct regexp_match_span_position position = {
            .token_index = 0U,
            .value_index = start,
        };

        if (!match_table_get(span_context->matches, span_context->value_length, 0U, start)) {
            continue;
        }
        out_match->matched = match_span_from(span_context, position, &out_match->end);
        out_match->start = start;
        if (out_match->matched) {
            return;
        }
    }
}

static void regexp_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    const struct regexp_sqlite_function_config *config =
        (const struct regexp_sqlite_function_config *)sqlite3_user_data(context);

    if (context == NULL || config == NULL) {
        sqlite3_result_error(context, "invalid MyLite regexp callback", -1);
        return;
    }

    switch (config->kind) {
    case REGEXP_SQLITE_FUNCTION_MATCH:
        regexp_sqlite_match_callback(context, config, argc, argv);
        return;
    case REGEXP_SQLITE_FUNCTION_INSTR:
        regexp_sqlite_instr_callback(context, config, argc, argv);
        return;
    case REGEXP_SQLITE_FUNCTION_SUBSTR:
        regexp_sqlite_substr_callback(context, config, argc, argv);
        return;
    case REGEXP_SQLITE_FUNCTION_REPLACE:
        regexp_sqlite_replace_callback(context, config, argc, argv);
        return;
    }

    sqlite3_result_error(context, "invalid MyLite regexp function", -1);
}

static void regexp_sqlite_match_callback(
    sqlite3_context *context,
    const struct regexp_sqlite_function_config *config,
    int argc,
    sqlite3_value **argv
) {
    struct mylite_regexp_program *program = NULL;
    struct mylite_regexp_program *compiled_program = NULL;
    const unsigned char *value = NULL;
    int value_length = 0;
    bool matches = false;
    enum mylite_regexp_match_status match_status = MYLITE_REGEXP_MATCH_OK;

    if (context == NULL || argc != 2 || argv == NULL || argv[0] == NULL || argv[1] == NULL) {
        sqlite3_result_error(context, "invalid MyLite regexp callback", -1);
        return;
    }
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }

    if (!regexp_sqlite_get_program(
            context,
            argv[0],
            config->case_sensitive,
            &program,
            &compiled_program
        )) {
        return;
    }

    if (sqlite3_value_type(argv[1]) == SQLITE_NULL) {
        regexp_sqlite_cache_program(context, compiled_program);
        sqlite3_result_null(context);
        return;
    }

    value = sqlite3_value_text(argv[1]);
    value_length = sqlite3_value_bytes(argv[1]);
    if (value == NULL || value_length < 0) {
        regexp_sqlite_free_compiled_program(compiled_program);
        sqlite3_result_error_nomem(context);
        return;
    }

    if (config->case_sensitive) {
        match_status = mylite_regexp_program_match_ascii_cs(
            program,
            (const char *)value,
            (size_t)value_length,
            &matches
        );
    } else {
        match_status = mylite_regexp_program_match_ascii_ci(
            program,
            (const char *)value,
            (size_t)value_length,
            &matches
        );
    }
    if (match_status != MYLITE_REGEXP_MATCH_OK) {
        regexp_sqlite_free_compiled_program(compiled_program);
        regexp_sqlite_result_match_error(context, match_status);
        return;
    }

    regexp_sqlite_cache_program(context, compiled_program);
    if (matches) {
        sqlite3_result_int(context, 1);
    } else {
        sqlite3_result_int(context, 0);
    }
}

static void regexp_sqlite_instr_callback(
    sqlite3_context *context,
    const struct regexp_sqlite_function_config *config,
    int argc,
    sqlite3_value **argv
) {
    struct mylite_regexp_match match = {
        .matched = false,
        .start = 0U,
        .end = 0U,
    };
    const unsigned char *value = NULL;
    int value_length = 0;
    bool pattern_is_empty = false;

    if (context == NULL || config == NULL || argc != 2 || argv == NULL || argv[0] == NULL ||
        argv[1] == NULL) {
        sqlite3_result_error(context, "invalid MyLite regexp_instr callback", -1);
        return;
    }
    if (regexp_sqlite_any_argument_is_null(argc, argv)) {
        sqlite3_result_null(context);
        return;
    }
    if (!regexp_sqlite_pattern_is_empty(context, argv[0], &pattern_is_empty)) {
        return;
    }
    if (pattern_is_empty) {
        regexp_sqlite_result_illegal_argument(context);
        return;
    }
    const struct regexp_sqlite_match_arguments match_arguments = {
        .pattern_value = argv[0],
        .value_value = argv[1],
    };
    if (!regexp_sqlite_find_match(
            context,
            config,
            &match_arguments,
            &match,
            &value,
            &value_length
        )) {
        return;
    }
    (void)value;
    (void)value_length;
    if (!match.matched) {
        sqlite3_result_int(context, 0);
        return;
    }
    sqlite3_result_int64(context, (sqlite3_int64)match.start + 1);
}

static void regexp_sqlite_substr_callback(
    sqlite3_context *context,
    const struct regexp_sqlite_function_config *config,
    int argc,
    sqlite3_value **argv
) {
    struct mylite_regexp_match match = {
        .matched = false,
        .start = 0U,
        .end = 0U,
    };
    const unsigned char *value = NULL;
    int value_length = 0;
    bool pattern_is_empty = false;

    if (context == NULL || config == NULL || argc != 2 || argv == NULL || argv[0] == NULL ||
        argv[1] == NULL) {
        sqlite3_result_error(context, "invalid MyLite regexp_substr callback", -1);
        return;
    }
    if (regexp_sqlite_any_argument_is_null(argc, argv)) {
        sqlite3_result_null(context);
        return;
    }
    if (!regexp_sqlite_pattern_is_empty(context, argv[0], &pattern_is_empty)) {
        return;
    }
    if (pattern_is_empty) {
        regexp_sqlite_result_illegal_argument(context);
        return;
    }
    const struct regexp_sqlite_match_arguments match_arguments = {
        .pattern_value = argv[0],
        .value_value = argv[1],
    };
    if (!regexp_sqlite_find_match(
            context,
            config,
            &match_arguments,
            &match,
            &value,
            &value_length
        )) {
        return;
    }
    (void)value_length;
    if (!match.matched) {
        sqlite3_result_null(context);
        return;
    }
    sqlite3_result_text(
        context,
        (const char *)&value[match.start],
        (int)(match.end - match.start),
        SQLITE_TRANSIENT
    );
}

static void regexp_sqlite_replace_callback(
    sqlite3_context *context,
    const struct regexp_sqlite_function_config *config,
    int argc,
    sqlite3_value **argv
) {
    struct regexp_sqlite_compiled_program programs = {
        .program = NULL,
        .compiled_program = NULL,
    };
    struct regexp_sqlite_replace_arguments arguments = {
        .value = NULL,
        .replacement = NULL,
        .value_length = 0U,
        .replacement_length = 0U,
    };
    struct regexp_string_buffer buffer = {0};
    bool pattern_is_empty = false;

    if (context == NULL || config == NULL || argc != 3 || argv == NULL || argv[0] == NULL ||
        argv[1] == NULL || argv[2] == NULL) {
        sqlite3_result_error(context, "invalid MyLite regexp_replace callback", -1);
        return;
    }
    if (regexp_sqlite_any_argument_is_null(argc, argv)) {
        sqlite3_result_null(context);
        return;
    }
    if (!regexp_sqlite_pattern_is_empty(context, argv[0], &pattern_is_empty)) {
        return;
    }
    if (pattern_is_empty) {
        regexp_sqlite_result_illegal_argument(context);
        return;
    }
    if (!regexp_sqlite_get_program(
            context,
            argv[0],
            config->case_sensitive,
            &programs.program,
            &programs.compiled_program
        )) {
        return;
    }

    if (!regexp_sqlite_replace_prepare_arguments(context, argv, &arguments) ||
        !regexp_sqlite_replace_all(context, config, &programs, &arguments, &buffer)) {
        regexp_string_buffer_deinit(&buffer);
        regexp_sqlite_free_compiled_program(programs.compiled_program);
        return;
    }

    regexp_sqlite_cache_program(context, programs.compiled_program);
    if (buffer.data == NULL) {
        sqlite3_result_text(context, "", 0, SQLITE_STATIC);
        return;
    }
    sqlite3_result_text(context, buffer.data, (int)buffer.length, sqlite3_free);
}

static bool regexp_sqlite_replace_prepare_arguments(
    sqlite3_context *context,
    sqlite3_value **argv,
    struct regexp_sqlite_replace_arguments *out_arguments
) {
    int value_length = 0;
    int replacement_length = 0;

    out_arguments->replacement = sqlite3_value_text(argv[2]);
    replacement_length = sqlite3_value_bytes(argv[2]);
    if (out_arguments->replacement == NULL || replacement_length < 0) {
        sqlite3_result_error_nomem(context);
        return false;
    }
    if (!regexp_value_is_supported_ascii(
            (const char *)out_arguments->replacement,
            (size_t)replacement_length
        )) {
        regexp_sqlite_result_match_error(context, MYLITE_REGEXP_MATCH_UNSUPPORTED_VALUE);
        return false;
    }

    out_arguments->value = sqlite3_value_text(argv[1]);
    value_length = sqlite3_value_bytes(argv[1]);
    if (out_arguments->value == NULL || value_length < 0) {
        sqlite3_result_error_nomem(context);
        return false;
    }
    if (!regexp_value_is_supported_ascii(
            (const char *)out_arguments->value,
            (size_t)value_length
        )) {
        regexp_sqlite_result_match_error(context, MYLITE_REGEXP_MATCH_UNSUPPORTED_VALUE);
        return false;
    }

    out_arguments->value_length = (size_t)value_length;
    out_arguments->replacement_length = (size_t)replacement_length;
    return true;
}

static bool regexp_sqlite_replace_all(
    sqlite3_context *context,
    const struct regexp_sqlite_function_config *config,
    const struct regexp_sqlite_compiled_program *programs,
    const struct regexp_sqlite_replace_arguments *arguments,
    struct regexp_string_buffer *buffer
) {
    size_t append_offset = 0U;
    size_t search_offset = 0U;

    if (arguments->value_length == 0U) {
        return true;
    }

    while (search_offset <= arguments->value_length) {
        struct mylite_regexp_match match = {
            .matched = false,
            .start = 0U,
            .end = 0U,
        };

        if (!regexp_sqlite_replace_find_match(
                context,
                config,
                programs,
                arguments,
                search_offset,
                &match
            )) {
            return false;
        }
        if (!match.matched) {
            break;
        }
        if (!regexp_string_buffer_append(
                buffer,
                (const char *)&arguments->value[append_offset],
                match.start - append_offset
            ) ||
            !regexp_string_buffer_append(
                buffer,
                (const char *)arguments->replacement,
                arguments->replacement_length
            )) {
            sqlite3_result_error_nomem(context);
            return false;
        }

        append_offset = match.end;
        search_offset = match.end;
        if (match.start == match.end) {
            if (search_offset >= arguments->value_length) {
                break;
            }
            if (!regexp_string_buffer_append(
                    buffer,
                    (const char *)&arguments->value[search_offset],
                    1U
                )) {
                sqlite3_result_error_nomem(context);
                return false;
            }
            ++search_offset;
            append_offset = search_offset;
        }
    }
    if (!regexp_string_buffer_append(
            buffer,
            (const char *)&arguments->value[append_offset],
            arguments->value_length - append_offset
        )) {
        sqlite3_result_error_nomem(context);
        return false;
    }
    return true;
}

static bool regexp_sqlite_replace_find_match(
    sqlite3_context *context,
    const struct regexp_sqlite_function_config *config,
    const struct regexp_sqlite_compiled_program *programs,
    const struct regexp_sqlite_replace_arguments *arguments,
    size_t search_offset,
    struct mylite_regexp_match *out_match
) {
    enum mylite_regexp_match_status match_status = MYLITE_REGEXP_MATCH_OK;

    if (config->case_sensitive) {
        match_status = mylite_regexp_program_find_ascii_cs(
            programs->program,
            (const char *)arguments->value,
            arguments->value_length,
            search_offset,
            out_match
        );
    } else {
        match_status = mylite_regexp_program_find_ascii_ci(
            programs->program,
            (const char *)arguments->value,
            arguments->value_length,
            search_offset,
            out_match
        );
    }
    if (match_status != MYLITE_REGEXP_MATCH_OK) {
        regexp_sqlite_result_match_error(context, match_status);
        return false;
    }
    return true;
}

static bool regexp_sqlite_find_match(
    sqlite3_context *context,
    const struct regexp_sqlite_function_config *config,
    const struct regexp_sqlite_match_arguments *arguments,
    struct mylite_regexp_match *out_match,
    const unsigned char **out_value,
    int *out_value_length
) {
    struct regexp_sqlite_compiled_program programs = {
        .program = NULL,
        .compiled_program = NULL,
    };

    if (!regexp_sqlite_get_program(
            context,
            arguments->pattern_value,
            config->case_sensitive,
            &programs.program,
            &programs.compiled_program
        )) {
        return false;
    }
    return regexp_sqlite_match_value_span(
        context,
        config,
        &programs,
        arguments->value_value,
        0U,
        out_match,
        out_value,
        out_value_length
    );
}

static bool regexp_sqlite_match_value_span(
    sqlite3_context *context,
    const struct regexp_sqlite_function_config *config,
    const struct regexp_sqlite_compiled_program *programs,
    sqlite3_value *value_value,
    size_t start_offset,
    struct mylite_regexp_match *out_match,
    const unsigned char **out_value,
    int *out_value_length
) {
    enum mylite_regexp_match_status match_status = MYLITE_REGEXP_MATCH_OK;

    if (out_match == NULL || out_value == NULL || out_value_length == NULL) {
        regexp_sqlite_free_compiled_program(programs->compiled_program);
        sqlite3_result_error(context, "invalid MyLite regexp match", -1);
        return false;
    }
    *out_match = (struct mylite_regexp_match){
        .matched = false,
        .start = 0U,
        .end = 0U,
    };
    *out_value = sqlite3_value_text(value_value);
    *out_value_length = sqlite3_value_bytes(value_value);
    if (*out_value == NULL || *out_value_length < 0) {
        regexp_sqlite_free_compiled_program(programs->compiled_program);
        sqlite3_result_error_nomem(context);
        return false;
    }

    if (config->case_sensitive) {
        match_status = mylite_regexp_program_find_ascii_cs(
            programs->program,
            (const char *)*out_value,
            (size_t)*out_value_length,
            start_offset,
            out_match
        );
    } else {
        match_status = mylite_regexp_program_find_ascii_ci(
            programs->program,
            (const char *)*out_value,
            (size_t)*out_value_length,
            start_offset,
            out_match
        );
    }
    if (match_status != MYLITE_REGEXP_MATCH_OK) {
        regexp_sqlite_free_compiled_program(programs->compiled_program);
        regexp_sqlite_result_match_error(context, match_status);
        return false;
    }

    regexp_sqlite_cache_program(context, programs->compiled_program);
    return true;
}

static void regexp_string_buffer_deinit(struct regexp_string_buffer *buffer) {
    if (buffer == NULL) {
        return;
    }
    sqlite3_free(buffer->data);
    *buffer = (struct regexp_string_buffer){0};
}

static bool regexp_string_buffer_append(
    struct regexp_string_buffer *buffer,
    const char *text,
    size_t length
) {
    char *data = NULL;
    size_t needed = 0U;
    size_t capacity = 0U;

    if (buffer == NULL || (text == NULL && length != 0U)) {
        return false;
    }
    if (length == 0U) {
        return true;
    }
    if (buffer->length > SIZE_MAX - length) {
        return false;
    }
    needed = buffer->length + length;
    if (needed <= buffer->capacity) {
        memcpy(buffer->data + buffer->length, text, length);
        buffer->length = needed;
        return true;
    }

    capacity = buffer->capacity == 0U ? regexp_string_buffer_initial_capacity : buffer->capacity;
    while (capacity < needed) {
        if (capacity > SIZE_MAX / 2U) {
            capacity = needed;
            break;
        }
        capacity *= 2U;
    }
    data = (char *)sqlite3_realloc64(buffer->data, (sqlite3_uint64)capacity);
    if (data == NULL) {
        return false;
    }
    buffer->data = data;
    buffer->capacity = capacity;
    memcpy(buffer->data + buffer->length, text, length);
    buffer->length = needed;
    return true;
}

static bool regexp_sqlite_get_program(
    sqlite3_context *context,
    sqlite3_value *pattern_value,
    bool case_sensitive,
    struct mylite_regexp_program **out_program,
    struct mylite_regexp_program **out_compiled_program
) {
    const unsigned char *pattern = NULL;
    int pattern_length = 0;
    enum mylite_regexp_compile_status compile_status = MYLITE_REGEXP_COMPILE_OK;

    *out_compiled_program = NULL;
    *out_program = (struct mylite_regexp_program *)sqlite3_get_auxdata(context, 0);
    if (*out_program != NULL) {
        return true;
    }

    pattern = sqlite3_value_text(pattern_value);
    pattern_length = sqlite3_value_bytes(pattern_value);
    if (pattern == NULL || pattern_length < 0) {
        sqlite3_result_error_nomem(context);
        return false;
    }

    if (case_sensitive) {
        compile_status = mylite_regexp_compile_ascii_cs(
            (const char *)pattern,
            (size_t)pattern_length,
            out_compiled_program
        );
    } else {
        compile_status = mylite_regexp_compile_ascii_ci(
            (const char *)pattern,
            (size_t)pattern_length,
            out_compiled_program
        );
    }
    if (compile_status != MYLITE_REGEXP_COMPILE_OK) {
        regexp_sqlite_result_compile_error(context, compile_status);
        return false;
    }

    *out_program = *out_compiled_program;
    return true;
}

static bool regexp_sqlite_pattern_is_empty(
    sqlite3_context *context,
    sqlite3_value *pattern_value,
    bool *out_is_empty
) {
    const unsigned char *pattern = NULL;
    int pattern_length = 0;

    if (out_is_empty == NULL) {
        sqlite3_result_error(context, "invalid MyLite regexp pattern", -1);
        return false;
    }
    *out_is_empty = false;
    pattern = sqlite3_value_text(pattern_value);
    pattern_length = sqlite3_value_bytes(pattern_value);
    if (pattern == NULL || pattern_length < 0) {
        sqlite3_result_error_nomem(context);
        return false;
    }
    (void)pattern;
    *out_is_empty = pattern_length == 0;
    return true;
}

static bool regexp_sqlite_any_argument_is_null(int argc, sqlite3_value **argv) {
    for (int index = 0; index < argc; ++index) {
        if (sqlite3_value_type(argv[index]) == SQLITE_NULL) {
            return true;
        }
    }
    return false;
}

static void regexp_sqlite_result_illegal_argument(sqlite3_context *context) {
    const char *message = "Illegal argument to a regular expression.";

    regexp_sqlite_set_diagnostic(context, mysql_error_regexp_illegal_argument, "HY000", message);
    sqlite3_result_error(context, message, -1);
}

static void regexp_sqlite_result_compile_error(
    sqlite3_context *context,
    enum mylite_regexp_compile_status status
) {
    if (status == MYLITE_REGEXP_COMPILE_NOMEM) {
        sqlite3_result_error_nomem(context);
        return;
    }
    regexp_sqlite_set_compile_diagnostic(context, status);
    sqlite3_result_error(context, regexp_compile_status_message(status), -1);
}

static void regexp_sqlite_result_match_error(
    sqlite3_context *context,
    enum mylite_regexp_match_status status
) {
    if (status == MYLITE_REGEXP_MATCH_NOMEM) {
        sqlite3_result_error_nomem(context);
        return;
    }
    regexp_sqlite_set_match_diagnostic(context, status);
    sqlite3_result_error(context, regexp_match_status_message(status), -1);
}

static void regexp_sqlite_set_compile_diagnostic(
    sqlite3_context *context,
    enum mylite_regexp_compile_status status
) {
    const char *message = regexp_compile_status_message(status);

    if (status == MYLITE_REGEXP_COMPILE_UNCLOSED_BRACKET) {
        regexp_sqlite_set_diagnostic(context, mysql_error_regular_expression, "HY000", message);
        return;
    }
    if (status == MYLITE_REGEXP_COMPILE_INVALID_RANGE) {
        regexp_sqlite_set_diagnostic(
            context,
            mysql_error_regular_expression_character_range,
            "HY000",
            message
        );
        return;
    }
    regexp_sqlite_set_diagnostic(context, mysql_error_parse, "42000", message);
}

static void regexp_sqlite_set_match_diagnostic(
    sqlite3_context *context,
    enum mylite_regexp_match_status status
) {
    regexp_sqlite_set_diagnostic(
        context,
        mysql_error_parse,
        "42000",
        regexp_match_status_message(status)
    );
}

static void regexp_sqlite_set_diagnostic(
    sqlite3_context *context,
    int error_code,
    const char *sqlstate,
    const char *message
) {
    struct mylite_db *database = mylite_sqlite_bootstrap_owner_from_context(context);

    if (database == NULL) {
        return;
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        error_code,
        sqlstate,
        message
    );
}

static void regexp_sqlite_cache_program(
    sqlite3_context *context,
    struct mylite_regexp_program *compiled_program
) {
    if (compiled_program == NULL) {
        return;
    }
    sqlite3_set_auxdata(context, 0, compiled_program, mylite_regexp_program_free);
}

static void regexp_sqlite_free_compiled_program(struct mylite_regexp_program *compiled_program) {
    if (compiled_program == NULL) {
        return;
    }
    mylite_regexp_program_free(compiled_program);
}

static const char *regexp_compile_status_message(enum mylite_regexp_compile_status status) {
    switch (status) {
    case MYLITE_REGEXP_COMPILE_UNCLOSED_BRACKET:
        return "The regular expression contains an unclosed bracket expression.";
    case MYLITE_REGEXP_COMPILE_INVALID_RANGE:
        return "The regular expression contains an invalid character range.";
    case MYLITE_REGEXP_COMPILE_DANGLING_ESCAPE:
        return "The regular expression contains a trailing backslash.";
    case MYLITE_REGEXP_COMPILE_TOO_LARGE:
        return "The regular expression pattern is too large for MyLite's baseline subset.";
    case MYLITE_REGEXP_COMPILE_UNSUPPORTED:
        return "The regular expression uses syntax outside MyLite's baseline subset.";
    case MYLITE_REGEXP_COMPILE_NOMEM:
    case MYLITE_REGEXP_COMPILE_OK:
        break;
    }
    return "The regular expression is not supported by MyLite's baseline subset.";
}

static const char *regexp_match_status_message(enum mylite_regexp_match_status status) {
    switch (status) {
    case MYLITE_REGEXP_MATCH_UNSUPPORTED_VALUE:
        return "The regular expression input supports only ASCII text without NUL bytes.";
    case MYLITE_REGEXP_MATCH_VALUE_TOO_LARGE:
        return "The regular expression input is too large for MyLite's baseline subset.";
    case MYLITE_REGEXP_MATCH_NOMEM:
    case MYLITE_REGEXP_MATCH_OK:
        break;
    }
    return "The regular expression match failed.";
}

static enum mylite_regexp_compile_status compile_next_token(
    struct mylite_regexp_program *program,
    const char *pattern,
    size_t pattern_length,
    size_t *index
) {
    struct regexp_token token = {
        .kind = REGEXP_TOKEN_LITERAL,
        .quantifier = REGEXP_QUANTIFIER_ONE,
    };
    enum mylite_regexp_compile_status status = MYLITE_REGEXP_COMPILE_OK;
    unsigned char byte = 0U;

    byte = (unsigned char)pattern[*index];
    if (byte > ascii_max) {
        return MYLITE_REGEXP_COMPILE_UNSUPPORTED;
    }
    if (byte == '^') {
        if (program->token_count == 0U && !program->anchored_start) {
            program->anchored_start = true;
            ++(*index);
            return MYLITE_REGEXP_COMPILE_OK;
        }
        return MYLITE_REGEXP_COMPILE_UNSUPPORTED;
    }
    if (byte == '$') {
        if (*index + 1U != pattern_length) {
            return MYLITE_REGEXP_COMPILE_UNSUPPORTED;
        }
        token.kind = REGEXP_TOKEN_END_ANCHOR;
        ++(*index);
        return append_token(program, token);
    }
    if (byte == '(') {
        status =
            compile_optional_literal_group_token(program, pattern, pattern_length, index, &token);
        if (status != MYLITE_REGEXP_COMPILE_OK) {
            return status;
        }
        return append_token(program, token);
    }

    status = compile_atom_token(program, pattern, pattern_length, index, &token);
    if (status != MYLITE_REGEXP_COMPILE_OK) {
        return status;
    }
    if (*index < pattern_length) {
        switch ((unsigned char)pattern[*index]) {
        case '?':
            token.quantifier = REGEXP_QUANTIFIER_ZERO_OR_ONE;
            ++(*index);
            break;
        case '*':
            token.quantifier = REGEXP_QUANTIFIER_ZERO_OR_MORE;
            ++(*index);
            break;
        case '+':
            token.quantifier = REGEXP_QUANTIFIER_ONE_OR_MORE;
            ++(*index);
            break;
        case '{': {
            size_t count = 0U;
            struct regexp_parse_cursor cursor = {
                .pattern = pattern,
                .pattern_length = pattern_length,
                .index = *index,
            };

            status = parse_fixed_repetition_count(&cursor, &count);
            *index = cursor.index;
            if (status != MYLITE_REGEXP_COMPILE_OK) {
                return status;
            }
            return append_repeated_token(program, token, count);
        }
        default:
            break;
        }
    }

    return append_token(program, token);
}

static enum mylite_regexp_compile_status compile_atom_token(
    struct mylite_regexp_program *program,
    const char *pattern,
    size_t pattern_length,
    size_t *index,
    struct regexp_token *out_token
) {
    unsigned char byte = (unsigned char)pattern[*index];

    if (byte == '.') {
        out_token->kind = REGEXP_TOKEN_ANY;
        ++(*index);
        return MYLITE_REGEXP_COMPILE_OK;
    }
    if (byte == '[') {
        return compile_class_token(program, pattern, pattern_length, index, out_token);
    }
    if (byte == '\\') {
        enum mylite_regexp_compile_status status =
            parse_escaped_literal(pattern, pattern_length, index, &byte);

        if (status != MYLITE_REGEXP_COMPILE_OK) {
            return status;
        }
        out_token->kind = REGEXP_TOKEN_LITERAL;
        out_token->literal = normalize_ascii_byte(program, byte);
        return MYLITE_REGEXP_COMPILE_OK;
    }
    if (byte > ascii_max || is_quantifier_byte(byte) || is_unsupported_regex_metacharacter(byte)) {
        return MYLITE_REGEXP_COMPILE_UNSUPPORTED;
    }

    out_token->kind = REGEXP_TOKEN_LITERAL;
    out_token->literal = normalize_ascii_byte(program, byte);
    ++(*index);
    return MYLITE_REGEXP_COMPILE_OK;
}

static enum mylite_regexp_compile_status compile_optional_literal_group_token(
    struct mylite_regexp_program *program,
    const char *pattern,
    size_t pattern_length,
    size_t *index,
    struct regexp_token *out_token
) {
    const size_t sequence_start = program->literal_sequence_length;
    enum mylite_regexp_compile_status status = MYLITE_REGEXP_COMPILE_OK;

    ++(*index);
    if (*index >= pattern_length || pattern[*index] == ')') {
        return MYLITE_REGEXP_COMPILE_UNSUPPORTED;
    }

    while (*index < pattern_length && pattern[*index] != ')') {
        unsigned char byte = 0U;

        status = parse_literal_group_byte(pattern, pattern_length, index, &byte);
        if (status != MYLITE_REGEXP_COMPILE_OK) {
            return status;
        }
        status = append_literal_sequence_byte(program, normalize_ascii_byte(program, byte));
        if (status != MYLITE_REGEXP_COMPILE_OK) {
            return status;
        }
    }
    if (*index >= pattern_length || pattern[*index] != ')' || *index + 1U >= pattern_length ||
        pattern[*index + 1U] != '?') {
        return MYLITE_REGEXP_COMPILE_UNSUPPORTED;
    }
    *index += 2U;

    out_token->kind = REGEXP_TOKEN_OPTIONAL_LITERAL_SEQUENCE;
    out_token->literal_sequence_start = sequence_start;
    out_token->literal_sequence_length = program->literal_sequence_length - sequence_start;
    return MYLITE_REGEXP_COMPILE_OK;
}

static enum mylite_regexp_compile_status compile_class_token(
    struct mylite_regexp_program *program,
    const char *pattern,
    size_t pattern_length,
    size_t *index,
    struct regexp_token *out_token
) {
    size_t range_start = program->range_count;
    bool class_is_negated = false;

    ++(*index);
    if (*index < pattern_length && pattern[*index] == '^') {
        class_is_negated = true;
        ++(*index);
    }
    if (*index >= pattern_length) {
        return MYLITE_REGEXP_COMPILE_UNCLOSED_BRACKET;
    }

    while (*index < pattern_length && pattern[*index] != ']') {
        unsigned char first = 0U;
        unsigned char last = 0U;
        enum mylite_regexp_compile_status status =
            parse_class_byte(pattern, pattern_length, index, &first);

        if (status != MYLITE_REGEXP_COMPILE_OK) {
            return status;
        }
        first = normalize_ascii_byte(program, first);
        last = first;
        if (*index + 1U < pattern_length && pattern[*index] == '-' && pattern[*index + 1U] != ']') {
            ++(*index);
            status = parse_class_byte(pattern, pattern_length, index, &last);
            if (status != MYLITE_REGEXP_COMPILE_OK) {
                return status;
            }
            last = normalize_ascii_byte(program, last);
            if (first > last) {
                return MYLITE_REGEXP_COMPILE_INVALID_RANGE;
            }
        }
        status = append_class_range(program, first, last);
        if (status != MYLITE_REGEXP_COMPILE_OK) {
            return status;
        }
    }

    if (*index >= pattern_length) {
        return MYLITE_REGEXP_COMPILE_UNCLOSED_BRACKET;
    }
    if (program->range_count == range_start) {
        return MYLITE_REGEXP_COMPILE_UNSUPPORTED;
    }
    ++(*index);

    out_token->kind = REGEXP_TOKEN_CLASS;
    out_token->class_is_negated = class_is_negated;
    out_token->range_start = range_start;
    out_token->range_count = program->range_count - range_start;
    return MYLITE_REGEXP_COMPILE_OK;
}

static enum mylite_regexp_compile_status parse_class_byte(
    const char *pattern,
    size_t pattern_length,
    size_t *index,
    unsigned char *out_byte
) {
    unsigned char byte = 0U;

    if (*index >= pattern_length) {
        return MYLITE_REGEXP_COMPILE_UNCLOSED_BRACKET;
    }
    byte = (unsigned char)pattern[*index];
    if (byte == '\\') {
        if (*index + 1U >= pattern_length) {
            return MYLITE_REGEXP_COMPILE_DANGLING_ESCAPE;
        }
        byte = (unsigned char)pattern[*index + 1U];
        if (byte > ascii_max || !is_supported_class_escape(byte)) {
            return MYLITE_REGEXP_COMPILE_UNSUPPORTED;
        }
        *out_byte = byte;
        *index += 2U;
        return MYLITE_REGEXP_COMPILE_OK;
    }
    if (byte > ascii_max) {
        return MYLITE_REGEXP_COMPILE_UNSUPPORTED;
    }
    *out_byte = byte;
    ++(*index);
    return MYLITE_REGEXP_COMPILE_OK;
}

static enum mylite_regexp_compile_status parse_literal_group_byte(
    const char *pattern,
    size_t pattern_length,
    size_t *index,
    unsigned char *out_byte
) {
    unsigned char byte = 0U;

    if (*index >= pattern_length) {
        return MYLITE_REGEXP_COMPILE_UNSUPPORTED;
    }
    byte = (unsigned char)pattern[*index];
    if (byte == '\\') {
        return parse_escaped_literal(pattern, pattern_length, index, out_byte);
    }
    if (byte > ascii_max || byte == '.' || byte == '^' || byte == '$' || byte == '[' ||
        is_quantifier_byte(byte) || is_unsupported_regex_metacharacter(byte)) {
        return MYLITE_REGEXP_COMPILE_UNSUPPORTED;
    }

    *out_byte = byte;
    ++(*index);
    return MYLITE_REGEXP_COMPILE_OK;
}

static enum mylite_regexp_compile_status parse_escaped_literal(
    const char *pattern,
    size_t pattern_length,
    size_t *index,
    unsigned char *out_byte
) {
    unsigned char byte = 0U;

    if (*index + 1U >= pattern_length) {
        return MYLITE_REGEXP_COMPILE_DANGLING_ESCAPE;
    }
    byte = (unsigned char)pattern[*index + 1U];
    if (byte > ascii_max || !is_supported_literal_escape(byte)) {
        return MYLITE_REGEXP_COMPILE_UNSUPPORTED;
    }
    *out_byte = byte;
    *index += 2U;
    return MYLITE_REGEXP_COMPILE_OK;
}

static enum mylite_regexp_compile_status parse_fixed_repetition_count(
    struct regexp_parse_cursor *cursor,
    size_t *out_count
) {
    size_t count = 0U;
    bool saw_digit = false;

    if (cursor == NULL || out_count == NULL || cursor->index >= cursor->pattern_length ||
        cursor->pattern[cursor->index] != '{') {
        return MYLITE_REGEXP_COMPILE_UNSUPPORTED;
    }
    ++cursor->index;
    while (cursor->index < cursor->pattern_length && cursor->pattern[cursor->index] >= '0' &&
           cursor->pattern[cursor->index] <= '9') {
        const size_t digit = (size_t)(cursor->pattern[cursor->index] - '0');

        saw_digit = true;
        if (count > (regexp_pattern_length_max - digit) / decimal_radix) {
            return MYLITE_REGEXP_COMPILE_TOO_LARGE;
        }
        count = (count * decimal_radix) + digit;
        ++cursor->index;
    }
    if (!saw_digit || cursor->index >= cursor->pattern_length ||
        cursor->pattern[cursor->index] != '}') {
        return MYLITE_REGEXP_COMPILE_UNSUPPORTED;
    }
    ++cursor->index;
    if (count > regexp_pattern_length_max) {
        return MYLITE_REGEXP_COMPILE_TOO_LARGE;
    }

    *out_count = count;
    return MYLITE_REGEXP_COMPILE_OK;
}

static bool is_supported_literal_escape(unsigned char byte) {
    switch (byte) {
    case '.':
    case '^':
    case '$':
    case '*':
    case '+':
    case '?':
    case '[':
    case ']':
    case '|':
    case '\\':
        return true;
    default:
        return false;
    }
}

static bool is_supported_class_escape(unsigned char byte) {
    switch (byte) {
    case ']':
    case '-':
    case '^':
    case '\\':
        return true;
    default:
        return false;
    }
}

static bool is_quantifier_byte(unsigned char byte) {
    if (byte == '?') {
        return true;
    }
    if (byte == '*') {
        return true;
    }
    if (byte == '+') {
        return true;
    }
    return false;
}

static bool is_unsupported_regex_metacharacter(unsigned char byte) {
    switch (byte) {
    case '(':
    case ')':
    case '{':
    case '}':
    case '|':
    case ']':
        return true;
    default:
        return false;
    }
}

static enum mylite_regexp_compile_status append_token(
    struct mylite_regexp_program *program,
    struct regexp_token token
) {
    struct regexp_token *tokens = NULL;

    if (program->token_count == SIZE_MAX / sizeof(*program->tokens)) {
        return MYLITE_REGEXP_COMPILE_TOO_LARGE;
    }
    tokens = (struct regexp_token *)
        realloc(program->tokens, (program->token_count + 1U) * sizeof(*program->tokens));
    if (tokens == NULL) {
        return MYLITE_REGEXP_COMPILE_NOMEM;
    }
    program->tokens = tokens;
    program->tokens[program->token_count] = token;
    ++program->token_count;
    return MYLITE_REGEXP_COMPILE_OK;
}

static enum mylite_regexp_compile_status append_repeated_token(
    struct mylite_regexp_program *program,
    struct regexp_token token,
    size_t count
) {
    for (size_t index = 0U; index < count; ++index) {
        enum mylite_regexp_compile_status status = append_token(program, token);

        if (status != MYLITE_REGEXP_COMPILE_OK) {
            return status;
        }
    }
    return MYLITE_REGEXP_COMPILE_OK;
}

static enum mylite_regexp_compile_status append_class_range(
    struct mylite_regexp_program *program,
    unsigned char first,
    unsigned char last
) {
    struct regexp_class_range *ranges = NULL;

    if (program->range_count == SIZE_MAX / sizeof(*program->ranges)) {
        return MYLITE_REGEXP_COMPILE_TOO_LARGE;
    }
    ranges = (struct regexp_class_range *)
        realloc(program->ranges, (program->range_count + 1U) * sizeof(*program->ranges));
    if (ranges == NULL) {
        return MYLITE_REGEXP_COMPILE_NOMEM;
    }
    program->ranges = ranges;
    program->ranges[program->range_count] =
        (struct regexp_class_range){.first = first, .last = last};
    ++program->range_count;
    return MYLITE_REGEXP_COMPILE_OK;
}

static enum mylite_regexp_compile_status append_literal_sequence_byte(
    struct mylite_regexp_program *program,
    unsigned char byte
) {
    unsigned char *bytes = NULL;

    if (program->literal_sequence_length == SIZE_MAX / sizeof(*program->literal_sequence_bytes)) {
        return MYLITE_REGEXP_COMPILE_TOO_LARGE;
    }
    bytes = (unsigned char *)realloc(
        program->literal_sequence_bytes,
        (program->literal_sequence_length + 1U) * sizeof(*program->literal_sequence_bytes)
    );
    if (bytes == NULL) {
        return MYLITE_REGEXP_COMPILE_NOMEM;
    }
    program->literal_sequence_bytes = bytes;
    program->literal_sequence_bytes[program->literal_sequence_length] = byte;
    ++program->literal_sequence_length;
    return MYLITE_REGEXP_COMPILE_OK;
}

static unsigned char normalize_ascii_byte(
    const struct mylite_regexp_program *program,
    unsigned char byte
) {
    if (program != NULL && program->case_sensitive) {
        return byte;
    }
    return fold_ascii(byte);
}

static unsigned char fold_ascii(unsigned char byte) {
    if (byte >= ascii_upper_a && byte <= ascii_upper_z) {
        return (unsigned char)(byte + ascii_lower_delta);
    }
    return byte;
}

static bool value_length_is_supported(size_t value_length) {
    if (value_length <= regexp_value_length_max) {
        return true;
    }
    return false;
}

static bool regexp_value_is_supported_ascii(const char *value, size_t value_length) {
    if (value == NULL) {
        return false;
    }
    for (size_t index = 0U; index < value_length; ++index) {
        unsigned char byte = (unsigned char)value[index];

        if (byte == '\0' || byte > ascii_max) {
            return false;
        }
    }
    return true;
}

static void fill_match_table(
    const struct mylite_regexp_program *program,
    const unsigned char *value,
    size_t value_length,
    unsigned char *matches
) {
    for (size_t value_index = 0U; value_index <= value_length; ++value_index) {
        match_table_set(matches, value_length, program->token_count, value_index, true);
    }

    for (size_t token_index = program->token_count; token_index > 0U; --token_index) {
        const size_t current_index = token_index - 1U;
        const struct regexp_token *token = &program->tokens[current_index];

        for (size_t value_index = value_length + 1U; value_index > 0U; --value_index) {
            const size_t current_value_index = value_index - 1U;
            const bool matched = match_token_suffix_from(
                program,
                token,
                value,
                value_length,
                current_index,
                current_value_index,
                matches
            );

            match_table_set(matches, value_length, current_index, current_value_index, matched);
        }
    }
}

static bool match_span_from(
    const struct regexp_match_span_context *context,
    struct regexp_match_span_position position,
    size_t *out_end
) {
    while (position.token_index < context->program->token_count) {
        if (!match_span_advance(context, &position)) {
            return false;
        }
    }

    *out_end = position.value_index;
    return true;
}

static bool match_span_advance(
    const struct regexp_match_span_context *context,
    struct regexp_match_span_position *position
) {
    const struct regexp_token *token = &context->program->tokens[position->token_index];
    bool current_byte_matches = false;

    if (token->kind == REGEXP_TOKEN_END_ANCHOR) {
        if (position->value_index != context->value_length || !match_table_get(
                                                                  context->matches,
                                                                  context->value_length,
                                                                  position->token_index + 1U,
                                                                  position->value_index
                                                              )) {
            return false;
        }
        ++position->token_index;
        return true;
    }
    if (token->kind == REGEXP_TOKEN_OPTIONAL_LITERAL_SEQUENCE) {
        return match_span_advance_optional_literal_sequence(context, token, position);
    }

    if (position->value_index < context->value_length) {
        current_byte_matches =
            token_matches_byte(context->program, token, context->value[position->value_index]);
    }

    switch (token->quantifier) {
    case REGEXP_QUANTIFIER_ONE:
        return match_span_advance_one(context, current_byte_matches, position);
    case REGEXP_QUANTIFIER_ZERO_OR_ONE:
        return match_span_advance_zero_or_one(context, current_byte_matches, position);
    case REGEXP_QUANTIFIER_ZERO_OR_MORE:
        return match_span_advance_repeated(context, token, false, position);
    case REGEXP_QUANTIFIER_ONE_OR_MORE:
        return match_span_advance_repeated(context, token, true, position);
    }
    return false;
}

static bool match_span_advance_optional_literal_sequence(
    const struct regexp_match_span_context *context,
    const struct regexp_token *token,
    struct regexp_match_span_position *position
) {
    if (literal_sequence_matches_at(
            context->program,
            token,
            context->value,
            context->value_length,
            position->value_index
        ) &&
        match_table_get(
            context->matches,
            context->value_length,
            position->token_index + 1U,
            position->value_index + token->literal_sequence_length
        )) {
        ++position->token_index;
        position->value_index += token->literal_sequence_length;
        return true;
    }
    if (!match_table_get(
            context->matches,
            context->value_length,
            position->token_index + 1U,
            position->value_index
        )) {
        return false;
    }

    ++position->token_index;
    return true;
}

static bool match_span_advance_one(
    const struct regexp_match_span_context *context,
    bool current_byte_matches,
    struct regexp_match_span_position *position
) {
    if (!current_byte_matches || !match_table_get(
                                     context->matches,
                                     context->value_length,
                                     position->token_index + 1U,
                                     position->value_index + 1U
                                 )) {
        return false;
    }

    ++position->token_index;
    ++position->value_index;
    return true;
}

static bool match_span_advance_zero_or_one(
    const struct regexp_match_span_context *context,
    bool current_byte_matches,
    struct regexp_match_span_position *position
) {
    if (current_byte_matches && match_table_get(
                                    context->matches,
                                    context->value_length,
                                    position->token_index + 1U,
                                    position->value_index + 1U
                                )) {
        ++position->token_index;
        ++position->value_index;
        return true;
    }
    if (!match_table_get(
            context->matches,
            context->value_length,
            position->token_index + 1U,
            position->value_index
        )) {
        return false;
    }

    ++position->token_index;
    return true;
}

static bool match_span_advance_repeated(
    const struct regexp_match_span_context *context,
    const struct regexp_token *token,
    bool require_one,
    struct regexp_match_span_position *position
) {
    const size_t run_length = token_match_run_length(
        context->program,
        token,
        context->value,
        context->value_length,
        position->value_index
    );
    size_t count = run_length + 1U;

    if (require_one) {
        if (run_length == 0U) {
            return false;
        }
        count = run_length;
    }
    while (count > 0U) {
        size_t consumed = count - 1U;
        size_t next_value_index = 0U;

        if (require_one) {
            consumed = count;
        }
        next_value_index = position->value_index + consumed;

        if (match_table_get(
                context->matches,
                context->value_length,
                position->token_index + 1U,
                next_value_index
            )) {
            ++position->token_index;
            position->value_index = next_value_index;
            return true;
        }
        --count;
    }
    return false;
}

static size_t token_match_run_length(
    const struct mylite_regexp_program *program,
    const struct regexp_token *token,
    const unsigned char *value,
    size_t value_length,
    size_t value_index
) {
    size_t run_length = 0U;

    while (value_index + run_length < value_length &&
           token_matches_byte(program, token, value[value_index + run_length])) {
        ++run_length;
    }
    return run_length;
}

static bool match_table_get(
    const unsigned char *matches,
    size_t value_length,
    size_t token_index,
    size_t value_index
) {
    return matches[match_table_index(value_length, token_index, value_index)] != 0U;
}

static void match_table_set(
    unsigned char *matches,
    size_t value_length,
    size_t token_index,
    size_t value_index,
    bool value
) {
    if (value) {
        matches[match_table_index(value_length, token_index, value_index)] = 1U;
    } else {
        matches[match_table_index(value_length, token_index, value_index)] = 0U;
    }
}

static size_t match_table_index(size_t value_length, size_t token_index, size_t value_index) {
    return (token_index * (value_length + 1U)) + value_index;
}

static bool match_token_suffix_from(
    const struct mylite_regexp_program *program,
    const struct regexp_token *token,
    const unsigned char *value,
    size_t value_length,
    size_t token_index,
    size_t value_index,
    const unsigned char *matches
) {
    bool current_byte_matches = false;

    if (token->kind == REGEXP_TOKEN_END_ANCHOR) {
        if (value_index != value_length) {
            return false;
        }
        return match_table_get(matches, value_length, token_index + 1U, value_index);
    }
    if (token->kind == REGEXP_TOKEN_OPTIONAL_LITERAL_SEQUENCE) {
        return match_optional_literal_sequence_suffix_from(
            program,
            token,
            value,
            value_length,
            token_index,
            value_index,
            matches
        );
    }

    if (value_index < value_length) {
        current_byte_matches = token_matches_byte(program, token, value[value_index]);
    }

    switch (token->quantifier) {
    case REGEXP_QUANTIFIER_ONE:
        if (!current_byte_matches) {
            return false;
        }
        return match_table_get(matches, value_length, token_index + 1U, value_index + 1U);
    case REGEXP_QUANTIFIER_ZERO_OR_ONE:
        if (match_table_get(matches, value_length, token_index + 1U, value_index)) {
            return true;
        }
        if (!current_byte_matches) {
            return false;
        }
        return match_table_get(matches, value_length, token_index + 1U, value_index + 1U);
    case REGEXP_QUANTIFIER_ZERO_OR_MORE:
        if (match_table_get(matches, value_length, token_index + 1U, value_index)) {
            return true;
        }
        if (!current_byte_matches) {
            return false;
        }
        return match_table_get(matches, value_length, token_index, value_index + 1U);
    case REGEXP_QUANTIFIER_ONE_OR_MORE:
        if (!current_byte_matches) {
            return false;
        }
        if (match_table_get(matches, value_length, token_index + 1U, value_index + 1U)) {
            return true;
        }
        return match_table_get(matches, value_length, token_index, value_index + 1U);
    }
    return false;
}

static bool match_optional_literal_sequence_suffix_from(
    const struct mylite_regexp_program *program,
    const struct regexp_token *token,
    const unsigned char *value,
    size_t value_length,
    size_t token_index,
    size_t value_index,
    const unsigned char *matches
) {
    if (match_table_get(matches, value_length, token_index + 1U, value_index)) {
        return true;
    }
    if (!literal_sequence_matches_at(program, token, value, value_length, value_index)) {
        return false;
    }
    return match_table_get(
        matches,
        value_length,
        token_index + 1U,
        value_index + token->literal_sequence_length
    );
}

static bool token_matches_byte(
    const struct mylite_regexp_program *program,
    const struct regexp_token *token,
    unsigned char byte
) {
    switch (token->kind) {
    case REGEXP_TOKEN_LITERAL:
        if (normalize_ascii_byte(program, byte) == token->literal) {
            return true;
        }
        return false;
    case REGEXP_TOKEN_ANY:
        if (regexp_byte_is_line_terminator(byte)) {
            return false;
        }
        return true;
    case REGEXP_TOKEN_CLASS:
        return class_contains_byte(program, token, normalize_ascii_byte(program, byte));
    case REGEXP_TOKEN_END_ANCHOR:
    case REGEXP_TOKEN_OPTIONAL_LITERAL_SEQUENCE:
        break;
    }
    return false;
}

static bool literal_sequence_matches_at(
    const struct mylite_regexp_program *program,
    const struct regexp_token *token,
    const unsigned char *value,
    size_t value_length,
    size_t value_index
) {
    if (value_index > value_length || token->literal_sequence_length > value_length - value_index) {
        return false;
    }
    for (size_t index = 0U; index < token->literal_sequence_length; ++index) {
        const unsigned char sequence_byte =
            program->literal_sequence_bytes[token->literal_sequence_start + index];

        if (normalize_ascii_byte(program, value[value_index + index]) != sequence_byte) {
            return false;
        }
    }
    return true;
}

static bool regexp_byte_is_line_terminator(unsigned char byte) {
    if (byte == '\n' || byte == '\r') {
        return true;
    }
    return false;
}

static bool class_contains_byte(
    const struct mylite_regexp_program *program,
    const struct regexp_token *token,
    unsigned char byte
) {
    bool contains = false;

    for (size_t index = 0U; index < token->range_count; ++index) {
        const struct regexp_class_range *range = &program->ranges[token->range_start + index];

        if (byte >= range->first && byte <= range->last) {
            contains = true;
            break;
        }
    }

    if (token->class_is_negated) {
        if (contains) {
            return false;
        }
        return true;
    }
    return contains;
}
