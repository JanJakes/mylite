#include "mylite_sqlite_value.h"

#include "mylite_span.h"

int mylite_sqlite_copy_column_value(
    sqlite3_stmt *sqlite_stmt,
    size_t column_index,
    struct mylite_expression_value *out_value
) {
    int sqlite_type = SQLITE_NULL;

    if (sqlite_stmt == NULL) {
        return -1;
    }

    sqlite_type = sqlite3_column_type(sqlite_stmt, (int)column_index);
    switch (sqlite_type) {
    case SQLITE_NULL:
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return 0;
    case SQLITE_INTEGER:
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_INT64,
            .int64_value = sqlite3_column_int64(sqlite_stmt, (int)column_index)
        };
        return 0;
    case SQLITE_FLOAT:
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_REAL,
            .real_value = sqlite3_column_double(sqlite_stmt, (int)column_index)
        };
        return 0;
    case SQLITE_TEXT:
    case SQLITE_BLOB: {
        const unsigned char *text = sqlite3_column_text(sqlite_stmt, (int)column_index);
        int bytes = sqlite3_column_bytes(sqlite_stmt, (int)column_index);

        out_value->kind = MYLITE_EXPRESSION_VALUE_TEXT;
        out_value->preserve_temporal_fraction_digits = false;
        out_value->text_length = bytes < 0 ? 0U : (size_t)bytes;
        out_value->text_value = mylite_copy_span_text((const char *)text, out_value->text_length);
        return out_value->text_value == NULL ? -1 : 0;
    }
    default:
        break;
    }
    return -1;
}
