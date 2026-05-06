#include "mylite_prepared_statements.h"

#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_span.h"
#include "mylite_statement.h"
#include "mylite_statement_ast.h"
#include "mylite_statement_prepare.h"
#include "mylite_statement_types.h"
#include "mylite_user_variables.h"
#include "sql/mylite_lexer.h"
#include "sql/mylite_parser.h"
#include "sqlite3.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int copy_prepare_statement_plan(
    const struct mylite_sql_ast_node *statement,
    struct mylite_prepare_statement_plan *plan
);

static int copy_execute_statement_plan(
    const struct mylite_sql_ast_node *statement,
    struct mylite_execute_prepared_plan *plan
);

static int copy_deallocate_statement_plan(
    const struct mylite_sql_ast_node *statement,
    struct mylite_deallocate_prepare_plan *plan
);

static int copy_statement_name(const struct mylite_sql_ast_node *name_node, char **out_name);

static void lowercase_ascii_text(char *text);

static int clone_plan_source_node(
    const struct mylite_sql_ast_node *source,
    struct mylite_prepare_statement_plan *plan
);

static int copy_prepare_source_sql(
    mylite_db *database,
    const struct mylite_sql_ast_node *source,
    char **out_sql
);

static int validate_prepared_statement_sql(
    mylite_db *database,
    const char *sql,
    size_t sql_length,
    size_t *out_parameter_count
);

static int substitute_parameter_markers(
    mylite_db *database,
    const char *sql,
    size_t sql_length,
    char **replacements,
    size_t replacement_count,
    char **out_sql,
    size_t *out_parameter_count
);

static int append_substitution_text(
    char **buffer,
    size_t *length,
    size_t *capacity,
    const char *text,
    size_t text_length
);

static bool prepared_statement_kind_is_unsupported(const struct mylite_sql_ast_node *statement);

static int store_prepared_statement(
    mylite_db *database,
    const char *name,
    const char *sql,
    size_t parameter_count
);

static const struct mylite_prepared_statement_entry *find_prepared_statement_entry(
    const struct mylite_prepared_statement_store *store,
    const char *name
);

static bool remove_prepared_statement(
    struct mylite_prepared_statement_store *store,
    const char *name
);

static void prepared_statement_entry_deinit(struct mylite_prepared_statement_entry *entry);

static int set_unknown_prepared_statement_error(
    mylite_db *database,
    const char *name,
    const char *operation
);

static int set_prepared_statement_error(
    mylite_db *database,
    unsigned int code,
    const char *message
);

static int materialize_execute_sql(
    mylite_stmt *stmt,
    const struct mylite_prepared_statement_entry *entry,
    char **out_sql
);

static int copy_user_variable_literal(mylite_db *database, const char *name, char **out_literal);

static int copy_sql_literal_from_value(
    const struct mylite_expression_value *value,
    char **out_literal
);

static int copy_quoted_sql_text_literal(const char *text, size_t text_length, char **out_literal);

static int copy_static_text(const char *text, char **out_text);

int mylite_prepared_statement_prepare_prepare_statement(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_stmt **out_stmt
) {
    mylite_stmt *stmt = calloc(1U, sizeof(*stmt));
    int status = MYLITE_OK;

    if (stmt == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    *stmt = (mylite_stmt){
        .database = database,
        .kind = MYLITE_STMT_PREPARE_STATEMENT,
        .affected_rows = 0,
    };

    status = copy_prepare_statement_plan(statement, &stmt->prepare_statement);
    if (status != MYLITE_OK) {
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        mylite_finalize(stmt);
        return status;
    }

    *out_stmt = stmt;
    return MYLITE_OK;
}

int mylite_prepared_statement_prepare_execute_statement(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_stmt **out_stmt
) {
    mylite_stmt *stmt = calloc(1U, sizeof(*stmt));
    int status = MYLITE_OK;

    if (stmt == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    *stmt = (mylite_stmt){
        .database = database,
        .kind = MYLITE_STMT_EXECUTE_PREPARED,
        .affected_rows = -1,
    };

    status = copy_execute_statement_plan(statement, &stmt->execute_prepared);
    if (status != MYLITE_OK) {
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        mylite_finalize(stmt);
        return status;
    }

    *out_stmt = stmt;
    return MYLITE_OK;
}

int mylite_prepared_statement_prepare_deallocate_statement(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_stmt **out_stmt
) {
    mylite_stmt *stmt = calloc(1U, sizeof(*stmt));
    int status = MYLITE_OK;

    if (stmt == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    *stmt = (mylite_stmt){
        .database = database,
        .kind = MYLITE_STMT_DEALLOCATE_PREPARE,
        .affected_rows = 0,
    };

    status = copy_deallocate_statement_plan(statement, &stmt->deallocate_prepare);
    if (status != MYLITE_OK) {
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        mylite_finalize(stmt);
        return status;
    }

    *out_stmt = stmt;
    return MYLITE_OK;
}

int mylite_prepared_statement_execute_prepare(mylite_stmt *stmt) {
    const struct mylite_prepare_statement_plan *plan =
        stmt == NULL ? NULL : &stmt->prepare_statement;
    char *source_sql = NULL;
    size_t parameter_count = 0U;
    int status = MYLITE_OK;

    if (stmt == NULL || stmt->database == NULL || plan == NULL || plan->name == NULL) {
        return MYLITE_MISUSE;
    }

    (void)remove_prepared_statement(&stmt->database->prepared_statements, plan->name);

    status = copy_prepare_source_sql(stmt->database, plan->source, &source_sql);
    if (status != MYLITE_OK) {
        stmt->affected_rows = -1;
        free(source_sql);
        return status;
    }

    status = validate_prepared_statement_sql(
        stmt->database,
        source_sql,
        strlen(source_sql),
        &parameter_count
    );
    if (status == MYLITE_OK) {
        status = store_prepared_statement(stmt->database, plan->name, source_sql, parameter_count);
    }
    free(source_sql);
    if (status != MYLITE_OK) {
        stmt->affected_rows = -1;
    }
    return status;
}

int mylite_prepared_statement_execute_execute(mylite_stmt *stmt) {
    const struct mylite_execute_prepared_plan *plan = stmt == NULL ? NULL : &stmt->execute_prepared;
    const struct mylite_prepared_statement_entry *entry = NULL;
    char *execute_sql = NULL;
    int status = MYLITE_OK;

    if (stmt == NULL || stmt->database == NULL || plan == NULL || plan->name == NULL) {
        return MYLITE_MISUSE;
    }
    if (stmt->prepared_execute_stmt == NULL) {
        entry = find_prepared_statement_entry(&stmt->database->prepared_statements, plan->name);
        if (entry == NULL) {
            return set_unknown_prepared_statement_error(stmt->database, plan->name, "EXECUTE");
        }
        if (entry->parameter_count != plan->using_count) {
            return set_prepared_statement_error(
                stmt->database,
                MYLITE_MYSQL_ER_WRONG_ARGUMENTS,
                "Incorrect arguments to EXECUTE"
            );
        }

        status = materialize_execute_sql(stmt, entry, &execute_sql);
        if (status != MYLITE_OK) {
            free(execute_sql);
            return status;
        }
        status = mylite_prepare(
            stmt->database,
            execute_sql,
            strlen(execute_sql),
            &stmt->prepared_execute_stmt
        );
        free(execute_sql);
        if (status != MYLITE_OK) {
            return status;
        }
        stmt->executed = true;
    }

    status = mylite_step(stmt->prepared_execute_stmt);
    if (status == MYLITE_DONE) {
        stmt->affected_rows = mylite_affected_rows(stmt->prepared_execute_stmt);
        stmt->found_rows = stmt->prepared_execute_stmt->found_rows;
    }
    return status;
}

int mylite_prepared_statement_execute_deallocate(mylite_stmt *stmt) {
    const struct mylite_deallocate_prepare_plan *plan =
        stmt == NULL ? NULL : &stmt->deallocate_prepare;

    if (stmt == NULL || stmt->database == NULL || plan == NULL || plan->name == NULL) {
        return MYLITE_MISUSE;
    }
    if (!remove_prepared_statement(&stmt->database->prepared_statements, plan->name)) {
        stmt->affected_rows = -1;
        return set_unknown_prepared_statement_error(
            stmt->database,
            plan->name,
            "DEALLOCATE PREPARE"
        );
    }
    return MYLITE_OK;
}

void mylite_prepared_statement_store_deinit(struct mylite_prepared_statement_store *store) {
    if (store == NULL) {
        return;
    }
    for (size_t index = 0U; index < store->count; ++index) {
        prepared_statement_entry_deinit(&store->items[index]);
    }
    free(store->items);
    *store = (struct mylite_prepared_statement_store){0};
}

void mylite_prepared_statement_prepare_plan_deinit(struct mylite_prepare_statement_plan *plan) {
    if (plan == NULL) {
        return;
    }
    free(plan->name);
    free(plan->source_sql_text);
    mylite_sql_ast_deinit(&plan->source_ast);
    *plan = (struct mylite_prepare_statement_plan){0};
}

void mylite_prepared_statement_execute_plan_deinit(struct mylite_execute_prepared_plan *plan) {
    if (plan == NULL) {
        return;
    }
    free(plan->name);
    for (size_t index = 0U; index < plan->using_count; ++index) {
        free(plan->using_names[index]);
    }
    free(plan->using_names);
    *plan = (struct mylite_execute_prepared_plan){0};
}

void mylite_prepared_statement_deallocate_plan_deinit(struct mylite_deallocate_prepare_plan *plan) {
    if (plan == NULL) {
        return;
    }
    free(plan->name);
    *plan = (struct mylite_deallocate_prepare_plan){0};
}

static int copy_prepare_statement_plan(
    const struct mylite_sql_ast_node *statement,
    struct mylite_prepare_statement_plan *plan
) {
    const struct mylite_sql_ast_node *name_node = mylite_ast_child_at(statement, 0U);
    const struct mylite_sql_ast_node *source = mylite_ast_child_at(statement, 1U);
    int status = copy_statement_name(name_node, &plan->name);

    if (status != MYLITE_OK) {
        return status;
    }
    return clone_plan_source_node(source, plan);
}

static int copy_execute_statement_plan(
    const struct mylite_sql_ast_node *statement,
    struct mylite_execute_prepared_plan *plan
) {
    const struct mylite_sql_ast_node *name_node = mylite_ast_child_at(statement, 0U);
    const struct mylite_sql_ast_node *using_list = mylite_ast_child_at(statement, 1U);
    int status = copy_statement_name(name_node, &plan->name);

    if (status != MYLITE_OK) {
        return status;
    }
    for (const struct mylite_sql_ast_node *variable = using_list == NULL ? NULL
                                                                         : using_list->first_child;
         variable != NULL;
         variable = variable->next_sibling) {
        char **using_names =
            realloc(plan->using_names, (plan->using_count + 1U) * sizeof(*plan->using_names));

        if (using_names == NULL) {
            return MYLITE_NOMEM;
        }
        plan->using_names = using_names;
        plan->using_names[plan->using_count] = NULL;
        status = mylite_user_variable_copy_identifier_name(
            variable,
            &plan->using_names[plan->using_count]
        );
        if (status != MYLITE_OK) {
            return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_UNSUPPORTED;
        }
        ++plan->using_count;
    }
    return MYLITE_OK;
}

static int copy_deallocate_statement_plan(
    const struct mylite_sql_ast_node *statement,
    struct mylite_deallocate_prepare_plan *plan
) {
    return copy_statement_name(mylite_ast_child_at(statement, 0U), &plan->name);
}

static int copy_statement_name(const struct mylite_sql_ast_node *name_node, char **out_name) {
    if (out_name == NULL) {
        return MYLITE_MISUSE;
    }
    *out_name = mylite_copy_identifier_span(name_node);
    if (*out_name == NULL) {
        return MYLITE_NOMEM;
    }
    lowercase_ascii_text(*out_name);
    return MYLITE_OK;
}

static void lowercase_ascii_text(char *text) {
    for (size_t index = 0U; text != NULL && text[index] != '\0'; ++index) {
        if (text[index] >= 'A' && text[index] <= 'Z') {
            text[index] = (char)(text[index] - 'A' + 'a');
        }
    }
}

static int clone_plan_source_node(
    const struct mylite_sql_ast_node *source,
    struct mylite_prepare_statement_plan *plan
) {
    struct mylite_sql_ast_node *source_clone = NULL;
    int status = MYLITE_OK;

    if (source == NULL || source->span.text == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    plan->source_sql_text = mylite_copy_span_text(source->span.text, source->span.length);
    if (plan->source_sql_text == NULL) {
        return MYLITE_NOMEM;
    }
    status = mylite_statement_ast_clone_subtree(
        &plan->source_ast,
        source,
        source->span.text,
        plan->source_sql_text,
        source->span.length,
        &source_clone
    );
    if (status != MYLITE_OK) {
        return status;
    }
    plan->source = source_clone;
    return MYLITE_OK;
}

static int copy_prepare_source_sql(
    mylite_db *database,
    const struct mylite_sql_ast_node *source,
    char **out_sql
) {
    struct mylite_expression_value value = {0};
    int status = MYLITE_OK;

    *out_sql = NULL;
    if (source == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    if (source->kind == MYLITE_SQL_AST_LITERAL &&
        source->literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
        *out_sql = mylite_copy_string_literal_span(source);
        if (*out_sql == NULL) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
        return MYLITE_OK;
    }
    if (!mylite_user_variable_identifier_is_user_variable(source)) {
        return MYLITE_UNSUPPORTED;
    }

    status = mylite_user_variable_eval_identifier(database, source, &value);
    if (status != 0) {
        return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
    }
    if (value.kind == MYLITE_EXPRESSION_VALUE_NULL) {
        status = copy_static_text("NULL", out_sql);
    } else {
        *out_sql = mylite_expression_value_to_text(&value);
        status = *out_sql == NULL ? MYLITE_NOMEM : MYLITE_OK;
    }
    mylite_expression_value_deinit(&value);
    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
    }
    return status;
}

static int validate_prepared_statement_sql(
    mylite_db *database,
    const char *sql,
    size_t sql_length,
    size_t *out_parameter_count
) {
    struct mylite_sql_parse_result parse_result = {0};
    char *validation_sql = NULL;
    enum mylite_sql_parse_status parse_status = MYLITE_SQL_PARSE_OK;
    const struct mylite_sql_ast_node *statement = NULL;
    int status = substitute_parameter_markers(
        database,
        sql,
        sql_length,
        NULL,
        0U,
        &validation_sql,
        out_parameter_count
    );

    if (status != MYLITE_OK) {
        free(validation_sql);
        return status;
    }

    parse_status = mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = validation_sql,
            .length = strlen(validation_sql),
            .modes = 0U,
        },
        &parse_result
    );
    if (parse_status != MYLITE_SQL_PARSE_OK) {
        status = mylite_statement_map_parse_status(database, parse_status);
        if (status != MYLITE_NOMEM) {
            (void)mylite_diagnostics_append_current_error_condition(
                database,
                MYLITE_MYSQL_ER_PARSE_ERROR
            );
        }
        goto done;
    }

    statement = mylite_ast_single_statement(parse_result.root);
    if (statement == NULL) {
        status =
            set_prepared_statement_error(database, MYLITE_MYSQL_ER_PARSE_ERROR, "syntax_error");
    } else if (prepared_statement_kind_is_unsupported(statement)) {
        status = set_prepared_statement_error(
            database,
            MYLITE_MYSQL_ER_UNSUPPORTED_PS,
            "This command is not supported in the prepared statement protocol yet"
        );
    }

done:
    mylite_sql_parse_result_deinit(&parse_result);
    free(validation_sql);
    return status;
}

static int substitute_parameter_markers(
    mylite_db *database,
    const char *sql,
    size_t sql_length,
    char **replacements,
    size_t replacement_count,
    char **out_sql,
    size_t *out_parameter_count
) {
    struct mylite_sql_lexer lexer = {0};
    char *buffer = NULL;
    size_t length = 0U;
    size_t capacity = 0U;
    size_t copied_offset = 0U;
    size_t parameter_count = 0U;

    *out_sql = NULL;
    if (out_parameter_count != NULL) {
        *out_parameter_count = 0U;
    }
    mylite_sql_lexer_init(
        &lexer,
        (struct mylite_sql_lexer_config){
            .input = sql,
            .length = sql_length,
            .modes = 0U,
        }
    );

    for (;;) {
        struct mylite_sql_token token = {0};

        if (mylite_sql_lexer_next(&lexer, &token) != 0 || token.kind == MYLITE_SQL_TOKEN_ERROR) {
            free(buffer);
            return set_prepared_statement_error(
                database,
                MYLITE_MYSQL_ER_PARSE_ERROR,
                "lexer_error"
            );
        }
        if (token.kind == MYLITE_SQL_TOKEN_EOF) {
            if (append_substitution_text(
                    &buffer,
                    &length,
                    &capacity,
                    sql + copied_offset,
                    sql_length - copied_offset
                ) != MYLITE_OK) {
                goto out_of_memory;
            }
            break;
        }
        if (token.kind != MYLITE_SQL_TOKEN_PARAMETER) {
            continue;
        }

        if (append_substitution_text(
                &buffer,
                &length,
                &capacity,
                sql + copied_offset,
                token.offset - copied_offset
            ) != MYLITE_OK) {
            goto out_of_memory;
        }
        if (replacements == NULL) {
            if (append_substitution_text(&buffer, &length, &capacity, "0", 1U) != MYLITE_OK) {
                goto out_of_memory;
            }
        } else {
            if (parameter_count >= replacement_count) {
                goto incorrect_argument_count;
            }
            if (append_substitution_text(
                    &buffer,
                    &length,
                    &capacity,
                    replacements[parameter_count],
                    strlen(replacements[parameter_count])
                ) != MYLITE_OK) {
                goto out_of_memory;
            }
        }
        copied_offset = token.offset + token.length;
        ++parameter_count;
    }

    if (buffer == NULL) {
        buffer = mylite_copy_span_text("", 0U);
        if (buffer == NULL) {
            goto out_of_memory;
        }
    }
    if (out_parameter_count != NULL) {
        *out_parameter_count = parameter_count;
    }
    *out_sql = buffer;
    return MYLITE_OK;

incorrect_argument_count:
    free(buffer);
    return set_prepared_statement_error(
        database,
        MYLITE_MYSQL_ER_WRONG_ARGUMENTS,
        "Incorrect arguments to EXECUTE"
    );

out_of_memory:
    free(buffer);
    (void)mylite_diagnostics_set_error_message(database, "out of memory");
    return MYLITE_NOMEM;
}

static int append_substitution_text(
    char **buffer,
    size_t *length,
    size_t *capacity,
    const char *text,
    size_t text_length
) {
    if (text_length == 0U) {
        return MYLITE_OK;
    }
    if (*capacity - *length <= text_length) {
        size_t new_capacity = *capacity == 0U ? 64U : *capacity;
        char *new_buffer = NULL;

        while (new_capacity - *length <= text_length) {
            if (new_capacity > (size_t)-1 / 2U) {
                return MYLITE_NOMEM;
            }
            new_capacity *= 2U;
        }
        new_buffer = realloc(*buffer, new_capacity);
        if (new_buffer == NULL) {
            return MYLITE_NOMEM;
        }
        *buffer = new_buffer;
        *capacity = new_capacity;
    }
    memcpy(*buffer + *length, text, text_length);
    *length += text_length;
    (*buffer)[*length] = '\0';
    return MYLITE_OK;
}

static bool prepared_statement_kind_is_unsupported(const struct mylite_sql_ast_node *statement) {
    if (statement == NULL) {
        return false;
    }
    return statement->kind == MYLITE_SQL_AST_PREPARE_STATEMENT ||
           statement->kind == MYLITE_SQL_AST_EXECUTE_STATEMENT ||
           statement->kind == MYLITE_SQL_AST_DEALLOCATE_PREPARE_STATEMENT;
}

static int store_prepared_statement(
    mylite_db *database,
    const char *name,
    const char *sql,
    size_t parameter_count
) {
    struct mylite_prepared_statement_entry *items = realloc(
        database->prepared_statements.items,
        (database->prepared_statements.count + 1U) * sizeof(*database->prepared_statements.items)
    );
    struct mylite_prepared_statement_entry *entry = NULL;

    if (items == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    database->prepared_statements.items = items;
    entry = &database->prepared_statements.items[database->prepared_statements.count++];
    *entry = (struct mylite_prepared_statement_entry){0};

    entry->name = mylite_copy_span_text(name, strlen(name));
    entry->sql_text = mylite_copy_span_text(sql, strlen(sql));
    if (entry->name == NULL || entry->sql_text == NULL) {
        prepared_statement_entry_deinit(entry);
        --database->prepared_statements.count;
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    entry->parameter_count = parameter_count;
    return MYLITE_OK;
}

static const struct mylite_prepared_statement_entry *find_prepared_statement_entry(
    const struct mylite_prepared_statement_store *store,
    const char *name
) {
    if (store == NULL || name == NULL) {
        return NULL;
    }
    for (size_t index = 0U; index < store->count; ++index) {
        if (strcmp(store->items[index].name, name) == 0) {
            return &store->items[index];
        }
    }
    return NULL;
}

static bool remove_prepared_statement(
    struct mylite_prepared_statement_store *store,
    const char *name
) {
    if (store == NULL || name == NULL) {
        return false;
    }
    for (size_t index = 0U; index < store->count; ++index) {
        if (strcmp(store->items[index].name, name) == 0) {
            prepared_statement_entry_deinit(&store->items[index]);
            if (index + 1U < store->count) {
                memmove(
                    &store->items[index],
                    &store->items[index + 1U],
                    (store->count - index - 1U) * sizeof(*store->items)
                );
            }
            --store->count;
            return true;
        }
    }
    return false;
}

static void prepared_statement_entry_deinit(struct mylite_prepared_statement_entry *entry) {
    if (entry == NULL) {
        return;
    }
    free(entry->name);
    free(entry->sql_text);
    *entry = (struct mylite_prepared_statement_entry){0};
}

static int set_unknown_prepared_statement_error(
    mylite_db *database,
    const char *name,
    const char *operation
) {
    char *message =
        sqlite3_mprintf("Unknown prepared statement handler (%q) given to %s", name, operation);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = set_prepared_statement_error(database, MYLITE_MYSQL_ER_UNKNOWN_STMT_HANDLER, message);
    sqlite3_free(message);
    return status;
}

static int set_prepared_statement_error(
    mylite_db *database,
    unsigned int code,
    const char *message
) {
    int status = mylite_diagnostics_set_error_message(database, message);

    if (status == MYLITE_OK) {
        status = mylite_diagnostics_append_error(database, code, message);
    }
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int materialize_execute_sql(
    mylite_stmt *stmt,
    const struct mylite_prepared_statement_entry *entry,
    char **out_sql
) {
    char **literals = NULL;
    size_t parameter_count = 0U;
    int status = MYLITE_OK;

    *out_sql = NULL;
    if (entry->parameter_count > 0U) {
        literals = calloc(entry->parameter_count, sizeof(*literals));
        if (literals == NULL) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
    }

    for (size_t index = 0U; index < entry->parameter_count; ++index) {
        status = copy_user_variable_literal(
            stmt->database,
            stmt->execute_prepared.using_names[index],
            &literals[index]
        );
        if (status != MYLITE_OK) {
            goto done;
        }
    }

    status = substitute_parameter_markers(
        stmt->database,
        entry->sql_text,
        strlen(entry->sql_text),
        literals,
        entry->parameter_count,
        out_sql,
        &parameter_count
    );
    if (status == MYLITE_OK && parameter_count != entry->parameter_count) {
        free(*out_sql);
        *out_sql = NULL;
        status = set_prepared_statement_error(
            stmt->database,
            MYLITE_MYSQL_ER_WRONG_ARGUMENTS,
            "Incorrect arguments to EXECUTE"
        );
    }

done:
    for (size_t index = 0U; index < entry->parameter_count; ++index) {
        free(literals[index]);
    }
    free(literals);
    return status;
}

static int copy_user_variable_literal(mylite_db *database, const char *name, char **out_literal) {
    struct mylite_sql_ast_node variable = {
        .kind = MYLITE_SQL_AST_IDENTIFIER,
    };
    char *variable_text = NULL;
    struct mylite_expression_value value = {0};
    int status = MYLITE_OK;

    *out_literal = NULL;
    variable_text = sqlite3_mprintf("@%s", name);
    if (variable_text == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    variable.span = (struct mylite_sql_source_span){
        .text = variable_text,
        .length = strlen(variable_text),
    };
    status = mylite_user_variable_eval_identifier(database, &variable, &value);
    sqlite3_free(variable_text);
    if (status != 0) {
        return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
    }

    status = copy_sql_literal_from_value(&value, out_literal);
    mylite_expression_value_deinit(&value);
    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
    }
    return status;
}

static int copy_sql_literal_from_value(
    const struct mylite_expression_value *value,
    char **out_literal
) {
    char buffer[64];
    int length = 0;

    if (value == NULL || value->kind == MYLITE_EXPRESSION_VALUE_NULL) {
        return copy_static_text("NULL", out_literal);
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        return copy_quoted_sql_text_literal(
            value->text_value,
            value->text_value == NULL ? 0U : value->text_length,
            out_literal
        );
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_INT64) {
        length = snprintf(buffer, sizeof(buffer), "%lld", (long long)value->int64_value);
    } else if (value->kind == MYLITE_EXPRESSION_VALUE_UINT64) {
        length = snprintf(buffer, sizeof(buffer), "%llu", (unsigned long long)value->uint64_value);
    } else {
        if (!isfinite(value->real_value)) {
            return copy_static_text("NULL", out_literal);
        }
        length = snprintf(buffer, sizeof(buffer), "%.17g", value->real_value);
    }

    if (length <= 0 || (size_t)length >= sizeof(buffer)) {
        return MYLITE_NOMEM;
    }
    *out_literal = mylite_copy_span_text(buffer, (size_t)length);
    return *out_literal == NULL ? MYLITE_NOMEM : MYLITE_OK;
}

static int copy_quoted_sql_text_literal(const char *text, size_t text_length, char **out_literal) {
    size_t quote_count = 0U;
    char *literal = NULL;
    size_t output = 0U;

    for (size_t index = 0U; index < text_length; ++index) {
        if (text[index] == '\'') {
            ++quote_count;
        }
    }
    if (text_length > (size_t)-1 - quote_count - 3U) {
        return MYLITE_NOMEM;
    }

    literal = malloc(text_length + quote_count + 3U);
    if (literal == NULL) {
        return MYLITE_NOMEM;
    }
    literal[output++] = '\'';
    for (size_t index = 0U; index < text_length; ++index) {
        literal[output++] = text[index];
        if (text[index] == '\'') {
            literal[output++] = '\'';
        }
    }
    literal[output++] = '\'';
    literal[output] = '\0';
    *out_literal = literal;
    return MYLITE_OK;
}

static int copy_static_text(const char *text, char **out_text) {
    *out_text = mylite_copy_span_text(text, strlen(text));
    return *out_text == NULL ? MYLITE_NOMEM : MYLITE_OK;
}
