#include "mylite_show.h"

#include "mylite_runtime.h"
#include "mylite_show_types.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stdint.h>

static bool diagnostic_condition_matches(
    const struct mylite_expression_warning *condition,
    enum mylite_sql_ast_show_diagnostics_kind kind
);

static const char *diagnostic_condition_level_name(
    const struct mylite_expression_warning *condition
);

char *mylite_show_diagnostics_sql(
    mylite_db *database,
    const struct mylite_show_diagnostics_query *query
) {
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);
    uint64_t matched = 0U;
    uint64_t emitted = 0U;
    bool first = true;

    if (sql == NULL) {
        return NULL;
    }

    for (size_t index = 0U; index < database->warnings.count; ++index) {
        const struct mylite_expression_warning *condition = &database->warnings.items[index];

        if (!diagnostic_condition_matches(condition, query->kind)) {
            continue;
        }
        if (query->has_limit && matched++ < query->offset) {
            continue;
        }
        if (query->has_limit && emitted >= query->row_count) {
            break;
        }
        if (!first) {
            sqlite3_str_appendall(sql, " UNION ALL ");
        }
        sqlite3_str_appendf(
            sql,
            "SELECT %Q AS \"Level\", %u AS \"Code\", %Q AS \"Message\"",
            diagnostic_condition_level_name(condition),
            condition->code,
            condition->message == NULL ? "" : condition->message
        );
        first = false;
        ++emitted;
    }

    if (first) {
        sqlite3_str_appendall(
            sql,
            "SELECT CAST(NULL AS TEXT) AS \"Level\", "
            "CAST(NULL AS INTEGER) AS \"Code\", "
            "CAST(NULL AS TEXT) AS \"Message\" WHERE 0"
        );
    }
    return sqlite3_str_finish(sql);
}

char *mylite_show_diagnostics_count_sql(
    mylite_db *database,
    enum mylite_sql_ast_show_diagnostics_kind kind
) {
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);
    uint64_t count = 0U;
    const char *column_name = kind == MYLITE_SQL_AST_SHOW_DIAGNOSTICS_ERRORS
                                  ? "@@session.error_count"
                                  : "@@session.warning_count";

    if (sql == NULL) {
        return NULL;
    }

    for (size_t index = 0U; index < database->warnings.count; ++index) {
        if (diagnostic_condition_matches(&database->warnings.items[index], kind)) {
            ++count;
        }
    }

    sqlite3_str_appendf(sql, "SELECT %llu AS \"%w\"", (unsigned long long)count, column_name);
    return sqlite3_str_finish(sql);
}

static bool diagnostic_condition_matches(
    const struct mylite_expression_warning *condition,
    enum mylite_sql_ast_show_diagnostics_kind kind
) {
    if (kind == MYLITE_SQL_AST_SHOW_DIAGNOSTICS_WARNINGS) {
        return true;
    }
    return condition->level == MYLITE_EXPRESSION_WARNING_LEVEL_ERROR;
}

static const char *diagnostic_condition_level_name(
    const struct mylite_expression_warning *condition
) {
    if (condition->level == MYLITE_EXPRESSION_WARNING_LEVEL_ERROR) {
        return "Error";
    }
    if (condition->level == MYLITE_EXPRESSION_WARNING_LEVEL_NOTE) {
        return "Note";
    }
    return "Warning";
}
