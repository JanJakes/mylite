#include "mylite_value_list_column_type.h"

#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_span.h"
#include "sql/mylite_lexer.h"
#include "sqlite3.h"
#include <mylite_fork/mylite_sqlite_fork.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct value_list_column_type {
    struct mylite_sqlite_fork_enum_value *values;
    char **texts;
    size_t value_count;
};

struct value_list_column_type_parse_config {
    const char *physical_table_name;
    const char *column_name;
    const char *data_type;
    const char *column_type;
};

static int parse_value_list_column_type(
    mylite_db *database,
    const struct value_list_column_type_parse_config *config,
    struct value_list_column_type *out_value_list
);

static int parse_value_list_column_type_header(
    mylite_db *database,
    const struct value_list_column_type_parse_config *config,
    struct mylite_sql_lexer *lexer,
    struct mylite_sql_token *token
);

static int parse_value_list_column_type_member(
    mylite_db *database,
    const struct value_list_column_type_parse_config *config,
    struct mylite_sql_lexer *lexer,
    struct mylite_sql_token *token,
    struct value_list_column_type *out_value_list,
    bool *out_done
);

static int parse_value_list_column_type_trailer(
    mylite_db *database,
    const struct value_list_column_type_parse_config *config,
    struct mylite_sql_lexer *lexer,
    struct mylite_sql_token *token,
    const struct value_list_column_type *value_list
);

static int append_value_list_member(struct value_list_column_type *value_list, char *text);

static void value_list_column_type_deinit(struct value_list_column_type *value_list);

static int set_value_list_column_type_error(
    mylite_db *database,
    const char *physical_table_name,
    const char *column_name
);

static int set_value_list_descriptor_error(
    mylite_db *database,
    const char *physical_table_name,
    const char *column_name,
    int rc
);

static bool token_text_equals_ci(const struct mylite_sql_token *token, const char *text);

static bool token_is_punctuation(const struct mylite_sql_token *token, char punctuation);

bool mylite_value_list_column_type_is_supported(const char *data_type) {
    if (mylite_ascii_case_equal(data_type, "enum")) {
        return true;
    }
    if (mylite_ascii_case_equal(data_type, "set")) {
        return true;
    }
    return false;
}

int mylite_configure_value_list_column_type(
    mylite_db *database,
    const char *physical_table_name,
    const char *column_name,
    const char *data_type,
    const char *column_type
) {
    struct value_list_column_type value_list = {0};
    const struct value_list_column_type_parse_config config = {
        .physical_table_name = physical_table_name,
        .column_name = column_name,
        .data_type = data_type,
        .column_type = column_type,
    };
    int status = parse_value_list_column_type(database, &config, &value_list);
    int rc = SQLITE_OK;

    if (status != MYLITE_OK) {
        value_list_column_type_deinit(&value_list);
        return status;
    }

    if (mylite_ascii_case_equal(data_type, "enum")) {
        const struct mylite_sqlite_fork_enum_column_type type = {
            .values = value_list.values,
            .value_count = (sqlite3_uint64)value_list.value_count,
        };

        rc = mylite_sqlite_fork_set_enum_column_type(
            database->sqlite,
            NULL,
            physical_table_name,
            column_name,
            &type
        );
    } else {
        const struct mylite_sqlite_fork_set_column_type type = {
            .values = value_list.values,
            .value_count = (sqlite3_uint64)value_list.value_count,
        };

        rc = mylite_sqlite_fork_set_set_column_type(
            database->sqlite,
            NULL,
            physical_table_name,
            column_name,
            &type
        );
    }

    value_list_column_type_deinit(&value_list);
    if (rc != SQLITE_OK) {
        return set_value_list_descriptor_error(database, physical_table_name, column_name, rc);
    }
    return MYLITE_OK;
}

static int parse_value_list_column_type(
    mylite_db *database,
    const struct value_list_column_type_parse_config *config,
    struct value_list_column_type *out_value_list
) {
    struct mylite_sql_lexer lexer;
    struct mylite_sql_token token;
    int status = MYLITE_OK;

    *out_value_list = (struct value_list_column_type){0};
    if (config == NULL) {
        return set_value_list_column_type_error(database, NULL, NULL);
    }
    if (!mylite_value_list_column_type_is_supported(config->data_type) ||
        config->column_type == NULL) {
        return set_value_list_column_type_error(
            database,
            config->physical_table_name,
            config->column_name
        );
    }

    mylite_sql_lexer_init(
        &lexer,
        (struct mylite_sql_lexer_config){
            .input = config->column_type,
            .length = strlen(config->column_type),
        }
    );

    status = parse_value_list_column_type_header(database, config, &lexer, &token);
    if (status != MYLITE_OK) {
        return status;
    }

    for (;;) {
        bool done = false;

        status = parse_value_list_column_type_member(
            database,
            config,
            &lexer,
            &token,
            out_value_list,
            &done
        );
        if (status != MYLITE_OK) {
            return status;
        }
        if (done) {
            break;
        }
    }

    return parse_value_list_column_type_trailer(database, config, &lexer, &token, out_value_list);
}

static int parse_value_list_column_type_header(
    mylite_db *database,
    const struct value_list_column_type_parse_config *config,
    struct mylite_sql_lexer *lexer,
    struct mylite_sql_token *token
) {
    if (mylite_sql_lexer_next(lexer, token) != 0 ||
        !(token->kind == MYLITE_SQL_TOKEN_KEYWORD || token->kind == MYLITE_SQL_TOKEN_IDENTIFIER) ||
        !token_text_equals_ci(token, config->data_type)) {
        return set_value_list_column_type_error(
            database,
            config->physical_table_name,
            config->column_name
        );
    }
    if (mylite_sql_lexer_next(lexer, token) != 0 || !token_is_punctuation(token, '(')) {
        return set_value_list_column_type_error(
            database,
            config->physical_table_name,
            config->column_name
        );
    }
    return MYLITE_OK;
}

static int parse_value_list_column_type_member(
    mylite_db *database,
    const struct value_list_column_type_parse_config *config,
    struct mylite_sql_lexer *lexer,
    struct mylite_sql_token *token,
    struct value_list_column_type *out_value_list,
    bool *out_done
) {
    struct mylite_sql_ast_node literal = {0};
    char *text = NULL;
    int status = MYLITE_OK;

    *out_done = false;
    if (mylite_sql_lexer_next(lexer, token) != 0 || token->kind != MYLITE_SQL_TOKEN_STRING) {
        value_list_column_type_deinit(out_value_list);
        return set_value_list_column_type_error(
            database,
            config->physical_table_name,
            config->column_name
        );
    }
    literal = (struct mylite_sql_ast_node){
        .kind = MYLITE_SQL_AST_LITERAL,
        .literal_kind = MYLITE_SQL_AST_LITERAL_STRING,
        .span = {
            .text = token->text,
            .length = token->length,
        },
    };
    text = mylite_copy_string_literal_span(&literal);
    if (text == NULL) {
        value_list_column_type_deinit(out_value_list);
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = append_value_list_member(out_value_list, text);
    if (status != MYLITE_OK) {
        free(text);
        value_list_column_type_deinit(out_value_list);
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return status;
    }

    if (mylite_sql_lexer_next(lexer, token) != 0) {
        value_list_column_type_deinit(out_value_list);
        return set_value_list_column_type_error(
            database,
            config->physical_table_name,
            config->column_name
        );
    }
    if (token_is_punctuation(token, ',')) {
        return MYLITE_OK;
    }
    if (token_is_punctuation(token, ')')) {
        *out_done = true;
        return MYLITE_OK;
    }
    value_list_column_type_deinit(out_value_list);
    return set_value_list_column_type_error(
        database,
        config->physical_table_name,
        config->column_name
    );
}

static int parse_value_list_column_type_trailer(
    mylite_db *database,
    const struct value_list_column_type_parse_config *config,
    struct mylite_sql_lexer *lexer,
    struct mylite_sql_token *token,
    const struct value_list_column_type *value_list
) {
    if (mylite_sql_lexer_next(lexer, token) != 0 || token->kind != MYLITE_SQL_TOKEN_EOF ||
        value_list->value_count == 0U) {
        return set_value_list_column_type_error(
            database,
            config->physical_table_name,
            config->column_name
        );
    }
    return MYLITE_OK;
}

static int append_value_list_member(struct value_list_column_type *value_list, char *text) {
    char **texts = NULL;
    struct mylite_sqlite_fork_enum_value *values = NULL;

    if (value_list->value_count == SIZE_MAX / sizeof(*value_list->texts) ||
        value_list->value_count == SIZE_MAX / sizeof(*value_list->values)) {
        return MYLITE_NOMEM;
    }

    texts = (char **)realloc(
        (void *)value_list->texts,
        (value_list->value_count + 1U) * sizeof(*value_list->texts)
    );
    if (texts == NULL) {
        return MYLITE_NOMEM;
    }
    value_list->texts = texts;

    values = (struct mylite_sqlite_fork_enum_value *)realloc(
        (void *)value_list->values,
        (value_list->value_count + 1U) * sizeof(*value_list->values)
    );
    if (values == NULL) {
        return MYLITE_NOMEM;
    }
    value_list->values = values;
    value_list->texts[value_list->value_count] = text;
    value_list->values[value_list->value_count] = (struct mylite_sqlite_fork_enum_value){
        .text = text,
        .byte_length = (sqlite3_uint64)strlen(text),
    };
    ++value_list->value_count;
    return MYLITE_OK;
}

static void value_list_column_type_deinit(struct value_list_column_type *value_list) {
    if (value_list == NULL) {
        return;
    }

    for (size_t index = 0U; index < value_list->value_count; ++index) {
        free(value_list->texts[index]);
    }
    free((void *)value_list->texts);
    free(value_list->values);
    *value_list = (struct value_list_column_type){0};
}

static int set_value_list_column_type_error(
    mylite_db *database,
    const char *physical_table_name,
    const char *column_name
) {
    char *message = sqlite3_mprintf(
        "malformed MySQL value-list column type for '%q.%q'",
        physical_table_name == NULL ? "" : physical_table_name,
        column_name == NULL ? "" : column_name
    );
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_set_error_message(database, message);
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_value_list_descriptor_error(
    mylite_db *database,
    const char *physical_table_name,
    const char *column_name,
    int rc
) {
    char *message = sqlite3_mprintf(
        "failed to configure SQLite fork column descriptor for '%q.%q' (rc=%d)",
        physical_table_name == NULL ? "" : physical_table_name,
        column_name == NULL ? "" : column_name,
        rc
    );
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_set_error_message(database, message);
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_SQLITE_ERROR;
}

static bool token_text_equals_ci(const struct mylite_sql_token *token, const char *text) {
    struct mylite_sql_source_span span = {
        .text = token == NULL ? NULL : token->text,
        .length = token == NULL ? 0U : token->length,
    };

    return mylite_span_equal_ci(span, text);
}

static bool token_is_punctuation(const struct mylite_sql_token *token, char punctuation) {
    if (token == NULL) {
        return false;
    }
    if (token->kind != MYLITE_SQL_TOKEN_PUNCTUATION) {
        return false;
    }
    if (token->length != 1U) {
        return false;
    }
    return token->text[0] == punctuation;
}
