#include "mylite_table_ddl_create_validate.h"

#include "mylite_catalog.h"
#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_foreign_key_catalog.h"
#include "mylite_span.h"
#include "mylite_table_ddl.h"
#include "mylite_table_ddl_check_validate.h"
#include "mylite_table_ddl_create_options.h"
#include "mylite_table_ddl_plan_lookup.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static bool validate_create_table_column_names(
    mylite_db *database,
    const struct mylite_create_table_plan *plan
);

static bool validate_create_table_indexes(
    mylite_db *database,
    const struct mylite_create_table_plan *plan
);

static int validate_create_table_checks(
    mylite_db *database,
    const struct mylite_create_table_plan *plan
);

static int validate_create_table_check_expressions(
    mylite_db *database,
    const struct mylite_create_table_plan *plan
);

static bool validate_create_table_foreign_keys(
    mylite_db *database,
    const char *schema_name,
    struct mylite_create_table_plan *plan
);

static bool validate_create_table_foreign_key(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_create_table_plan *plan,
    struct mylite_create_table_foreign_key *foreign_key
);

static bool create_table_foreign_key_name_exists(
    const struct mylite_create_table_plan *plan,
    const char *name,
    size_t before_index
);

static bool create_table_check_name_exists(
    const struct mylite_create_table_plan *plan,
    const char *name,
    size_t before_index
);

static int set_create_table_exists_error(mylite_db *database, const char *table_name);

static bool set_create_table_duplicate_column_error(mylite_db *database, const char *column_name);

static bool set_create_table_duplicate_key_error(mylite_db *database, const char *index_name);

static bool set_create_table_multiple_primary_key_error(mylite_db *database);

static int set_create_table_duplicate_check_error(mylite_db *database, const char *constraint_name);

static int set_create_table_duplicate_foreign_key_error(
    mylite_db *database,
    const char *constraint_name
);

static bool create_table_foreign_key_columns_exist(
    mylite_db *database,
    const struct mylite_create_table_plan *plan,
    const struct mylite_create_table_foreign_key *foreign_key
);

static int resolve_self_referenced_unique_constraint(
    const struct mylite_create_table_plan *plan,
    const struct mylite_create_table_foreign_key *foreign_key,
    char **out_constraint_name,
    bool *out_found
);

static int resolve_referenced_unique_constraint(
    mylite_db *database,
    const struct mylite_create_table_foreign_key *foreign_key,
    char **out_constraint_name,
    bool *out_found
);

static int referenced_unique_candidate_matches(
    mylite_db *database,
    const char *candidate_name,
    const struct mylite_create_table_foreign_key *foreign_key,
    bool *out_matches
);

static sqlite3_destructor_type sqlite_transient_destructor(void);

static int append_create_table_exists_note(mylite_db *database, const char *table_name);

static void apply_create_table_primary_key_nullability(struct mylite_create_table_plan *plan);

static bool create_table_index_name_exists(
    const struct mylite_create_table_plan *plan,
    const char *name,
    size_t before_index
);

int mylite_table_ddl_validate_create_table_plan(
    mylite_db *database,
    const char *schema_name,
    struct mylite_create_table_plan *plan,
    bool if_not_exists,
    struct mylite_schema_default *schema_default,
    bool *out_skip_create
) {
    struct mylite_schema_presence presence;
    bool exists = false;
    int status = mylite_catalog_schema_exists(database, schema_name, &presence);

    *out_skip_create = false;
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

    if (plan->temporary) {
        status =
            mylite_catalog_temporary_table_exists(database, schema_name, plan->table_name, &exists);
    } else {
        status = mylite_catalog_persistent_table_exists(
            database,
            schema_name,
            plan->table_name,
            &exists
        );
    }
    if (status != MYLITE_OK) {
        return status;
    }
    if (exists) {
        if (if_not_exists) {
            int note_status = append_create_table_exists_note(database, plan->table_name);

            if (note_status == MYLITE_OK) {
                *out_skip_create = true;
            }
            return note_status;
        }
        return set_create_table_exists_error(database, plan->table_name);
    }

    status = mylite_catalog_schema_default_by_name(database, schema_name, schema_default);
    if (status != MYLITE_OK) {
        return status;
    }
    status = mylite_table_ddl_normalize_create_table_options(
        database,
        schema_name,
        schema_default,
        &plan->options
    );
    if (status != MYLITE_OK) {
        return status;
    }
    if (!validate_create_table_column_names(database, plan)) {
        return MYLITE_EXEC_ERROR;
    }
    status = mylite_table_ddl_assign_generated_index_names(database, plan);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!validate_create_table_indexes(database, plan)) {
        return MYLITE_EXEC_ERROR;
    }
    status = validate_create_table_checks(database, plan);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!validate_create_table_foreign_keys(database, schema_name, plan)) {
        return MYLITE_EXEC_ERROR;
    }
    apply_create_table_primary_key_nullability(plan);
    return MYLITE_OK;
}

static bool validate_create_table_column_names(
    mylite_db *database,
    const struct mylite_create_table_plan *plan
) {
    if (plan->column_count == 0U) {
        (void)mylite_diagnostics_set_error_message(
            database,
            "CREATE TABLE requires at least one column"
        );
        return false;
    }

    for (size_t left = 0U; left < plan->column_count; ++left) {
        for (size_t right = left + 1U; right < plan->column_count; ++right) {
            if (mylite_ascii_case_equal(plan->columns[left].name, plan->columns[right].name)) {
                return set_create_table_duplicate_column_error(database, plan->columns[right].name);
            }
        }
    }
    return true;
}

static bool validate_create_table_indexes(
    mylite_db *database,
    const struct mylite_create_table_plan *plan
) {
    bool has_primary = false;

    for (size_t index = 0U; index < plan->index_count; ++index) {
        const struct mylite_create_table_index *table_index = &plan->indexes[index];

        if (table_index->is_primary) {
            if (has_primary) {
                return set_create_table_multiple_primary_key_error(database);
            }
            has_primary = true;
        }
        if (table_index->explicit_name &&
            create_table_index_name_exists(plan, table_index->name, index)) {
            return set_create_table_duplicate_key_error(database, table_index->name);
        }
        for (size_t part = 0U; part < table_index->part_count; ++part) {
            if (mylite_table_ddl_find_create_table_column(
                    plan,
                    table_index->parts[part].column_name
                ) == NULL) {
                (void)mylite_diagnostics_set_error_message_parts(
                    database,
                    "Key column '",
                    table_index->parts[part].column_name,
                    "' doesn't exist in table"
                );
                return false;
            }
        }
    }
    return true;
}

static int validate_create_table_checks(
    mylite_db *database,
    const struct mylite_create_table_plan *plan
) {
    for (size_t index = 0U; index < plan->check_count; ++index) {
        if (create_table_check_name_exists(plan, plan->checks[index].name, index)) {
            return set_create_table_duplicate_check_error(database, plan->checks[index].name);
        }
    }
    return validate_create_table_check_expressions(database, plan);
}

static int validate_create_table_check_expressions(
    mylite_db *database,
    const struct mylite_create_table_plan *plan
) {
    struct mylite_table_ddl_check_column *columns = NULL;
    int status = MYLITE_OK;

    if (plan->check_count == 0U) {
        return MYLITE_OK;
    }
    if (plan->column_count > 0U) {
        columns = calloc(plan->column_count, sizeof(*columns));
    }
    if (plan->column_count > 0U && columns == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    for (size_t index = 0U; index < plan->column_count; ++index) {
        columns[index] = (struct mylite_table_ddl_check_column){
            .name = plan->columns[index].name,
            .auto_increment = plan->columns[index].auto_increment,
        };
    }
    for (size_t index = 0U; index < plan->check_count; ++index) {
        const struct mylite_table_ddl_check_validation_input input = {
            .constraint_name = plan->checks[index].name,
            .expression = plan->checks[index].expression,
            .columns = columns,
            .column_count = plan->column_count,
            .context = MYLITE_TABLE_DDL_CHECK_VALIDATE_CREATE_TABLE,
        };

        status = mylite_table_ddl_validate_check_expression(database, &input);
        if (status != MYLITE_OK) {
            break;
        }
    }
    free(columns);
    return status;
}

static bool create_table_check_name_exists(
    const struct mylite_create_table_plan *plan,
    const char *name,
    size_t before_index
) {
    for (size_t index = 0U; index < before_index; ++index) {
        if (mylite_ascii_case_equal(plan->checks[index].name, name)) {
            return true;
        }
    }
    return false;
}

static int set_create_table_exists_error(mylite_db *database, const char *table_name) {
    int status = mylite_diagnostics_set_error_message_parts(
        database,
        "Table '",
        table_name,
        "' already exists"
    );

    if (status == MYLITE_OK) {
        status = mylite_diagnostics_append_error(
            database,
            MYLITE_MYSQL_ER_TABLE_EXISTS_ERROR,
            mylite_error_message(database)
        );
    }
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static bool set_create_table_duplicate_column_error(mylite_db *database, const char *column_name) {
    int status = mylite_diagnostics_set_error_message_parts(
        database,
        "Duplicate column name '",
        column_name,
        "'"
    );

    if (status == MYLITE_OK) {
        status = mylite_diagnostics_append_error(
            database,
            MYLITE_MYSQL_ER_DUP_FIELDNAME,
            mylite_error_message(database)
        );
    }
    return false;
}

static bool set_create_table_duplicate_key_error(mylite_db *database, const char *index_name) {
    int status = mylite_diagnostics_set_error_message_parts(
        database,
        "Duplicate key name '",
        index_name,
        "'"
    );

    if (status == MYLITE_OK) {
        status = mylite_diagnostics_append_error(
            database,
            MYLITE_MYSQL_ER_DUP_KEYNAME,
            mylite_error_message(database)
        );
    }
    return false;
}

static bool set_create_table_multiple_primary_key_error(mylite_db *database) {
    int status = mylite_diagnostics_set_error_message(database, "Multiple primary key defined");

    if (status == MYLITE_OK) {
        status = mylite_diagnostics_append_error(
            database,
            MYLITE_MYSQL_ER_MULTIPLE_PRI_KEY,
            mylite_error_message(database)
        );
    }
    return false;
}

static int set_create_table_duplicate_check_error(
    mylite_db *database,
    const char *constraint_name
) {
    char *message = sqlite3_mprintf("Duplicate check constraint name '%q'.", constraint_name);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_set_error_message(database, message);
    if (status == MYLITE_OK) {
        status = mylite_diagnostics_append_error(
            database,
            MYLITE_MYSQL_ER_CHECK_CONSTRAINT_DUP_NAME,
            message
        );
    }
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static bool validate_create_table_foreign_keys(
    mylite_db *database,
    const char *schema_name,
    struct mylite_create_table_plan *plan
) {
    if (plan->temporary && plan->foreign_key_count > 0U) {
        (void)mylite_diagnostics_set_error_message(database, "Cannot add foreign key constraint");
        return false;
    }
    for (size_t index = 0U; index < plan->foreign_key_count; ++index) {
        bool catalog_name_exists = false;
        int status = MYLITE_OK;

        if (create_table_foreign_key_name_exists(
                plan,
                plan->foreign_keys[index].constraint_name,
                index
            )) {
            (void)set_create_table_duplicate_foreign_key_error(
                database,
                plan->foreign_keys[index].constraint_name
            );
            return false;
        }
        status = mylite_foreign_key_catalog_child_constraint_exists(
            database,
            schema_name,
            plan->foreign_keys[index].constraint_name,
            &catalog_name_exists
        );
        if (status != MYLITE_OK) {
            return false;
        }
        if (catalog_name_exists) {
            (void)set_create_table_duplicate_foreign_key_error(
                database,
                plan->foreign_keys[index].constraint_name
            );
            return false;
        }
        if (!validate_create_table_foreign_key(
                database,
                schema_name,
                plan,
                &plan->foreign_keys[index]
            )) {
            return false;
        }
    }
    return true;
}

static int set_create_table_duplicate_foreign_key_error(
    mylite_db *database,
    const char *constraint_name
) {
    int status = mylite_diagnostics_set_error_message_parts(
        database,
        "Duplicate foreign key constraint name '",
        constraint_name,
        "'"
    );

    if (status == MYLITE_OK) {
        status = mylite_diagnostics_append_error(
            database,
            MYLITE_MYSQL_ER_FK_DUP_NAME,
            mylite_error_message(database)
        );
    }
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static bool validate_create_table_foreign_key(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_create_table_plan *plan,
    struct mylite_create_table_foreign_key *foreign_key
) {
    bool parent_exists = false;
    bool found_unique_constraint = false;
    char *unique_constraint_name = NULL;
    int status = MYLITE_OK;

    if (foreign_key->referenced_schema_name == NULL) {
        foreign_key->referenced_schema_name =
            mylite_copy_span_text(schema_name, strlen(schema_name));
        if (foreign_key->referenced_schema_name == NULL) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return false;
        }
    }
    if (!create_table_foreign_key_columns_exist(database, plan, foreign_key)) {
        return false;
    }
    if (mylite_ascii_case_equal(foreign_key->referenced_schema_name, schema_name) &&
        mylite_ascii_case_equal(foreign_key->referenced_table_name, plan->table_name)) {
        status = resolve_self_referenced_unique_constraint(
            plan,
            foreign_key,
            &unique_constraint_name,
            &found_unique_constraint
        );
        if (status != MYLITE_OK) {
            if (status == MYLITE_NOMEM) {
                (void)mylite_diagnostics_set_error_message(database, "out of memory");
            }
            return false;
        }
        if (!found_unique_constraint) {
            (void)mylite_diagnostics_set_foreign_key_missing_unique_parent_error(
                database,
                foreign_key->constraint_name,
                foreign_key->referenced_table_name
            );
            return false;
        }
        foreign_key->unique_constraint_name = unique_constraint_name;
        return true;
    }
    status = mylite_catalog_persistent_table_exists(
        database,
        foreign_key->referenced_schema_name,
        foreign_key->referenced_table_name,
        &parent_exists
    );
    if (status != MYLITE_OK) {
        return false;
    }
    if (!parent_exists && !mylite_connection_foreign_key_checks(database)) {
        return true;
    }
    if (!parent_exists) {
        (void)mylite_diagnostics_set_error_message_parts(
            database,
            "Failed to open the referenced table '",
            foreign_key->referenced_table_name,
            "'"
        );
        return false;
    }

    status = resolve_referenced_unique_constraint(
        database,
        foreign_key,
        &unique_constraint_name,
        &found_unique_constraint
    );
    if (status != MYLITE_OK) {
        free(unique_constraint_name);
        return false;
    }
    if (!found_unique_constraint) {
        (void)mylite_diagnostics_set_foreign_key_missing_unique_parent_error(
            database,
            foreign_key->constraint_name,
            foreign_key->referenced_table_name
        );
        free(unique_constraint_name);
        return false;
    }
    foreign_key->unique_constraint_name = unique_constraint_name;
    return true;
}

static bool create_table_foreign_key_name_exists(
    const struct mylite_create_table_plan *plan,
    const char *name,
    size_t before_index
) {
    for (size_t index = 0U; index < before_index; ++index) {
        if (mylite_ascii_case_equal(plan->foreign_keys[index].constraint_name, name)) {
            return true;
        }
    }
    return false;
}

static bool create_table_foreign_key_columns_exist(
    mylite_db *database,
    const struct mylite_create_table_plan *plan,
    const struct mylite_create_table_foreign_key *foreign_key
) {
    for (size_t index = 0U; index < foreign_key->column_count; ++index) {
        if (mylite_table_ddl_find_create_table_column(plan, foreign_key->column_names[index]) ==
            NULL) {
            (void)mylite_diagnostics_set_error_message_parts(
                database,
                "Key column '",
                foreign_key->column_names[index],
                "' doesn't exist in table"
            );
            return false;
        }
    }
    return true;
}

static int resolve_self_referenced_unique_constraint(
    const struct mylite_create_table_plan *plan,
    const struct mylite_create_table_foreign_key *foreign_key,
    char **out_constraint_name,
    bool *out_found
) {
    *out_constraint_name = NULL;
    *out_found = false;
    for (size_t index = 0U; index < plan->index_count; ++index) {
        const struct mylite_create_table_index *candidate = &plan->indexes[index];
        bool matches = candidate->part_count == foreign_key->referenced_column_count;

        if (!candidate->is_primary && !candidate->is_unique) {
            continue;
        }
        for (size_t part = 0U; matches && part < candidate->part_count; ++part) {
            matches = mylite_ascii_case_equal(
                candidate->parts[part].column_name,
                foreign_key->referenced_column_names[part]
            );
        }
        if (matches) {
            *out_constraint_name = mylite_copy_span_text(
                candidate->name,
                candidate->name == NULL ? 0U : strlen(candidate->name)
            );
            if (*out_constraint_name == NULL) {
                return MYLITE_NOMEM;
            }
            *out_found = true;
            return MYLITE_OK;
        }
    }
    return MYLITE_OK;
}

static int resolve_referenced_unique_constraint(
    mylite_db *database,
    const struct mylite_create_table_foreign_key *foreign_key,
    char **out_constraint_name,
    bool *out_found
) {
    static const char sql[] =
        "SELECT index_name FROM __mylite_index_catalog "
        "WHERE table_schema = ? AND table_name = ? AND non_unique = 0 "
        "GROUP BY index_name "
        "ORDER BY CASE WHEN index_name = 'PRIMARY' THEN 0 ELSE 1 END, MIN(rowid)";
    sqlite3_stmt *select = NULL;
    int rc = SQLITE_OK;

    *out_constraint_name = NULL;
    *out_found = false;
    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &select, NULL);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_bind_text(
        select,
        1,
        foreign_key->referenced_schema_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(
        select,
        2,
        foreign_key->referenced_table_name,
        -1,
        sqlite_transient_destructor()
    );

    while ((rc = sqlite3_step(select)) == SQLITE_ROW) {
        const char *candidate_name = (const char *)sqlite3_column_text(select, 0);
        bool matches = false;
        int status = MYLITE_OK;

        if (candidate_name != NULL) {
            status = referenced_unique_candidate_matches(
                database,
                candidate_name,
                foreign_key,
                &matches
            );
            if (status != MYLITE_OK) {
                sqlite3_finalize(select);
                return status;
            }
        }
        if (candidate_name != NULL && matches) {
            *out_constraint_name = mylite_copy_span_text(candidate_name, strlen(candidate_name));
            if (*out_constraint_name == NULL) {
                sqlite3_finalize(select);
                return MYLITE_NOMEM;
            }
            *out_found = true;
            break;
        }
    }

    sqlite3_finalize(select);
    return rc == SQLITE_DONE || *out_found ? MYLITE_OK
                                           : mylite_diagnostics_set_sqlite_error(database);
}

static int referenced_unique_candidate_matches(
    mylite_db *database,
    const char *candidate_name,
    const struct mylite_create_table_foreign_key *foreign_key,
    bool *out_matches
) {
    static const char sql[] = "SELECT column_name FROM __mylite_index_catalog "
                              "WHERE table_schema = ? AND table_name = ? AND index_name = ? "
                              "ORDER BY seq_in_index";
    sqlite3_stmt *select = NULL;
    size_t matched = 0U;
    int rc = SQLITE_OK;

    *out_matches = false;
    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &select, NULL);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_bind_text(
        select,
        1,
        foreign_key->referenced_schema_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(
        select,
        2,
        foreign_key->referenced_table_name,
        -1,
        sqlite_transient_destructor()
    );
    sqlite3_bind_text(select, 3, candidate_name, -1, sqlite_transient_destructor());

    while ((rc = sqlite3_step(select)) == SQLITE_ROW) {
        const char *column_name = (const char *)sqlite3_column_text(select, 0);

        if (matched >= foreign_key->referenced_column_count || column_name == NULL ||
            !mylite_ascii_case_equal(column_name, foreign_key->referenced_column_names[matched])) {
            sqlite3_finalize(select);
            return MYLITE_OK;
        }
        ++matched;
    }

    if (rc != SQLITE_DONE) {
        sqlite3_finalize(select);
        return mylite_diagnostics_set_sqlite_error(database);
    }
    *out_matches = matched == foreign_key->referenced_column_count;
    sqlite3_finalize(select);
    return MYLITE_OK;
}

static sqlite3_destructor_type sqlite_transient_destructor(void) {
    return SQLITE_TRANSIENT; // NOLINT(performance-no-int-to-ptr)
}

static int append_create_table_exists_note(mylite_db *database, const char *table_name) {
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

static void apply_create_table_primary_key_nullability(struct mylite_create_table_plan *plan) {
    for (size_t index = 0U; index < plan->index_count; ++index) {
        const struct mylite_create_table_index *table_index = &plan->indexes[index];

        if (!table_index->is_primary) {
            continue;
        }
        for (size_t part = 0U; part < table_index->part_count; ++part) {
            for (size_t column = 0U; column < plan->column_count; ++column) {
                if (mylite_ascii_case_equal(
                        plan->columns[column].name,
                        table_index->parts[part].column_name
                    )) {
                    plan->columns[column].nullable = false;
                }
            }
        }
    }
}

static bool create_table_index_name_exists(
    const struct mylite_create_table_plan *plan,
    const char *name,
    size_t before_index
) {
    for (size_t index = 0U; index < before_index; ++index) {
        if (plan->indexes[index].name != NULL &&
            mylite_ascii_case_equal(plan->indexes[index].name, name)) {
            return true;
        }
    }
    return false;
}
