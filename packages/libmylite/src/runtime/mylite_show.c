#include "mylite_show.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_field_descriptor.h"
#include "mylite_metadata.h"
#include "mylite_runtime.h"
#include "mylite_show_types.h"
#include "mylite_span.h"
#include "mylite_statement.h"
#include "mylite_storage_engine.h"
#include "sqlite3.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

enum show_where_task_kind {
    SHOW_WHERE_TASK_EXPRESSION,
    SHOW_WHERE_TASK_TEXT,
};

enum { show_where_initial_task_capacity = 16U };

struct show_where_task {
    enum show_where_task_kind kind;
    const struct mylite_sql_ast_node *expression;
    const char *text;
};

struct show_where_task_stack {
    mylite_db *database;
    struct show_where_task *items;
    size_t count;
    size_t capacity;
};

static struct mylite_field_descriptor show_engines_field_descriptor(uint64_t length, bool nullable);

static int show_engines_sql(mylite_db *database, char **out_sql);

static int push_show_where_expression(
    struct show_where_task_stack *stack,
    const struct mylite_sql_ast_node *expression
);

static int push_show_where_text(struct show_where_task_stack *stack, const char *text);

static int push_show_where_task(struct show_where_task_stack *stack, struct show_where_task task);

static int append_show_where_expression_iterative(
    mylite_db *database,
    sqlite3_str *sql,
    const struct mylite_sql_ast_node *expression,
    const char *const *column_names,
    size_t column_count
);

static int queue_show_where_expression(
    mylite_db *database,
    sqlite3_str *sql,
    struct show_where_task_stack *stack,
    const struct mylite_sql_ast_node *expression,
    const char *const *column_names,
    size_t column_count
);

static int queue_show_where_unary_expression(
    struct show_where_task_stack *stack,
    const struct mylite_sql_ast_node *expression
);

static int queue_show_where_binary_expression(
    struct show_where_task_stack *stack,
    const struct mylite_sql_ast_node *expression
);

static int queue_show_where_in_expression(
    struct show_where_task_stack *stack,
    const struct mylite_sql_ast_node *expression
);

static int queue_show_where_operator_expression(
    struct show_where_task_stack *stack,
    const struct mylite_sql_ast_node *expression
);

static int queue_show_where_like_escape(
    struct show_where_task_stack *stack,
    const struct mylite_sql_ast_node *expression
);

static int queue_show_where_expression_list(
    struct show_where_task_stack *stack,
    const struct mylite_sql_ast_node *list
);

static int append_show_where_identifier(
    mylite_db *database,
    sqlite3_str *sql,
    const struct mylite_sql_ast_node *expression,
    const char *const *column_names,
    size_t column_count
);

static int append_show_where_literal(
    mylite_db *database,
    sqlite3_str *sql,
    const struct mylite_sql_ast_node *expression
);

static int append_show_where_raw_span(
    sqlite3_str *sql,
    const struct mylite_sql_ast_node *expression
);

static int show_where_column_name(
    mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *const *column_names,
    size_t column_count,
    const char **out_column_name
);

static int set_show_unknown_column_error(mylite_db *database, const char *column_name);

static const char *show_where_binary_operator_sql(enum mylite_sql_ast_operator operator_kind);
static const unsigned int mylite_show_latin1_swedish_ci_charset_id = 8U;
static const unsigned int mylite_show_not_fixed_decimals = 31U;

static const char show_schemas_sql[] =
    "SELECT name AS \"Database\" FROM __mylite_schema_catalog ORDER BY name COLLATE BINARY";

int mylite_show_prepare_engines_statement(mylite_db *database, mylite_stmt **out_stmt) {
    char *sqlite_sql = NULL;
    mylite_stmt *stmt = NULL;
    int status = show_engines_sql(database, &sqlite_sql);

    *out_stmt = NULL;
    if (status == MYLITE_OK) {
        status = mylite_statement_prepare_sqlite(database, sqlite_sql, &stmt);
    }
    if (status == MYLITE_OK) {
        status = mylite_show_attach_engines_result_metadata(database, stmt);
    }

    if (status == MYLITE_OK) {
        *out_stmt = stmt;
    } else {
        mylite_finalize(stmt);
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
    }
    sqlite3_free(sqlite_sql);
    return status;
}

int mylite_show_prepare_schemas_statement(mylite_db *database, mylite_stmt **out_stmt) {
    return mylite_statement_prepare_sqlite(database, mylite_show_schemas_sql(), out_stmt);
}

static int show_engines_sql(mylite_db *database, char **out_sql) {
    return mylite_storage_engine_show_sql(database, out_sql);
}

int mylite_show_columns_sql(
    mylite_db *database,
    const struct mylite_show_columns_query *query,
    char **out_sql
) {
    static const char *const standard_columns[] = {
        "Field",
        "Type",
        "Null",
        "Key",
        "Default",
        "Extra",
    };
    static const char *const full_columns[] = {
        "Field",
        "Type",
        "Collation",
        "Null",
        "Key",
        "Default",
        "Extra",
        "Privileges",
        "Comment",
    };
    const char *const *where_columns = standard_columns;
    size_t where_column_count = sizeof(standard_columns) / sizeof(standard_columns[0]);
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);
    int status = MYLITE_OK;

    *out_sql = NULL;
    if (sql == NULL) {
        return MYLITE_NOMEM;
    }
    if ((int)query->full != 0) {
        where_columns = full_columns;
        where_column_count = sizeof(full_columns) / sizeof(full_columns[0]);
    }

    sqlite3_str_appendf(sql, "SELECT column_name AS \"Field\", column_type AS \"Type\"");
    if ((int)query->full != 0) {
        sqlite3_str_appendf(sql, ", collation_name AS \"Collation\"");
    }
    sqlite3_str_appendf(
        sql,
        ", is_nullable AS \"Null\", column_key AS \"Key\", "
        "column_default AS \"Default\", extra AS \"Extra\""
    );
    if ((int)query->full != 0) {
        sqlite3_str_appendf(sql, ", privileges AS \"Privileges\", column_comment AS \"Comment\"");
    }
    sqlite3_str_appendf(
        sql,
        " FROM %s WHERE table_schema = %Q AND table_name = %Q",
        mylite_catalog_column_catalog_name(query->temporary),
        query->schema_name,
        query->table_name
    );
    if (query->like_pattern != NULL) {
        sqlite3_str_appendf(sql, " AND column_name LIKE %Q ESCAPE '\\'", query->like_pattern);
    }
    if (query->where_expression != NULL) {
        sqlite3_str_appendall(sql, " AND ");
        status = mylite_show_append_where_expression(
            database,
            sql,
            query->where_expression,
            where_columns,
            where_column_count
        );
    }
    sqlite3_str_appendf(sql, " ORDER BY ordinal_position");

    *out_sql = sqlite3_str_finish(sql);

    if (status != MYLITE_OK) {
        sqlite3_free(*out_sql);
        *out_sql = NULL;
        if (status == MYLITE_UNSUPPORTED) {
            (void)mylite_diagnostics_set_error_message(
                database,
                "SHOW COLUMNS WHERE expression is not supported"
            );
        }
        return status;
    }
    return *out_sql == NULL ? MYLITE_NOMEM : MYLITE_OK;
}

int mylite_show_index_sql(
    mylite_db *database,
    const struct mylite_show_index_query *query,
    char **out_sql
) {
    static const char *const columns[] = {
        "Table",
        "Non_unique",
        "Key_name",
        "Seq_in_index",
        "Column_name",
        "Collation",
        "Cardinality",
        "Sub_part",
        "Packed",
        "Null",
        "Index_type",
        "Comment",
        "Index_comment",
        "Visible",
        "Expression",
    };
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);
    char *finished_sql = NULL;
    int status = MYLITE_OK;

    *out_sql = NULL;
    if (sql == NULL) {
        return MYLITE_NOMEM;
    }

    sqlite3_str_appendf(
        sql,
        "SELECT \"Table\", \"Non_unique\", \"Key_name\", \"Seq_in_index\", "
        "\"Column_name\", \"Collation\", \"Cardinality\", \"Sub_part\", "
        "\"Packed\", \"Null\", \"Index_type\", \"Comment\", \"Index_comment\", "
        "\"Visible\", \"Expression\" FROM ("
        "SELECT table_name AS \"Table\", non_unique AS \"Non_unique\", "
        "index_name AS \"Key_name\", seq_in_index AS \"Seq_in_index\", "
        "column_name AS \"Column_name\", collation AS \"Collation\", "
        "cardinality AS \"Cardinality\", sub_part AS \"Sub_part\", "
        "packed AS \"Packed\", nullable AS \"Null\", index_type AS \"Index_type\", "
        "comment AS \"Comment\", index_comment AS \"Index_comment\", "
        "is_visible AS \"Visible\", expression AS \"Expression\", "
        "rowid AS \"__mylite_order\" "
        "FROM %s "
        "WHERE table_schema = %Q AND table_name = %Q) AS show_index",
        mylite_catalog_index_catalog_name(query->temporary),
        query->schema_name,
        query->table_name
    );
    if (status == MYLITE_OK && query->where_expression != NULL) {
        sqlite3_str_appendall(sql, " WHERE ");
        status = mylite_show_append_where_expression(
            database,
            sql,
            query->where_expression,
            columns,
            sizeof(columns) / sizeof(columns[0])
        );
    }
    if (status == MYLITE_OK) {
        sqlite3_str_appendall(sql, " ORDER BY \"__mylite_order\"");
    }

    finished_sql = sqlite3_str_finish(sql);
    if (status != MYLITE_OK) {
        sqlite3_free(finished_sql);
        if (status == MYLITE_UNSUPPORTED) {
            (void)mylite_diagnostics_set_error_message(
                database,
                "SHOW INDEX WHERE expression is not supported"
            );
        }
        return status;
    }
    if (finished_sql == NULL) {
        return MYLITE_NOMEM;
    }

    *out_sql = finished_sql;
    return MYLITE_OK;
}

int mylite_show_append_where_expression(
    mylite_db *database,
    sqlite3_str *sql,
    const struct mylite_sql_ast_node *expression,
    const char *const *column_names,
    size_t column_count
) {
    if (database == NULL || sql == NULL || column_names == NULL || column_count == 0U) {
        return MYLITE_UNSUPPORTED;
    }
    sqlite3_str_appendall(sql, "(");
    int status = append_show_where_expression_iterative(
        database,
        sql,
        expression,
        column_names,
        column_count
    );

    if (status == MYLITE_OK) {
        sqlite3_str_appendall(sql, ")");
    }
    return status;
}

static int push_show_where_expression(
    struct show_where_task_stack *stack,
    const struct mylite_sql_ast_node *expression
) {
    return push_show_where_task(
        stack,
        (struct show_where_task){
            .kind = SHOW_WHERE_TASK_EXPRESSION,
            .expression = expression,
        }
    );
}

static int push_show_where_text(struct show_where_task_stack *stack, const char *text) {
    return push_show_where_task(
        stack,
        (struct show_where_task){
            .kind = SHOW_WHERE_TASK_TEXT,
            .text = text,
        }
    );
}

static int push_show_where_task(struct show_where_task_stack *stack, struct show_where_task task) {
    struct show_where_task *items = NULL;
    size_t capacity = 0U;

    if (stack->count == stack->capacity) {
        if (stack->capacity == 0U) {
            capacity = show_where_initial_task_capacity;
        } else if (stack->capacity > SIZE_MAX / 2U) {
            (void)mylite_diagnostics_set_error_message(stack->database, "out of memory");
            return MYLITE_NOMEM;
        } else {
            capacity = stack->capacity * 2U;
        }
        if (capacity > SIZE_MAX / sizeof(*items)) {
            (void)mylite_diagnostics_set_error_message(stack->database, "out of memory");
            return MYLITE_NOMEM;
        }
        items = realloc(stack->items, capacity * sizeof(*items));
        if (items == NULL) {
            (void)mylite_diagnostics_set_error_message(stack->database, "out of memory");
            return MYLITE_NOMEM;
        }
        stack->items = items;
        stack->capacity = capacity;
    }
    stack->items[stack->count] = task;
    ++stack->count;
    return MYLITE_OK;
}

static int append_show_where_expression_iterative(
    mylite_db *database,
    sqlite3_str *sql,
    const struct mylite_sql_ast_node *expression,
    const char *const *column_names,
    size_t column_count
) {
    struct show_where_task_stack stack = {.database = database};
    int status = push_show_where_expression(&stack, expression);

    while (status == MYLITE_OK && stack.count != 0U) {
        struct show_where_task task = stack.items[stack.count - 1U];

        --stack.count;
        if (task.kind == SHOW_WHERE_TASK_TEXT) {
            sqlite3_str_appendall(sql, task.text);
        } else {
            status = queue_show_where_expression(
                database,
                sql,
                &stack,
                task.expression,
                column_names,
                column_count
            );
        }
    }

    free(stack.items);
    return status;
}

static int queue_show_where_expression(
    mylite_db *database,
    sqlite3_str *sql,
    struct show_where_task_stack *stack,
    const struct mylite_sql_ast_node *expression,
    const char *const *column_names,
    size_t column_count
) {
    const char *column_name = NULL;

    expression = mylite_sql_ast_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return MYLITE_UNSUPPORTED;
    }

    int column_status =
        show_where_column_name(database, expression, column_names, column_count, &column_name);

    if (column_status != MYLITE_UNSUPPORTED) {
        if (column_status != MYLITE_OK) {
            return column_status;
        }
        return append_show_where_identifier(database, sql, expression, column_names, column_count);
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_LITERAL:
        return append_show_where_literal(database, sql, expression);
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
        return queue_show_where_unary_expression(stack, expression);
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
        return queue_show_where_binary_expression(stack, expression);
    case MYLITE_SQL_AST_EXPRESSION_LIST:
        return queue_show_where_expression_list(stack, expression);
    default:
        return MYLITE_UNSUPPORTED;
    }
}

static int queue_show_where_unary_expression(
    struct show_where_task_stack *stack,
    const struct mylite_sql_ast_node *expression
) {
    switch (expression->operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
        return push_show_where_expression(stack, mylite_ast_child_at(expression, 0U)) == MYLITE_OK
                   ? push_show_where_text(stack, "+")
                   : MYLITE_NOMEM;
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
        return push_show_where_expression(stack, mylite_ast_child_at(expression, 0U)) == MYLITE_OK
                   ? push_show_where_text(stack, "-")
                   : MYLITE_NOMEM;
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT:
        return push_show_where_expression(stack, mylite_ast_child_at(expression, 0U)) == MYLITE_OK
                   ? push_show_where_text(stack, "NOT ")
                   : MYLITE_NOMEM;
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL: {
        const char *suffix = expression->operator_kind == MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL
                                 ? " IS NOT NULL)"
                                 : " IS NULL)";
        int status = push_show_where_text(stack, suffix);

        if (status == MYLITE_OK) {
            status = push_show_where_expression(stack, mylite_ast_child_at(expression, 0U));
        }
        if (status == MYLITE_OK) {
            status = push_show_where_text(stack, "(");
        }
        return status;
    }
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LESS:
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_GREATER:
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_ADD:
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
    case MYLITE_SQL_AST_OPERATOR_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_NOT:
    case MYLITE_SQL_AST_OPERATOR_BINARY_CAST:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_LEFT:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_RIGHT:
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_NOT_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_IN:
    case MYLITE_SQL_AST_OPERATOR_NOT_IN:
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_MODULO:
    case MYLITE_SQL_AST_OPERATOR_LIKE:
    case MYLITE_SQL_AST_OPERATOR_NOT_LIKE:
    case MYLITE_SQL_AST_OPERATOR_REGEXP:
    case MYLITE_SQL_AST_OPERATOR_NOT_REGEXP:
    case MYLITE_SQL_AST_OPERATOR_JSON_EXTRACT:
    case MYLITE_SQL_AST_OPERATOR_JSON_UNQUOTE_EXTRACT:
    case MYLITE_SQL_AST_OPERATOR_NONE:
        return MYLITE_UNSUPPORTED;
    }
    return MYLITE_UNSUPPORTED;
}

static int queue_show_where_binary_expression(
    struct show_where_task_stack *stack,
    const struct mylite_sql_ast_node *expression
) {
    switch (expression->operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_IN:
    case MYLITE_SQL_AST_OPERATOR_NOT_IN:
        return queue_show_where_in_expression(stack, expression);
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LESS:
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_GREATER:
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_LIKE:
    case MYLITE_SQL_AST_OPERATOR_NOT_LIKE:
        return queue_show_where_operator_expression(stack, expression);
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
    case MYLITE_SQL_AST_OPERATOR_ADD:
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
    case MYLITE_SQL_AST_OPERATOR_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_NOT:
    case MYLITE_SQL_AST_OPERATOR_BINARY_CAST:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_LEFT:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_RIGHT:
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_NOT_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_MODULO:
    case MYLITE_SQL_AST_OPERATOR_REGEXP:
    case MYLITE_SQL_AST_OPERATOR_NOT_REGEXP:
    case MYLITE_SQL_AST_OPERATOR_JSON_EXTRACT:
    case MYLITE_SQL_AST_OPERATOR_JSON_UNQUOTE_EXTRACT:
    case MYLITE_SQL_AST_OPERATOR_NONE:
        return MYLITE_UNSUPPORTED;
    }
    return MYLITE_UNSUPPORTED;
}

static int queue_show_where_in_expression(
    struct show_where_task_stack *stack,
    const struct mylite_sql_ast_node *expression
) {
    const char *operator_sql = " IN (";
    int status = push_show_where_text(stack, "))");

    if (expression->operator_kind == MYLITE_SQL_AST_OPERATOR_NOT_IN) {
        operator_sql = " NOT IN (";
    }
    if (status != MYLITE_OK) {
        return status;
    }
    status = queue_show_where_expression_list(stack, mylite_ast_child_at(expression, 1U));
    if (status == MYLITE_OK) {
        status = push_show_where_text(stack, operator_sql);
    }
    if (status == MYLITE_OK) {
        status = push_show_where_expression(stack, mylite_ast_child_at(expression, 0U));
    }
    if (status == MYLITE_OK) {
        status = push_show_where_text(stack, "(");
    }
    return status;
}

static int queue_show_where_operator_expression(
    struct show_where_task_stack *stack,
    const struct mylite_sql_ast_node *expression
) {
    const char *operator_sql = show_where_binary_operator_sql(expression->operator_kind);
    int status = MYLITE_OK;

    if (operator_sql == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    status = push_show_where_text(stack, ")");
    if (status == MYLITE_OK) {
        status = queue_show_where_like_escape(stack, expression);
    }
    if (status == MYLITE_OK) {
        status = push_show_where_expression(stack, mylite_ast_child_at(expression, 1U));
    }
    if (status == MYLITE_OK) {
        status = push_show_where_text(stack, " ");
    }
    if (status == MYLITE_OK) {
        status = push_show_where_text(stack, operator_sql);
    }
    if (status == MYLITE_OK) {
        status = push_show_where_text(stack, " ");
    }
    if (status == MYLITE_OK) {
        status = push_show_where_expression(stack, mylite_ast_child_at(expression, 0U));
    }
    if (status == MYLITE_OK) {
        status = push_show_where_text(stack, "(");
    }
    return status;
}

static int queue_show_where_like_escape(
    struct show_where_task_stack *stack,
    const struct mylite_sql_ast_node *expression
) {
    if (expression->operator_kind != MYLITE_SQL_AST_OPERATOR_LIKE &&
        expression->operator_kind != MYLITE_SQL_AST_OPERATOR_NOT_LIKE) {
        return MYLITE_OK;
    }
    if (mylite_sql_ast_node_child_count(expression) != 3U) {
        return MYLITE_OK;
    }
    int status = push_show_where_expression(stack, mylite_ast_child_at(expression, 2U));

    if (status == MYLITE_OK) {
        status = push_show_where_text(stack, " ESCAPE ");
    }
    return status;
}

static int queue_show_where_expression_list(
    struct show_where_task_stack *stack,
    const struct mylite_sql_ast_node *list
) {
    size_t item_count = 0U;

    if (list == NULL || list->kind != MYLITE_SQL_AST_EXPRESSION_LIST) {
        return MYLITE_UNSUPPORTED;
    }
    item_count = mylite_sql_ast_node_child_count(list);
    if (item_count == 0U) {
        return MYLITE_UNSUPPORTED;
    }
    for (size_t index = item_count; index > 0U; --index) {
        int status = push_show_where_expression(stack, mylite_ast_child_at(list, index - 1U));

        if (status != MYLITE_OK) {
            return status;
        }
        if (index > 1U) {
            status = push_show_where_text(stack, ", ");
            if (status != MYLITE_OK) {
                return status;
            }
        }
    }
    return MYLITE_OK;
}

static int append_show_where_identifier(
    mylite_db *database,
    sqlite3_str *sql,
    const struct mylite_sql_ast_node *expression,
    const char *const *column_names,
    size_t column_count
) {
    const char *column_name = NULL;
    int status =
        show_where_column_name(database, expression, column_names, column_count, &column_name);

    if (status != MYLITE_OK) {
        return status;
    }
    sqlite3_str_appendf(sql, "\"%w\"", column_name);
    return MYLITE_OK;
}

static int append_show_where_literal(
    mylite_db *database,
    sqlite3_str *sql,
    const struct mylite_sql_ast_node *expression
) {
    char *text = NULL;

    switch (expression->literal_kind) {
    case MYLITE_SQL_AST_LITERAL_NULL:
        sqlite3_str_appendall(sql, "NULL");
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_TRUE:
        sqlite3_str_appendall(sql, "TRUE");
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_FALSE:
        sqlite3_str_appendall(sql, "FALSE");
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_INTEGER:
    case MYLITE_SQL_AST_LITERAL_DECIMAL:
    case MYLITE_SQL_AST_LITERAL_FLOAT:
        return append_show_where_raw_span(sql, expression);
    case MYLITE_SQL_AST_LITERAL_STRING:
    case MYLITE_SQL_AST_LITERAL_NATIONAL_STRING:
        text = mylite_copy_string_literal_span(expression);
        if (text == NULL) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
        sqlite3_str_appendf(sql, "'%q'", text);
        free(text);
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_HEX:
    case MYLITE_SQL_AST_LITERAL_BIT:
    case MYLITE_SQL_AST_LITERAL_NONE:
        return MYLITE_UNSUPPORTED;
    }
    return MYLITE_UNSUPPORTED;
}

static int append_show_where_raw_span(
    sqlite3_str *sql,
    const struct mylite_sql_ast_node *expression
) {
    if (expression == NULL || expression->span.length > (size_t)INT_MAX) {
        return MYLITE_NOMEM;
    }
    sqlite3_str_append(sql, expression->span.text, (int)expression->span.length);
    return MYLITE_OK;
}

static int show_where_column_name(
    mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *const *column_names,
    size_t column_count,
    const char **out_column_name
) {
    char *name = NULL;

    *out_column_name = NULL;
    if (expression != NULL && expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        expression = mylite_ast_child_at(expression, 1U);
    }
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return MYLITE_UNSUPPORTED;
    }

    name = mylite_copy_identifier_span(expression);
    if (name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    for (size_t index = 0U; index < column_count; ++index) {
        if (column_names[index] != NULL && mylite_ascii_case_equal(name, column_names[index])) {
            *out_column_name = column_names[index];
            free(name);
            return MYLITE_OK;
        }
    }

    int status = set_show_unknown_column_error(database, name);

    free(name);
    return status;
}

static int set_show_unknown_column_error(mylite_db *database, const char *column_name) {
    int status = mylite_diagnostics_set_error_message_parts(
        database,
        "Unknown column '",
        column_name,
        "' in 'where clause'"
    );

    if (status == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_error(
        database,
        MYLITE_MYSQL_ER_BAD_FIELD_ERROR,
        mylite_error_message(database)
    );
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static const char *show_where_binary_operator_sql(enum mylite_sql_ast_operator operator_kind) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
        return "=";
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
        return "<>";
    case MYLITE_SQL_AST_OPERATOR_LESS:
        return "<";
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
        return "<=";
    case MYLITE_SQL_AST_OPERATOR_GREATER:
        return ">";
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        return ">=";
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
        return "AND";
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
        return "OR";
    case MYLITE_SQL_AST_OPERATOR_LIKE:
        return "LIKE";
    case MYLITE_SQL_AST_OPERATOR_NOT_LIKE:
        return "NOT LIKE";
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR:
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
    case MYLITE_SQL_AST_OPERATOR_ADD:
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
    case MYLITE_SQL_AST_OPERATOR_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_NOT:
    case MYLITE_SQL_AST_OPERATOR_BINARY_CAST:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_LEFT:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_RIGHT:
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_NOT_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_IN:
    case MYLITE_SQL_AST_OPERATOR_NOT_IN:
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_MODULO:
    case MYLITE_SQL_AST_OPERATOR_REGEXP:
    case MYLITE_SQL_AST_OPERATOR_NOT_REGEXP:
    case MYLITE_SQL_AST_OPERATOR_JSON_EXTRACT:
    case MYLITE_SQL_AST_OPERATOR_JSON_UNQUOTE_EXTRACT:
    case MYLITE_SQL_AST_OPERATOR_NONE:
        return NULL;
    }
    return NULL;
}

int mylite_show_tables_sql(
    mylite_db *database,
    const struct mylite_show_tables_query *query,
    char **out_sql
) {
    const char *const columns[] = {query->column_name, "Table_type"};
    size_t column_count = 1U;
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);
    int status = MYLITE_OK;

    *out_sql = NULL;
    if (sql == NULL) {
        return MYLITE_NOMEM;
    }
    if ((int)query->full != 0) {
        column_count = 2U;
    }

    sqlite3_str_appendf(sql, "SELECT TABLE_NAME AS \"%w\"", query->column_name);
    if ((int)query->full != 0) {
        sqlite3_str_appendf(sql, ", TABLE_TYPE AS \"Table_type\"");
    }
    sqlite3_str_appendf(
        sql,
        " FROM ("
        "SELECT 'information_schema' AS TABLE_SCHEMA, table_name AS TABLE_NAME, "
        "'SYSTEM VIEW' AS TABLE_TYPE FROM ("
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
        "SELECT table_schema AS TABLE_SCHEMA, table_name AS TABLE_NAME, table_type AS TABLE_TYPE "
        "FROM __mylite_table_catalog) "
        "WHERE TABLE_SCHEMA = %Q",
        query->schema_name
    );
    if (query->glob_pattern != NULL) {
        sqlite3_str_appendf(sql, " AND TABLE_NAME GLOB %Q", query->glob_pattern);
    }
    if (query->where_expression != NULL) {
        sqlite3_str_appendall(sql, " AND ");
        status = mylite_show_append_where_expression(
            database,
            sql,
            query->where_expression,
            columns,
            column_count
        );
    }
    sqlite3_str_appendf(sql, " ORDER BY TABLE_NAME COLLATE BINARY");
    *out_sql = sqlite3_str_finish(sql);
    if (status != MYLITE_OK) {
        sqlite3_free(*out_sql);
        *out_sql = NULL;
        if (status == MYLITE_UNSUPPORTED) {
            (void)mylite_diagnostics_set_error_message(
                database,
                "SHOW TABLES WHERE expression is not supported"
            );
        }
        return status;
    }
    return *out_sql == NULL ? MYLITE_NOMEM : MYLITE_OK;
}

int mylite_show_table_status_sql(
    mylite_db *database,
    const struct mylite_show_table_status_query *query,
    char **out_sql
) {
    static const char *const columns[] = {
        "Name",
        "Engine",
        "Version",
        "Row_format",
        "Rows",
        "Avg_row_length",
        "Data_length",
        "Max_data_length",
        "Index_length",
        "Data_free",
        "Auto_increment",
        "Create_time",
        "Update_time",
        "Check_time",
        "Collation",
        "Checksum",
        "Create_options",
        "Comment",
    };
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);
    int status = MYLITE_OK;

    *out_sql = NULL;
    if (sql == NULL) {
        return MYLITE_NOMEM;
    }

    sqlite3_str_appendall(
        sql,
        "SELECT Name, Engine, Version, Row_format, Rows, Avg_row_length, Data_length, "
        "Max_data_length, Index_length, Data_free, \"Auto_increment\" AS \"Auto_increment\", "
        "Create_time, Update_time, Check_time, Collation, Checksum, Create_options, "
        "Comment FROM ("
        "SELECT 'information_schema' AS TABLE_SCHEMA, table_name AS Name, NULL AS Engine, "
        "10 AS Version, NULL AS Row_format, 0 AS Rows, 0 AS Avg_row_length, 0 AS Data_length, "
        "0 AS Max_data_length, 0 AS Index_length, 0 AS Data_free, NULL AS \"Auto_increment\", "
        "'1970-01-01 00:00:00' AS Create_time, NULL AS Update_time, NULL AS Check_time, "
        "NULL AS Collation, NULL AS Checksum, '' AS Create_options, '' AS Comment FROM ("
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
        "SELECT table_schema AS TABLE_SCHEMA, table_name AS Name, engine AS Engine, "
        "version AS Version, COALESCE(row_format, 'Dynamic') AS Row_format, "
        "COALESCE(table_rows, 0) AS Rows, COALESCE(avg_row_length, 0) AS Avg_row_length, "
        "COALESCE(data_length, 0) AS Data_length, COALESCE(max_data_length, 0) AS "
        "Max_data_length, COALESCE(index_length, 0) AS Index_length, "
        "COALESCE(data_free, 0) AS Data_free, \"auto_increment\" AS \"Auto_increment\", "
        "create_time AS Create_time, update_time AS Update_time, check_time AS Check_time, "
        "table_collation AS Collation, checksum AS Checksum, create_options AS Create_options, "
        "table_comment AS Comment FROM __mylite_table_catalog) "
    );
    sqlite3_str_appendf(sql, "WHERE TABLE_SCHEMA = %Q", query->schema_name);
    if (query->glob_pattern != NULL) {
        sqlite3_str_appendf(sql, " AND Name GLOB %Q", query->glob_pattern);
    }
    if (query->where_expression != NULL) {
        sqlite3_str_appendall(sql, " AND ");
        status = mylite_show_append_where_expression(
            database,
            sql,
            query->where_expression,
            columns,
            sizeof(columns) / sizeof(columns[0])
        );
    }
    sqlite3_str_appendall(sql, " ORDER BY Name COLLATE BINARY");
    *out_sql = sqlite3_str_finish(sql);
    if (status != MYLITE_OK) {
        sqlite3_free(*out_sql);
        *out_sql = NULL;
        if (status == MYLITE_UNSUPPORTED) {
            (void)mylite_diagnostics_set_error_message(
                database,
                "SHOW TABLE STATUS WHERE expression is not supported"
            );
        }
        return status;
    }
    return *out_sql == NULL ? MYLITE_NOMEM : MYLITE_OK;
}

const char *mylite_show_schemas_sql(void) {
    return show_schemas_sql;
}

int mylite_show_attach_engines_result_metadata(mylite_db *database, mylite_stmt *stmt) {
    static const struct mylite_show_engines_metadata_column columns[] = {
        {"Engine", 64U, false},
        {"Support", 8U, false},
        {"Comment", 80U, false},
        {"Transactions", 3U, true},
        {"XA", 3U, true},
        {"Savepoints", 3U, true},
    };
    struct mylite_result_metadata metadata = {0};

    metadata.columns = calloc(sizeof(columns) / sizeof(columns[0]), sizeof(*metadata.columns));
    if (metadata.columns == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    metadata.column_count = sizeof(columns) / sizeof(columns[0]);

    for (size_t index = 0U; index < metadata.column_count; ++index) {
        int status = mylite_result_metadata_copy_text(
            database,
            &metadata.columns[index].name,
            columns[index].name
        );

        if (status != MYLITE_OK) {
            mylite_result_metadata_deinit(&metadata);
            return status;
        }
        metadata.columns[index].descriptor =
            show_engines_field_descriptor(columns[index].length, columns[index].nullable);
    }

    mylite_result_metadata_deinit(&stmt->result_metadata);
    stmt->result_metadata = metadata;
    return MYLITE_OK;
}

static struct mylite_field_descriptor show_engines_field_descriptor(
    uint64_t length,
    bool nullable
) {
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_VAR_STRING,
        .length = length,
        .decimals = mylite_show_not_fixed_decimals,
        .charset_id = mylite_show_latin1_swedish_ci_charset_id,
        .nullable = nullable,
    };

    mylite_field_descriptor_set_nullable(&descriptor, nullable);
    return descriptor;
}
