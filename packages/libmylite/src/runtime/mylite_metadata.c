#include "mylite_metadata.h"

#include "sqlite3.h"

const struct mylite_result_column_metadata *mylite_result_metadata_column(const mylite_stmt *stmt,
                                                                          int column)
{
    if (stmt == NULL || column < 0 || stmt->result_metadata.columns == NULL ||
        (size_t)column >= stmt->result_metadata.column_count) {
        return NULL;
    }
    return &stmt->result_metadata.columns[column];
}

int mylite_column_count(const mylite_stmt *stmt)
{
    if (stmt == NULL) {
        return 0;
    }

    if (stmt->kind == MYLITE_STMT_SCALAR_SELECT) {
        return (int)stmt->scalar_result.value_count;
    }
    if (stmt->kind == MYLITE_STMT_TABLE_SELECT || stmt->kind == MYLITE_STMT_UNION_QUERY) {
        return (int)stmt->result_metadata.column_count;
    }
    if (stmt->sqlite_stmt == NULL) {
        return 0;
    }

    return sqlite3_column_count(stmt->sqlite_stmt);
}

const char *mylite_column_name(const mylite_stmt *stmt, int column)
{
    const struct mylite_result_column_metadata *metadata =
        mylite_result_metadata_column(stmt, column);

    if (metadata != NULL && metadata->name != NULL) {
        return metadata->name;
    }
    if (stmt != NULL && stmt->kind == MYLITE_STMT_SCALAR_SELECT && column >= 0 &&
        (size_t)column < stmt->result_metadata.column_count) {
        return stmt->result_metadata.columns[column].name;
    }
    if (stmt == NULL || stmt->sqlite_stmt == NULL || column < 0 ||
        column >= sqlite3_column_count(stmt->sqlite_stmt)) {
        return NULL;
    }

    return sqlite3_column_name(stmt->sqlite_stmt, column);
}

const char *mylite_column_schema_name(const mylite_stmt *stmt, int column)
{
    const struct mylite_result_column_metadata *metadata =
        mylite_result_metadata_column(stmt, column);

    return metadata == NULL ? NULL : metadata->schema_name;
}

const char *mylite_column_table_name(const mylite_stmt *stmt, int column)
{
    const struct mylite_result_column_metadata *metadata =
        mylite_result_metadata_column(stmt, column);

    return metadata == NULL ? NULL : metadata->table_name;
}

const char *mylite_column_origin_schema_name(const mylite_stmt *stmt, int column)
{
    const struct mylite_result_column_metadata *metadata =
        mylite_result_metadata_column(stmt, column);

    return metadata == NULL ? NULL : metadata->origin_schema_name;
}

const char *mylite_column_origin_table_name(const mylite_stmt *stmt, int column)
{
    const struct mylite_result_column_metadata *metadata =
        mylite_result_metadata_column(stmt, column);

    return metadata == NULL ? NULL : metadata->origin_table_name;
}

const char *mylite_column_origin_name(const mylite_stmt *stmt, int column)
{
    const struct mylite_result_column_metadata *metadata =
        mylite_result_metadata_column(stmt, column);

    return metadata == NULL ? NULL : metadata->origin_column_name;
}

int mylite_column_field_type(const mylite_stmt *stmt, int column)
{
    const struct mylite_result_column_metadata *metadata =
        mylite_result_metadata_column(stmt, column);

    return metadata == NULL ? MYLITE_FIELD_TYPE_INVALID : metadata->descriptor.type;
}

unsigned int mylite_column_flags(const mylite_stmt *stmt, int column)
{
    const struct mylite_result_column_metadata *metadata =
        mylite_result_metadata_column(stmt, column);

    return metadata == NULL ? 0U : metadata->descriptor.flags;
}

uint64_t mylite_column_declared_length(const mylite_stmt *stmt, int column)
{
    const struct mylite_result_column_metadata *metadata =
        mylite_result_metadata_column(stmt, column);

    return metadata == NULL ? 0U : metadata->descriptor.length;
}

uint64_t mylite_column_max_length(const mylite_stmt *stmt, int column)
{
    const struct mylite_result_column_metadata *metadata =
        mylite_result_metadata_column(stmt, column);

    return metadata == NULL ? 0U : metadata->descriptor.max_length;
}

unsigned int mylite_column_decimals(const mylite_stmt *stmt, int column)
{
    const struct mylite_result_column_metadata *metadata =
        mylite_result_metadata_column(stmt, column);

    return metadata == NULL ? 0U : metadata->descriptor.decimals;
}

unsigned int mylite_column_charset_id(const mylite_stmt *stmt, int column)
{
    const struct mylite_result_column_metadata *metadata =
        mylite_result_metadata_column(stmt, column);

    return metadata == NULL ? 0U : metadata->descriptor.charset_id;
}

int mylite_column_is_nullable(const mylite_stmt *stmt, int column)
{
    const struct mylite_result_column_metadata *metadata =
        mylite_result_metadata_column(stmt, column);

    if (metadata == NULL) {
        return 0;
    }
    if (metadata->descriptor.nullable) {
        return 1;
    }
    return 0;
}
