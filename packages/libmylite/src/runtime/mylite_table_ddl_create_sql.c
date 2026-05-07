#include "mylite_table_ddl_create_sql.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_span.h"
#include "mylite_table_ddl.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stdlib.h>

static char *build_create_physical_table_sql(
    mylite_db *database,
    const char *physical_name,
    const struct mylite_schema_default *schema_default,
    const struct mylite_create_table_plan *plan
);

static int create_physical_secondary_indexes(
    mylite_db *database,
    const char *physical_name,
    const struct mylite_create_table_plan *plan
);

static int create_physical_secondary_index(
    mylite_db *database,
    const char *physical_name,
    const struct mylite_create_table_index *index
);

static char *build_create_physical_index_sql(
    mylite_db *database,
    const char *physical_name,
    const struct mylite_create_table_index *index
);

static int append_physical_column_definition(
    sqlite3_str *sql,
    const struct mylite_create_table_column *column,
    const struct mylite_column_type_descriptor *descriptor,
    bool rowid_auto_increment
);

static void append_physical_column_default(
    sqlite3_str *sql,
    const struct mylite_create_table_column *column
);

static bool column_uses_rowid_auto_increment(
    const struct mylite_create_table_plan *plan,
    const struct mylite_create_table_column *column,
    const struct mylite_column_type_descriptor *descriptor
);

static bool index_is_single_column_primary_key(
    const struct mylite_create_table_index *index,
    const char *column_name
);

static bool physical_index_can_be_created(const struct mylite_create_table_index *index);

static bool index_has_prefix_part(const struct mylite_create_table_index *index);

static bool default_is_current_timestamp(const struct mylite_create_table_column *column);

static const char *sqlite_affinity_for_descriptor(
    const struct mylite_column_type_descriptor *descriptor
);

int mylite_table_ddl_create_physical_table(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_schema_default *schema_default,
    const struct mylite_create_table_plan *plan
) {
    char *physical_name = mylite_catalog_physical_table_name(schema_name, plan->table_name);
    char *sql = NULL;
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    if (physical_name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    sql = build_create_physical_table_sql(database, physical_name, schema_default, plan);
    if (sql == NULL) {
        free(physical_name);
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_exec(database->sqlite, sql, NULL, NULL, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        free(physical_name);
        return mylite_diagnostics_set_sqlite_error(database);
    }
    status = create_physical_secondary_indexes(database, physical_name, plan);
    free(physical_name);
    return status;
}

static char *build_create_physical_table_sql(
    mylite_db *database,
    const char *physical_name,
    const struct mylite_schema_default *schema_default,
    const struct mylite_create_table_plan *plan
) {
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);
    const char *temporary_keyword = "";

    if (sql == NULL) {
        return NULL;
    }
    if (plan->temporary) {
        temporary_keyword = "TEMPORARY ";
    }

    sqlite3_str_appendf(sql, "CREATE %sTABLE \"%w\"(", temporary_keyword, physical_name);
    for (size_t index = 0U; index < plan->column_count; ++index) {
        struct mylite_column_type_descriptor descriptor;
        int status = mylite_table_ddl_describe_create_table_column(
            &plan->columns[index],
            schema_default,
            &plan->options,
            &descriptor
        );

        if (status != MYLITE_OK) {
            sqlite3_free(sqlite3_str_finish(sql));
            return NULL;
        }
        if (index != 0U) {
            sqlite3_str_append(sql, ",", 1);
        }
        status = append_physical_column_definition(
            sql,
            &plan->columns[index],
            &descriptor,
            column_uses_rowid_auto_increment(plan, &plan->columns[index], &descriptor)
        );
        if (status != MYLITE_OK) {
            sqlite3_free(sqlite3_str_finish(sql));
            return NULL;
        }
    }
    sqlite3_str_append(sql, ")", 1);
    return sqlite3_str_finish(sql);
}

static int create_physical_secondary_indexes(
    mylite_db *database,
    const char *physical_name,
    const struct mylite_create_table_plan *plan
) {
    for (size_t index = 0U; index < plan->index_count; ++index) {
        int status =
            create_physical_secondary_index(database, physical_name, &plan->indexes[index]);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int create_physical_secondary_index(
    mylite_db *database,
    const char *physical_name,
    const struct mylite_create_table_index *index
) {
    char *sql = NULL;
    int rc = SQLITE_OK;

    if (!physical_index_can_be_created(index)) {
        return MYLITE_OK;
    }

    sql = build_create_physical_index_sql(database, physical_name, index);
    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_exec(database->sqlite, sql, NULL, NULL, NULL);
    sqlite3_free(sql);
    return rc == SQLITE_OK ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static char *build_create_physical_index_sql(
    mylite_db *database,
    const char *physical_name,
    const struct mylite_create_table_index *index
) {
    char *physical_index_name = NULL;
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);
    const char *unique_keyword = "";

    if (sql == NULL) {
        return NULL;
    }
    if (index->is_unique) {
        unique_keyword = "UNIQUE ";
    }

    physical_index_name = sqlite3_mprintf("%s__%s", physical_name, index->name);
    if (physical_index_name == NULL) {
        sqlite3_free(sqlite3_str_finish(sql));
        return NULL;
    }

    sqlite3_str_appendf(
        sql,
        "CREATE %sINDEX \"%w\" ON \"%w\"(",
        unique_keyword,
        physical_index_name,
        physical_name
    );
    sqlite3_free(physical_index_name);

    for (size_t part = 0U; part < index->part_count; ++part) {
        if (part != 0U) {
            sqlite3_str_append(sql, ",", 1);
        }
        sqlite3_str_appendf(sql, "\"%w\"", index->parts[part].column_name);
        if (index->parts[part].order == MYLITE_SQL_AST_KEY_PART_ORDER_DESC) {
            sqlite3_str_appendall(sql, " DESC");
        }
    }
    sqlite3_str_append(sql, ")", 1);
    return sqlite3_str_finish(sql);
}

static int append_physical_column_definition(
    sqlite3_str *sql,
    const struct mylite_create_table_column *column,
    const struct mylite_column_type_descriptor *descriptor,
    bool rowid_auto_increment
) {
    sqlite3_str_appendf(sql, "\"%w\" ", column->name);
    if (rowid_auto_increment) {
        sqlite3_str_appendall(sql, "INTEGER PRIMARY KEY AUTOINCREMENT");
        return MYLITE_OK;
    }

    sqlite3_str_appendall(sql, sqlite_affinity_for_descriptor(descriptor));
    if (descriptor->is_character_string && descriptor->collation_name != NULL) {
        sqlite3_str_appendf(sql, " COLLATE \"%w\"", descriptor->collation_name);
    }
    if (!column->nullable) {
        sqlite3_str_appendall(sql, " NOT NULL");
    }
    append_physical_column_default(sql, column);
    return MYLITE_OK;
}

static void append_physical_column_default(
    sqlite3_str *sql,
    const struct mylite_create_table_column *column
) {
    if (column->default_text == NULL) {
        return;
    }
    if (default_is_current_timestamp(column)) {
        sqlite3_str_appendall(sql, " DEFAULT CURRENT_TIMESTAMP");
        return;
    }
    if (column->has_generated_default) {
        return;
    }
    sqlite3_str_appendf(sql, " DEFAULT %Q", column->default_text);
}

static bool column_uses_rowid_auto_increment(
    const struct mylite_create_table_plan *plan,
    const struct mylite_create_table_column *column,
    const struct mylite_column_type_descriptor *descriptor
) {
    if (!column->auto_increment || descriptor->integer_type == MYLITE_COLUMN_INTEGER_NONE) {
        return false;
    }
    for (size_t index = 0U; index < plan->index_count; ++index) {
        if (index_is_single_column_primary_key(&plan->indexes[index], column->name)) {
            return true;
        }
    }
    return false;
}

static bool index_is_single_column_primary_key(
    const struct mylite_create_table_index *index,
    const char *column_name
) {
    if (!index->is_primary || index->part_count != 1U) {
        return false;
    }
    return mylite_ascii_case_equal(index->parts[0].column_name, column_name);
}

static bool physical_index_can_be_created(const struct mylite_create_table_index *index) {
    if (index->is_primary) {
        return false;
    }
    if (index->part_count == 0U || index->name == NULL) {
        return false;
    }
    if (index_has_prefix_part(index)) {
        return false;
    }
    return true;
}

static bool index_has_prefix_part(const struct mylite_create_table_index *index) {
    for (size_t part = 0U; part < index->part_count; ++part) {
        if (index->parts[part].has_prefix_length) {
            return true;
        }
    }
    return false;
}

static bool default_is_current_timestamp(const struct mylite_create_table_column *column) {
    if (!column->has_generated_default || column->default_text == NULL) {
        return false;
    }
    return mylite_ascii_case_equal(column->default_text, "CURRENT_TIMESTAMP");
}

static const char *sqlite_affinity_for_descriptor(
    const struct mylite_column_type_descriptor *descriptor
) {
    if (descriptor->integer_type != MYLITE_COLUMN_INTEGER_NONE || descriptor->is_boolean_alias) {
        return "INTEGER";
    }
    if (descriptor->is_bit) {
        return "INTEGER";
    }
    if (descriptor->is_enum || descriptor->is_set) {
        return "INTEGER";
    }
    if (descriptor->is_approximate_numeric) {
        return "REAL";
    }
    if (descriptor->is_exact_numeric) {
        return "NUMERIC";
    }
    if (descriptor->is_binary_string) {
        return "BLOB";
    }
    return "TEXT";
}
