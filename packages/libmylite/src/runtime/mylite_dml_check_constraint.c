#include "mylite_dml.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_dml_insert_sqlite_bind.h"
#include "mylite_error_codes.h"
#include "mylite_select_types.h"
#include "sql/mylite_expression.h"
#include "sqlite3.h"

#include <limits.h>
#include <stdbool.h>
#include <string.h>

enum check_constraint_row_kind {
    CHECK_CONSTRAINT_INSERT_ROW = 0,
    CHECK_CONSTRAINT_UPDATE_ROW = 1,
};

struct check_constraint_row {
    enum check_constraint_row_kind kind;
    const struct mylite_insert_table *insert_table;
    const struct mylite_insert_bound_value *insert_values;
    const struct mylite_insert_table *update_table;
    const struct mylite_update_row *update_row;
};

struct check_constraint_definition {
    const char *constraint_name;
    const char *check_clause;
};

static int validate_check_constraints(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    bool ignore,
    const struct check_constraint_row *row,
    bool *out_ignored
);

static int validate_check_constraint(
    mylite_db *database,
    const struct check_constraint_definition *definition,
    bool ignore,
    const struct check_constraint_row *row,
    bool *out_ignored
);

static char *build_check_constraint_eval_sql(
    mylite_db *database,
    const char *check_clause,
    const struct check_constraint_row *row
);

static int append_check_constraint_row_projection(
    sqlite3_str *sql,
    const struct check_constraint_row *row
);

static int bind_check_constraint_values(
    mylite_db *database,
    sqlite3_stmt *check,
    const struct check_constraint_row *row
);

static int bind_update_check_constraint_values(
    mylite_db *database,
    sqlite3_stmt *check,
    const struct mylite_update_row *row
);

static int bind_update_check_constraint_value(
    sqlite3_stmt *check,
    int index,
    const struct mylite_expression_value *value
);

static int handle_check_constraint_violation(
    mylite_db *database,
    const char *constraint_name,
    bool ignore,
    bool *out_ignored
);

static const struct mylite_insert_table *check_constraint_insert_table(
    const struct check_constraint_row *row
);

static sqlite3_destructor_type sqlite_transient_destructor(void);

int mylite_dml_validate_insert_check_constraints(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    bool ignore,
    const struct mylite_insert_table *table,
    const struct mylite_insert_bound_value *values,
    bool *out_ignored
) {
    const struct check_constraint_row row = {
        .kind = CHECK_CONSTRAINT_INSERT_ROW,
        .insert_table = table,
        .insert_values = values,
    };

    if (database == NULL || schema_name == NULL || table_name == NULL || table == NULL ||
        values == NULL || out_ignored == NULL) {
        return MYLITE_MISUSE;
    }
    return validate_check_constraints(database, schema_name, table_name, ignore, &row, out_ignored);
}

int mylite_dml_validate_update_check_constraints(
    mylite_db *database,
    const struct mylite_select_table *table,
    const struct mylite_insert_table *write_table,
    const struct mylite_update_row *candidate,
    bool ignore,
    bool *out_ignored
) {
    const struct check_constraint_row row = {
        .kind = CHECK_CONSTRAINT_UPDATE_ROW,
        .update_table = write_table,
        .update_row = candidate,
    };

    if (database == NULL || table == NULL || write_table == NULL || candidate == NULL ||
        out_ignored == NULL) {
        return MYLITE_MISUSE;
    }
    return validate_check_constraints(
        database,
        table->schema_name,
        table->table_name,
        ignore,
        &row,
        out_ignored
    );
}

static int validate_check_constraints(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    bool ignore,
    const struct check_constraint_row *row,
    bool *out_ignored
) {
    sqlite3_stmt *select = NULL;
    bool temporary = false;
    char *sql = NULL;
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    *out_ignored = false;
    status = mylite_catalog_temporary_table_exists(database, schema_name, table_name, &temporary);
    if (status != MYLITE_OK) {
        return status;
    }

    sql = sqlite3_mprintf(
        "SELECT constraint_name, check_clause FROM %s "
        "WHERE table_schema = ? AND table_name = ? AND enforced = 'YES' "
        "ORDER BY ordinal_position",
        mylite_catalog_check_constraint_catalog_name(temporary)
    );
    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &select, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_bind_text(select, 1, schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(select, 2, table_name, -1, sqlite_transient_destructor());
    while ((rc = sqlite3_step(select)) == SQLITE_ROW) {
        const struct check_constraint_definition definition = {
            .constraint_name = (const char *)sqlite3_column_text(select, 0),
            .check_clause = (const char *)sqlite3_column_text(select, 1),
        };

        status = validate_check_constraint(database, &definition, ignore, row, out_ignored);
        if (status != MYLITE_OK || *out_ignored) {
            sqlite3_finalize(select);
            return status;
        }
    }

    sqlite3_finalize(select);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int validate_check_constraint(
    mylite_db *database,
    const struct check_constraint_definition *definition,
    bool ignore,
    const struct check_constraint_row *row,
    bool *out_ignored
) {
    sqlite3_stmt *check = NULL;
    char *sql = build_check_constraint_eval_sql(
        database,
        definition == NULL ? NULL : definition->check_clause,
        row
    );
    int passed = 0;
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &check, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    status = bind_check_constraint_values(database, check, row);
    if (status == MYLITE_OK) {
        rc = sqlite3_step(check);
        if (rc == SQLITE_ROW) {
            passed = sqlite3_column_int(check, 0);
        } else {
            status = rc == SQLITE_DONE ? MYLITE_EXEC_ERROR
                                       : mylite_diagnostics_set_sqlite_error(database);
        }
    }
    sqlite3_finalize(check);
    if (status != MYLITE_OK) {
        return status;
    }
    if (passed) {
        return MYLITE_OK;
    }
    return handle_check_constraint_violation(
        database,
        definition == NULL ? NULL : definition->constraint_name,
        ignore,
        out_ignored
    );
}

static char *build_check_constraint_eval_sql(
    mylite_db *database,
    const char *check_clause,
    const struct check_constraint_row *row
) {
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);

    if (sql == NULL || check_clause == NULL || row == NULL) {
        sqlite3_free(sqlite3_str_finish(sql));
        return NULL;
    }

    sqlite3_str_appendall(sql, "WITH _mylite_check_row AS (SELECT ");
    if (append_check_constraint_row_projection(sql, row) != MYLITE_OK) {
        sqlite3_free(sqlite3_str_finish(sql));
        return NULL;
    }
    sqlite3_str_appendall(
        sql,
        ") SELECT CASE WHEN _mylite_check_value OR _mylite_check_value IS NULL "
        "THEN 1 ELSE 0 END FROM (SELECT "
    );
    sqlite3_str_appendall(sql, check_clause);
    sqlite3_str_appendall(sql, " AS _mylite_check_value FROM _mylite_check_row)");
    return sqlite3_str_finish(sql);
}

static int append_check_constraint_row_projection(
    sqlite3_str *sql,
    const struct check_constraint_row *row
) {
    const struct mylite_insert_table *table = check_constraint_insert_table(row);

    if (table == NULL || table->column_count == 0U) {
        return MYLITE_MISUSE;
    }

    for (size_t index = 0U; index < table->column_count; ++index) {
        if (index != 0U) {
            sqlite3_str_append(sql, ",", 1);
        }
        sqlite3_str_appendf(sql, "? AS \"%w\"", table->columns[index].name);
    }
    return sqlite3_str_errcode(sql) == SQLITE_OK ? MYLITE_OK : MYLITE_NOMEM;
}

static int bind_check_constraint_values(
    mylite_db *database,
    sqlite3_stmt *check,
    const struct check_constraint_row *row
) {
    switch (row->kind) {
    case CHECK_CONSTRAINT_INSERT_ROW:
        return mylite_dml_bind_insert_row_values(
            database,
            check,
            row->insert_values,
            row->insert_table->column_count
        );
    case CHECK_CONSTRAINT_UPDATE_ROW:
        return bind_update_check_constraint_values(database, check, row->update_row);
    }
    return MYLITE_MISUSE;
}

static int bind_update_check_constraint_values(
    mylite_db *database,
    sqlite3_stmt *check,
    const struct mylite_update_row *row
) {
    for (size_t index = 0U; index < row->value_count; ++index) {
        int rc = bind_update_check_constraint_value(check, (int)index + 1, &row->values[index]);

        if (rc != SQLITE_OK) {
            return mylite_diagnostics_set_sqlite_error(database);
        }
    }
    return MYLITE_OK;
}

static int bind_update_check_constraint_value(
    sqlite3_stmt *check,
    int index,
    const struct mylite_expression_value *value
) {
    if (check == NULL || value == NULL) {
        return SQLITE_MISUSE;
    }

    switch (value->kind) {
    case MYLITE_EXPRESSION_VALUE_NULL:
        return sqlite3_bind_null(check, index);
    case MYLITE_EXPRESSION_VALUE_INT64:
        return sqlite3_bind_int64(check, index, (sqlite3_int64)value->int64_value);
    case MYLITE_EXPRESSION_VALUE_UINT64:
        if (value->uint64_value > (uint64_t)INT64_MAX) {
            return sqlite3_bind_double(check, index, (double)value->uint64_value);
        }
        return sqlite3_bind_int64(check, index, (sqlite3_int64)value->uint64_value);
    case MYLITE_EXPRESSION_VALUE_REAL:
        return sqlite3_bind_double(check, index, value->real_value);
    case MYLITE_EXPRESSION_VALUE_TEXT:
        if (value->text_length > (size_t)INT_MAX) {
            return SQLITE_TOOBIG;
        }
        return sqlite3_bind_text(
            check,
            index,
            value->text_value,
            (int)value->text_length,
            sqlite_transient_destructor()
        );
    }
    return SQLITE_MISUSE;
}

static int handle_check_constraint_violation(
    mylite_db *database,
    const char *constraint_name,
    bool ignore,
    bool *out_ignored
) {
    char *message = sqlite3_mprintf(
        "Check constraint '%q' is violated.",
        constraint_name == NULL ? "" : constraint_name
    );
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    if (ignore) {
        *out_ignored = true;
        status = mylite_diagnostics_append_warning(
            database,
            MYLITE_MYSQL_ER_CHECK_CONSTRAINT_VIOLATED,
            message
        );
    } else {
        status = mylite_diagnostics_set_error_message(database, message);
        if (status == MYLITE_OK) {
            status = mylite_diagnostics_append_error(
                database,
                MYLITE_MYSQL_ER_CHECK_CONSTRAINT_VIOLATED,
                message
            );
        }
    }

    sqlite3_free(message);
    if (status != MYLITE_OK) {
        return status;
    }
    if (ignore) {
        return MYLITE_OK;
    }
    return MYLITE_EXEC_ERROR;
}

static const struct mylite_insert_table *check_constraint_insert_table(
    const struct check_constraint_row *row
) {
    switch (row->kind) {
    case CHECK_CONSTRAINT_INSERT_ROW:
        return row->insert_table;
    case CHECK_CONSTRAINT_UPDATE_ROW:
        return row->update_table;
    }
    return NULL;
}

static sqlite3_destructor_type sqlite_transient_destructor(void) {
    return SQLITE_TRANSIENT; // NOLINT(performance-no-int-to-ptr)
}
