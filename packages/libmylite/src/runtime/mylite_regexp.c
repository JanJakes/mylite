#include "mylite_regexp.h"

#include "mylite_sqlite_registration.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
    regexp_pattern_length_max = 256,
    regexp_value_length_max = 4096,
    ascii_max = 0x7f,
    ascii_upper_a = 'A',
    ascii_upper_z = 'Z',
    ascii_lower_delta = 'a' - 'A',
};

enum regexp_token_kind {
    REGEXP_TOKEN_LITERAL = 0,
    REGEXP_TOKEN_ANY = 1,
    REGEXP_TOKEN_CLASS = 2,
    REGEXP_TOKEN_END_ANCHOR = 3,
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
};

struct mylite_regexp_program {
    bool anchored_start;
    bool case_sensitive;
    struct regexp_token *tokens;
    size_t token_count;
    struct regexp_class_range *ranges;
    size_t range_count;
};

static void regexp_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static bool regexp_sqlite_get_program(
    sqlite3_context *context,
    sqlite3_value *pattern_value,
    struct mylite_regexp_program **out_program,
    struct mylite_regexp_program **out_compiled_program
);
static void regexp_sqlite_result_compile_error(
    sqlite3_context *context,
    enum mylite_regexp_compile_status status
);
static void regexp_sqlite_result_match_error(
    sqlite3_context *context,
    enum mylite_regexp_match_status status
);
static void regexp_sqlite_cache_program(
    sqlite3_context *context,
    struct mylite_regexp_program *compiled_program
);
static void regexp_sqlite_free_compiled_program(struct mylite_regexp_program *compiled_program);
static const char *regexp_compile_status_message(enum mylite_regexp_compile_status status);
static const char *regexp_match_status_message(enum mylite_regexp_match_status status);
static enum mylite_regexp_compile_status regexp_compile_ascii(
    const char *pattern,
    size_t pattern_length,
    bool case_sensitive,
    struct mylite_regexp_program **out_program
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
static enum mylite_regexp_compile_status parse_escaped_literal(
    const char *pattern,
    size_t pattern_length,
    size_t *index,
    unsigned char *out_byte
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
static enum mylite_regexp_compile_status append_token(
    struct mylite_regexp_program *program,
    struct regexp_token token
);
static enum mylite_regexp_compile_status append_class_range(
    struct mylite_regexp_program *program,
    unsigned char first,
    unsigned char last
);
static unsigned char normalize_ascii_byte(
    const struct mylite_regexp_program *program,
    unsigned char byte
);
static unsigned char fold_ascii(unsigned char byte);
static bool value_length_is_supported(size_t value_length);
static void fill_match_table(
    const struct mylite_regexp_program *program,
    const unsigned char *value,
    size_t value_length,
    unsigned char *matches
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
static bool token_matches_byte(
    const struct mylite_regexp_program *program,
    const struct regexp_token *token,
    unsigned char byte
);
static bool class_contains_byte(
    const struct mylite_regexp_program *program,
    const struct regexp_token *token,
    unsigned char byte
);

int mylite_sqlite_register_regexp_functions(sqlite3 *sqlite) {
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_regexp_ci_ascii",
            .argument_count = 2,
            .text_representation =
                SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS | SQLITE_DETERMINISTIC,
            .application_data = NULL,
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

    free(regexp_program->tokens);
    free(regexp_program->ranges);
    free(regexp_program);
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

static enum mylite_regexp_compile_status regexp_compile_ascii(
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

static enum mylite_regexp_match_status regexp_program_match_ascii(
    const struct mylite_regexp_program *program,
    const char *value,
    size_t value_length,
    bool *out_matches
) {
    unsigned char *matches = NULL;
    size_t cell_count = 0U;

    if (program == NULL || value == NULL || out_matches == NULL) {
        return MYLITE_REGEXP_MATCH_VALUE_TOO_LARGE;
    }
    *out_matches = false;
    if (!value_length_is_supported(value_length)) {
        return MYLITE_REGEXP_MATCH_VALUE_TOO_LARGE;
    }
    if ((program->token_count + 1U) > SIZE_MAX / (value_length + 1U)) {
        return MYLITE_REGEXP_MATCH_VALUE_TOO_LARGE;
    }

    cell_count = (program->token_count + 1U) * (value_length + 1U);
    matches = (unsigned char *)calloc(cell_count, sizeof(*matches));
    if (matches == NULL) {
        return MYLITE_REGEXP_MATCH_NOMEM;
    }

    fill_match_table(program, (const unsigned char *)value, value_length, matches);
    if (program->anchored_start) {
        *out_matches = match_table_get(matches, value_length, 0U, 0U);
    } else {
        for (size_t start = 0U; start <= value_length && !*out_matches; ++start) {
            *out_matches = match_table_get(matches, value_length, 0U, start);
        }
    }

    free(matches);
    return MYLITE_REGEXP_MATCH_OK;
}

static void regexp_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
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

    if (!regexp_sqlite_get_program(context, argv[0], &program, &compiled_program)) {
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

    match_status = mylite_regexp_program_match_ascii_ci(
        program,
        (const char *)value,
        (size_t)value_length,
        &matches
    );
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

static bool regexp_sqlite_get_program(
    sqlite3_context *context,
    sqlite3_value *pattern_value,
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

    compile_status = mylite_regexp_compile_ascii_ci(
        (const char *)pattern,
        (size_t)pattern_length,
        out_compiled_program
    );
    if (compile_status != MYLITE_REGEXP_COMPILE_OK) {
        regexp_sqlite_result_compile_error(context, compile_status);
        return false;
    }

    *out_program = *out_compiled_program;
    return true;
}

static void regexp_sqlite_result_compile_error(
    sqlite3_context *context,
    enum mylite_regexp_compile_status status
) {
    if (status == MYLITE_REGEXP_COMPILE_NOMEM) {
        sqlite3_result_error_nomem(context);
        return;
    }
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
    sqlite3_result_error(context, regexp_match_status_message(status), -1);
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
        return true;
    case REGEXP_TOKEN_CLASS:
        return class_contains_byte(program, token, normalize_ascii_byte(program, byte));
    case REGEXP_TOKEN_END_ANCHOR:
        break;
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
