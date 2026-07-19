#include <mylite/mylite.h>

#include "mylite_ast.h"
#include "mylite_catalog.h"
#include "mylite_collation.h"
#include "mylite_connection.h"
#include "mylite_date_interval_second.h"
#include "mylite_diagnostics.h"
#include "mylite_dynamic_string.h"
#include "mylite_execution_catalog.h"
#include "mylite_execution_ddl_sql_lowering.h"
#include "mylite_execution_loaded_catalog.h"
#include "mylite_execution_plan_types.h"
#include "mylite_execution_result_rows.h"
#include "mylite_execution_scalar.h"
#include "mylite_execution_scalar_regexp.h"
#include "mylite_execution_scalar_string_position.h"
#include "mylite_execution_sql_normalization.h"
#include "mylite_execution_sqlite_internal.h"
#include "mylite_execution_statement_transaction.h"
#include "mylite_execution_value.h"
#include "mylite_json.h"
#include "mylite_mysql_error_codes.h"
#include "mylite_result_metadata.h"
#include "mylite_spatial.h"
#include "mylite_statement_completion.h"
#include "mylite_statement_context.h"
#include "mylite_string_bitmask.h"
#include "mylite_string_padding.h"
#include "mylite_string_search.h"
#include "mylite_string_substring_index.h"
#include "mylite_sys_functions.h"
#include "sqlite3.h"

#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mylite_execution_ddl_sql_lowering_support.h"
#include "mylite_execution_declarations_00_constants.inc"
#include "mylite_execution_declarations_01_types.inc"

static int integer_range_for_logical_type(
    struct mylite_db *database,
    struct integer_logical_type_range_request request,
    struct integer_column_range *out_range
);
static int append_numbered_parameter(struct mylite_dynamic_string *string, size_t parameter_index);
static int append_size_literal(struct mylite_dynamic_string *string, size_t value);
static int append_uint64_literal(struct mylite_dynamic_string *string, uint64_t value);
static bool planned_secondary_index_is_fulltext(const struct planned_secondary_index *index);
static bool planned_secondary_index_is_spatial(const struct planned_secondary_index *index);
static bool planned_column_is_char_or_varchar(const struct planned_column *column);
static bool planned_column_is_string_family(const struct planned_column *column);
static bool column_descriptor_is_string_family(const struct mylite_catalog_column_descriptor *column
);
static bool column_descriptor_is_char_or_varchar(
    const struct mylite_catalog_column_descriptor *column
);
static bool text_equals_ascii_case_insensitive(const char *left, const char *right);
static bool loaded_index_part_requires_string_key_validation(const struct loaded_index_part *part);
static int append_alter_table_add_column_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_column *plan
);

#include "mylite_execution_ddl_sql_lowering_declarations.inc"

#include "mylite_execution_sql_builder_alter_order_force_rename_truncate.inc"
#include "mylite_execution_sql_builder_create_table_index_helpers.inc"
#include "mylite_execution_sql_builder_drop_alter_add_column_index.inc"

static int integer_range_for_logical_type(
    struct mylite_db *database,
    struct integer_logical_type_range_request request,
    struct integer_column_range *out_range
) {
    return mylite_execution_ddl_integer_range_for_logical_type(
        database,
        request.logical_type,
        request.unsupported_message,
        out_range
    );
}

static int append_numbered_parameter(struct mylite_dynamic_string *string, size_t parameter_index) {
    return mylite_execution_ddl_append_numbered_parameter(string, parameter_index);
}

static int append_size_literal(struct mylite_dynamic_string *string, size_t value) {
    return mylite_execution_ddl_append_size_literal(string, value);
}

static int append_uint64_literal(struct mylite_dynamic_string *string, uint64_t value) {
    return mylite_execution_ddl_append_uint64_literal(string, value);
}

static bool planned_secondary_index_is_fulltext(const struct planned_secondary_index *index) {
    return mylite_execution_ddl_planned_secondary_index_is_fulltext(index);
}

static bool planned_secondary_index_is_spatial(const struct planned_secondary_index *index) {
    return mylite_execution_ddl_planned_secondary_index_is_spatial(index);
}

static bool planned_column_is_char_or_varchar(const struct planned_column *column) {
    return mylite_execution_ddl_planned_column_is_char_or_varchar(column);
}

static bool planned_column_is_string_family(const struct planned_column *column) {
    return mylite_execution_ddl_planned_column_is_string_family(column);
}

static bool column_descriptor_is_string_family(const struct mylite_catalog_column_descriptor *column
) {
    return mylite_execution_ddl_column_descriptor_is_string_family(column);
}

static bool column_descriptor_is_char_or_varchar(
    const struct mylite_catalog_column_descriptor *column
) {
    return mylite_execution_ddl_column_descriptor_is_char_or_varchar(column);
}

static bool text_equals_ascii_case_insensitive(const char *left, const char *right) {
    return mylite_execution_ddl_text_equals_ascii_case_insensitive(left, right);
}

static bool loaded_index_part_requires_string_key_validation(const struct loaded_index_part *part) {
    return mylite_execution_ddl_loaded_index_part_requires_string_key_validation(part);
}

static int append_alter_table_add_column_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_column *plan
) {
    return mylite_execution_ddl_append_alter_table_add_column_default(database, string, plan);
}
