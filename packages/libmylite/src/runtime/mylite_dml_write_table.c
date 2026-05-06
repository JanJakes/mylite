#include "mylite_dml.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_span.h"
#include "sqlite3.h"
#include <mylite_fork/mylite_sqlite_fork.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct dml_write_table_load_context {
    mylite_db *database;
    struct mylite_insert_table *table;
};

struct write_table_integer_bounds {
    sqlite3_int64 minimum;
    sqlite3_int64 maximum;
};

static int load_insert_columns(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    struct mylite_insert_table *table
);

static int load_insert_column_from_catalog_row(
    void *context,
    const struct mylite_catalog_column_row *row
);

static int add_insert_table_column(
    struct mylite_insert_table *table,
    struct mylite_insert_table_column column
);

static int load_insert_unique_indexes(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    struct mylite_insert_table *table
);

static int load_insert_unique_index_part_from_catalog_row(
    void *context,
    const struct mylite_catalog_unique_index_part_row *row
);

static int add_insert_unique_index_part(
    mylite_db *database,
    struct mylite_insert_table *table,
    const struct mylite_catalog_unique_index_part_row *part
);

static int append_insert_unique_index_part(
    struct mylite_insert_unique_index *index,
    const struct mylite_insert_unique_index_part *part
);

static int configure_write_table_column_types(
    mylite_db *database,
    const struct mylite_insert_table *table
);

static bool write_table_column_fork_type(
    const struct mylite_insert_table_column *column,
    bool allow_zero_temporal,
    struct mylite_sqlite_fork_column_type *out_type
);

static bool write_table_column_signed_integer_bounds(
    const struct mylite_insert_table_column *column,
    struct write_table_integer_bounds *out_bounds
);

static bool write_table_column_unsigned_integer_maximum(
    const struct mylite_insert_table_column *column,
    sqlite3_int64 *out_maximum
);

static bool write_table_column_uses_double_type(const struct mylite_insert_table_column *column);

static bool write_table_column_uses_varchar_type(const struct mylite_insert_table_column *column);

static bool write_table_column_uses_binary_type(const struct mylite_insert_table_column *column);

static bool write_table_column_uses_varbinary_type(const struct mylite_insert_table_column *column);

static bool write_table_column_uses_text_family_type(
    const struct mylite_insert_table_column *column
);

static bool write_table_column_uses_blob_family_type(
    const struct mylite_insert_table_column *column
);

static bool write_table_column_uses_decimal_type(const struct mylite_insert_table_column *column);

static bool write_table_column_uses_date_type(const struct mylite_insert_table_column *column);

static bool write_table_column_uses_datetime_type(const struct mylite_insert_table_column *column);

static bool write_table_column_uses_time_type(const struct mylite_insert_table_column *column);

static int set_write_table_descriptor_error(
    mylite_db *database,
    const struct mylite_insert_table *table,
    const struct mylite_insert_table_column *column,
    int rc
);

static int initialize_insert_auto_increment(
    mylite_db *database,
    struct mylite_insert_table *table,
    const struct mylite_catalog_table_metadata *metadata
);

static int read_insert_auto_increment_max(
    mylite_db *database,
    const struct mylite_insert_table *table,
    uint64_t *out_next_auto_increment
);

static size_t insert_table_column_index(
    const struct mylite_insert_table *table,
    const char *column_name
);

static const sqlite3_int64 mylite_tinyint_signed_minimum = -128;
static const sqlite3_int64 mylite_tinyint_signed_maximum = 127;
static const sqlite3_int64 mylite_tinyint_unsigned_maximum = 255;
static const sqlite3_int64 mylite_smallint_signed_minimum = -32768;
static const sqlite3_int64 mylite_smallint_signed_maximum = 32767;
static const sqlite3_int64 mylite_smallint_unsigned_maximum = 65535;
static const sqlite3_int64 mylite_mediumint_signed_minimum = -8388608;
static const sqlite3_int64 mylite_mediumint_signed_maximum = 8388607;
static const sqlite3_int64 mylite_mediumint_unsigned_maximum = 16777215;
static const sqlite3_int64 mylite_int_signed_minimum = -2147483647 - 1;
static const sqlite3_int64 mylite_int_signed_maximum = 2147483647;
static const sqlite3_int64 mylite_int_unsigned_maximum = 4294967295LL;

int mylite_dml_load_write_table(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    unsigned int flags,
    struct mylite_insert_table *out_table
) {
    struct mylite_catalog_table_metadata metadata = {0};
    int status = MYLITE_OK;

    if (database == NULL || out_table == NULL) {
        return MYLITE_MISUSE;
    }

    *out_table = (struct mylite_insert_table){0};
    out_table->allow_zero_temporal = (flags & MYLITE_DML_WRITE_TABLE_ALLOW_ZERO_TEMPORAL) != 0U;
    status = mylite_catalog_load_table_metadata(database, schema_name, table_name, &metadata);
    if (status != MYLITE_OK) {
        return status;
    }

    out_table->physical_name = mylite_catalog_physical_table_name(schema_name, table_name);
    if (out_table->physical_name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = load_insert_columns(database, schema_name, table_name, out_table);
    if (status == MYLITE_OK) {
        status = load_insert_unique_indexes(database, schema_name, table_name, out_table);
    }
    if (status == MYLITE_OK) {
        status = configure_write_table_column_types(database, out_table);
    }
    if (status == MYLITE_OK) {
        status = initialize_insert_auto_increment(database, out_table, &metadata);
    }
    if (status != MYLITE_OK) {
        mylite_dml_insert_table_deinit(out_table);
    }
    return status;
}

static int load_insert_columns(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    struct mylite_insert_table *table
) {
    struct dml_write_table_load_context context = {
        .database = database,
        .table = table,
    };
    int status = mylite_catalog_load_table_columns(
        database,
        schema_name,
        table_name,
        load_insert_column_from_catalog_row,
        &context
    );

    if (status != MYLITE_OK) {
        return status;
    }
    if (table->column_count == 0U) {
        (void)mylite_diagnostics_set_error_message(database, "INSERT target table has no columns");
        return MYLITE_EXEC_ERROR;
    }
    return MYLITE_OK;
}

static int load_insert_column_from_catalog_row(
    void *context,
    const struct mylite_catalog_column_row *row
) {
    struct dml_write_table_load_context *load_context = context;
    struct mylite_insert_table_column column = {0};
    int status = MYLITE_OK;

    if (mylite_ascii_case_equal(row->is_nullable, "YES")) {
        column.nullable = true;
    }
    column.name = mylite_copy_span_text(row->name, row->name == NULL ? 0U : strlen(row->name));
    if (row->default_text != NULL) {
        column.default_text = mylite_copy_span_text(row->default_text, strlen(row->default_text));
    }
    column.data_type = mylite_copy_span_text(
        row->data_type == NULL ? "" : row->data_type,
        row->data_type == NULL ? 0U : strlen(row->data_type)
    );
    column.column_type = mylite_copy_span_text(
        row->column_type == NULL ? "" : row->column_type,
        row->column_type == NULL ? 0U : strlen(row->column_type)
    );
    if (row->collation_name != NULL) {
        column.collation_name =
            mylite_copy_span_text(row->collation_name, strlen(row->collation_name));
    }
    column.extra = mylite_copy_span_text(
        row->extra == NULL ? "" : row->extra,
        row->extra == NULL ? 0U : strlen(row->extra)
    );
    if (column.name == NULL || (row->default_text != NULL && column.default_text == NULL) ||
        column.data_type == NULL || column.column_type == NULL ||
        (row->collation_name != NULL && column.collation_name == NULL) || column.extra == NULL) {
        mylite_dml_insert_table_column_deinit(&column);
        (void)mylite_diagnostics_set_error_message(load_context->database, "out of memory");
        return MYLITE_NOMEM;
    }

    column.has_character_maximum_length = row->has_character_maximum_length;
    column.character_maximum_length = row->character_maximum_length;
    column.has_numeric_precision = row->has_numeric_precision;
    column.numeric_precision = row->numeric_precision;
    column.has_numeric_scale = row->has_numeric_scale;
    column.numeric_scale = row->numeric_scale;
    column.has_datetime_precision = row->has_datetime_precision;
    column.datetime_precision = row->datetime_precision;
    column.auto_increment = mylite_text_contains_word(column.extra, "auto_increment");
    column.generated_default = mylite_text_contains_word(column.extra, "DEFAULT_GENERATED");
    if (column.auto_increment) {
        load_context->table->has_auto_increment = true;
        load_context->table->auto_increment_column_index = load_context->table->column_count;
    }

    status = add_insert_table_column(load_context->table, column);
    if (status != MYLITE_OK) {
        mylite_dml_insert_table_column_deinit(&column);
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(load_context->database, "out of memory");
        }
    }
    return status;
}

static int add_insert_table_column(
    struct mylite_insert_table *table,
    struct mylite_insert_table_column column
) {
    struct mylite_insert_table_column *columns =
        realloc(table->columns, (table->column_count + 1U) * sizeof(*table->columns));

    if (columns == NULL) {
        return MYLITE_NOMEM;
    }

    table->columns = columns;
    table->columns[table->column_count++] = column;
    return MYLITE_OK;
}

static int load_insert_unique_indexes(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    struct mylite_insert_table *table
) {
    struct dml_write_table_load_context context = {
        .database = database,
        .table = table,
    };

    return mylite_catalog_load_unique_index_parts(
        database,
        schema_name,
        table_name,
        load_insert_unique_index_part_from_catalog_row,
        &context
    );
}

static int load_insert_unique_index_part_from_catalog_row(
    void *context,
    const struct mylite_catalog_unique_index_part_row *row
) {
    struct dml_write_table_load_context *load_context = context;

    return add_insert_unique_index_part(load_context->database, load_context->table, row);
}

static int add_insert_unique_index_part(
    mylite_db *database,
    struct mylite_insert_table *table,
    const struct mylite_catalog_unique_index_part_row *part
) {
    struct mylite_insert_unique_index *index = NULL;
    size_t column_index = insert_table_column_index(table, part->column_name);
    const struct mylite_insert_unique_index_part insert_part = {
        .column_index = column_index,
        .prefix_length = part->prefix_length,
        .has_prefix_length = part->has_prefix_length,
    };

    if (column_index == table->column_count) {
        (void)mylite_diagnostics_set_error_message_parts(
            database,
            "Index references unknown column '",
            part->column_name,
            "'"
        );
        return MYLITE_EXEC_ERROR;
    }

    for (size_t current = 0U; current < table->unique_index_count; ++current) {
        if (mylite_ascii_case_equal(table->unique_indexes[current].name, part->index_name)) {
            int status =
                append_insert_unique_index_part(&table->unique_indexes[current], &insert_part);

            if (status == MYLITE_NOMEM) {
                (void)mylite_diagnostics_set_error_message(database, "out of memory");
            }
            return status;
        }
    }

    index = realloc(
        table->unique_indexes,
        (table->unique_index_count + 1U) * sizeof(*table->unique_indexes)
    );
    if (index == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    table->unique_indexes = index;
    index = &table->unique_indexes[table->unique_index_count++];
    *index = (struct mylite_insert_unique_index){
        .is_primary = mylite_ascii_case_equal(part->index_name, "PRIMARY"),
    };
    index->name = mylite_copy_span_text(
        part->index_name,
        part->index_name == NULL ? 0U : strlen(part->index_name)
    );
    if (index->name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    int status = append_insert_unique_index_part(index, &insert_part);
    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
    }
    return status;
}

static int append_insert_unique_index_part(
    struct mylite_insert_unique_index *index,
    const struct mylite_insert_unique_index_part *part
) {
    size_t *column_indexes =
        realloc(index->column_indexes, (index->column_count + 1U) * sizeof(*index->column_indexes));
    uint64_t *prefix_lengths = NULL;
    uint64_t stored_prefix_length = 0U;

    if (column_indexes == NULL) {
        return MYLITE_NOMEM;
    }

    index->column_indexes = column_indexes;
    prefix_lengths =
        realloc(index->prefix_lengths, (index->column_count + 1U) * sizeof(*index->prefix_lengths));
    if (prefix_lengths == NULL) {
        return MYLITE_NOMEM;
    }
    index->prefix_lengths = prefix_lengths;
    if (part->has_prefix_length) {
        stored_prefix_length = part->prefix_length;
    }
    index->column_indexes[index->column_count++] = part->column_index;
    index->prefix_lengths[index->column_count - 1U] = stored_prefix_length;
    return MYLITE_OK;
}

static int configure_write_table_column_types(
    mylite_db *database,
    const struct mylite_insert_table *table
) {
    if (database == NULL || table == NULL) {
        return MYLITE_MISUSE;
    }

    for (size_t index = 0U; index < table->column_count; ++index) {
        struct mylite_sqlite_fork_column_type type = {0};
        const struct mylite_insert_table_column *column = &table->columns[index];
        int rc = SQLITE_OK;

        if (write_table_column_fork_type(column, table->allow_zero_temporal, &type)) {
            rc = mylite_sqlite_fork_set_column_type(
                database->sqlite,
                NULL,
                table->physical_name,
                column->name,
                &type
            );
        } else {
            rc = mylite_sqlite_fork_clear_column_type(
                database->sqlite,
                NULL,
                table->physical_name,
                column->name
            );
        }
        if (rc != SQLITE_OK) {
            return set_write_table_descriptor_error(database, table, column, rc);
        }
    }
    return MYLITE_OK;
}

static bool write_table_column_fork_type(
    const struct mylite_insert_table_column *column,
    bool allow_zero_temporal,
    struct mylite_sqlite_fork_column_type *out_type
) {
    struct write_table_integer_bounds signed_bounds = {0};
    sqlite3_int64 maximum = 0;

    if (column == NULL || out_type == NULL) {
        return false;
    }

    *out_type = (struct mylite_sqlite_fork_column_type){0};
    if (write_table_column_unsigned_integer_maximum(column, &maximum)) {
        out_type->kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_UNSIGNED_INTEGER;
        out_type->integer_maximum = maximum;
        return true;
    }
    if (write_table_column_signed_integer_bounds(column, &signed_bounds)) {
        out_type->kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_SIGNED_INTEGER;
        out_type->integer_minimum = signed_bounds.minimum;
        out_type->integer_maximum = signed_bounds.maximum;
        return true;
    }
    if (write_table_column_uses_double_type(column)) {
        out_type->kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_DOUBLE;
        return true;
    }
    if (write_table_column_uses_varchar_type(column)) {
        out_type->kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_VARCHAR;
        out_type->character_maximum_length = column->character_maximum_length;
        return true;
    }
    if (write_table_column_uses_binary_type(column)) {
        out_type->kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_BINARY;
        out_type->byte_maximum_length = column->character_maximum_length;
        return true;
    }
    if (write_table_column_uses_varbinary_type(column)) {
        out_type->kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_VARBINARY;
        out_type->byte_maximum_length = column->character_maximum_length;
        return true;
    }
    if (write_table_column_uses_text_family_type(column)) {
        out_type->kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_TEXT;
        out_type->byte_maximum_length = column->character_maximum_length;
        return true;
    }
    if (write_table_column_uses_blob_family_type(column)) {
        out_type->kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_BLOB;
        out_type->byte_maximum_length = column->character_maximum_length;
        return true;
    }
    if (write_table_column_uses_decimal_type(column)) {
        out_type->kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_DECIMAL;
        out_type->numeric_precision = column->numeric_precision;
        out_type->numeric_scale = column->numeric_scale;
        if (mylite_text_contains_word(column->column_type, "unsigned")) {
            out_type->flags |= MYLITE_SQLITE_FORK_COLUMN_TYPE_UNSIGNED;
        }
        return true;
    }
    if (write_table_column_uses_date_type(column)) {
        out_type->kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_DATE;
        if (allow_zero_temporal) {
            out_type->flags |= MYLITE_SQLITE_FORK_COLUMN_TYPE_ALLOW_ZERO_TEMPORAL;
        }
        return true;
    }
    if (write_table_column_uses_datetime_type(column)) {
        out_type->kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_DATETIME;
        out_type->datetime_precision = column->datetime_precision;
        if (allow_zero_temporal) {
            out_type->flags |= MYLITE_SQLITE_FORK_COLUMN_TYPE_ALLOW_ZERO_TEMPORAL;
        }
        return true;
    }
    if (write_table_column_uses_time_type(column)) {
        out_type->kind = MYLITE_SQLITE_FORK_COLUMN_TYPE_TIME;
        out_type->datetime_precision = column->datetime_precision;
        return true;
    }
    return false;
}

static bool write_table_column_signed_integer_bounds(
    const struct mylite_insert_table_column *column,
    struct write_table_integer_bounds *out_bounds
) {
    if (column == NULL || out_bounds == NULL ||
        mylite_text_contains_word(column->column_type, "unsigned")) {
        return false;
    }
    if (mylite_ascii_case_equal(column->data_type, "tinyint")) {
        out_bounds->minimum = mylite_tinyint_signed_minimum;
        out_bounds->maximum = mylite_tinyint_signed_maximum;
        return true;
    }
    if (mylite_ascii_case_equal(column->data_type, "smallint")) {
        out_bounds->minimum = mylite_smallint_signed_minimum;
        out_bounds->maximum = mylite_smallint_signed_maximum;
        return true;
    }
    if (mylite_ascii_case_equal(column->data_type, "mediumint")) {
        out_bounds->minimum = mylite_mediumint_signed_minimum;
        out_bounds->maximum = mylite_mediumint_signed_maximum;
        return true;
    }
    if (mylite_ascii_case_equal(column->data_type, "int")) {
        out_bounds->minimum = mylite_int_signed_minimum;
        out_bounds->maximum = mylite_int_signed_maximum;
        return true;
    }
    if (mylite_ascii_case_equal(column->data_type, "bigint")) {
        out_bounds->minimum = INT64_MIN;
        out_bounds->maximum = INT64_MAX;
        return true;
    }
    return false;
}

static bool write_table_column_unsigned_integer_maximum(
    const struct mylite_insert_table_column *column,
    sqlite3_int64 *out_maximum
) {
    if (column == NULL || !mylite_text_contains_word(column->column_type, "unsigned")) {
        return false;
    }
    if (mylite_ascii_case_equal(column->data_type, "tinyint")) {
        *out_maximum = mylite_tinyint_unsigned_maximum;
        return true;
    }
    if (mylite_ascii_case_equal(column->data_type, "smallint")) {
        *out_maximum = mylite_smallint_unsigned_maximum;
        return true;
    }
    if (mylite_ascii_case_equal(column->data_type, "mediumint")) {
        *out_maximum = mylite_mediumint_unsigned_maximum;
        return true;
    }
    if (mylite_ascii_case_equal(column->data_type, "int")) {
        *out_maximum = mylite_int_unsigned_maximum;
        return true;
    }
    if (mylite_ascii_case_equal(column->data_type, "bigint")) {
        *out_maximum = INT64_MAX;
        return true;
    }
    return false;
}

static bool write_table_column_uses_double_type(const struct mylite_insert_table_column *column) {
    if (column == NULL) {
        return false;
    }
    return (mylite_ascii_case_equal(column->data_type, "float") ||
            mylite_ascii_case_equal(column->data_type, "double")) != 0;
}

static bool write_table_column_uses_varchar_type(const struct mylite_insert_table_column *column) {
    if (column == NULL || !column->has_character_maximum_length) {
        return false;
    }
    return (mylite_ascii_case_equal(column->data_type, "char") ||
            mylite_ascii_case_equal(column->data_type, "varchar")) != 0;
}

static bool write_table_column_uses_binary_type(const struct mylite_insert_table_column *column) {
    if (column == NULL || !column->has_character_maximum_length) {
        return false;
    }
    return mylite_ascii_case_equal(column->data_type, "binary");
}

static bool write_table_column_uses_varbinary_type(
    const struct mylite_insert_table_column *column
) {
    if (column == NULL || !column->has_character_maximum_length) {
        return false;
    }
    return mylite_ascii_case_equal(column->data_type, "varbinary");
}

static bool write_table_column_uses_text_family_type(
    const struct mylite_insert_table_column *column
) {
    if (column == NULL || !column->has_character_maximum_length) {
        return false;
    }
    return (mylite_ascii_case_equal(column->data_type, "tinytext") ||
            mylite_ascii_case_equal(column->data_type, "text") ||
            mylite_ascii_case_equal(column->data_type, "mediumtext") ||
            mylite_ascii_case_equal(column->data_type, "longtext")) != 0;
}

static bool write_table_column_uses_blob_family_type(
    const struct mylite_insert_table_column *column
) {
    if (column == NULL || !column->has_character_maximum_length) {
        return false;
    }
    return (mylite_ascii_case_equal(column->data_type, "tinyblob") ||
            mylite_ascii_case_equal(column->data_type, "blob") ||
            mylite_ascii_case_equal(column->data_type, "mediumblob") ||
            mylite_ascii_case_equal(column->data_type, "longblob")) != 0;
}

static bool write_table_column_uses_decimal_type(const struct mylite_insert_table_column *column) {
    if (column == NULL || !column->has_numeric_precision || !column->has_numeric_scale) {
        return false;
    }
    return mylite_ascii_case_equal(column->data_type, "decimal");
}

static bool write_table_column_uses_date_type(const struct mylite_insert_table_column *column) {
    if (column == NULL) {
        return false;
    }
    return mylite_ascii_case_equal(column->data_type, "date");
}

static bool write_table_column_uses_datetime_type(const struct mylite_insert_table_column *column) {
    if (column == NULL || !column->has_datetime_precision) {
        return false;
    }
    return mylite_ascii_case_equal(column->data_type, "datetime");
}

static bool write_table_column_uses_time_type(const struct mylite_insert_table_column *column) {
    if (column == NULL || !column->has_datetime_precision) {
        return false;
    }
    return mylite_ascii_case_equal(column->data_type, "time");
}

static int set_write_table_descriptor_error(
    mylite_db *database,
    const struct mylite_insert_table *table,
    const struct mylite_insert_table_column *column,
    int rc
) {
    char *message = sqlite3_mprintf(
        "failed to configure SQLite fork column descriptor for '%q.%q' (rc=%d)",
        table == NULL ? "" : table->physical_name,
        column == NULL ? "" : column->name,
        rc
    );
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_set_error_message(database, message);
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_SQLITE_ERROR;
}

static int initialize_insert_auto_increment(
    mylite_db *database,
    struct mylite_insert_table *table,
    const struct mylite_catalog_table_metadata *metadata
) {
    uint64_t max_next_auto_increment = 1U;
    int status = MYLITE_OK;

    if (!table->has_auto_increment) {
        return MYLITE_OK;
    }

    status = read_insert_auto_increment_max(database, table, &max_next_auto_increment);
    if (status != MYLITE_OK) {
        return status;
    }
    table->next_auto_increment = max_next_auto_increment;
    if (metadata->has_auto_increment && metadata->auto_increment > table->next_auto_increment) {
        table->next_auto_increment = metadata->auto_increment;
    }
    if (table->next_auto_increment == 0U) {
        table->next_auto_increment = 1U;
    }
    return MYLITE_OK;
}

static int read_insert_auto_increment_max(
    mylite_db *database,
    const struct mylite_insert_table *table,
    uint64_t *out_next_auto_increment
) {
    sqlite3_stmt *select = NULL;
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);
    char *select_sql = NULL;
    int rc = SQLITE_OK;

    *out_next_auto_increment = 1U;
    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    sqlite3_str_appendf(
        sql,
        "SELECT max(\"%w\") FROM \"%w\"",
        table->columns[table->auto_increment_column_index].name,
        table->physical_name
    );
    select_sql = sqlite3_str_finish(sql);
    if (select_sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_prepare_v3(
        database->sqlite,
        select_sql,
        -1,
        SQLITE_PREPARE_PERSISTENT,
        &select,
        NULL
    );
    sqlite3_free(select_sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    rc = sqlite3_step(select);
    if (rc == SQLITE_ROW && sqlite3_column_type(select, 0) != SQLITE_NULL) {
        sqlite3_int64 max_value = sqlite3_column_int64(select, 0);

        if (max_value >= 0) {
            *out_next_auto_increment = (uint64_t)max_value + 1U;
        }
    }
    sqlite3_finalize(select);
    return rc == SQLITE_ROW ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static size_t insert_table_column_index(
    const struct mylite_insert_table *table,
    const char *column_name
) {
    for (size_t index = 0U; index < table->column_count; ++index) {
        if (mylite_ascii_case_equal(table->columns[index].name, column_name)) {
            return index;
        }
    }
    return table->column_count;
}
