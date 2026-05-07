#include <mylite/mylite.h>

#include "mylite_ast.h"
#include "mylite_catalog.h"
#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_parser.h"
#include "mylite_result.h"
#include "mylite_sqlite_registration.h"
#include "mylite_statement_context.h"
#include "sqlite3.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    mysql_error_parse = 1064,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_table_exists = 1050,
    mysql_error_unknown_table = 1051,
    mysql_error_identifier_too_long = 1059,
    mysql_error_duplicate_column = 1060,
    mysql_error_incorrect_database_name = 1102,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_unknown = 1105,
    mysql_error_incorrect_column_name = 1166,
    table_name_part_capacity = 3,
};

struct table_name_resolution {
    struct mylite_catalog_schema_descriptor schema;
    char table_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
};

struct planned_column {
    char name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    const char *logical_type;
    const char *physical_type;
    bool is_nullable;
};

struct planned_create_table {
    struct table_name_resolution target;
    struct planned_column *columns;
    size_t column_count;
};

struct dynamic_string {
    char *text;
    size_t length;
    size_t capacity;
};

struct show_tables_context {
    mylite_result *result;
};

static int execute_parsed_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_empty_statement(struct mylite_db *database, mylite_result **out_result);
static int execute_use_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_create_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_drop_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_show_tables_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int finish_successful_result(
    struct mylite_db *database,
    mylite_result *result,
    mylite_result **out_result
);

static int plan_create_table(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_create_table *out_plan
);
static void planned_create_table_deinit(struct planned_create_table *plan);
static int create_table_from_plan(
    struct mylite_db *database,
    const struct planned_create_table *plan
);
static int insert_create_table_catalog_rows(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    const char *physical_name,
    struct mylite_catalog_table_descriptor *out_table
);
static int execute_physical_create_table(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const char *physical_name
);
static int execute_physical_drop_table(struct mylite_db *database, const char *physical_name);

static int resolve_table_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct table_name_resolution *out_resolution
);
static int resolve_schema_name(
    struct mylite_db *database,
    const char *schema_name,
    struct mylite_catalog_schema_descriptor *out_schema
);
static int resolve_selected_schema(
    struct mylite_db *database,
    struct mylite_catalog_schema_descriptor *out_schema
);
static int collect_identifier_parts(
    const struct mylite_sql_ast_node *node,
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_capacity,
    size_t *part_count,
    struct mylite_db *database
);
static int copy_identifier_text(
    const struct mylite_sql_ast_node *node,
    char *destination,
    size_t destination_size,
    struct mylite_db *database
);
static int copy_quoted_identifier_text(
    const char *source,
    size_t source_size,
    char *destination,
    size_t destination_size
);
static int copy_unquoted_identifier_text(
    const char *source,
    size_t source_size,
    char *destination,
    size_t destination_size
);

static int plan_columns(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_list,
    struct planned_column *columns,
    size_t column_count
);
static int plan_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    struct planned_column *out_column
);
static int check_duplicate_column_names(
    struct mylite_db *database,
    const struct planned_column *columns,
    size_t column_count
);
static bool text_equals_ascii_case_insensitive(const char *left, const char *right);
static char ascii_lower(unsigned char byte);
static int map_integer_type(
    const struct mylite_sql_ast_node *type_node,
    const char **out_logical_type,
    const char **out_physical_type
);
static bool column_is_nullable(const struct mylite_sql_ast_node *nullability_node);

static int append_show_table(const struct mylite_catalog_table_descriptor *table, void *user_data);

static int build_physical_table_name(int64_t table_id, char *destination, size_t destination_size);
static int build_create_table_sql(
    const struct planned_create_table *plan,
    const char *physical_name,
    char **out_sql
);
static int build_drop_table_sql(const char *physical_name, char **out_sql);
static int execute_sqlite_schema_sql(struct mylite_db *database, const char *sql);

static void dynamic_string_init(struct dynamic_string *string);
static void dynamic_string_deinit(struct dynamic_string *string);
static int dynamic_string_append(struct dynamic_string *string, const char *text);
static int dynamic_string_append_char(struct dynamic_string *string, char byte);
static int dynamic_string_append_quoted_identifier(struct dynamic_string *string, const char *text);
static int dynamic_string_reserve(struct dynamic_string *string, size_t required_capacity);
static char *dynamic_string_take(struct dynamic_string *string);

static const struct mylite_sql_ast_node *child_at(
    const struct mylite_sql_ast_node *node,
    size_t index
);
static int script_statement_count(const struct mylite_sql_ast_node *root, size_t *out_count);
static void set_parse_error(
    struct mylite_db *database,
    const struct mylite_sql_parse_result *parse_result
);
static void set_unsupported_error(struct mylite_db *database, const char *message);
static void set_no_database_error(struct mylite_db *database);
static void set_unknown_database_error(struct mylite_db *database, const char *schema_name);
static void set_table_exists_error(struct mylite_db *database, const char *table_name);
static void set_unknown_table_error(
    struct mylite_db *database,
    const char *schema_name,
    const char *table_name
);
static void set_duplicate_column_error(struct mylite_db *database, const char *column_name);
static void set_identifier_too_long_error(struct mylite_db *database, const char *kind);
static void set_reserved_name_error(struct mylite_db *database, const char *kind, const char *name);
static void set_nomem_error(struct mylite_db *database);
static void set_physical_sqlite_error(struct mylite_db *database);
static void set_runtime_error(struct mylite_db *database, const char *message);
static void set_internal_error_if_clear(struct mylite_db *database, int rc, const char *message);
static int status_from_parse_status(enum mylite_sql_parse_status status);

int mylite_execute(
    mylite_db *database,
    const char *sql,
    size_t sql_size,
    mylite_result **out_result
) {
    struct mylite_statement_context context;
    struct mylite_sql_parse_result parse_result;
    const struct mylite_sql_ast_node *statement = NULL;
    size_t statement_count = 0U;
    int rc = MYLITE_OK;

    if (out_result == NULL) {
        if (database != NULL) {
            mylite_diagnostics_set_error(
                mylite_connection_diagnostics(database),
                MYLITE_MISUSE,
                "HY000",
                mylite_diagnostics_misuse_message()
            );
        }
        return MYLITE_MISUSE;
    }
    *out_result = NULL;
    if (database == NULL || sql == NULL) {
        if (database != NULL) {
            mylite_diagnostics_set_error(
                mylite_connection_diagnostics(database),
                MYLITE_MISUSE,
                "HY000",
                mylite_diagnostics_misuse_message()
            );
        }
        return MYLITE_MISUSE;
    }

    mylite_statement_context_init(&context);
    rc = mylite_statement_context_begin(&context, database, sql, sql_size);
    if (rc != MYLITE_OK) {
        mylite_statement_context_deinit(&context);
        return rc;
    }

    rc = status_from_parse_status(mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = sql,
            .length = sql_size,
            .modes = 0U,
        },
        &parse_result
    ));
    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        } else {
            set_parse_error(database, &parse_result);
        }
        mylite_sql_parse_result_deinit(&parse_result);
        (void)mylite_statement_context_end(&context, rc);
        mylite_statement_context_deinit(&context);
        return rc;
    }

    rc = script_statement_count(parse_result.root, &statement_count);
    if (rc == MYLITE_OK && statement_count == 0U) {
        rc = execute_empty_statement(database, out_result);
    } else if (rc == MYLITE_OK && statement_count == 1U) {
        statement = child_at(parse_result.root, 0U);
        rc = execute_parsed_statement(database, statement, out_result);
    } else if (rc == MYLITE_OK) {
        set_unsupported_error(database, "multiple statements are not supported");
        rc = MYLITE_ERROR;
    }

    mylite_sql_parse_result_deinit(&parse_result);
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "statement execution failed");
        mylite_result_free(*out_result);
        *out_result = NULL;
    }
    (void)mylite_statement_context_end(&context, rc);
    mylite_statement_context_deinit(&context);

    return rc;
}

static int execute_parsed_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    if (statement == NULL) {
        set_unsupported_error(database, "empty statement is not supported");
        return MYLITE_ERROR;
    }

    switch (statement->kind) {
    case MYLITE_SQL_AST_USE_STATEMENT:
        return execute_use_statement(database, statement, out_result);
    case MYLITE_SQL_AST_CREATE_TABLE_STATEMENT:
        return execute_create_table_statement(database, statement, out_result);
    case MYLITE_SQL_AST_DROP_TABLE_STATEMENT:
        return execute_drop_table_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SHOW_TABLES_STATEMENT:
        return execute_show_tables_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SELECT_STATEMENT:
    case MYLITE_SQL_AST_SCRIPT:
    case MYLITE_SQL_AST_SELECT_LIST:
    case MYLITE_SQL_AST_SELECT_ITEM:
    case MYLITE_SQL_AST_FROM_DUAL:
    case MYLITE_SQL_AST_IDENTIFIER:
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER:
    case MYLITE_SQL_AST_WILDCARD:
    case MYLITE_SQL_AST_LITERAL:
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
    case MYLITE_SQL_AST_COLUMN_DEFINITION_LIST:
    case MYLITE_SQL_AST_COLUMN_DEFINITION:
    case MYLITE_SQL_AST_INTEGER_TYPE:
    case MYLITE_SQL_AST_NULLABILITY:
        break;
    }

    set_unsupported_error(database, "statement is not supported by this MyLite build");

    return MYLITE_ERROR;
}

static int execute_empty_statement(struct mylite_db *database, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_use_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct mylite_catalog_schema_descriptor schema = {0};
    char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    mylite_result *result = NULL;
    int written = 0;
    int rc =
        copy_identifier_text(child_at(statement, 0U), schema_name, sizeof(schema_name), database);

    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(schema_name)) {
        set_reserved_name_error(database, "database", schema_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = resolve_schema_name(database, schema_name, &schema);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    written = snprintf(
        database->session.selected_schema,
        sizeof(database->session.selected_schema),
        "%s",
        schema.name
    );
    if (written < 0 || (size_t)written >= sizeof(database->session.selected_schema)) {
        mylite_result_free(result);
        set_identifier_too_long_error(database, "database");
        return MYLITE_ERROR;
    }
    database->session.has_selected_schema = true;

    return finish_successful_result(database, result, out_result);
}

static int execute_create_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_create_table plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_create_table(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = create_table_from_plan(database, &plan);
    }
    planned_create_table_deinit(&plan);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_drop_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct table_name_resolution target = {0};
    struct mylite_catalog_table_descriptor table = {0};
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = resolve_table_name(database, child_at(statement, 0U), &target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(target.table_name)) {
        set_reserved_name_error(database, "table", target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_read_table_by_name(
            database,
            target.schema.schema_id,
            target.table_name,
            &table
        );
        if (rc != MYLITE_OK) {
            set_unknown_table_error(database, target.schema.name, target.table_name);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_begin_mutation(database, &mutation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_delete_table_in_mutation(database, &mutation, table.table_id);
    }
    if (rc == MYLITE_OK) {
        rc = execute_physical_drop_table(database, table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        mylite_catalog_rollback_mutation(database, &mutation);
        mylite_result_free(result);
        return rc;
    }

    ++database->session.sqlite_schema_generation;

    return finish_successful_result(database, result, out_result);
}

static int execute_show_tables_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct mylite_catalog_schema_descriptor schema = {0};
    const struct mylite_sql_ast_node *schema_node = child_at(statement, 0U);
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY + sizeof("Tables_in_")];
    struct show_tables_context context = {0};
    mylite_result *result = NULL;
    int written = 0;
    int rc = MYLITE_OK;

    if (schema_node == NULL) {
        rc = resolve_selected_schema(database, &schema);
    } else {
        char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];

        rc = copy_identifier_text(schema_node, schema_name, sizeof(schema_name), database);
        if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(schema_name)) {
            set_reserved_name_error(database, "database", schema_name);
            rc = MYLITE_ERROR;
        }
        if (rc == MYLITE_OK) {
            rc = resolve_schema_name(database, schema_name, &schema);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        written = snprintf(column_name, sizeof(column_name), "Tables_in_%s", schema.name);
        if (written < 0 || (size_t)written >= sizeof(column_name)) {
            set_identifier_too_long_error(database, "database");
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_column(result, column_name);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        context.result = result;
        rc = mylite_catalog_for_each_table_in_schema(
            database,
            schema.schema_id,
            append_show_table,
            &context
        );
        if (rc != MYLITE_OK) {
            set_runtime_error(database, "failed to build SHOW TABLES result");
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int finish_successful_result(
    struct mylite_db *database,
    mylite_result *result,
    mylite_result **out_result
) {
    mylite_result_set_affected_rows(result, 0);
    mylite_result_set_warning_count(
        result,
        mylite_diagnostics_warning_count(mylite_connection_diagnostics(database))
    );
    *out_result = result;

    return MYLITE_OK;
}

static int plan_create_table(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_create_table *out_plan
) {
    const struct mylite_sql_ast_node *column_list = child_at(statement, 1U);
    size_t column_count = mylite_sql_ast_node_child_count(column_list);
    int rc = MYLITE_OK;

    *out_plan = (struct planned_create_table){0};
    rc = resolve_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        return MYLITE_ERROR;
    }

    if (column_count == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (column_count > SIZE_MAX / sizeof(*out_plan->columns)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    out_plan->columns = calloc(column_count, sizeof(*out_plan->columns));
    if (out_plan->columns == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_plan->column_count = column_count;

    rc = plan_columns(database, column_list, out_plan->columns, out_plan->column_count);
    if (rc == MYLITE_OK) {
        rc = check_duplicate_column_names(database, out_plan->columns, out_plan->column_count);
    }

    if (rc != MYLITE_OK) {
        planned_create_table_deinit(out_plan);
        return rc;
    }

    return MYLITE_OK;
}

static void planned_create_table_deinit(struct planned_create_table *plan) {
    if (plan == NULL) {
        return;
    }

    free(plan->columns);
    *plan = (struct planned_create_table){0};
}

static int create_table_from_plan(
    struct mylite_db *database,
    const struct planned_create_table *plan
) {
    struct mylite_catalog_table_descriptor existing_table = {0};
    struct mylite_catalog_table_descriptor table = {0};
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    char physical_name[MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY];
    int64_t table_id = 0;
    int rc = mylite_catalog_read_table_by_name(
        database,
        plan->target.schema.schema_id,
        plan->target.table_name,
        &existing_table
    );

    if (rc == MYLITE_OK) {
        set_table_exists_error(database, plan->target.table_name);
        return MYLITE_ERROR;
    }

    rc = mylite_catalog_begin_mutation(database, &mutation);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_allocate_table_id_in_mutation(database, &mutation, &table_id);
    }
    if (rc == MYLITE_OK) {
        rc = build_physical_table_name(table_id, physical_name, sizeof(physical_name));
    }
    if (rc == MYLITE_OK) {
        rc = insert_create_table_catalog_rows(
            database,
            plan,
            &mutation,
            table_id,
            physical_name,
            &table
        );
    }
    if (rc == MYLITE_OK) {
        rc = execute_physical_create_table(database, plan, table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    ++database->session.sqlite_schema_generation;

    return MYLITE_OK;
}

static int insert_create_table_catalog_rows(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    const char *physical_name,
    struct mylite_catalog_table_descriptor *out_table
) {
    int rc = mylite_catalog_insert_table_in_mutation(
        database,
        mutation,
        table_id,
        plan->target.schema.schema_id,
        plan->target.table_name,
        physical_name,
        MYLITE_CATALOG_TABLE_KIND_BASE,
        out_table
    );

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        const struct planned_column *column = &plan->columns[column_index];

        rc = mylite_catalog_insert_column_in_mutation(
            database,
            mutation,
            table_id,
            (int64_t)column_index + 1,
            column->name,
            column->logical_type,
            column->physical_type,
            column->is_nullable,
            NULL
        );
    }

    return rc;
}

static int execute_physical_create_table(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const char *physical_name
) {
    char *sql = NULL;
    int rc = build_create_table_sql(plan, physical_name, &sql);

    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);

    return rc;
}

static int execute_physical_drop_table(struct mylite_db *database, const char *physical_name) {
    char *sql = NULL;
    int rc = build_drop_table_sql(physical_name, &sql);

    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);

    return rc;
}

static int resolve_table_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct table_name_resolution *out_resolution
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t part_count = 0U;
    int rc = MYLITE_OK;

    *out_resolution = (struct table_name_resolution){0};
    rc = collect_identifier_parts(
        database == NULL ? NULL : node,
        parts,
        table_name_part_capacity,
        &part_count,
        database
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (part_count == 1U) {
        rc = resolve_selected_schema(database, &out_resolution->schema);
        if (rc == MYLITE_OK) {
            memcpy(out_resolution->table_name, parts[0], sizeof(out_resolution->table_name));
        }
        return rc;
    }
    if (part_count == 2U) {
        if (mylite_catalog_name_is_reserved(parts[0])) {
            set_reserved_name_error(database, "database", parts[0]);
            return MYLITE_ERROR;
        }
        rc = resolve_schema_name(database, parts[0], &out_resolution->schema);
        if (rc == MYLITE_OK) {
            memcpy(out_resolution->table_name, parts[1], sizeof(out_resolution->table_name));
        }
        return rc;
    }

    set_parse_error(database, NULL);

    return MYLITE_ERROR;
}

static int resolve_schema_name(
    struct mylite_db *database,
    const char *schema_name,
    struct mylite_catalog_schema_descriptor *out_schema
) {
    int rc = mylite_catalog_read_schema_by_name(database, schema_name, out_schema);

    if (rc != MYLITE_OK) {
        set_unknown_database_error(database, schema_name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int resolve_selected_schema(
    struct mylite_db *database,
    struct mylite_catalog_schema_descriptor *out_schema
) {
    if (database == NULL || !database->session.has_selected_schema) {
        set_no_database_error(database);
        return MYLITE_ERROR;
    }

    return resolve_schema_name(database, database->session.selected_schema, out_schema);
}

static int collect_identifier_parts(
    const struct mylite_sql_ast_node *node,
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_capacity,
    size_t *part_count,
    struct mylite_db *database
) {
    const struct mylite_sql_ast_node *tail_nodes[table_name_part_capacity];
    const struct mylite_sql_ast_node *current = node;
    size_t tail_count = 0U;
    int rc = MYLITE_OK;

    if (current == NULL || part_count == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    while (current->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        if (tail_count >= part_capacity || tail_count >= table_name_part_capacity) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        tail_nodes[tail_count] = child_at(current, 1U);
        ++tail_count;
        current = child_at(current, 0U);
        if (current == NULL) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
    }

    if (current->kind != MYLITE_SQL_AST_IDENTIFIER) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (*part_count >= part_capacity) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    rc = copy_identifier_text(
        current,
        parts[*part_count],
        MYLITE_CATALOG_IDENTIFIER_CAPACITY,
        database
    );
    if (rc != MYLITE_OK) {
        return MYLITE_ERROR;
    }
    ++(*part_count);

    while (tail_count > 0U) {
        --tail_count;
        if (*part_count >= part_capacity) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        rc = copy_identifier_text(
            tail_nodes[tail_count],
            parts[*part_count],
            MYLITE_CATALOG_IDENTIFIER_CAPACITY,
            database
        );
        if (rc != MYLITE_OK) {
            return MYLITE_ERROR;
        }
        ++(*part_count);
    }

    return MYLITE_OK;
}

static int copy_identifier_text(
    const struct mylite_sql_ast_node *node,
    char *destination,
    size_t destination_size,
    struct mylite_db *database
) {
    const char *source = NULL;
    size_t source_size = 0U;
    int rc = MYLITE_OK;

    if (node == NULL || node->kind != MYLITE_SQL_AST_IDENTIFIER || destination == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    source = node->span.text;
    source_size = node->span.length;
    if (source == NULL || source_size == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    if (source[0] == '`') {
        rc = copy_quoted_identifier_text(source, source_size, destination, destination_size);
    } else {
        rc = copy_unquoted_identifier_text(source, source_size, destination, destination_size);
    }
    if (rc != MYLITE_OK) {
        set_identifier_too_long_error(database, "identifier");
        return rc;
    }

    return MYLITE_OK;
}

static int copy_quoted_identifier_text(
    const char *source,
    size_t source_size,
    char *destination,
    size_t destination_size
) {
    size_t destination_index = 0U;

    if (source_size < 2U || source[source_size - 1U] != '`') {
        return MYLITE_ERROR;
    }

    for (size_t source_index = 1U; source_index + 1U < source_size; ++source_index) {
        if (destination_index + 1U >= destination_size) {
            return MYLITE_ERROR;
        }
        if (source[source_index] == '`' && source[source_index + 1U] == '`') {
            destination[destination_index] = '`';
            ++source_index;
        } else {
            destination[destination_index] = source[source_index];
        }
        ++destination_index;
    }
    destination[destination_index] = '\0';

    return destination_index == 0U ? MYLITE_ERROR : MYLITE_OK;
}

static int copy_unquoted_identifier_text(
    const char *source,
    size_t source_size,
    char *destination,
    size_t destination_size
) {
    if (source_size == 0U || source_size >= destination_size) {
        return MYLITE_ERROR;
    }

    memcpy(destination, source, source_size);
    destination[source_size] = '\0';

    return MYLITE_OK;
}

static int plan_columns(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_list,
    struct planned_column *columns,
    size_t column_count
) {
    const struct mylite_sql_ast_node *column_node = child_at(column_list, 0U);

    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        int rc = plan_column(database, column_node, &columns[column_index]);

        if (rc != MYLITE_OK) {
            return rc;
        }
        column_node = column_node == NULL ? NULL : column_node->next_sibling;
    }

    return MYLITE_OK;
}

static int plan_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    struct planned_column *out_column
) {
    int rc = MYLITE_OK;

    *out_column = (struct planned_column){0};
    rc = copy_identifier_text(
        child_at(column_node, 0U),
        out_column->name,
        sizeof(out_column->name),
        database
    );
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_column->name)) {
        set_reserved_name_error(database, "column", out_column->name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = map_integer_type(
            child_at(column_node, 1U),
            &out_column->logical_type,
            &out_column->physical_type
        );
    }
    if (rc == MYLITE_OK) {
        out_column->is_nullable = column_is_nullable(child_at(column_node, 2U));
    }

    return rc;
}

static int check_duplicate_column_names(
    struct mylite_db *database,
    const struct planned_column *columns,
    size_t column_count
) {
    for (size_t left = 0U; left < column_count; ++left) {
        for (size_t right = left + 1U; right < column_count; ++right) {
            if (text_equals_ascii_case_insensitive(columns[left].name, columns[right].name)) {
                set_duplicate_column_error(database, columns[right].name);
                return MYLITE_ERROR;
            }
        }
    }

    return MYLITE_OK;
}

static bool text_equals_ascii_case_insensitive(const char *left, const char *right) {
    size_t index = 0U;

    if (left == NULL || right == NULL) {
        return false;
    }
    while (left[index] != '\0' && right[index] != '\0') {
        if (ascii_lower((unsigned char)left[index]) != ascii_lower((unsigned char)right[index])) {
            return false;
        }
        ++index;
    }

    if (left[index] != '\0') {
        return false;
    }
    if (right[index] != '\0') {
        return false;
    }

    return true;
}

static char ascii_lower(unsigned char byte) {
    if (byte >= 'A' && byte <= 'Z') {
        return (char)(byte + ('a' - 'A'));
    }

    return (char)byte;
}

static int map_integer_type(
    const struct mylite_sql_ast_node *type_node,
    const char **out_logical_type,
    const char **out_physical_type
) {
    enum mylite_sql_ast_integer_type type = mylite_sql_ast_node_integer_type(type_node);
    int is_unsigned = mylite_sql_ast_node_integer_type_is_unsigned(type_node);

    if (out_logical_type == NULL || out_physical_type == NULL) {
        return MYLITE_MISUSE;
    }

    *out_physical_type = "INTEGER";
    if (type == MYLITE_SQL_AST_INTEGER_TYPE_INT && is_unsigned == 0) {
        *out_logical_type = "INT";
        return MYLITE_OK;
    }
    if (type == MYLITE_SQL_AST_INTEGER_TYPE_INT && is_unsigned != 0) {
        *out_logical_type = "INT UNSIGNED";
        return MYLITE_OK;
    }
    if (type == MYLITE_SQL_AST_INTEGER_TYPE_BIGINT && is_unsigned == 0) {
        *out_logical_type = "BIGINT";
        return MYLITE_OK;
    }
    if (type == MYLITE_SQL_AST_INTEGER_TYPE_BIGINT && is_unsigned != 0) {
        *out_logical_type = "BIGINT UNSIGNED";
        return MYLITE_OK;
    }

    return MYLITE_ERROR;
}

static bool column_is_nullable(const struct mylite_sql_ast_node *nullability_node) {
    return mylite_sql_ast_node_nullability(nullability_node) != MYLITE_SQL_AST_NULLABILITY_NOT_NULL;
}

static int append_show_table(const struct mylite_catalog_table_descriptor *table, void *user_data) {
    struct show_tables_context *context = user_data;
    const char *values[1] = {NULL};

    if (table == NULL || context == NULL || context->result == NULL) {
        return MYLITE_MISUSE;
    }

    values[0] = table->name;

    return mylite_result_append_text_row(context->result, values);
}

static int build_physical_table_name(int64_t table_id, char *destination, size_t destination_size) {
    int written = snprintf(destination, destination_size, "_mylite_user_table_%" PRId64, table_id);

    if (written < 0 || (size_t)written >= destination_size) {
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int build_create_table_sql(
    const struct planned_create_table *plan,
    const char *physical_name,
    char **out_sql
) {
    struct dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    dynamic_string_init(&string);

    rc = dynamic_string_append(&string, "CREATE TABLE ");
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_quoted_identifier(&string, physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append(&string, " (");
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        const struct planned_column *column = &plan->columns[column_index];

        if (column_index != 0U) {
            rc = dynamic_string_append(&string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = dynamic_string_append_quoted_identifier(&string, column->name);
        }
        if (rc == MYLITE_OK) {
            rc = dynamic_string_append(&string, " INTEGER");
        }
        if (rc == MYLITE_OK && !column->is_nullable) {
            rc = dynamic_string_append(&string, " NOT NULL");
        }
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_char(&string, ')');
    }
    if (rc == MYLITE_OK) {
        *out_sql = dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    dynamic_string_deinit(&string);

    return rc;
}

static int build_drop_table_sql(const char *physical_name, char **out_sql) {
    struct dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    dynamic_string_init(&string);

    rc = dynamic_string_append(&string, "DROP TABLE ");
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_quoted_identifier(&string, physical_name);
    }
    if (rc == MYLITE_OK) {
        *out_sql = dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    dynamic_string_deinit(&string);

    return rc;
}

static int execute_sqlite_schema_sql(struct mylite_db *database, const char *sql) {
    int sqlite_rc = sqlite3_exec(database->sqlite, sql, NULL, NULL, NULL);
    int rc = mylite_sqlite_status_to_mylite(sqlite_rc);

    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
        return rc;
    }
    if (rc != MYLITE_OK) {
        set_physical_sqlite_error(database);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static void dynamic_string_init(struct dynamic_string *string) {
    *string = (struct dynamic_string){0};
}

static void dynamic_string_deinit(struct dynamic_string *string) {
    if (string == NULL) {
        return;
    }

    free(string->text);
    *string = (struct dynamic_string){0};
}

static int dynamic_string_append(struct dynamic_string *string, const char *text) {
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (string == NULL || text == NULL) {
        return MYLITE_MISUSE;
    }

    text_length = strlen(text);
    if (text_length > SIZE_MAX - string->length - 1U) {
        return MYLITE_NOMEM;
    }

    rc = dynamic_string_reserve(string, string->length + text_length + 1U);
    if (rc != MYLITE_OK) {
        return rc;
    }

    memcpy(&string->text[string->length], text, text_length);
    string->length += text_length;
    string->text[string->length] = '\0';

    return MYLITE_OK;
}

static int dynamic_string_append_char(struct dynamic_string *string, char byte) {
    int rc = MYLITE_OK;

    if (string == NULL) {
        return MYLITE_MISUSE;
    }
    if (string->length > SIZE_MAX - 2U) {
        return MYLITE_NOMEM;
    }

    rc = dynamic_string_reserve(string, string->length + 2U);
    if (rc != MYLITE_OK) {
        return rc;
    }

    string->text[string->length] = byte;
    ++string->length;
    string->text[string->length] = '\0';

    return MYLITE_OK;
}

static int dynamic_string_append_quoted_identifier(
    struct dynamic_string *string,
    const char *text
) {
    int rc = dynamic_string_append_char(string, '"');

    for (size_t index = 0U; rc == MYLITE_OK && text[index] != '\0'; ++index) {
        if (text[index] == '"') {
            rc = dynamic_string_append(string, "\"\"");
        } else {
            rc = dynamic_string_append_char(string, text[index]);
        }
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_char(string, '"');
    }

    return rc;
}

static int dynamic_string_reserve(struct dynamic_string *string, size_t required_capacity) {
    enum { initial_capacity = 128 };

    char *text = NULL;
    size_t capacity = string->capacity;

    if (required_capacity <= capacity) {
        return MYLITE_OK;
    }
    if (capacity == 0U) {
        capacity = initial_capacity;
    }
    while (capacity < required_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }

    text = realloc(string->text, capacity);
    if (text == NULL) {
        return MYLITE_NOMEM;
    }
    if (string->capacity == 0U) {
        text[0] = '\0';
    }
    string->text = text;
    string->capacity = capacity;

    return MYLITE_OK;
}

static char *dynamic_string_take(struct dynamic_string *string) {
    char *text = NULL;

    if (string == NULL) {
        return NULL;
    }

    text = string->text;
    string->text = NULL;
    string->length = 0U;
    string->capacity = 0U;

    return text;
}

static const struct mylite_sql_ast_node *child_at(
    const struct mylite_sql_ast_node *node,
    size_t index
) {
    const struct mylite_sql_ast_node *child = NULL;

    if (node == NULL) {
        return NULL;
    }

    child = node->first_child;
    for (size_t current = 0U; current < index && child != NULL; ++current) {
        child = child->next_sibling;
    }
    return child;
}

static int script_statement_count(const struct mylite_sql_ast_node *root, size_t *out_count) {
    if (root == NULL || root->kind != MYLITE_SQL_AST_SCRIPT || out_count == NULL) {
        return MYLITE_ERROR;
    }

    *out_count = mylite_sql_ast_node_child_count(root);

    return MYLITE_OK;
}

static void set_parse_error(
    struct mylite_db *database,
    const struct mylite_sql_parse_result *parse_result
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    const char *near_text = "end of input";
    int written = 0;

    if (parse_result != NULL && parse_result->error_token.text != NULL &&
        parse_result->error_token.length != 0U) {
        near_text = parse_result->error_token.text;
        written = snprintf(
            message,
            sizeof(message),
            "You have an error in your SQL syntax near '%.*s' at line %zu",
            (int)parse_result->error_token.length,
            near_text,
            parse_result->error_token.line
        );
    } else {
        written = snprintf(
            message,
            sizeof(message),
            "You have an error in your SQL syntax near '%s'",
            near_text
        );
    }
    if (written < 0) {
        message[0] = '\0';
    }

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_parse,
        "42000",
        message
    );
}

static void set_unsupported_error(struct mylite_db *database, const char *message) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_parse,
        "42000",
        message
    );
}

static void set_no_database_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_no_database_selected,
        "3D000",
        "No database selected"
    );
}

static void set_unknown_database_error(struct mylite_db *database, const char *schema_name) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Unknown database '%s'", schema_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_database,
        "42000",
        message
    );
}

static void set_table_exists_error(struct mylite_db *database, const char *table_name) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Table '%s' already exists", table_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_table_exists,
        "42S01",
        message
    );
}

static void set_unknown_table_error(
    struct mylite_db *database,
    const char *schema_name,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Unknown table '%s.%s'", schema_name, table_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_table,
        "42S02",
        message
    );
}

static void set_duplicate_column_error(struct mylite_db *database, const char *column_name) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Duplicate column name '%s'", column_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_duplicate_column,
        "42S21",
        message
    );
}

static void set_identifier_too_long_error(struct mylite_db *database, const char *kind) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "%s identifier is too long", kind);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_identifier_too_long,
        "42000",
        message
    );
}

static void set_reserved_name_error(
    struct mylite_db *database,
    const char *kind,
    const char *name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int code = mysql_error_incorrect_table_name;
    int written = 0;

    if (strcmp(kind, "database") == 0) {
        code = mysql_error_incorrect_database_name;
    } else if (strcmp(kind, "column") == 0) {
        code = mysql_error_incorrect_column_name;
    }

    written = snprintf(message, sizeof(message), "Incorrect %s name '%s'", kind, name);
    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(mylite_connection_diagnostics(database), code, "42000", message);
}

static void set_nomem_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        MYLITE_NOMEM,
        "HY001",
        "out of memory"
    );
}

static void set_physical_sqlite_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown,
        "HY000",
        "internal SQLite schema operation failed"
    );
}

static void set_runtime_error(struct mylite_db *database, const char *message) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown,
        "HY000",
        message
    );
}

static void set_internal_error_if_clear(struct mylite_db *database, int rc, const char *message) {
    if (database == NULL) {
        return;
    }
    if (mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) != MYLITE_OK) {
        return;
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
        return;
    }
    if (rc == MYLITE_MISUSE) {
        mylite_diagnostics_set_error(
            mylite_connection_diagnostics(database),
            MYLITE_MISUSE,
            "HY000",
            mylite_diagnostics_misuse_message()
        );
        return;
    }

    set_runtime_error(database, message);
}

static int status_from_parse_status(enum mylite_sql_parse_status status) {
    switch (status) {
    case MYLITE_SQL_PARSE_OK:
        return MYLITE_OK;
    case MYLITE_SQL_PARSE_NOMEM:
        return MYLITE_NOMEM;
    case MYLITE_SQL_PARSE_MISUSE:
        return MYLITE_MISUSE;
    case MYLITE_SQL_PARSE_LEXER_ERROR:
    case MYLITE_SQL_PARSE_SYNTAX_ERROR:
    case MYLITE_SQL_PARSE_STACK_OVERFLOW:
        return MYLITE_ERROR;
    }

    return MYLITE_ERROR;
}
