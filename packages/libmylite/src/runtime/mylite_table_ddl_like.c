#include "mylite_table_ddl.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_runtime.h"
#include "mylite_schema_types.h"
#include "mylite_span.h"
#include "mylite_transactions.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

static int validate_create_like_target_schema(mylite_db *database, const char *schema_name);

static int resolve_create_like_source(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    bool *out_temporary
);

static int create_like_target_exists(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_create_table_plan *plan,
    bool *out_exists
);

static int append_create_like_exists_note(mylite_db *database, const char *table_name);

static int set_create_like_table_exists_error(mylite_db *database, const char *table_name);

static int set_create_like_nonunique_table_error(mylite_db *database, const char *table_name);

static int create_like_transaction(
    mylite_db *database,
    const char *target_schema_name,
    const char *source_schema_name,
    bool source_temporary,
    const struct mylite_create_table_plan *plan
);

static int create_like_physical_table(
    mylite_db *database,
    const char *target_schema_name,
    const char *source_schema_name,
    const struct mylite_create_table_plan *plan
);

static int clone_like_catalog_rows(
    mylite_db *database,
    const char *target_schema_name,
    const char *source_schema_name,
    bool source_temporary,
    const struct mylite_create_table_plan *plan
);

static int clone_like_table_catalog_row(
    mylite_db *database,
    const char *target_schema_name,
    const char *source_schema_name,
    bool source_temporary,
    const struct mylite_create_table_plan *plan
);

static int source_like_has_auto_increment_column(
    mylite_db *database,
    const char *source_schema_name,
    const char *source_table_name,
    bool source_temporary,
    bool *out_has_auto_increment
);

static char *create_like_current_timestamp_text(void);

static int clone_like_column_catalog_rows(
    mylite_db *database,
    const char *target_schema_name,
    const char *source_schema_name,
    bool source_temporary,
    const struct mylite_create_table_plan *plan
);

static int clone_like_index_catalog_rows(
    mylite_db *database,
    const char *target_schema_name,
    const char *source_schema_name,
    bool source_temporary,
    const struct mylite_create_table_plan *plan
);

static int clone_like_check_catalog_rows(
    mylite_db *database,
    const char *target_schema_name,
    const char *source_schema_name,
    bool source_temporary,
    const struct mylite_create_table_plan *plan
);

static sqlite3_destructor_type sqlite_transient_destructor(void);

int mylite_table_ddl_execute_create_table_like_statement(
    mylite_db *database,
    const char *selected_schema,
    struct mylite_create_table_plan *plan,
    bool if_not_exists
) {
    const char *target_schema_name =
        plan->schema_name == NULL ? selected_schema : plan->schema_name;
    const char *source_schema_name =
        plan->source_schema_name == NULL ? selected_schema : plan->source_schema_name;
    bool source_temporary = false;
    bool target_exists = false;
    int status = MYLITE_OK;

    if (target_schema_name == NULL || source_schema_name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "No database selected");
        return MYLITE_EXEC_ERROR;
    }
    if (mylite_ascii_case_equal(target_schema_name, source_schema_name) &&
        mylite_ascii_case_equal(plan->table_name, plan->source_table_name)) {
        return set_create_like_nonunique_table_error(database, plan->table_name);
    }

    status = validate_create_like_target_schema(database, target_schema_name);
    if (status != MYLITE_OK) {
        return status;
    }
    status = resolve_create_like_source(
        database,
        source_schema_name,
        plan->source_table_name,
        &source_temporary
    );
    if (status != MYLITE_OK) {
        return status;
    }
    status = create_like_target_exists(database, target_schema_name, plan, &target_exists);
    if (status != MYLITE_OK) {
        return status;
    }
    if (target_exists) {
        if (if_not_exists) {
            return append_create_like_exists_note(database, plan->table_name);
        }
        return set_create_like_table_exists_error(database, plan->table_name);
    }

    return create_like_transaction(
        database,
        target_schema_name,
        source_schema_name,
        source_temporary,
        plan
    );
}

static int validate_create_like_target_schema(mylite_db *database, const char *schema_name) {
    struct mylite_schema_presence presence;
    int status = mylite_catalog_schema_exists(database, schema_name, &presence);

    if (status != MYLITE_OK) {
        return status;
    }
    if (!presence.exists) {
        (void)mylite_diagnostics_set_error_message_parts(
            database,
            "Unknown database '",
            schema_name,
            "'"
        );
        return MYLITE_EXEC_ERROR;
    }
    if (presence.is_system) {
        return mylite_diagnostics_set_schema_access_denied_error(database, schema_name);
    }
    return MYLITE_OK;
}

static int resolve_create_like_source(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    bool *out_temporary
) {
    bool exists = false;
    int status = mylite_catalog_temporary_table_exists(database, schema_name, table_name, &exists);

    *out_temporary = false;
    if (status != MYLITE_OK) {
        return status;
    }
    if (exists) {
        *out_temporary = true;
        return MYLITE_OK;
    }

    status = mylite_catalog_persistent_table_exists(database, schema_name, table_name, &exists);
    if (status != MYLITE_OK) {
        return status;
    }
    if (exists) {
        return MYLITE_OK;
    }
    return mylite_diagnostics_set_table_doesnt_exist_error(database, schema_name, table_name);
}

static int create_like_target_exists(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_create_table_plan *plan,
    bool *out_exists
) {
    if (plan->temporary) {
        return mylite_catalog_temporary_table_exists(
            database,
            schema_name,
            plan->table_name,
            out_exists
        );
    }
    return mylite_catalog_persistent_table_exists(
        database,
        schema_name,
        plan->table_name,
        out_exists
    );
}

static int append_create_like_exists_note(mylite_db *database, const char *table_name) {
    char *message = sqlite3_mprintf("Table '%q' already exists", table_name);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_diagnostics_append_note(database, MYLITE_MYSQL_ER_TABLE_EXISTS_ERROR, message);
    sqlite3_free(message);
    return status;
}

static int set_create_like_table_exists_error(mylite_db *database, const char *table_name) {
    char *message = sqlite3_mprintf("Table '%q' already exists", table_name);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_diagnostics_set_error_message(database, message);
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_create_like_nonunique_table_error(mylite_db *database, const char *table_name) {
    char *message = sqlite3_mprintf("Not unique table/alias: '%q'", table_name);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_diagnostics_set_error_message(database, message);
    if (status == MYLITE_OK) {
        status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_NONUNIQ_TABLE, message);
    }
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int create_like_transaction(
    mylite_db *database,
    const char *target_schema_name,
    const char *source_schema_name,
    bool source_temporary,
    const struct mylite_create_table_plan *plan
) {
    int status = mylite_transaction_begin_storage(database);

    if (status == MYLITE_OK) {
        status = create_like_physical_table(database, target_schema_name, source_schema_name, plan);
    }
    if (status == MYLITE_OK) {
        status = clone_like_catalog_rows(
            database,
            target_schema_name,
            source_schema_name,
            source_temporary,
            plan
        );
    }
    if (status == MYLITE_OK) {
        status = mylite_transaction_commit_storage(database);
        if (status == MYLITE_OK) {
            return MYLITE_OK;
        }
    }

    mylite_transaction_rollback_storage(database);
    return status;
}

static int create_like_physical_table(
    mylite_db *database,
    const char *target_schema_name,
    const char *source_schema_name,
    const struct mylite_create_table_plan *plan
) {
    char *target_physical_name =
        mylite_catalog_physical_table_name(target_schema_name, plan->table_name);
    char *source_physical_name =
        mylite_catalog_physical_table_name(source_schema_name, plan->source_table_name);
    char *sql = NULL;
    const char *temporary_keyword = "";
    int rc = SQLITE_OK;

    if (plan->temporary) {
        temporary_keyword = "TEMPORARY ";
    }

    if (target_physical_name == NULL || source_physical_name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        free(target_physical_name);
        free(source_physical_name);
        return MYLITE_NOMEM;
    }

    sql = sqlite3_mprintf(
        "CREATE %sTABLE \"%w\" AS SELECT * FROM \"%w\" WHERE 0",
        temporary_keyword,
        target_physical_name,
        source_physical_name
    );
    free(target_physical_name);
    free(source_physical_name);
    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_exec(database->sqlite, sql, NULL, NULL, NULL);
    sqlite3_free(sql);
    return rc == SQLITE_OK ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int clone_like_catalog_rows(
    mylite_db *database,
    const char *target_schema_name,
    const char *source_schema_name,
    bool source_temporary,
    const struct mylite_create_table_plan *plan
) {
    int status = clone_like_table_catalog_row(
        database,
        target_schema_name,
        source_schema_name,
        source_temporary,
        plan
    );

    if (status == MYLITE_OK) {
        status = clone_like_column_catalog_rows(
            database,
            target_schema_name,
            source_schema_name,
            source_temporary,
            plan
        );
    }
    if (status == MYLITE_OK) {
        status = clone_like_index_catalog_rows(
            database,
            target_schema_name,
            source_schema_name,
            source_temporary,
            plan
        );
    }
    if (status == MYLITE_OK) {
        status = clone_like_check_catalog_rows(
            database,
            target_schema_name,
            source_schema_name,
            source_temporary,
            plan
        );
    }
    return status;
}

static int clone_like_table_catalog_row(
    mylite_db *database,
    const char *target_schema_name,
    const char *source_schema_name,
    bool source_temporary,
    const struct mylite_create_table_plan *plan
) {
    enum {
        bind_target_schema = 1,
        bind_target_table = 2,
        bind_auto_increment = 3,
        bind_create_time = 4,
        bind_source_schema = 5,
        bind_source_table = 6,
    };

    sqlite3_stmt *insert = NULL;
    char *create_time = create_like_current_timestamp_text();
    char *sql = sqlite3_mprintf(
        "INSERT INTO %s("
        "table_catalog, table_schema, table_name, table_type, engine, version, row_format, "
        "table_rows, avg_row_length, data_length, max_data_length, index_length, data_free, "
        "auto_increment, create_time, update_time, check_time, table_collation, checksum, "
        "create_options, table_comment)"
        " SELECT 'def', ?, ?, table_type, engine, version, row_format, 0, 0, 0, 0, 0, 0, "
        "?, ?, NULL, NULL, table_collation, NULL, '', table_comment FROM %s "
        "WHERE table_schema = ? AND table_name = ?",
        mylite_catalog_table_catalog_name(plan->temporary),
        mylite_catalog_table_catalog_name(source_temporary)
    );
    bool has_auto_increment = false;
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    if (create_time == NULL || sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        free(create_time);
        sqlite3_free(sql);
        return MYLITE_NOMEM;
    }

    status = source_like_has_auto_increment_column(
        database,
        source_schema_name,
        plan->source_table_name,
        source_temporary,
        &has_auto_increment
    );
    if (status != MYLITE_OK) {
        free(create_time);
        sqlite3_free(sql);
        return status;
    }

    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &insert, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        free(create_time);
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_bind_text(
        insert,
        bind_target_schema,
        target_schema_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(
        insert,
        bind_target_table,
        plan->table_name,
        -1,
        sqlite_transient_destructor()
    );
    if (has_auto_increment) {
        sqlite3_bind_int64(insert, bind_auto_increment, 1);
    } else {
        sqlite3_bind_null(insert, bind_auto_increment);
    }
    sqlite3_bind_text(insert, bind_create_time, create_time, -1, sqlite_transient_destructor());
    sqlite3_bind_text(
        insert,
        bind_source_schema,
        source_schema_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(
        insert,
        bind_source_table,
        plan->source_table_name,
        -1,
        sqlite_transient_destructor()
    );
    free(create_time);

    rc = sqlite3_step(insert);
    sqlite3_finalize(insert);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int source_like_has_auto_increment_column(
    mylite_db *database,
    const char *source_schema_name,
    const char *source_table_name,
    bool source_temporary,
    bool *out_has_auto_increment
) {
    sqlite3_stmt *select = NULL;
    char *sql = sqlite3_mprintf(
        "SELECT 1 FROM %s WHERE table_schema = ? AND table_name = ? "
        "AND extra LIKE '%%auto_increment%%' LIMIT 1",
        mylite_catalog_column_catalog_name(source_temporary)
    );
    int rc = SQLITE_OK;

    *out_has_auto_increment = false;
    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &select, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_bind_text(select, 1, source_schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(select, 2, source_table_name, -1, sqlite_transient_destructor());
    rc = sqlite3_step(select);
    if (rc == SQLITE_ROW) {
        *out_has_auto_increment = true;
        sqlite3_finalize(select);
        return MYLITE_OK;
    }
    sqlite3_finalize(select);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static char *create_like_current_timestamp_text(void) {
    enum { timestamp_length = 19U };

    time_t now = time(NULL);
    struct tm tm_value;
    char *timestamp = malloc(timestamp_length + 1U);

    if (timestamp == NULL) {
        return NULL;
    }
#ifdef _WIN32
    if (gmtime_s(&tm_value, &now) != 0) {
        free(timestamp);
        return NULL;
    }
#else
    if (gmtime_r(&now, &tm_value) == NULL) {
        free(timestamp);
        return NULL;
    }
#endif
    if (strftime(timestamp, timestamp_length + 1U, "%Y-%m-%d %H:%M:%S", &tm_value) == 0U) {
        free(timestamp);
        return NULL;
    }
    return timestamp;
}

static int clone_like_column_catalog_rows(
    mylite_db *database,
    const char *target_schema_name,
    const char *source_schema_name,
    bool source_temporary,
    const struct mylite_create_table_plan *plan
) {
    sqlite3_stmt *insert = NULL;
    char *sql = sqlite3_mprintf(
        "INSERT INTO %s("
        "table_catalog, table_schema, table_name, column_name, ordinal_position, column_default, "
        "has_default, is_nullable, data_type, character_maximum_length, character_octet_length, "
        "numeric_precision, numeric_scale, datetime_precision, character_set_name, "
        "collation_name, column_type, column_key, extra, privileges, column_comment, "
        "generation_expression, srs_id)"
        " SELECT 'def', ?, ?, column_name, ordinal_position, column_default, has_default, "
        "is_nullable, "
        "data_type, character_maximum_length, character_octet_length, numeric_precision, "
        "numeric_scale, datetime_precision, character_set_name, collation_name, column_type, "
        "column_key, extra, privileges, column_comment, generation_expression, srs_id "
        "FROM %s WHERE table_schema = ? AND table_name = ? ORDER BY ordinal_position",
        mylite_catalog_column_catalog_name(plan->temporary),
        mylite_catalog_column_catalog_name(source_temporary)
    );
    int rc = SQLITE_OK;

    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &insert, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_bind_text(insert, 1, target_schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(insert, 2, plan->table_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(insert, 3, source_schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(insert, 4, plan->source_table_name, -1, sqlite_transient_destructor());
    rc = sqlite3_step(insert);
    sqlite3_finalize(insert);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int clone_like_index_catalog_rows(
    mylite_db *database,
    const char *target_schema_name,
    const char *source_schema_name,
    bool source_temporary,
    const struct mylite_create_table_plan *plan
) {
    enum {
        bind_target_schema = 1,
        bind_target_table = 2,
        bind_index_schema = 3,
        bind_source_schema = 4,
        bind_source_table = 5,
    };

    sqlite3_stmt *insert = NULL;
    char *sql = sqlite3_mprintf(
        "INSERT INTO %s("
        "table_catalog, table_schema, table_name, non_unique, index_schema, index_name, "
        "seq_in_index, column_name, collation, cardinality, sub_part, packed, nullable, "
        "index_type, display_index_type, comment, index_comment, is_visible, expression)"
        " SELECT 'def', ?, ?, non_unique, ?, index_name, seq_in_index, column_name, collation, "
        "0, sub_part, packed, nullable, index_type, display_index_type, comment, index_comment, "
        "is_visible, expression FROM %s WHERE table_schema = ? AND table_name = ? ORDER BY rowid",
        mylite_catalog_index_catalog_name(plan->temporary),
        mylite_catalog_index_catalog_name(source_temporary)
    );
    int rc = SQLITE_OK;

    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &insert, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_bind_text(
        insert,
        bind_target_schema,
        target_schema_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(
        insert,
        bind_target_table,
        plan->table_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(
        insert,
        bind_index_schema,
        target_schema_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(
        insert,
        bind_source_schema,
        source_schema_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(
        insert,
        bind_source_table,
        plan->source_table_name,
        -1,
        sqlite_transient_destructor()
    );
    rc = sqlite3_step(insert);
    sqlite3_finalize(insert);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int clone_like_check_catalog_rows(
    mylite_db *database,
    const char *target_schema_name,
    const char *source_schema_name,
    bool source_temporary,
    const struct mylite_create_table_plan *plan
) {
    enum {
        bind_constraint_schema = 1,
        bind_constraint_name_prefix = 2,
        bind_table_schema = 3,
        bind_table_name = 4,
        bind_source_schema = 5,
        bind_source_table = 6,
    };

    sqlite3_stmt *insert = NULL;
    char *sql = sqlite3_mprintf(
        "INSERT INTO %s("
        "constraint_catalog, constraint_schema, constraint_name, table_schema, table_name, "
        "check_clause, enforced, ordinal_position)"
        " SELECT 'def', ?, ? || '_chk_' || CAST(ordinal_position AS TEXT), ?, ?, "
        "check_clause, enforced, ordinal_position FROM %s "
        "WHERE table_schema = ? AND table_name = ? ORDER BY ordinal_position",
        mylite_catalog_check_constraint_catalog_name(plan->temporary),
        mylite_catalog_check_constraint_catalog_name(source_temporary)
    );
    int rc = SQLITE_OK;

    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &insert, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_bind_text(
        insert,
        bind_constraint_schema,
        target_schema_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(
        insert,
        bind_constraint_name_prefix,
        plan->table_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(
        insert,
        bind_table_schema,
        target_schema_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(insert, bind_table_name, plan->table_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(
        insert,
        bind_source_schema,
        source_schema_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(
        insert,
        bind_source_table,
        plan->source_table_name,
        -1,
        sqlite_transient_destructor()
    );
    rc = sqlite3_step(insert);
    sqlite3_finalize(insert);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static sqlite3_destructor_type sqlite_transient_destructor(void) {
    return SQLITE_TRANSIENT; // NOLINT(performance-no-int-to-ptr)
}
