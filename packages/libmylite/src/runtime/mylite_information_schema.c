#include "mylite_information_schema.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_information_schema_dynamic.h"
#include "mylite_information_schema_target.h"
#include "mylite_show.h"
#include "mylite_span.h"
#include "mylite_statement.h"
#include "mylite_storage_engine.h"
#include "sqlite3.h"

#include <stdlib.h>

enum { information_schema_predicate_stack_initial_capacity = 4 };

struct information_schema_predicate_stack {
    const struct mylite_sql_ast_node **items;
    size_t count;
    size_t capacity;
};

static int information_schema_dynamic_table_sql(
    mylite_db *database,
    enum mylite_information_schema_table table,
    char **out_sql
);

static const char *information_schema_table_sql(enum mylite_information_schema_table table);

static int information_schema_table_query_sql(
    mylite_db *database,
    const char *table_name,
    char **out_sql
);

static int information_schema_tables_filtered_select_sql(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    char **out_sql
);

static bool information_schema_tables_from_clause(const struct mylite_sql_ast_node *from_clause);

static int append_information_schema_tables_projection(
    sqlite3_str *sql,
    const struct mylite_sql_ast_node *select_list
);

static int append_information_schema_tables_filter(
    mylite_db *database,
    sqlite3_str *sql,
    const struct mylite_sql_ast_node *where_clause
);

static int append_information_schema_tables_predicate(
    mylite_db *database,
    sqlite3_str *sql,
    const struct mylite_sql_ast_node *expression
);

static int append_information_schema_tables_predicate_term(
    mylite_db *database,
    sqlite3_str *sql,
    const struct mylite_sql_ast_node *expression
);

static const struct mylite_sql_ast_node *information_schema_tables_unwrap_parenthesized_expression(
    const struct mylite_sql_ast_node *expression
);

static bool information_schema_tables_predicate_stack_push(
    struct information_schema_predicate_stack *stack,
    const struct mylite_sql_ast_node *expression
);

static void information_schema_tables_predicate_stack_deinit(
    struct information_schema_predicate_stack *stack
);

static int append_information_schema_tables_equality(
    mylite_db *database,
    sqlite3_str *sql,
    const struct mylite_sql_ast_node *left,
    const struct mylite_sql_ast_node *right
);

static int append_information_schema_tables_null_predicate(
    sqlite3_str *sql,
    const struct mylite_sql_ast_node *operand,
    enum mylite_sql_ast_operator operator_kind
);

static int append_information_schema_tables_value(
    mylite_db *database,
    sqlite3_str *sql,
    const struct mylite_sql_ast_node *expression
);

static int append_information_schema_tables_order(
    sqlite3_str *sql,
    const struct mylite_sql_ast_node *order_by_clause
);

static const char *information_schema_tables_column_name(
    const struct mylite_sql_ast_node *expression
);

static bool information_schema_tables_database_function(
    const struct mylite_sql_ast_node *expression
);

static const char information_schema_schemata_sql[] =
    "SELECT 'def' AS CATALOG_NAME,"
    "name AS SCHEMA_NAME,"
    "default_character_set AS DEFAULT_CHARACTER_SET_NAME,"
    "default_collation AS DEFAULT_COLLATION_NAME,"
    "NULL AS SQL_PATH,"
    "CASE WHEN upper(default_encryption) = 'Y' THEN 'YES' ELSE 'NO' END AS DEFAULT_ENCRYPTION "
    "FROM __mylite_schema_catalog ORDER BY name COLLATE BINARY";

static const char information_schema_tables_sql[] =
    "SELECT * FROM ("
    "SELECT 'def' AS TABLE_CATALOG,"
    "'information_schema' AS TABLE_SCHEMA,"
    "table_name AS TABLE_NAME,"
    "'SYSTEM VIEW' AS TABLE_TYPE,"
    "NULL AS ENGINE,"
    "10 AS VERSION,"
    "NULL AS ROW_FORMAT,"
    "0 AS TABLE_ROWS,"
    "0 AS AVG_ROW_LENGTH,"
    "0 AS DATA_LENGTH,"
    "0 AS MAX_DATA_LENGTH,"
    "0 AS INDEX_LENGTH,"
    "0 AS DATA_FREE,"
    "NULL AS AUTO_INCREMENT,"
    "strftime('%Y-%m-%d %H:%M:%S', 'now') AS CREATE_TIME,"
    "NULL AS UPDATE_TIME,"
    "NULL AS CHECK_TIME,"
    "NULL AS TABLE_COLLATION,"
    "NULL AS CHECKSUM,"
    "'' AS CREATE_OPTIONS,"
    "'' AS TABLE_COMMENT "
    "FROM ("
    "SELECT 'CHARACTER_SETS' AS table_name "
    "UNION ALL SELECT 'CHECK_CONSTRAINTS' "
    "UNION ALL SELECT 'COLLATION_CHARACTER_SET_APPLICABILITY' "
    "UNION ALL SELECT 'COLLATIONS' "
    "UNION ALL SELECT 'SCHEMATA' "
    "UNION ALL SELECT 'TABLES' "
    "UNION ALL SELECT 'COLUMNS' "
    "UNION ALL SELECT 'ENGINES' "
    "UNION ALL SELECT 'KEYWORDS' "
    "UNION ALL SELECT 'KEY_COLUMN_USAGE' "
    "UNION ALL SELECT 'REFERENTIAL_CONSTRAINTS' "
    "UNION ALL SELECT 'STATISTICS' "
    "UNION ALL SELECT 'TABLE_CONSTRAINTS') "
    "UNION ALL "
    "SELECT table_catalog AS TABLE_CATALOG,"
    "table_schema AS TABLE_SCHEMA,"
    "table_name AS TABLE_NAME,"
    "table_type AS TABLE_TYPE,"
    "engine AS ENGINE,"
    "version AS VERSION,"
    "row_format AS ROW_FORMAT,"
    "table_rows AS TABLE_ROWS,"
    "avg_row_length AS AVG_ROW_LENGTH,"
    "data_length AS DATA_LENGTH,"
    "max_data_length AS MAX_DATA_LENGTH,"
    "index_length AS INDEX_LENGTH,"
    "data_free AS DATA_FREE,"
    "auto_increment AS AUTO_INCREMENT,"
    "create_time AS CREATE_TIME,"
    "update_time AS UPDATE_TIME,"
    "check_time AS CHECK_TIME,"
    "table_collation AS TABLE_COLLATION,"
    "checksum AS CHECKSUM,"
    "create_options AS CREATE_OPTIONS,"
    "table_comment AS TABLE_COMMENT "
    "FROM __mylite_table_catalog) "
    "ORDER BY TABLE_SCHEMA COLLATE BINARY, TABLE_NAME COLLATE BINARY";
static const char information_schema_columns_sql[] =
    "SELECT table_catalog AS TABLE_CATALOG,"
    "table_schema AS TABLE_SCHEMA,"
    "table_name AS TABLE_NAME,"
    "column_name AS COLUMN_NAME,"
    "ordinal_position AS ORDINAL_POSITION,"
    "column_default AS COLUMN_DEFAULT,"
    "is_nullable AS IS_NULLABLE,"
    "data_type AS DATA_TYPE,"
    "character_maximum_length AS CHARACTER_MAXIMUM_LENGTH,"
    "character_octet_length AS CHARACTER_OCTET_LENGTH,"
    "numeric_precision AS NUMERIC_PRECISION,"
    "numeric_scale AS NUMERIC_SCALE,"
    "datetime_precision AS DATETIME_PRECISION,"
    "character_set_name AS CHARACTER_SET_NAME,"
    "collation_name AS COLLATION_NAME,"
    "column_type AS COLUMN_TYPE,"
    "column_key AS COLUMN_KEY,"
    "extra AS EXTRA,"
    "privileges AS PRIVILEGES,"
    "column_comment AS COLUMN_COMMENT,"
    "generation_expression AS GENERATION_EXPRESSION,"
    "srs_id AS SRS_ID "
    "FROM __mylite_column_catalog "
    "ORDER BY table_schema COLLATE BINARY, table_name COLLATE BINARY, ordinal_position";
static const char information_schema_statistics_sql[] =
    "SELECT table_catalog AS TABLE_CATALOG,"
    "table_schema AS TABLE_SCHEMA,"
    "table_name AS TABLE_NAME,"
    "non_unique AS NON_UNIQUE,"
    "index_schema AS INDEX_SCHEMA,"
    "index_name AS INDEX_NAME,"
    "seq_in_index AS SEQ_IN_INDEX,"
    "column_name AS COLUMN_NAME,"
    "collation AS COLLATION,"
    "cardinality AS CARDINALITY,"
    "sub_part AS SUB_PART,"
    "packed AS PACKED,"
    "nullable AS NULLABLE,"
    "index_type AS INDEX_TYPE,"
    "comment AS COMMENT,"
    "index_comment AS INDEX_COMMENT,"
    "is_visible AS IS_VISIBLE,"
    "expression AS EXPRESSION "
    "FROM __mylite_index_catalog "
    "ORDER BY table_schema COLLATE BINARY, table_name COLLATE BINARY, "
    "index_name COLLATE BINARY, seq_in_index";
static const char information_schema_table_constraints_sql[] =
    "SELECT CONSTRAINT_CATALOG,"
    "CONSTRAINT_SCHEMA,"
    "CONSTRAINT_NAME,"
    "TABLE_SCHEMA,"
    "TABLE_NAME,"
    "CONSTRAINT_TYPE,"
    "ENFORCED "
    "FROM ("
    "SELECT 'def' AS CONSTRAINT_CATALOG,"
    "table_schema AS CONSTRAINT_SCHEMA,"
    "CASE WHEN index_name = 'PRIMARY' THEN 'PRIMARY' ELSE index_name END AS CONSTRAINT_NAME,"
    "table_schema AS TABLE_SCHEMA,"
    "table_name AS TABLE_NAME,"
    "CASE WHEN index_name = 'PRIMARY' THEN 'PRIMARY KEY' ELSE 'UNIQUE' END AS CONSTRAINT_TYPE,"
    "'YES' AS ENFORCED,"
    "CASE WHEN index_name = 'PRIMARY' THEN 0 ELSE 1 END AS constraint_order,"
    "MIN(rowid) AS first_rowid "
    "FROM __mylite_index_catalog "
    "WHERE non_unique = 0 "
    "GROUP BY table_schema, table_name, index_name) "
    "ORDER BY TABLE_SCHEMA COLLATE BINARY, TABLE_NAME COLLATE BINARY, "
    "CONSTRAINT_NAME COLLATE NOCASE, constraint_order, first_rowid";

static const char information_schema_key_column_usage_sql[] =
    "SELECT CONSTRAINT_CATALOG,"
    "CONSTRAINT_SCHEMA,"
    "CONSTRAINT_NAME,"
    "TABLE_CATALOG,"
    "TABLE_SCHEMA,"
    "TABLE_NAME,"
    "COLUMN_NAME,"
    "ORDINAL_POSITION,"
    "POSITION_IN_UNIQUE_CONSTRAINT,"
    "REFERENCED_TABLE_SCHEMA,"
    "REFERENCED_TABLE_NAME,"
    "REFERENCED_COLUMN_NAME "
    "FROM ("
    "SELECT 'def' AS CONSTRAINT_CATALOG,"
    "i.table_schema AS CONSTRAINT_SCHEMA,"
    "CASE WHEN i.index_name = 'PRIMARY' THEN 'PRIMARY' ELSE i.index_name END AS CONSTRAINT_NAME,"
    "'def' AS TABLE_CATALOG,"
    "i.table_schema AS TABLE_SCHEMA,"
    "i.table_name AS TABLE_NAME,"
    "i.column_name AS COLUMN_NAME,"
    "i.seq_in_index AS ORDINAL_POSITION,"
    "CAST(NULL AS INTEGER) AS POSITION_IN_UNIQUE_CONSTRAINT,"
    "CAST(NULL AS TEXT) AS REFERENCED_TABLE_SCHEMA,"
    "CAST(NULL AS TEXT) AS REFERENCED_TABLE_NAME,"
    "CAST(NULL AS TEXT) AS REFERENCED_COLUMN_NAME,"
    "CASE WHEN i.index_name = 'PRIMARY' THEN 0 ELSE 1 END AS constraint_order,"
    "logical_index.first_rowid AS first_rowid "
    "FROM __mylite_index_catalog AS i "
    "JOIN ("
    "SELECT table_schema, table_name, index_name, MIN(rowid) AS first_rowid "
    "FROM __mylite_index_catalog "
    "WHERE non_unique = 0 "
    "GROUP BY table_schema, table_name, index_name"
    ") AS logical_index "
    "ON logical_index.table_schema = i.table_schema "
    "AND logical_index.table_name = i.table_name "
    "AND logical_index.index_name = i.index_name "
    "WHERE i.non_unique = 0 AND i.column_name IS NOT NULL) "
    "ORDER BY TABLE_SCHEMA COLLATE BINARY, TABLE_NAME COLLATE BINARY, "
    "constraint_order, first_rowid, ORDINAL_POSITION";
static const char information_schema_check_constraints_sql[] = "SELECT 'def' AS CONSTRAINT_CATALOG,"
                                                               "'' AS CONSTRAINT_SCHEMA,"
                                                               "'' AS CONSTRAINT_NAME,"
                                                               "'' AS CHECK_CLAUSE "
                                                               "WHERE 0";
static const char information_schema_referential_constraints_sql[] =
    "SELECT 'def' AS CONSTRAINT_CATALOG,"
    "'' AS CONSTRAINT_SCHEMA,"
    "'' AS CONSTRAINT_NAME,"
    "'def' AS UNIQUE_CONSTRAINT_CATALOG,"
    "'' AS UNIQUE_CONSTRAINT_SCHEMA,"
    "'' AS UNIQUE_CONSTRAINT_NAME,"
    "'' AS MATCH_OPTION,"
    "'' AS UPDATE_RULE,"
    "'' AS DELETE_RULE,"
    "'' AS TABLE_NAME,"
    "'' AS REFERENCED_TABLE_NAME "
    "WHERE 0";

int mylite_information_schema_prepare_select_statement(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_stmt **out_stmt
) {
    enum mylite_information_schema_table table = MYLITE_INFORMATION_SCHEMA_NONE;
    char *sqlite_sql = NULL;
    const char *sql = NULL;
    int status = information_schema_tables_filtered_select_sql(database, statement, &sqlite_sql);

    *out_stmt = NULL;
    if (status != MYLITE_UNSUPPORTED) {
        if (status == MYLITE_OK) {
            status = mylite_statement_prepare_sqlite(database, sqlite_sql, out_stmt);
        }
        sqlite3_free(sqlite_sql);
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        return status;
    }

    status = mylite_information_schema_table_from_select(statement, &table);
    if (status != MYLITE_OK) {
        return status;
    }
    if (table == MYLITE_INFORMATION_SCHEMA_NONE) {
        return MYLITE_UNSUPPORTED;
    }

    status = information_schema_dynamic_table_sql(database, table, &sqlite_sql);
    if (status != MYLITE_UNSUPPORTED) {
        if (status == MYLITE_OK) {
            status = mylite_statement_prepare_sqlite(database, sqlite_sql, out_stmt);
        }
        sqlite3_free(sqlite_sql);
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        return status;
    }

    sql = information_schema_table_sql(table);
    if (sql == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    return mylite_statement_prepare_sqlite(database, sql, out_stmt);
}

static int information_schema_tables_filtered_select_sql(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    char **out_sql
) {
    const struct mylite_sql_ast_node *select_list = mylite_ast_child_at(statement, 0U);
    const struct mylite_sql_ast_node *from_clause = mylite_ast_child_at(statement, 1U);
    const struct mylite_sql_ast_node *where_clause =
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_WHERE_CLAUSE);
    const struct mylite_sql_ast_node *order_by_clause =
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    sqlite3_str *sql = NULL;
    int status = MYLITE_OK;

    *out_sql = NULL;
    if (statement == NULL || statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT ||
        !information_schema_tables_from_clause(from_clause)) {
        return MYLITE_UNSUPPORTED;
    }
    if (statement->select_duplicate_mode_explicit ||
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_GROUP_BY_CLAUSE) != NULL ||
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_HAVING_CLAUSE) != NULL ||
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE) != NULL) {
        return MYLITE_UNSUPPORTED;
    }

    sql = sqlite3_str_new(database->sqlite);
    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    sqlite3_str_appendall(sql, "SELECT ");
    status = append_information_schema_tables_projection(sql, select_list);
    if (status == MYLITE_OK) {
        sqlite3_str_appendall(sql, " FROM (");
        sqlite3_str_appendall(sql, information_schema_tables_sql);
        sqlite3_str_appendall(sql, ") AS mylite_information_schema_tables");
    }
    if (status == MYLITE_OK) {
        status = append_information_schema_tables_filter(database, sql, where_clause);
    }
    if (status == MYLITE_OK) {
        status = append_information_schema_tables_order(sql, order_by_clause);
    }

    if (status != MYLITE_OK) {
        sqlite3_free(sqlite3_str_finish(sql));
        return status;
    }

    int errcode = sqlite3_str_errcode(sql);

    *out_sql = sqlite3_str_finish(sql);
    if (errcode != SQLITE_OK) {
        (void)mylite_diagnostics_set_error_message(database, sqlite3_errstr(errcode));
        return errcode == SQLITE_NOMEM ? MYLITE_NOMEM : MYLITE_SQLITE_ERROR;
    }
    if (*out_sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

static bool information_schema_tables_from_clause(const struct mylite_sql_ast_node *from_clause) {
    const struct mylite_sql_ast_node *identifier = mylite_ast_child_at(from_clause, 0U);
    const struct mylite_sql_ast_node *schema = mylite_ast_child_at(identifier, 0U);
    const struct mylite_sql_ast_node *table = mylite_ast_child_at(identifier, 1U);

    if (from_clause == NULL || from_clause->kind != MYLITE_SQL_AST_FROM_TABLE ||
        identifier == NULL || identifier->kind != MYLITE_SQL_AST_QUALIFIED_IDENTIFIER ||
        schema == NULL || schema->kind != MYLITE_SQL_AST_IDENTIFIER || table == NULL ||
        table->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return false;
    }
    if (!mylite_span_equal_ci(schema->span, "information_schema")) {
        return false;
    }
    return mylite_span_equal_ci(table->span, "tables");
}

static int append_information_schema_tables_projection(
    sqlite3_str *sql,
    const struct mylite_sql_ast_node *select_list
) {
    size_t output_index = 0U;

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        return MYLITE_UNSUPPORTED;
    }

    for (const struct mylite_sql_ast_node *item = select_list->first_child; item != NULL;
         item = item->next_sibling) {
        const struct mylite_sql_ast_node *expression = mylite_ast_child_at(item, 0U);
        const struct mylite_sql_ast_node *alias = mylite_ast_child_at(item, 1U);
        const char *column_name = information_schema_tables_column_name(expression);

        if (item->kind != MYLITE_SQL_AST_SELECT_ITEM || expression == NULL || alias != NULL) {
            return MYLITE_UNSUPPORTED;
        }
        if (expression->kind == MYLITE_SQL_AST_WILDCARD &&
            mylite_ast_child_at(expression, 0U) == NULL && item->next_sibling == NULL &&
            output_index == 0U) {
            sqlite3_str_append(sql, "*", 1);
            return MYLITE_OK;
        }
        if (column_name == NULL) {
            return MYLITE_UNSUPPORTED;
        }
        if (output_index != 0U) {
            sqlite3_str_append(sql, ",", 1);
        }
        sqlite3_str_appendf(sql, "\"%w\"", column_name);
        ++output_index;
    }
    return output_index == 0U ? MYLITE_UNSUPPORTED : MYLITE_OK;
}

static int append_information_schema_tables_filter(
    mylite_db *database,
    sqlite3_str *sql,
    const struct mylite_sql_ast_node *where_clause
) {
    const struct mylite_sql_ast_node *predicate = mylite_ast_child_at(where_clause, 0U);

    if (where_clause == NULL) {
        return MYLITE_OK;
    }
    if (where_clause->kind != MYLITE_SQL_AST_WHERE_CLAUSE || predicate == NULL) {
        return MYLITE_UNSUPPORTED;
    }

    sqlite3_str_appendall(sql, " WHERE ");
    return append_information_schema_tables_predicate(database, sql, predicate);
}

static int append_information_schema_tables_predicate(
    mylite_db *database,
    sqlite3_str *sql,
    const struct mylite_sql_ast_node *expression
) {
    struct information_schema_predicate_stack stack = {0};
    bool appended = false;
    int status = MYLITE_OK;

    if (!information_schema_tables_predicate_stack_push(&stack, expression)) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    while (status == MYLITE_OK && stack.count != 0U) {
        expression = stack.items[--stack.count];
        expression = information_schema_tables_unwrap_parenthesized_expression(expression);
        if (expression == NULL) {
            status = MYLITE_UNSUPPORTED;
            break;
        }
        if (expression->kind == MYLITE_SQL_AST_BINARY_EXPRESSION &&
            expression->operator_kind == MYLITE_SQL_AST_OPERATOR_LOGICAL_AND) {
            if (!information_schema_tables_predicate_stack_push(
                    &stack,
                    mylite_ast_child_at(expression, 1U)
                ) ||
                !information_schema_tables_predicate_stack_push(
                    &stack,
                    mylite_ast_child_at(expression, 0U)
                )) {
                (void)mylite_diagnostics_set_error_message(database, "out of memory");
                status = MYLITE_NOMEM;
            }
            continue;
        }
        if (appended) {
            sqlite3_str_appendall(sql, ") AND (");
        } else {
            sqlite3_str_append(sql, "(", 1);
            appended = true;
        }
        status = append_information_schema_tables_predicate_term(database, sql, expression);
    }
    if (status == MYLITE_OK && appended) {
        sqlite3_str_append(sql, ")", 1);
    }
    if (status == MYLITE_OK && !appended) {
        status = MYLITE_UNSUPPORTED;
    }
    information_schema_tables_predicate_stack_deinit(&stack);
    return status;
}

static int append_information_schema_tables_predicate_term(
    mylite_db *database,
    sqlite3_str *sql,
    const struct mylite_sql_ast_node *expression
) {
    expression = information_schema_tables_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    if (expression->kind == MYLITE_SQL_AST_BINARY_EXPRESSION &&
        expression->operator_kind == MYLITE_SQL_AST_OPERATOR_EQUAL) {
        return append_information_schema_tables_equality(
            database,
            sql,
            mylite_ast_child_at(expression, 0U),
            mylite_ast_child_at(expression, 1U)
        );
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION &&
        (expression->operator_kind == MYLITE_SQL_AST_OPERATOR_IS_NULL ||
         expression->operator_kind == MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL)) {
        return append_information_schema_tables_null_predicate(
            sql,
            mylite_ast_child_at(expression, 0U),
            expression->operator_kind
        );
    }
    return MYLITE_UNSUPPORTED;
}

static const struct mylite_sql_ast_node *information_schema_tables_unwrap_parenthesized_expression(
    const struct mylite_sql_ast_node *expression
) {
    while (expression != NULL && expression->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        expression = mylite_ast_child_at(expression, 0U);
    }
    return expression;
}

static bool information_schema_tables_predicate_stack_push(
    struct information_schema_predicate_stack *stack,
    const struct mylite_sql_ast_node *expression
) {
    if (stack->count == stack->capacity) {
        size_t new_capacity = stack->capacity == 0U
                                  ? information_schema_predicate_stack_initial_capacity
                                  : stack->capacity * 2U;
        const struct mylite_sql_ast_node **new_items = (const struct mylite_sql_ast_node **)
            realloc((void *)stack->items, new_capacity * sizeof(*stack->items));

        if (new_items == NULL) {
            return false;
        }
        stack->items = new_items;
        stack->capacity = new_capacity;
    }
    stack->items[stack->count++] = expression;
    return true;
}

static void information_schema_tables_predicate_stack_deinit(
    struct information_schema_predicate_stack *stack
) {
    free((void *)stack->items);
    *stack = (struct information_schema_predicate_stack){0};
}

static int append_information_schema_tables_equality(
    mylite_db *database,
    sqlite3_str *sql,
    const struct mylite_sql_ast_node *left,
    const struct mylite_sql_ast_node *right
) {
    const char *left_column = information_schema_tables_column_name(left);
    const char *right_column = information_schema_tables_column_name(right);

    if (left_column != NULL && right_column == NULL) {
        sqlite3_str_appendf(sql, "upper(\"%w\") = upper(", left_column);
        int status = append_information_schema_tables_value(database, sql, right);

        if (status == MYLITE_OK) {
            sqlite3_str_append(sql, ")", 1);
        }
        return status;
    }
    if (right_column != NULL && left_column == NULL) {
        sqlite3_str_appendall(sql, "upper(");
        int status = append_information_schema_tables_value(database, sql, left);

        if (status == MYLITE_OK) {
            sqlite3_str_appendf(sql, ") = upper(\"%w\")", right_column);
        }
        return status;
    }
    return MYLITE_UNSUPPORTED;
}

static int append_information_schema_tables_null_predicate(
    sqlite3_str *sql,
    const struct mylite_sql_ast_node *operand,
    enum mylite_sql_ast_operator operator_kind
) {
    const char *column_name = information_schema_tables_column_name(operand);

    if (column_name == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    sqlite3_str_appendf(
        sql,
        "\"%w\" IS %sNULL",
        column_name,
        operator_kind == MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL ? "NOT " : ""
    );
    return MYLITE_OK;
}

static int append_information_schema_tables_value(
    mylite_db *database,
    sqlite3_str *sql,
    const struct mylite_sql_ast_node *expression
) {
    char *text = NULL;

    if (expression == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    if (information_schema_tables_database_function(expression)) {
        if (database->selected_schema == NULL) {
            sqlite3_str_appendall(sql, "NULL");
        } else {
            sqlite3_str_appendf(sql, "'%q'", database->selected_schema);
        }
        return MYLITE_OK;
    }
    if (expression->kind != MYLITE_SQL_AST_LITERAL ||
        expression->literal_kind != MYLITE_SQL_AST_LITERAL_STRING) {
        return MYLITE_UNSUPPORTED;
    }

    text = mylite_copy_string_literal_span(expression);
    if (text == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    sqlite3_str_appendf(sql, "'%q'", text);
    free(text);
    return MYLITE_OK;
}

static int append_information_schema_tables_order(
    sqlite3_str *sql,
    const struct mylite_sql_ast_node *order_by_clause
) {
    const struct mylite_sql_ast_node *items = mylite_ast_child_at(order_by_clause, 0U);

    if (order_by_clause == NULL) {
        sqlite3_str_appendall(
            sql,
            " ORDER BY TABLE_SCHEMA COLLATE BINARY, TABLE_NAME COLLATE BINARY"
        );
        return MYLITE_OK;
    }
    if (order_by_clause->kind != MYLITE_SQL_AST_ORDER_BY_CLAUSE || items == NULL ||
        items->kind != MYLITE_SQL_AST_ORDER_ITEM_LIST || items->first_child == NULL ||
        items->first_child->next_sibling != NULL) {
        return MYLITE_UNSUPPORTED;
    }

    const struct mylite_sql_ast_node *item = items->first_child;
    const char *column_name = information_schema_tables_column_name(mylite_ast_child_at(item, 0U));

    if (item->kind != MYLITE_SQL_AST_ORDER_ITEM || column_name == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    sqlite3_str_appendf(sql, " ORDER BY \"%w\" COLLATE BINARY", column_name);
    if (item->key_part_order == MYLITE_SQL_AST_KEY_PART_ORDER_DESC) {
        sqlite3_str_appendall(sql, " DESC");
    }
    return MYLITE_OK;
}

static const char *information_schema_tables_column_name(
    const struct mylite_sql_ast_node *expression
) {
    if (expression != NULL && expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        expression = mylite_ast_child_at(expression, 1U);
    }
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return NULL;
    }
    if (mylite_span_equal_ci(expression->span, "TABLE_CATALOG")) {
        return "TABLE_CATALOG";
    }
    if (mylite_span_equal_ci(expression->span, "TABLE_SCHEMA")) {
        return "TABLE_SCHEMA";
    }
    if (mylite_span_equal_ci(expression->span, "TABLE_NAME")) {
        return "TABLE_NAME";
    }
    if (mylite_span_equal_ci(expression->span, "TABLE_TYPE")) {
        return "TABLE_TYPE";
    }
    if (mylite_span_equal_ci(expression->span, "ENGINE")) {
        return "ENGINE";
    }
    return NULL;
}

static bool information_schema_tables_database_function(
    const struct mylite_sql_ast_node *expression
) {
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_FUNCTION_CALL || name == NULL ||
        name->kind != MYLITE_SQL_AST_IDENTIFIER || arguments == NULL ||
        arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST ||
        arguments->first_child != NULL) {
        return false;
    }
    return mylite_span_equal_ci(name->span, "DATABASE");
}

bool mylite_information_schema_has_table(const char *name) {
    return mylite_information_schema_table_from_name(name) != MYLITE_INFORMATION_SCHEMA_NONE;
}

int mylite_information_schema_prepare_table_view(
    mylite_db *database,
    const char *table_name,
    char **out_physical_name
) {
    char *physical_name = NULL;
    char *select_sql = NULL;
    char *create_sql = NULL;
    int status = MYLITE_OK;
    int rc = SQLITE_OK;

    if (!mylite_information_schema_has_table(table_name)) {
        return mylite_information_schema_set_unknown_table_error(database, table_name);
    }

    physical_name = mylite_catalog_physical_table_name("information_schema", table_name);
    if (physical_name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = information_schema_table_query_sql(database, table_name, &select_sql);
    if (status != MYLITE_OK) {
        free(physical_name);
        return status;
    }

    create_sql =
        sqlite3_mprintf("CREATE TEMP VIEW IF NOT EXISTS \"%w\" AS %s", physical_name, select_sql);
    sqlite3_free(select_sql);
    if (create_sql == NULL) {
        free(physical_name);
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_exec(database->sqlite, create_sql, NULL, NULL, NULL);
    sqlite3_free(create_sql);
    if (rc != SQLITE_OK) {
        free(physical_name);
        return mylite_diagnostics_set_sqlite_error(database);
    }

    *out_physical_name = physical_name;
    return MYLITE_OK;
}

int mylite_information_schema_set_unknown_table_error(mylite_db *database, const char *table_name) {
    char *display_name = mylite_copy_nonempty_cstring(table_name);
    char *message = NULL;
    int status = MYLITE_OK;

    if (display_name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    mylite_uppercase_ascii_text(display_name);

    message = sqlite3_mprintf("Unknown table '%q' in information_schema", display_name);
    free(display_name);
    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_diagnostics_set_error_message(database, message);
    if (status == MYLITE_OK) {
        status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_NO_SUCH_TABLE, message);
    }
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int information_schema_dynamic_table_sql(
    mylite_db *database,
    enum mylite_information_schema_table table,
    char **out_sql
) {
    *out_sql = NULL;
    switch (table) {
    case MYLITE_INFORMATION_SCHEMA_CHARACTER_SETS:
        return mylite_information_schema_character_sets_sql(database, out_sql);
    case MYLITE_INFORMATION_SCHEMA_COLLATIONS:
        return mylite_information_schema_collations_sql(database, out_sql);
    case MYLITE_INFORMATION_SCHEMA_COLLATION_CHARACTER_SET_APPLICABILITY:
        return mylite_information_schema_collation_character_set_applicability_sql(
            database,
            out_sql
        );
    case MYLITE_INFORMATION_SCHEMA_ENGINES:
        return mylite_storage_engine_information_schema_sql(database, out_sql);
    case MYLITE_INFORMATION_SCHEMA_KEYWORDS:
        return mylite_information_schema_keywords_sql(database, out_sql);
    case MYLITE_INFORMATION_SCHEMA_SCHEMATA:
    case MYLITE_INFORMATION_SCHEMA_TABLES:
    case MYLITE_INFORMATION_SCHEMA_COLUMNS:
    case MYLITE_INFORMATION_SCHEMA_STATISTICS:
    case MYLITE_INFORMATION_SCHEMA_TABLE_CONSTRAINTS:
    case MYLITE_INFORMATION_SCHEMA_KEY_COLUMN_USAGE:
    case MYLITE_INFORMATION_SCHEMA_CHECK_CONSTRAINTS:
    case MYLITE_INFORMATION_SCHEMA_REFERENTIAL_CONSTRAINTS:
    case MYLITE_INFORMATION_SCHEMA_NONE:
        return MYLITE_UNSUPPORTED;
    }
    return MYLITE_UNSUPPORTED;
}

static int information_schema_table_query_sql(
    mylite_db *database,
    const char *table_name,
    char **out_sql
) {
    enum mylite_information_schema_table table =
        mylite_information_schema_table_from_name(table_name);
    const char *static_sql = NULL;
    int status = information_schema_dynamic_table_sql(database, table, out_sql);

    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }

    static_sql = information_schema_table_sql(table);
    if (static_sql == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    *out_sql = sqlite3_mprintf("%s", static_sql);
    if (*out_sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

static const char *information_schema_table_sql(enum mylite_information_schema_table table) {
    switch (table) {
    case MYLITE_INFORMATION_SCHEMA_SCHEMATA:
        return information_schema_schemata_sql;
    case MYLITE_INFORMATION_SCHEMA_TABLES:
        return information_schema_tables_sql;
    case MYLITE_INFORMATION_SCHEMA_COLUMNS:
        return information_schema_columns_sql;
    case MYLITE_INFORMATION_SCHEMA_STATISTICS:
        return information_schema_statistics_sql;
    case MYLITE_INFORMATION_SCHEMA_TABLE_CONSTRAINTS:
        return information_schema_table_constraints_sql;
    case MYLITE_INFORMATION_SCHEMA_KEY_COLUMN_USAGE:
        return information_schema_key_column_usage_sql;
    case MYLITE_INFORMATION_SCHEMA_CHECK_CONSTRAINTS:
        return information_schema_check_constraints_sql;
    case MYLITE_INFORMATION_SCHEMA_REFERENTIAL_CONSTRAINTS:
        return information_schema_referential_constraints_sql;
    case MYLITE_INFORMATION_SCHEMA_ENGINES:
    case MYLITE_INFORMATION_SCHEMA_CHARACTER_SETS:
    case MYLITE_INFORMATION_SCHEMA_COLLATIONS:
    case MYLITE_INFORMATION_SCHEMA_COLLATION_CHARACTER_SET_APPLICABILITY:
    case MYLITE_INFORMATION_SCHEMA_KEYWORDS:
    case MYLITE_INFORMATION_SCHEMA_NONE:
        return NULL;
    }

    return NULL;
}
