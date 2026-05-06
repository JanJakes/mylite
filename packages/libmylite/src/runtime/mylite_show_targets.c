#include "mylite_show.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_information_schema.h"
#include "mylite_runtime.h"
#include "mylite_show_index_target.h"
#include "mylite_show_types.h"
#include "mylite_span.h"
#include "mylite_statement.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static int copy_show_columns_target(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct mylite_show_columns_target *out_target
);

static int normalize_show_columns_schema_name(char **schema_name);

static char *copy_show_columns_like_pattern(const struct mylite_sql_ast_node *statement);

static const struct mylite_sql_ast_node *show_columns_filter(
    const struct mylite_sql_ast_node *statement
);

static const struct mylite_sql_ast_node *show_columns_where_expression(
    const struct mylite_sql_ast_node *statement
);

static int copy_describe_table_target(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct mylite_show_columns_target *out_target
);

static char *copy_describe_column_pattern(const struct mylite_sql_ast_node *statement);

int mylite_show_prepare_columns_statement(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_stmt **out_stmt
) {
    struct mylite_show_columns_target target = {0};
    char *like_pattern = NULL;
    char *sqlite_sql = NULL;
    int status = copy_show_columns_target(database, statement, &target);

    if (status == MYLITE_OK) {
        status = mylite_show_validate_columns_target(
            database,
            &target,
            "SHOW COLUMNS for information_schema tables is not supported"
        );
    }
    if (status == MYLITE_OK) {
        like_pattern = copy_show_columns_like_pattern(statement);
        const struct mylite_sql_ast_node *filter = show_columns_filter(statement);

        if (filter != NULL && filter->kind == MYLITE_SQL_AST_LITERAL && like_pattern == NULL) {
            status = MYLITE_NOMEM;
        }
    }
    if (status == MYLITE_OK) {
        status = mylite_show_columns_sql(
            database,
            &(const struct mylite_show_columns_query){
                .schema_name = target.schema_name,
                .table_name = target.table_name,
                .like_pattern = like_pattern,
                .where_expression = show_columns_where_expression(statement),
                .full = statement->show_columns_full,
                .temporary = target.temporary,
            },
            &sqlite_sql
        );
    }
    if (status == MYLITE_OK) {
        status = mylite_statement_prepare_sqlite(database, sqlite_sql, out_stmt);
    }

    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
    }
    mylite_show_columns_target_deinit(&target);
    free(like_pattern);
    sqlite3_free(sqlite_sql);
    return status;
}

int mylite_show_prepare_describe_table_statement(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_stmt **out_stmt
) {
    struct mylite_show_columns_target target = {0};
    char *column_pattern = NULL;
    char *sqlite_sql = NULL;
    int status = copy_describe_table_target(database, statement, &target);

    if (status == MYLITE_OK) {
        status = mylite_show_validate_columns_target(
            database,
            &target,
            "DESCRIBE for information_schema tables is "
            "not supported"
        );
    }
    if (status == MYLITE_OK) {
        column_pattern = copy_describe_column_pattern(statement);
        if (mylite_ast_child_at(statement, 1U) != NULL && column_pattern == NULL) {
            status = MYLITE_NOMEM;
        }
    }
    if (status == MYLITE_OK) {
        status = mylite_show_columns_sql(
            database,
            &(const struct mylite_show_columns_query){
                .schema_name = target.schema_name,
                .table_name = target.table_name,
                .like_pattern = column_pattern,
                .full = false,
                .temporary = target.temporary,
            },
            &sqlite_sql
        );
    }
    if (status == MYLITE_OK) {
        status = mylite_statement_prepare_sqlite(database, sqlite_sql, out_stmt);
    }

    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
    }
    mylite_show_columns_target_deinit(&target);
    free(column_pattern);
    sqlite3_free(sqlite_sql);
    return status;
}

int mylite_show_prepare_index_statement(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_stmt **out_stmt
) {
    const struct mylite_sql_ast_node *where_clause =
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_WHERE_CLAUSE);
    struct mylite_show_index_target target = {0};
    char *sqlite_sql = NULL;
    int status = mylite_show_index_copy_target(database, statement, &target);

    if (status == MYLITE_OK) {
        status = mylite_show_index_validate_target(database, &target);
    }
    if (status == MYLITE_OK) {
        status = mylite_show_index_sql(
            database,
            &(const struct mylite_show_index_query){
                .schema_name = target.schema_name,
                .table_name = target.table_name,
                .temporary = target.temporary,
                .where_expression =
                    where_clause == NULL ? NULL : mylite_ast_child_at(where_clause, 0U),
            },
            &sqlite_sql
        );
    }
    if (status == MYLITE_OK) {
        status = mylite_statement_prepare_sqlite(database, sqlite_sql, out_stmt);
    }

    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
    }
    mylite_show_index_target_deinit(&target);
    sqlite3_free(sqlite_sql);
    return status;
}

int mylite_show_copy_columns_table_target(
    const struct mylite_show_columns_source_nodes *source,
    struct mylite_show_columns_target *out_target
) {
    char *parts[3] = {0};
    size_t part_count = 0U;
    int status = mylite_copy_identifier_parts(source->table_name, parts, &part_count);

    if (status != MYLITE_OK) {
        goto cleanup;
    }
    if (part_count == 0U || part_count > 2U) {
        status = MYLITE_UNSUPPORTED;
        goto cleanup;
    }

    if (source->explicit_schema != NULL) {
        out_target->schema_name = mylite_copy_identifier_span(source->explicit_schema);
        if (out_target->schema_name == NULL) {
            status = MYLITE_NOMEM;
            goto cleanup;
        }
        out_target->table_name = parts[part_count - 1U];
        parts[part_count - 1U] = NULL;
    } else if (part_count == 2U) {
        out_target->schema_name = parts[0];
        out_target->table_name = parts[1];
        parts[0] = NULL;
        parts[1] = NULL;
    } else {
        out_target->table_name = parts[0];
        parts[0] = NULL;
    }

    if (out_target->schema_name != NULL) {
        status = normalize_show_columns_schema_name(&out_target->schema_name);
    }

cleanup:
    for (size_t index = 0U; index < part_count; ++index) {
        free(parts[index]);
    }
    if (status != MYLITE_OK) {
        mylite_show_columns_target_deinit(out_target);
    }
    return status;
}

int mylite_show_copy_columns_selected_schema(
    mylite_db *database,
    struct mylite_show_columns_target *target
) {
    if (database->selected_schema == NULL || database->selected_schema[0] == '\0') {
        (void)mylite_diagnostics_set_error_message(database, "No database selected");
        return MYLITE_EXEC_ERROR;
    }

    target->schema_name = mylite_copy_nonempty_cstring(database->selected_schema);
    if (target->schema_name == NULL) {
        return MYLITE_NOMEM;
    }
    return normalize_show_columns_schema_name(&target->schema_name);
}

int mylite_show_validate_columns_target(
    mylite_db *database,
    struct mylite_show_columns_target *target,
    const char *information_schema_unsupported_message
) {
    struct mylite_schema_presence presence;
    bool exists = false;
    int status = mylite_catalog_schema_exists(database, target->schema_name, &presence);

    if (status != MYLITE_OK) {
        return status;
    }
    if (!presence.exists) {
        (void)mylite_diagnostics_set_error_message_parts(
            database,
            "Unknown database '",
            target->schema_name,
            "'"
        );
        return MYLITE_EXEC_ERROR;
    }
    if (mylite_ascii_case_equal(target->schema_name, "information_schema")) {
        if (!mylite_information_schema_has_table(target->table_name)) {
            return mylite_information_schema_set_unknown_table_error(database, target->table_name);
        }
        (void)
            mylite_diagnostics_set_error_message(database, information_schema_unsupported_message);
        return MYLITE_UNSUPPORTED;
    }

    status = mylite_catalog_temporary_table_exists(
        database,
        target->schema_name,
        target->table_name,
        &exists
    );
    if (status != MYLITE_OK) {
        return status;
    }
    if (exists) {
        target->temporary = true;
        return MYLITE_OK;
    }
    status = mylite_catalog_persistent_table_exists(
        database,
        target->schema_name,
        target->table_name,
        &exists
    );
    if (status != MYLITE_OK) {
        return status;
    }
    if (!exists) {
        return mylite_diagnostics_set_table_doesnt_exist_error(
            database,
            target->schema_name,
            target->table_name
        );
    }
    target->temporary = false;
    return MYLITE_OK;
}

void mylite_show_columns_target_deinit(struct mylite_show_columns_target *target) {
    if (target == NULL) {
        return;
    }

    free(target->schema_name);
    free(target->table_name);
    *target = (struct mylite_show_columns_target){0};
}

static int copy_show_columns_target(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct mylite_show_columns_target *out_target
) {
    const struct mylite_sql_ast_node *table_name = mylite_ast_child_at(statement, 0U);
    const struct mylite_sql_ast_node *possible_schema = mylite_ast_child_at(statement, 1U);
    const struct mylite_sql_ast_node *explicit_schema =
        possible_schema != NULL && possible_schema->kind == MYLITE_SQL_AST_IDENTIFIER
            ? possible_schema
            : NULL;
    int status = MYLITE_OK;

    *out_target = (struct mylite_show_columns_target){0};
    status = mylite_show_copy_columns_table_target(
        &(const struct mylite_show_columns_source_nodes){
            .table_name = table_name,
            .explicit_schema = explicit_schema,
        },
        out_target
    );
    if (status != MYLITE_OK) {
        if (status == MYLITE_UNSUPPORTED) {
            (void)mylite_diagnostics_set_error_message(
                database,
                "SHOW COLUMNS table names with more than two parts are not "
                "supported"
            );
        }
        return status;
    }
    if (out_target->schema_name == NULL) {
        status = mylite_show_copy_columns_selected_schema(database, out_target);
    }
    return status;
}

static int normalize_show_columns_schema_name(char **schema_name) {
    char *normalized = NULL;

    if (schema_name == NULL || *schema_name == NULL ||
        !mylite_ascii_case_equal(*schema_name, "information_schema") ||
        strcmp(*schema_name, "information_schema") == 0) {
        return MYLITE_OK;
    }

    normalized = mylite_copy_nonempty_cstring("information_schema");
    if (normalized == NULL) {
        return MYLITE_NOMEM;
    }

    free(*schema_name);
    *schema_name = normalized;
    return MYLITE_OK;
}

static char *copy_show_columns_like_pattern(const struct mylite_sql_ast_node *statement) {
    const struct mylite_sql_ast_node *literal = show_columns_filter(statement);

    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        return NULL;
    }
    return mylite_show_copy_like_pattern_span(literal);
}

static const struct mylite_sql_ast_node *show_columns_filter(
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *possible_schema = mylite_ast_child_at(statement, 1U);

    if (possible_schema == NULL || possible_schema->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return possible_schema;
    }
    return mylite_ast_child_at(statement, 2U);
}

static const struct mylite_sql_ast_node *show_columns_where_expression(
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *filter = show_columns_filter(statement);

    if (filter == NULL || filter->kind != MYLITE_SQL_AST_WHERE_CLAUSE) {
        return NULL;
    }
    return mylite_ast_child_at(filter, 0U);
}

static int copy_describe_table_target(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct mylite_show_columns_target *out_target
) {
    int status = MYLITE_OK;

    *out_target = (struct mylite_show_columns_target){0};
    status = mylite_show_copy_columns_table_target(
        &(const struct mylite_show_columns_source_nodes){
            .table_name = mylite_ast_child_at(statement, 0U),
            .explicit_schema = NULL,
        },
        out_target
    );
    if (status != MYLITE_OK) {
        if (status == MYLITE_UNSUPPORTED) {
            (void)mylite_diagnostics_set_error_message(
                database,
                "DESCRIBE table names with more than two parts are not "
                "supported"
            );
        }
        return status;
    }
    if (out_target->schema_name == NULL) {
        status = mylite_show_copy_columns_selected_schema(database, out_target);
    }
    return status;
}

static char *copy_describe_column_pattern(const struct mylite_sql_ast_node *statement) {
    const struct mylite_sql_ast_node *filter = mylite_ast_child_at(statement, 1U);

    if (filter == NULL) {
        return NULL;
    }
    if (filter->kind == MYLITE_SQL_AST_LITERAL) {
        return mylite_show_copy_like_pattern_span(filter);
    }
    return mylite_copy_identifier_span(filter);
}
