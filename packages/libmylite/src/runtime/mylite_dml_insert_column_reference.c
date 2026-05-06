#include "mylite_dml_insert_column_reference.h"

#include "mylite_dml_types.h"
#include "mylite_span.h"

static bool insert_column_reference_qualifiers_match(
    const struct mylite_insert_column_reference *reference,
    const char *schema_name,
    const char *table_name
);

size_t mylite_dml_insert_table_column_index(
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

size_t mylite_dml_insert_table_column_reference_index(
    const struct mylite_insert_table *table,
    const char *schema_name,
    const char *table_name,
    const struct mylite_insert_column_reference *reference
) {
    if (!insert_column_reference_qualifiers_match(reference, schema_name, table_name)) {
        return table->column_count;
    }
    return mylite_dml_insert_table_column_index(table, reference->column_name);
}

static bool insert_column_reference_qualifiers_match(
    const struct mylite_insert_column_reference *reference,
    const char *schema_name,
    const char *table_name
) {
    if (reference->schema_name != NULL &&
        !mylite_ascii_case_equal(reference->schema_name, schema_name)) {
        return false;
    }
    if (reference->table_name != NULL &&
        !mylite_ascii_case_equal(reference->table_name, table_name)) {
        return false;
    }
    return true;
}
