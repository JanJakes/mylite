#include "mylite_catalog_internal.h"

#include "mylite_sqlite_registration.h"
#include "sqlite3.h"

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define MYLITE_ARRAY_COUNT(values) (sizeof(values) / sizeof((values)[0]))

struct catalog_table_spec {
    const char *name;
    const char *const *columns;
    size_t column_count;
    const char *const *checks;
    size_t check_count;
};

struct catalog_index_spec {
    const char *table_name;
    const char *index_name;
    bool unique;
    const char *const *columns;
    size_t column_count;
};

struct normalized_sql_search {
    const char *sql;
    const char *fragment;
};

enum catalog_table_info_column {
    catalog_table_info_cid_column = 0,
    catalog_table_info_name_column = 1,
    catalog_table_info_type_column = 2,
    catalog_table_info_not_null_column = 3,
    catalog_table_info_primary_key_column = 4,
    catalog_table_info_hidden_column = 5,
    catalog_table_info_seen_capacity = 64,
};

static int validate_catalog_tables(sqlite3 *sqlite);
static int validate_catalog_table_shape(sqlite3 *sqlite, const struct catalog_table_spec *spec);
static int validate_catalog_table_checks(sqlite3 *sqlite, const struct catalog_table_spec *spec);
static const char *expected_catalog_column_type(const char *name);
static bool catalog_column_is_nullable(const char *name);
static bool normalized_sql_contains(struct normalized_sql_search search);
static int validate_catalog_indexes(sqlite3 *sqlite);
static int validate_catalog_index(sqlite3 *sqlite, const struct catalog_index_spec *spec);
static int catalog_index_columns_match(
    sqlite3 *sqlite,
    const char *index_name,
    const struct catalog_index_spec *spec,
    bool *out_matches
);
static int validate_catalog_rows(sqlite3 *sqlite);
static int validate_catalog_has_no_matching_row(sqlite3 *sqlite, const char *sql);
static int validate_physical_schema(sqlite3 *sqlite);

int mylite_catalog_validate_integrity(sqlite3 *sqlite) {
    int rc = sqlite == NULL ? MYLITE_MISUSE : validate_catalog_tables(sqlite);

    if (rc == MYLITE_OK) {
        rc = validate_catalog_indexes(sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_integrity_seal_triggers(sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = validate_catalog_rows(sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = validate_physical_schema(sqlite);
    }
    return rc;
}

int mylite_catalog_validate_mutation_integrity(sqlite3 *sqlite) {
    int rc = sqlite == NULL ? MYLITE_MISUSE : validate_catalog_rows(sqlite);

    if (rc == MYLITE_OK) {
        rc = validate_physical_schema(sqlite);
    }
    return rc;
}

static int validate_catalog_tables(sqlite3 *sqlite) {
    static const char *const state_columns[] = {
        "singleton_id",
        "schema_version",
        "minimum_reader_schema_version",
        "catalog_generation",
        "created_with_file_format_version",
        "integrity_catalog_generation",
        "integrity_sqlite_schema_version",
    };
    static const char *const state_checks[] = {
        "check(singleton_id=1)",
        "check(integrity_catalog_generation>=0)",
        "check(integrity_sqlite_schema_version>=0)",
    };
    static const char *const schema_columns[] = {
        "schema_id",
        "name",
        "default_charset",
        "default_collation",
        "descriptor_version",
        "created_catalog_generation",
        "updated_catalog_generation",
    };
    static const char *const table_columns[] = {
        "table_id",
        "schema_id",
        "name",
        "kind",
        "physical_name",
        "auto_increment_next",
        "auto_increment_status",
        "default_charset",
        "default_collation",
        "comment",
        "row_format_option",
        "key_block_size",
        "pack_keys",
        "checksum",
        "stats_persistent",
        "stats_auto_recalc",
        "stats_sample_pages",
        "min_rows",
        "max_rows",
        "avg_row_length",
        "delay_key_write",
        "fulltext_doc_id_initialized",
        "created_time_utc_epoch",
        "updated_time_utc_epoch",
        "descriptor_version",
        "created_catalog_generation",
        "updated_catalog_generation",
    };
    static const char *const table_checks[] = {
        "check(kindin(1,3))",
        "check(auto_increment_next>0)",
        "check(auto_increment_status>=0)",
        "check(key_block_sizein(0,1,2,4,8,16))",
        "check(pack_keysin(-1,0,1))",
        "check(checksumin(0,1))",
        "check(stats_persistentin(-1,0,1))",
        "check(stats_auto_recalcin(-1,0,1))",
        "check(stats_sample_pagesbetween0and65535)",
        "check(min_rows>=0)",
        "check(max_rows>=0)",
        "check(avg_row_length>=0)",
        "check(delay_key_writein(-1,0,1))",
        "check(fulltext_doc_id_initializedin(0,1))",
        "check(created_time_utc_epoch>=0)",
        "check(updated_time_utc_epoch>=0)",
    };
    static const char *const view_columns[] = {
        "table_id",
        "view_definition",
        "show_create_sql",
        "check_option",
        "is_updatable",
        "definer",
        "security_type",
        "character_set_client",
        "collation_connection",
        "source_schema_id",
        "source_table_id",
        "source_schema_name",
        "source_table_name",
        "descriptor_version",
        "created_catalog_generation",
        "updated_catalog_generation",
    };
    static const char *const column_columns[] = {
        "column_id",
        "table_id",
        "ordinal_position",
        "name",
        "logical_type",
        "physical_type",
        "is_nullable",
        "is_visible",
        "is_auto_increment",
        "default_kind",
        "default_integer",
        "default_text",
        "on_update_current_timestamp",
        "character_set_name",
        "collation_name",
        "comment",
        "is_generated",
        "generated_kind",
        "generation_expression",
        "sqlite_generation_expression",
        "descriptor_version",
        "created_catalog_generation",
        "updated_catalog_generation",
    };
    static const char *const column_checks[] = {
        "check(ordinal_position>0)",
        "check(is_nullablein(0,1))",
        "check(is_visiblein(0,1))",
        "check(is_auto_incrementin(0,1))",
        "check(default_kindin(0,1,2,3,4,5,6,7,8,9,10,11,12))",
        "check(on_update_current_timestampin(0,1))",
        "check(is_generatedin(0,1))",
        "check(generated_kindin(0,1,2))",
    };
    static const char *const index_columns[] = {
        "index_id",
        "table_id",
        "name",
        "kind",
        "is_unique",
        "is_visible",
        "physical_name",
        "comment",
        "show_create_explicit_btree",
        "descriptor_version",
        "created_catalog_generation",
        "updated_catalog_generation",
    };
    static const char *const index_checks[] = {
        "check(kindin(1,2,3,4))",
        "check(is_uniquein(0,1))",
        "check(is_visiblein(0,1))",
        "check(show_create_explicit_btreein(0,1))",
    };
    static const char *const index_column_columns[] = {
        "index_column_id",
        "index_id",
        "table_id",
        "column_id",
        "ordinal_position",
        "prefix_length",
        "sort_direction",
        "descriptor_version",
        "created_catalog_generation",
        "updated_catalog_generation",
    };
    static const char *const index_column_checks[] = {
        "check(ordinal_position>0)",
        "check(prefix_lengthisnullorprefix_length>0)",
        "check(sort_directionin(1,2))",
    };
    static const char *const foreign_key_columns[] = {
        "foreign_key_id",
        "child_table_id",
        "parent_table_id",
        "name",
        "parent_index_id",
        "child_index_id",
        "update_rule",
        "delete_rule",
        "match_option",
        "descriptor_version",
        "created_catalog_generation",
        "updated_catalog_generation",
    };
    static const char *const foreign_key_column_columns[] = {
        "foreign_key_column_id",
        "foreign_key_id",
        "child_table_id",
        "parent_table_id",
        "child_column_id",
        "parent_column_id",
        "ordinal_position",
        "position_in_unique_constraint",
        "descriptor_version",
        "created_catalog_generation",
        "updated_catalog_generation",
    };
    static const char *const foreign_key_column_checks[] = {
        "check(ordinal_position>0)",
        "check(position_in_unique_constraint>0)",
    };
    static const char *const check_constraint_columns[] = {
        "check_constraint_id",
        "table_id",
        "name",
        "physical_name",
        "check_clause",
        "sqlite_expression",
        "is_enforced",
        "name_is_generated",
        "generated_ordinal",
        "ordinal_position",
        "descriptor_version",
        "created_catalog_generation",
        "updated_catalog_generation",
    };
    static const char *const check_constraint_checks[] = {
        "check(is_enforcedin(0,1))",
        "check(name_is_generatedin(0,1))",
        "check(generated_ordinal>0)",
        "check(ordinal_position>0)",
    };
    static const struct catalog_table_spec specs[] = {
        {"_mylite_catalog_state",
         state_columns,
         MYLITE_ARRAY_COUNT(state_columns),
         state_checks,
         MYLITE_ARRAY_COUNT(state_checks)},
        {"_mylite_catalog_schemas", schema_columns, MYLITE_ARRAY_COUNT(schema_columns), NULL, 0U},
        {"_mylite_catalog_tables",
         table_columns,
         MYLITE_ARRAY_COUNT(table_columns),
         table_checks,
         MYLITE_ARRAY_COUNT(table_checks)},
        {"_mylite_catalog_views", view_columns, MYLITE_ARRAY_COUNT(view_columns), NULL, 0U},
        {"_mylite_catalog_columns",
         column_columns,
         MYLITE_ARRAY_COUNT(column_columns),
         column_checks,
         MYLITE_ARRAY_COUNT(column_checks)},
        {"_mylite_catalog_indexes",
         index_columns,
         MYLITE_ARRAY_COUNT(index_columns),
         index_checks,
         MYLITE_ARRAY_COUNT(index_checks)},
        {"_mylite_catalog_index_columns",
         index_column_columns,
         MYLITE_ARRAY_COUNT(index_column_columns),
         index_column_checks,
         MYLITE_ARRAY_COUNT(index_column_checks)},
        {"_mylite_catalog_foreign_keys",
         foreign_key_columns,
         MYLITE_ARRAY_COUNT(foreign_key_columns),
         NULL,
         0U},
        {"_mylite_catalog_foreign_key_columns",
         foreign_key_column_columns,
         MYLITE_ARRAY_COUNT(foreign_key_column_columns),
         foreign_key_column_checks,
         MYLITE_ARRAY_COUNT(foreign_key_column_checks)},
        {"_mylite_catalog_check_constraints",
         check_constraint_columns,
         MYLITE_ARRAY_COUNT(check_constraint_columns),
         check_constraint_checks,
         MYLITE_ARRAY_COUNT(check_constraint_checks)},
    };
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < MYLITE_ARRAY_COUNT(specs); ++index) {
        rc = validate_catalog_table_shape(sqlite, &specs[index]);
        if (rc == MYLITE_OK) {
            rc = validate_catalog_table_checks(sqlite, &specs[index]);
        }
    }
    return rc;
}

static int validate_catalog_table_shape(sqlite3 *sqlite, const struct catalog_table_spec *spec) {
    sqlite3_stmt *statement = NULL;
    uint64_t seen_columns = 0U;
    size_t seen_count = 0U;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_prepare_statement(
        sqlite,
        "SELECT cid, name, type, \"notnull\", pk, hidden "
        "FROM pragma_table_xinfo(?1) ORDER BY cid",
        &statement
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 1, spec->name);
    }
    while (rc == MYLITE_OK && (sqlite_rc = mylite_catalog_sqlite3_step(statement)) == SQLITE_ROW) {
        const char *name = NULL;
        const char *type = NULL;
        const char *expected_type = NULL;
        size_t expected_index = spec->column_count;
        int expected_not_null = 0;

        if (sqlite3_column_type(statement, catalog_table_info_cid_column) != SQLITE_INTEGER ||
            sqlite3_column_type(statement, catalog_table_info_name_column) != SQLITE_TEXT ||
            sqlite3_column_type(statement, catalog_table_info_type_column) != SQLITE_TEXT ||
            sqlite3_column_type(statement, catalog_table_info_not_null_column) != SQLITE_INTEGER ||
            sqlite3_column_type(statement, catalog_table_info_primary_key_column) !=
                SQLITE_INTEGER ||
            sqlite3_column_type(statement, catalog_table_info_hidden_column) != SQLITE_INTEGER) {
            rc = MYLITE_ERROR;
            continue;
        }
        name = (const char *)sqlite3_column_text(statement, catalog_table_info_name_column);
        type = (const char *)sqlite3_column_text(statement, catalog_table_info_type_column);
        for (size_t index = 0U; name != NULL && index < spec->column_count; ++index) {
            if (strcmp(name, spec->columns[index]) == 0) {
                expected_index = index;
                break;
            }
        }
        if (expected_index >= spec->column_count ||
            expected_index >= catalog_table_info_seen_capacity ||
            (seen_columns & (UINT64_C(1) << expected_index)) != 0U) {
            rc = MYLITE_ERROR;
            continue;
        }
        expected_type = expected_catalog_column_type(spec->columns[expected_index]);
        expected_not_null =
            expected_index == 0U || catalog_column_is_nullable(spec->columns[expected_index]) ? 0
                                                                                              : 1;
        if (type == NULL || strcmp(type, expected_type) != 0 ||
            sqlite3_column_int(statement, catalog_table_info_not_null_column) !=
                expected_not_null ||
            sqlite3_column_int(statement, catalog_table_info_primary_key_column) !=
                (expected_index == 0U ? 1 : 0) ||
            sqlite3_column_int(statement, catalog_table_info_hidden_column) != 0) {
            rc = MYLITE_ERROR;
            continue;
        }
        seen_columns |= UINT64_C(1) << expected_index;
        ++seen_count;
    }
    if (rc == MYLITE_OK) {
        if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        } else if (seen_count != spec->column_count) {
            rc = MYLITE_ERROR;
        }
    }
    return mylite_catalog_finalize_statement(statement, rc);
}

static int validate_catalog_table_checks(sqlite3 *sqlite, const struct catalog_table_spec *spec) {
    sqlite3_stmt *statement = NULL;
    const char *sql = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_prepare_statement(
        sqlite,
        "SELECT sql FROM sqlite_schema WHERE type = 'table' AND name = ?1",
        &statement
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 1, spec->name);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = mylite_catalog_sqlite3_step(statement);
        if (sqlite_rc != SQLITE_ROW || sqlite3_column_type(statement, 0) != SQLITE_TEXT) {
            rc = sqlite_rc == SQLITE_ROW || sqlite_rc == SQLITE_DONE
                     ? MYLITE_ERROR
                     : mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    if (rc == MYLITE_OK) {
        sql = (const char *)sqlite3_column_text(statement, 0);
        for (size_t index = 0U; index < spec->check_count; ++index) {
            if (sql == NULL || !normalized_sql_contains((struct normalized_sql_search
                               ){.sql = sql, .fragment = spec->checks[index]})) {
                rc = MYLITE_ERROR;
                break;
            }
        }
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = mylite_catalog_sqlite3_step(statement);
        if (sqlite_rc != SQLITE_DONE) {
            rc = sqlite_rc == SQLITE_ROW ? MYLITE_ERROR : mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    return mylite_catalog_finalize_statement(statement, rc);
}

static const char *expected_catalog_column_type(const char *name) {
    static const char *const text_columns[] = {
        "name",
        "default_charset",
        "default_collation",
        "physical_name",
        "comment",
        "row_format_option",
        "logical_type",
        "physical_type",
        "default_text",
        "character_set_name",
        "collation_name",
        "generation_expression",
        "sqlite_generation_expression",
        "view_definition",
        "show_create_sql",
        "check_option",
        "is_updatable",
        "definer",
        "security_type",
        "character_set_client",
        "collation_connection",
        "source_schema_name",
        "source_table_name",
        "update_rule",
        "delete_rule",
        "match_option",
        "check_clause",
        "sqlite_expression",
    };

    for (size_t index = 0U; index < MYLITE_ARRAY_COUNT(text_columns); ++index) {
        if (strcmp(name, text_columns[index]) == 0) {
            return "TEXT";
        }
    }
    return "INTEGER";
}

static bool catalog_column_is_nullable(const char *name) {
    if (strcmp(name, "default_integer") == 0 || strcmp(name, "default_text") == 0 ||
        strcmp(name, "prefix_length") == 0) {
        return true;
    }
    return false;
}

static bool normalized_sql_contains(struct normalized_sql_search search) {
    for (const char *start = search.sql; *start != '\0'; ++start) {
        const char *left = start;
        const char *right = search.fragment;

        while (*right != '\0') {
            while (isspace((unsigned char)*left) != 0) {
                ++left;
            }
            if (*left == '\0' || tolower((unsigned char)*left) != (unsigned char)*right) {
                break;
            }
            ++left;
            ++right;
        }
        if (*right == '\0') {
            return true;
        }
    }
    return false;
}

static int validate_catalog_indexes(sqlite3 *sqlite) {
    static const char *const name[] = {"name"};
    static const char *const physical_name[] = {"physical_name"};
    static const char *const schema_name[] = {"schema_id", "name"};
    static const char *const table_ordinal[] = {"table_id", "ordinal_position"};
    static const char *const table_name[] = {"table_id", "name"};
    static const char *const index_ordinal[] = {"index_id", "ordinal_position"};
    static const char *const index_column[] = {"index_id", "column_id"};
    static const char *const child_name[] = {"child_table_id", "name"};
    static const char *const parent_foreign_key[] = {"parent_table_id", "foreign_key_id"};
    static const char *const foreign_key_ordinal[] = {"foreign_key_id", "ordinal_position"};
    static const char *const table_physical_name[] = {"table_id", "physical_name"};
    static const struct catalog_index_spec specs[] = {
        {"_mylite_catalog_schemas", NULL, true, name, MYLITE_ARRAY_COUNT(name)},
        {"_mylite_catalog_tables", NULL, true, physical_name, MYLITE_ARRAY_COUNT(physical_name)},
        {"_mylite_catalog_tables", NULL, true, schema_name, MYLITE_ARRAY_COUNT(schema_name)},
        {"_mylite_catalog_columns", NULL, true, table_ordinal, MYLITE_ARRAY_COUNT(table_ordinal)},
        {"_mylite_catalog_columns", NULL, true, table_name, MYLITE_ARRAY_COUNT(table_name)},
        {"_mylite_catalog_indexes", NULL, true, physical_name, MYLITE_ARRAY_COUNT(physical_name)},
        {"_mylite_catalog_indexes", NULL, true, table_name, MYLITE_ARRAY_COUNT(table_name)},
        {"_mylite_catalog_index_columns",
         NULL,
         true,
         index_ordinal,
         MYLITE_ARRAY_COUNT(index_ordinal)},
        {"_mylite_catalog_index_columns", NULL, true, index_column, MYLITE_ARRAY_COUNT(index_column)
        },
        {"_mylite_catalog_foreign_keys", NULL, true, child_name, MYLITE_ARRAY_COUNT(child_name)},
        {"_mylite_catalog_foreign_keys",
         "_mylite_catalog_foreign_keys_parent_table_id",
         false,
         parent_foreign_key,
         MYLITE_ARRAY_COUNT(parent_foreign_key)},
        {"_mylite_catalog_foreign_key_columns",
         NULL,
         true,
         foreign_key_ordinal,
         MYLITE_ARRAY_COUNT(foreign_key_ordinal)},
        {"_mylite_catalog_check_constraints", NULL, true, table_name, MYLITE_ARRAY_COUNT(table_name)
        },
        {"_mylite_catalog_check_constraints",
         NULL,
         true,
         table_physical_name,
         MYLITE_ARRAY_COUNT(table_physical_name)},
        {"_mylite_catalog_check_constraints",
         NULL,
         true,
         table_ordinal,
         MYLITE_ARRAY_COUNT(table_ordinal)},
    };
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < MYLITE_ARRAY_COUNT(specs); ++index) {
        rc = validate_catalog_index(sqlite, &specs[index]);
    }
    return rc;
}

static int validate_catalog_index(sqlite3 *sqlite, const struct catalog_index_spec *spec) {
    sqlite3_stmt *statement = NULL;
    bool found = false;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_prepare_statement(
        sqlite,
        "SELECT name, \"unique\" FROM pragma_index_list(?1) ORDER BY seq",
        &statement
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 1, spec->table_name);
    }
    while (rc == MYLITE_OK && (sqlite_rc = mylite_catalog_sqlite3_step(statement)) == SQLITE_ROW) {
        const char *name = NULL;
        bool columns_match = false;

        if (sqlite3_column_type(statement, 0) != SQLITE_TEXT ||
            sqlite3_column_type(statement, 1) != SQLITE_INTEGER) {
            rc = MYLITE_ERROR;
            break;
        }
        name = (const char *)sqlite3_column_text(statement, 0);
        if (name == NULL || sqlite3_column_int(statement, 1) != (int)spec->unique ||
            (spec->index_name != NULL && strcmp(name, spec->index_name) != 0)) {
            continue;
        }
        rc = catalog_index_columns_match(sqlite, name, spec, &columns_match);
        if (rc == MYLITE_OK && columns_match) {
            found = true;
            break;
        }
    }
    if (rc == MYLITE_OK && !found) {
        rc = sqlite_rc == SQLITE_DONE || sqlite_rc == SQLITE_ROW
                 ? MYLITE_ERROR
                 : mylite_sqlite_status_to_mylite(sqlite_rc);
    }
    return mylite_catalog_finalize_statement(statement, rc);
}

static int catalog_index_columns_match(
    sqlite3 *sqlite,
    const char *index_name,
    const struct catalog_index_spec *spec,
    bool *out_matches
) {
    sqlite3_stmt *statement = NULL;
    bool matches = true;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_prepare_statement(
        sqlite,
        "SELECT name FROM pragma_index_info(?1) ORDER BY seqno",
        &statement
    );

    *out_matches = false;
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 1, index_name);
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < spec->column_count; ++index) {
        const char *name = NULL;

        sqlite_rc = mylite_catalog_sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            matches = false;
            break;
        }
        if (sqlite_rc != SQLITE_ROW) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            break;
        }
        if (sqlite3_column_type(statement, 0) != SQLITE_TEXT) {
            matches = false;
            continue;
        }
        name = (const char *)sqlite3_column_text(statement, 0);
        if (name == NULL || strcmp(name, spec->columns[index]) != 0) {
            matches = false;
        }
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = mylite_catalog_sqlite3_step(statement);
        if (sqlite_rc != SQLITE_DONE && sqlite_rc != SQLITE_ROW) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        } else if (sqlite_rc == SQLITE_ROW) {
            matches = false;
        }
    }
    if (rc == MYLITE_OK) {
        *out_matches = matches;
    }
    return mylite_catalog_finalize_statement(statement, rc);
}

static int validate_catalog_rows(sqlite3 *sqlite) {
    static const char *const invalid_row_queries[] = {
        "SELECT 1 FROM _mylite_catalog_tables AS t "
        "LEFT JOIN _mylite_catalog_schemas AS s ON s.schema_id = t.schema_id "
        "WHERE s.schema_id IS NULL LIMIT 1",
        "SELECT 1 FROM _mylite_catalog_columns AS c "
        "LEFT JOIN _mylite_catalog_tables AS t ON t.table_id = c.table_id "
        "WHERE t.table_id IS NULL LIMIT 1",
        "SELECT 1 FROM _mylite_catalog_views AS v "
        "LEFT JOIN _mylite_catalog_tables AS t ON t.table_id = v.table_id "
        "LEFT JOIN _mylite_catalog_schemas AS ss ON ss.schema_id = v.source_schema_id "
        "LEFT JOIN _mylite_catalog_tables AS st ON st.table_id = v.source_table_id "
        "WHERE t.table_id IS NULL OR t.kind <> 3 "
        "OR ((v.source_schema_id = 0) <> (v.source_table_id = 0)) "
        "OR (v.source_table_id <> 0 AND (ss.schema_id IS NULL OR st.table_id IS NULL "
        "OR st.kind <> 1 OR st.schema_id <> v.source_schema_id)) LIMIT 1",
        "SELECT 1 FROM _mylite_catalog_tables AS t "
        "LEFT JOIN _mylite_catalog_views AS v ON v.table_id = t.table_id "
        "WHERE t.kind = 3 AND v.table_id IS NULL LIMIT 1",
        "SELECT 1 FROM _mylite_catalog_indexes AS i "
        "LEFT JOIN _mylite_catalog_tables AS t ON t.table_id = i.table_id "
        "WHERE t.table_id IS NULL OR t.kind <> 1 LIMIT 1",
        "SELECT 1 FROM _mylite_catalog_index_columns AS ic "
        "LEFT JOIN _mylite_catalog_indexes AS i ON i.index_id = ic.index_id "
        "LEFT JOIN _mylite_catalog_columns AS c ON c.column_id = ic.column_id "
        "WHERE i.index_id IS NULL OR c.column_id IS NULL OR ic.table_id <> i.table_id "
        "OR ic.table_id <> c.table_id LIMIT 1",
        "SELECT 1 FROM _mylite_catalog_indexes AS i WHERE NOT EXISTS "
        "(SELECT 1 FROM _mylite_catalog_index_columns AS ic WHERE ic.index_id = i.index_id) LIMIT "
        "1",
        "SELECT 1 FROM _mylite_catalog_foreign_keys AS fk "
        "LEFT JOIN _mylite_catalog_tables AS ct ON ct.table_id = fk.child_table_id "
        "LEFT JOIN _mylite_catalog_tables AS pt ON pt.table_id = fk.parent_table_id "
        "LEFT JOIN _mylite_catalog_indexes AS pi ON pi.index_id = fk.parent_index_id "
        "LEFT JOIN _mylite_catalog_indexes AS ci ON ci.index_id = fk.child_index_id "
        "WHERE ct.table_id IS NULL OR pt.table_id IS NULL OR ct.kind <> 1 OR pt.kind <> 1 "
        "OR pi.index_id IS NULL OR ci.index_id IS NULL OR pi.table_id <> fk.parent_table_id "
        "OR ci.table_id <> fk.child_table_id OR pi.is_unique <> 1 LIMIT 1",
        "SELECT 1 FROM _mylite_catalog_foreign_key_columns AS fkc "
        "LEFT JOIN _mylite_catalog_foreign_keys AS fk ON fk.foreign_key_id = fkc.foreign_key_id "
        "LEFT JOIN _mylite_catalog_columns AS cc ON cc.column_id = fkc.child_column_id "
        "LEFT JOIN _mylite_catalog_columns AS pc ON pc.column_id = fkc.parent_column_id "
        "LEFT JOIN _mylite_catalog_index_columns AS cic "
        "ON cic.index_id = fk.child_index_id AND cic.column_id = fkc.child_column_id "
        "AND cic.ordinal_position = fkc.ordinal_position "
        "LEFT JOIN _mylite_catalog_index_columns AS pic "
        "ON pic.index_id = fk.parent_index_id AND pic.column_id = fkc.parent_column_id "
        "AND pic.ordinal_position = fkc.position_in_unique_constraint "
        "WHERE fk.foreign_key_id IS NULL OR fkc.child_table_id <> fk.child_table_id "
        "OR fkc.parent_table_id <> fk.parent_table_id OR cc.table_id <> fk.child_table_id "
        "OR pc.table_id <> fk.parent_table_id OR cic.index_column_id IS NULL "
        "OR pic.index_column_id IS NULL LIMIT 1",
        "SELECT 1 FROM _mylite_catalog_foreign_keys AS fk WHERE NOT EXISTS "
        "(SELECT 1 FROM _mylite_catalog_foreign_key_columns AS fkc "
        "WHERE fkc.foreign_key_id = fk.foreign_key_id) LIMIT 1",
        "SELECT 1 FROM _mylite_catalog_check_constraints AS cc "
        "LEFT JOIN _mylite_catalog_tables AS t ON t.table_id = cc.table_id "
        "WHERE t.table_id IS NULL OR t.kind <> 1 LIMIT 1",
        "SELECT 1 FROM _mylite_catalog_columns GROUP BY table_id "
        "HAVING MIN(ordinal_position) <> 1 OR MAX(ordinal_position) <> COUNT(*) LIMIT 1",
        "SELECT 1 FROM _mylite_catalog_index_columns GROUP BY index_id "
        "HAVING MIN(ordinal_position) <> 1 OR MAX(ordinal_position) <> COUNT(*) LIMIT 1",
        "SELECT 1 FROM _mylite_catalog_foreign_key_columns GROUP BY foreign_key_id "
        "HAVING MIN(ordinal_position) <> 1 OR MAX(ordinal_position) <> COUNT(*) LIMIT 1",
        "SELECT 1 FROM _mylite_catalog_check_constraints GROUP BY table_id "
        "HAVING MIN(ordinal_position) <> 1 OR MAX(ordinal_position) <> COUNT(*) LIMIT 1",
        "SELECT 1 FROM ("
        "SELECT descriptor_version AS dv, created_catalog_generation AS cg, "
        "updated_catalog_generation AS ug FROM _mylite_catalog_schemas UNION ALL "
        "SELECT descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_tables UNION ALL "
        "SELECT descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_views UNION ALL "
        "SELECT descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_columns UNION ALL "
        "SELECT descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_indexes UNION ALL "
        "SELECT descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_index_columns UNION ALL "
        "SELECT descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_foreign_keys UNION ALL "
        "SELECT descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_foreign_key_columns UNION ALL "
        "SELECT descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_check_constraints) AS d "
        "WHERE dv < 1 OR cg < 1 OR ug < cg OR ug > "
        "(SELECT catalog_generation FROM _mylite_catalog_state WHERE singleton_id = 1) LIMIT 1",
    };
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < MYLITE_ARRAY_COUNT(invalid_row_queries);
         ++index) {
        rc = validate_catalog_has_no_matching_row(sqlite, invalid_row_queries[index]);
    }
    return rc;
}

static int validate_catalog_has_no_matching_row(sqlite3 *sqlite, const char *sql) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_prepare_statement(sqlite, sql, &statement);

    if (rc == MYLITE_OK) {
        sqlite_rc = mylite_catalog_sqlite3_step(statement);
        if (sqlite_rc != SQLITE_DONE) {
            rc = sqlite_rc == SQLITE_ROW ? MYLITE_ERROR : mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    return mylite_catalog_finalize_statement(statement, rc);
}

static int validate_physical_schema(sqlite3 *sqlite) {
    static const char *const invalid_physical_queries[] = {
        "SELECT 1 FROM _mylite_catalog_tables AS t "
        "LEFT JOIN sqlite_schema AS s ON s.name = t.physical_name AND s.type = 'table' "
        "WHERE t.kind = 1 AND s.name IS NULL LIMIT 1",
        "SELECT 1 FROM _mylite_catalog_indexes AS i "
        "JOIN _mylite_catalog_tables AS t ON t.table_id = i.table_id "
        "LEFT JOIN sqlite_schema AS s ON s.name = i.physical_name AND s.type = 'index' "
        "AND s.tbl_name = t.physical_name "
        "WHERE i.kind IN (1, 2) AND s.name IS NULL LIMIT 1",
        "SELECT 1 FROM _mylite_catalog_columns AS c "
        "JOIN _mylite_catalog_tables AS t ON t.table_id = c.table_id "
        "WHERE t.kind = 1 AND NOT EXISTS "
        "(SELECT 1 FROM pragma_table_xinfo(t.physical_name) AS p WHERE p.name = c.name) LIMIT 1",
        "SELECT 1 FROM _mylite_catalog_tables AS t "
        "JOIN pragma_table_xinfo(t.physical_name) AS p "
        "WHERE t.kind = 1 AND NOT EXISTS "
        "(SELECT 1 FROM _mylite_catalog_columns AS c "
        "WHERE c.table_id = t.table_id AND c.name = p.name) LIMIT 1",
        "SELECT 1 FROM _mylite_catalog_columns AS c "
        "JOIN _mylite_catalog_tables AS t ON t.table_id = c.table_id "
        "JOIN pragma_table_xinfo(t.physical_name) AS p ON p.name = c.name "
        "WHERE t.kind = 1 AND (UPPER(TRIM(p.type)) <> UPPER(TRIM(c.physical_type)) "
        "OR (c.is_nullable = 1 AND p.\"notnull\" <> 0) "
        "OR p.hidden <> CASE WHEN c.is_generated = 0 THEN 0 "
        "WHEN c.generated_kind = 2 THEN 3 ELSE 2 END) LIMIT 1",
        "SELECT 1 FROM _mylite_catalog_columns AS c "
        "JOIN _mylite_catalog_tables AS t ON t.table_id = c.table_id "
        "JOIN sqlite_schema AS s ON s.name = t.physical_name AND s.type = 'table' "
        "WHERE t.kind = 1 AND c.is_generated = 1 AND INSTR("
        "REPLACE(REPLACE(REPLACE(REPLACE(LOWER(s.sql), ' ', ''), CHAR(9), ''), CHAR(10), ''), "
        "CHAR(13), ''), CHAR(34) || REPLACE(LOWER(c.name), CHAR(34), "
        "CHAR(34) || CHAR(34)) || CHAR(34) || "
        "LOWER(c.physical_type) || 'generatedalwaysas(' || "
        "REPLACE(REPLACE(REPLACE(REPLACE(LOWER(c.sqlite_generation_expression), ' ', ''), "
        "CHAR(9), ''), CHAR(10), ''), CHAR(13), '') || ')' || "
        "CASE c.generated_kind WHEN 2 THEN 'stored' ELSE 'virtual' END) = 0 LIMIT 1",
        "SELECT 1 FROM _mylite_catalog_check_constraints AS cc "
        "JOIN _mylite_catalog_tables AS t ON t.table_id = cc.table_id "
        "JOIN sqlite_schema AS s ON s.name = t.physical_name AND s.type = 'table' "
        "WHERE t.kind = 1 AND cc.is_enforced = 1 AND INSTR("
        "REPLACE(REPLACE(REPLACE(REPLACE(LOWER(s.sql), ' ', ''), CHAR(9), ''), CHAR(10), ''), "
        "CHAR(13), ''), 'constraint' || CHAR(34) || "
        "REPLACE(LOWER(cc.physical_name), CHAR(34), CHAR(34) || CHAR(34)) || "
        "CHAR(34) || 'check(' || "
        "REPLACE(REPLACE(REPLACE(REPLACE(LOWER(cc.sqlite_expression), ' ', ''), CHAR(9), ''), "
        "CHAR(10), ''), CHAR(13), '') || ')') = 0 LIMIT 1",
        "SELECT 1 FROM _mylite_catalog_indexes AS i "
        "JOIN _mylite_catalog_tables AS t ON t.table_id = i.table_id "
        "WHERE i.kind IN (1, 2) AND NOT EXISTS "
        "(SELECT 1 FROM pragma_index_list(t.physical_name) AS pil "
        "WHERE pil.name = i.physical_name AND pil.\"unique\" = i.is_unique) LIMIT 1",
        "SELECT 1 FROM _mylite_catalog_indexes AS i "
        "JOIN _mylite_catalog_index_columns AS ic ON ic.index_id = i.index_id "
        "JOIN _mylite_catalog_columns AS c ON c.column_id = ic.column_id "
        "LEFT JOIN pragma_index_xinfo(i.physical_name) AS pix "
        "ON pix.seqno = ic.ordinal_position - 1 AND pix.key = 1 "
        "WHERE i.kind IN (1, 2) AND (pix.seqno IS NULL "
        "OR (ic.prefix_length IS NULL AND pix.name <> c.name) "
        "OR (ic.prefix_length IS NOT NULL AND (pix.cid <> -2 OR pix.name IS NOT NULL)) "
        "OR pix.desc <> CASE ic.sort_direction WHEN 2 THEN 1 ELSE 0 END) LIMIT 1",
        "SELECT 1 FROM _mylite_catalog_indexes AS i WHERE i.kind IN (1, 2) AND "
        "(SELECT COUNT(*) FROM _mylite_catalog_index_columns AS ic "
        "WHERE ic.index_id = i.index_id) <> "
        "(SELECT COUNT(*) FROM pragma_index_xinfo(i.physical_name) AS pix WHERE pix.key = 1) "
        "LIMIT 1",
        "SELECT 1 FROM _mylite_catalog_indexes AS i "
        "JOIN _mylite_catalog_index_columns AS ic ON ic.index_id = i.index_id "
        "JOIN _mylite_catalog_columns AS c ON c.column_id = ic.column_id "
        "JOIN sqlite_schema AS s ON s.name = i.physical_name AND s.type = 'index' "
        "WHERE i.kind IN (1, 2) AND ic.prefix_length IS NOT NULL AND "
        "INSTR(REPLACE(LOWER(s.sql), ' ', ''), "
        "'substr(\"' || LOWER(c.name) || '\",1,' || CAST(ic.prefix_length AS TEXT) || ')') = 0 "
        "LIMIT 1",
    };
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < MYLITE_ARRAY_COUNT(invalid_physical_queries);
         ++index) {
        rc = validate_catalog_has_no_matching_row(sqlite, invalid_physical_queries[index]);
    }
    return rc;
}

#undef MYLITE_ARRAY_COUNT
