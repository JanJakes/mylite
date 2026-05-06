#include "mylite_table_ddl_catalog.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_span.h"
#include "mylite_table_ddl.h"
#include "mylite_table_ddl_create_catalog_index.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <time.h>

static int insert_table_catalog_row(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_schema_default *schema_default,
    const struct mylite_create_table_plan *plan
);

static int insert_column_catalog_rows(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_schema_default *schema_default,
    const struct mylite_create_table_plan *plan
);

static int insert_check_constraint_catalog_rows(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_create_table_plan *plan
);

static int insert_check_constraint_catalog_row(
    mylite_db *database,
    sqlite3_stmt *insert,
    const char *schema_name,
    const struct mylite_create_table_plan *plan,
    const struct mylite_create_table_check *check,
    size_t check_index
);

static int insert_column_catalog_row(
    mylite_db *database,
    sqlite3_stmt *insert,
    const char *schema_name,
    const struct mylite_schema_default *schema_default,
    const struct mylite_create_table_plan *plan,
    const struct mylite_create_table_column *column,
    size_t column_index
);

static int insert_foreign_key_catalog_row(
    mylite_db *database,
    sqlite3_stmt *insert,
    const char *schema_name,
    const char *table_name,
    const struct mylite_create_table_foreign_key *foreign_key,
    size_t column_index
);

static const char *create_table_column_key(
    const struct mylite_create_table_plan *plan,
    const char *column_name
);

static struct mylite_create_table_column_index_status create_table_column_index_status(
    const struct mylite_create_table_plan *plan,
    const char *column_name
);

static int refresh_create_table_statistics(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_create_table_plan *plan
);

static const char *foreign_key_match_text(enum mylite_sql_ast_reference_match match);

static const char *foreign_key_action_text(enum mylite_sql_ast_reference_action action);

static char *create_table_current_timestamp_text(void);

static sqlite3_destructor_type sqlite_transient_destructor(void);

int mylite_table_ddl_insert_create_table_catalog_rows(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_schema_default *schema_default,
    const struct mylite_create_table_plan *plan
) {
    int status = insert_table_catalog_row(database, schema_name, schema_default, plan);

    if (status == MYLITE_OK) {
        status = insert_column_catalog_rows(database, schema_name, schema_default, plan);
    }
    if (status == MYLITE_OK) {
        status =
            mylite_table_ddl_insert_create_table_index_catalog_rows(database, schema_name, plan);
    }
    if (status == MYLITE_OK) {
        status = insert_check_constraint_catalog_rows(database, schema_name, plan);
    }
    if (status == MYLITE_OK) {
        status = mylite_table_ddl_insert_foreign_key_catalog_rows(
            database,
            schema_name,
            plan->table_name,
            plan->temporary,
            plan->foreign_keys,
            plan->foreign_key_count
        );
    }
    if (status == MYLITE_OK) {
        status = refresh_create_table_statistics(database, schema_name, plan);
    }
    return status;
}

static int insert_table_catalog_row(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_schema_default *schema_default,
    const struct mylite_create_table_plan *plan
) {
    enum {
        bind_auto_increment = 4,
        bind_create_time = 5,
        bind_table_collation = 6,
        bind_table_comment = 7,
    };

    sqlite3_stmt *insert = NULL;
    char *create_time = create_table_current_timestamp_text();
    char *sql = sqlite3_mprintf(
        "INSERT INTO %s("
        "table_catalog, table_schema, table_name, table_type, engine, version, row_format, "
        "table_rows, avg_row_length, data_length, max_data_length, index_length, data_free, "
        "auto_increment, create_time, update_time, check_time, table_collation, checksum, "
        "create_options, table_comment)"
        " VALUES('def', ?, ?, 'BASE TABLE', ?, 10, 'Dynamic', 0, 0, 0, 0, 0, 0, "
        "?, ?, NULL, NULL, ?, NULL, '', ?)",
        mylite_catalog_table_catalog_name(plan->temporary)
    );
    const char *collation =
        plan->options.collation == NULL ? schema_default->collation : plan->options.collation;
    const char *comment = plan->options.comment == NULL ? "" : plan->options.comment;
    int rc = SQLITE_OK;

    if (create_time == NULL || sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        free(create_time);
        sqlite3_free(sql);
        return MYLITE_NOMEM;
    }
    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &insert, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        free(create_time);
        return mylite_diagnostics_set_sqlite_error(database);
    }

    sqlite3_bind_text(insert, 1, schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(insert, 2, plan->table_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(insert, 3, "InnoDB", -1, SQLITE_STATIC);
    if (plan->options.has_auto_increment) {
        sqlite3_bind_int64(
            insert,
            bind_auto_increment,
            (sqlite3_int64)plan->options.auto_increment
        );
    } else {
        sqlite3_bind_null(insert, bind_auto_increment);
    }
    sqlite3_bind_text(insert, bind_create_time, create_time, -1, sqlite_transient_destructor());
    sqlite3_bind_text(insert, bind_table_collation, collation, -1, sqlite_transient_destructor());
    sqlite3_bind_text(insert, bind_table_comment, comment, -1, sqlite_transient_destructor());
    free(create_time);

    rc = sqlite3_step(insert);
    sqlite3_finalize(insert);
    if (rc != SQLITE_DONE) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    return MYLITE_OK;
}

static int insert_column_catalog_rows(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_schema_default *schema_default,
    const struct mylite_create_table_plan *plan
) {
    sqlite3_stmt *insert = NULL;
    char *sql = sqlite3_mprintf(
        "INSERT INTO %s("
        "table_catalog, table_schema, table_name, column_name, ordinal_position, column_default, "
        "is_nullable, data_type, character_maximum_length, character_octet_length, "
        "numeric_precision, numeric_scale, datetime_precision, character_set_name, "
        "collation_name, column_type, column_key, extra, privileges, column_comment, "
        "generation_expression, srs_id)"
        " VALUES('def', ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "'select,insert,update,references', ?, ?, NULL)",
        mylite_catalog_column_catalog_name(plan->temporary)
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

    for (size_t index = 0U; index < plan->column_count; ++index) {
        int status = insert_column_catalog_row(
            database,
            insert,
            schema_name,
            schema_default,
            plan,
            &plan->columns[index],
            index
        );
        if (status != MYLITE_OK) {
            sqlite3_finalize(insert);
            return status;
        }
    }

    sqlite3_finalize(insert);
    return MYLITE_OK;
}

static int insert_check_constraint_catalog_rows(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_create_table_plan *plan
) {
    sqlite3_stmt *insert = NULL;
    char *sql = NULL;
    int rc = SQLITE_OK;

    if (plan->check_count == 0U) {
        return MYLITE_OK;
    }

    sql = sqlite3_mprintf(
        "INSERT INTO %s("
        "constraint_catalog, constraint_schema, constraint_name, table_schema, table_name, "
        "check_clause, enforced, ordinal_position)"
        " VALUES('def', ?, ?, ?, ?, ?, ?, ?)",
        mylite_catalog_check_constraint_catalog_name(plan->temporary)
    );
    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &insert, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    for (size_t index = 0U; index < plan->check_count; ++index) {
        int status = insert_check_constraint_catalog_row(
            database,
            insert,
            schema_name,
            plan,
            &plan->checks[index],
            index
        );

        if (status != MYLITE_OK) {
            sqlite3_finalize(insert);
            return status;
        }
    }

    sqlite3_finalize(insert);
    return MYLITE_OK;
}

static int insert_check_constraint_catalog_row(
    mylite_db *database,
    sqlite3_stmt *insert,
    const char *schema_name,
    const struct mylite_create_table_plan *plan,
    const struct mylite_create_table_check *check,
    size_t check_index
) {
    enum {
        bind_constraint_schema = 1,
        bind_constraint_name = 2,
        bind_table_schema = 3,
        bind_table_name = 4,
        bind_check_clause = 5,
        bind_enforced = 6,
        bind_ordinal_position = 7,
    };

    const char *enforced = check->enforced ? "YES" : "NO";
    int rc = SQLITE_OK;

    sqlite3_reset(insert);
    sqlite3_clear_bindings(insert);
    sqlite3_bind_text(
        insert,
        bind_constraint_schema,
        schema_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(insert, bind_constraint_name, check->name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(insert, bind_table_schema, schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(insert, bind_table_name, plan->table_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(insert, bind_check_clause, check->clause, -1, sqlite_transient_destructor());
    sqlite3_bind_text(insert, bind_enforced, enforced, -1, SQLITE_STATIC);
    sqlite3_bind_int64(insert, bind_ordinal_position, (sqlite3_int64)check_index + 1);

    rc = sqlite3_step(insert);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

int mylite_table_ddl_insert_foreign_key_catalog_rows(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    bool temporary,
    const struct mylite_create_table_foreign_key *foreign_keys,
    size_t foreign_key_count
) {
    sqlite3_stmt *insert = NULL;
    char *sql = NULL;
    int rc = SQLITE_OK;

    if (foreign_key_count == 0U) {
        return MYLITE_OK;
    }

    sql = sqlite3_mprintf(
        "INSERT INTO %s("
        "constraint_catalog, constraint_schema, constraint_name, table_schema, table_name, "
        "column_name, ordinal_position, supporting_index_name, unique_constraint_catalog, "
        "unique_constraint_schema, unique_constraint_name, match_option, update_rule, "
        "delete_rule, referenced_table_schema, referenced_table_name, referenced_column_name, "
        "position_in_unique_constraint)"
        " VALUES('def', ?, ?, ?, ?, ?, ?, ?, 'def', ?, ?, ?, ?, ?, ?, ?, ?, ?)",
        mylite_catalog_foreign_key_catalog_name(temporary)
    );
    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &insert, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    for (size_t index = 0U; index < foreign_key_count; ++index) {
        for (size_t column = 0U; column < foreign_keys[index].column_count; ++column) {
            int status = insert_foreign_key_catalog_row(
                database,
                insert,
                schema_name,
                table_name,
                &foreign_keys[index],
                column
            );

            if (status != MYLITE_OK) {
                sqlite3_finalize(insert);
                return status;
            }
        }
    }

    sqlite3_finalize(insert);
    return MYLITE_OK;
}

static int insert_foreign_key_catalog_row(
    mylite_db *database,
    sqlite3_stmt *insert,
    const char *schema_name,
    const char *table_name,
    const struct mylite_create_table_foreign_key *foreign_key,
    size_t column_index
) {
    enum {
        bind_constraint_schema = 1,
        bind_constraint_name = 2,
        bind_table_schema = 3,
        bind_table_name = 4,
        bind_column_name = 5,
        bind_ordinal_position = 6,
        bind_supporting_index_name = 7,
        bind_unique_constraint_schema = 8,
        bind_unique_constraint_name = 9,
        bind_match_option = 10,
        bind_update_rule = 11,
        bind_delete_rule = 12,
        bind_referenced_table_schema = 13,
        bind_referenced_table_name = 14,
        bind_referenced_column_name = 15,
        bind_position_in_unique_constraint = 16,
    };

    int rc = SQLITE_OK;

    sqlite3_reset(insert);
    sqlite3_clear_bindings(insert);
    sqlite3_bind_text(
        insert,
        bind_constraint_schema,
        schema_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(
        insert,
        bind_constraint_name,
        foreign_key->constraint_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(insert, bind_table_schema, schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(insert, bind_table_name, table_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(
        insert,
        bind_column_name,
        foreign_key->column_names[column_index],
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_int64(insert, bind_ordinal_position, (sqlite3_int64)column_index + 1);
    sqlite3_bind_text(
        insert,
        bind_supporting_index_name,
        foreign_key->supporting_index_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(
        insert,
        bind_unique_constraint_schema,
        foreign_key->referenced_schema_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(
        insert,
        bind_unique_constraint_name,
        foreign_key->unique_constraint_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(
        insert,
        bind_match_option,
        foreign_key_match_text(foreign_key->match),
        -1,
        SQLITE_STATIC
    );
    sqlite3_bind_text(
        insert,
        bind_update_rule,
        foreign_key_action_text(foreign_key->on_update),
        -1,
        SQLITE_STATIC
    );
    sqlite3_bind_text(
        insert,
        bind_delete_rule,
        foreign_key_action_text(foreign_key->on_delete),
        -1,
        SQLITE_STATIC
    );
    sqlite3_bind_text(
        insert,
        bind_referenced_table_schema,
        foreign_key->referenced_schema_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(
        insert,
        bind_referenced_table_name,
        foreign_key->referenced_table_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(
        insert,
        bind_referenced_column_name,
        foreign_key->referenced_column_names[column_index],
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_int64(insert, bind_position_in_unique_constraint, (sqlite3_int64)column_index + 1);

    rc = sqlite3_step(insert);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int insert_column_catalog_row(
    mylite_db *database,
    sqlite3_stmt *insert,
    const char *schema_name,
    const struct mylite_schema_default *schema_default,
    const struct mylite_create_table_plan *plan,
    const struct mylite_create_table_column *column,
    size_t column_index
) {
    enum {
        bind_table_schema = 1,
        bind_table_name = 2,
        bind_column_name = 3,
        bind_ordinal_position = 4,
        bind_column_default = 5,
        bind_is_nullable = 6,
        bind_data_type = 7,
        bind_character_maximum_length = 8,
        bind_character_octet_length = 9,
        bind_numeric_precision = 10,
        bind_numeric_scale = 11,
        bind_datetime_precision = 12,
        bind_character_set_name = 13,
        bind_collation_name = 14,
        bind_column_type = 15,
        bind_column_key = 16,
        bind_extra = 17,
        bind_column_comment = 18,
        bind_generation_expression = 19,
    };
    struct mylite_column_type_descriptor descriptor;
    const char *column_key = create_table_column_key(plan, column->name);
    const char *extra = mylite_table_ddl_create_table_column_extra(column);
    const char *is_nullable = "NO";
    const char *comment = "";
    int status = mylite_table_ddl_describe_create_table_column(
        column,
        schema_default,
        &plan->options,
        &descriptor
    );
    int rc = SQLITE_OK;

    if (status != MYLITE_OK) {
        return status;
    }
    if (column->nullable) {
        is_nullable = "YES";
    }
    if (column->comment != NULL) {
        comment = column->comment;
    }

    sqlite3_reset(insert);
    sqlite3_clear_bindings(insert);
    sqlite3_bind_text(insert, bind_table_schema, schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(insert, bind_table_name, plan->table_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(insert, bind_column_name, column->name, -1, sqlite_transient_destructor());
    sqlite3_bind_int64(insert, bind_ordinal_position, (sqlite3_int64)column_index + 1);
    if (column->default_text == NULL) {
        sqlite3_bind_null(insert, bind_column_default);
    } else {
        sqlite3_bind_text(
            insert,
            bind_column_default,
            column->default_text,
            -1,
            sqlite_transient_destructor()
        );
    }
    sqlite3_bind_text(insert, bind_is_nullable, is_nullable, -1, SQLITE_STATIC);
    sqlite3_bind_text(insert, bind_data_type, descriptor.data_type, -1, SQLITE_STATIC);
    if (descriptor.is_character_string || descriptor.is_binary_string) {
        sqlite3_bind_int64(
            insert,
            bind_character_maximum_length,
            (sqlite3_int64)descriptor.character_maximum_length
        );
        sqlite3_bind_int64(
            insert,
            bind_character_octet_length,
            (sqlite3_int64)descriptor.character_octet_length
        );
    } else {
        sqlite3_bind_null(insert, bind_character_maximum_length);
        sqlite3_bind_null(insert, bind_character_octet_length);
    }
    if (descriptor.numeric_precision != 0U) {
        sqlite3_bind_int(insert, bind_numeric_precision, (int)descriptor.numeric_precision);
    } else {
        sqlite3_bind_null(insert, bind_numeric_precision);
    }
    if (descriptor.has_numeric_scale) {
        sqlite3_bind_int(insert, bind_numeric_scale, (int)descriptor.numeric_scale);
    } else {
        sqlite3_bind_null(insert, bind_numeric_scale);
    }
    if (descriptor.has_datetime_precision) {
        sqlite3_bind_int(insert, bind_datetime_precision, (int)descriptor.datetime_precision);
    } else {
        sqlite3_bind_null(insert, bind_datetime_precision);
    }
    if (descriptor.character_set_name == NULL) {
        sqlite3_bind_null(insert, bind_character_set_name);
    } else {
        sqlite3_bind_text(
            insert,
            bind_character_set_name,
            descriptor.character_set_name,
            -1,
            SQLITE_STATIC
        );
    }
    if (descriptor.collation_name == NULL) {
        sqlite3_bind_null(insert, bind_collation_name);
    } else {
        sqlite3_bind_text(
            insert,
            bind_collation_name,
            descriptor.collation_name,
            -1,
            SQLITE_STATIC
        );
    }
    sqlite3_bind_text(
        insert,
        bind_column_type,
        descriptor.column_type,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(insert, bind_column_key, column_key, -1, SQLITE_STATIC);
    if (extra == NULL || extra[0] == '\0') {
        sqlite3_bind_text(insert, bind_extra, "", -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_text(insert, bind_extra, extra, -1, SQLITE_STATIC);
    }
    sqlite3_bind_text(insert, bind_column_comment, comment, -1, sqlite_transient_destructor());
    sqlite3_bind_text(
        insert,
        bind_generation_expression,
        column->generation_expression == NULL ? "" : column->generation_expression,
        -1,
        sqlite_transient_destructor()
    );

    rc = sqlite3_step(insert);
    if (rc != SQLITE_DONE) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    return MYLITE_OK;
}

static const char *create_table_column_key(
    const struct mylite_create_table_plan *plan,
    const char *column_name
) {
    struct mylite_create_table_column_index_status status =
        create_table_column_index_status(plan, column_name);

    if (status.primary) {
        return "PRI";
    }
    if (status.unique) {
        return "UNI";
    }
    if (status.indexed) {
        return "MUL";
    }
    return "";
}

static struct mylite_create_table_column_index_status create_table_column_index_status(
    const struct mylite_create_table_plan *plan,
    const char *column_name
) {
    struct mylite_create_table_column_index_status status = {
        .indexed = false,
        .unique = false,
        .primary = false,
    };

    for (size_t index = 0U; index < plan->index_count; ++index) {
        const struct mylite_create_table_index *table_index = &plan->indexes[index];

        for (size_t part = 0U; part < table_index->part_count; ++part) {
            if (!mylite_ascii_case_equal(table_index->parts[part].column_name, column_name)) {
                continue;
            }
            status.indexed = true;
            if (table_index->is_primary) {
                status.primary = true;
            }
            if (table_index->is_unique && part == 0U) {
                status.unique = true;
            }
        }
    }
    return status;
}

static int refresh_create_table_statistics(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_create_table_plan *plan
) {
    char *physical_name = mylite_catalog_physical_table_name(schema_name, plan->table_name);
    int status = MYLITE_OK;

    if (physical_name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_catalog_refresh_table_statistics(
        database,
        schema_name,
        plan->table_name,
        physical_name
    );
    free(physical_name);
    return status;
}

static const char *foreign_key_match_text(enum mylite_sql_ast_reference_match match) {
    switch (match) {
    case MYLITE_SQL_AST_REFERENCE_MATCH_NONE:
    case MYLITE_SQL_AST_REFERENCE_MATCH_SIMPLE:
    case MYLITE_SQL_AST_REFERENCE_MATCH_FULL:
    case MYLITE_SQL_AST_REFERENCE_MATCH_PARTIAL:
        return "NONE";
    }
    return "NONE";
}

static const char *foreign_key_action_text(enum mylite_sql_ast_reference_action action) {
    switch (action) {
    case MYLITE_SQL_AST_REFERENCE_ACTION_RESTRICT:
        return "RESTRICT";
    case MYLITE_SQL_AST_REFERENCE_ACTION_CASCADE:
        return "CASCADE";
    case MYLITE_SQL_AST_REFERENCE_ACTION_SET_NULL:
        return "SET NULL";
    case MYLITE_SQL_AST_REFERENCE_ACTION_SET_DEFAULT:
        return "SET DEFAULT";
    case MYLITE_SQL_AST_REFERENCE_ACTION_NONE:
    case MYLITE_SQL_AST_REFERENCE_ACTION_NO_ACTION:
        return "NO ACTION";
    }
    return "NO ACTION";
}

static char *create_table_current_timestamp_text(void) {
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

static sqlite3_destructor_type sqlite_transient_destructor(void) {
    // SQLite's public macro intentionally uses this sentinel pointer value.
    return SQLITE_TRANSIENT; // NOLINT(performance-no-int-to-ptr)
}
