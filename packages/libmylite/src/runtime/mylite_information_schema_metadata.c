#include "mylite_information_schema_metadata.h"

#include "mylite_diagnostics.h"
#include "mylite_metadata.h"
#include "mylite_metadata_constants.h"
#include "mylite_runtime.h"
#include "mylite_span.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stdlib.h>

static const uint64_t information_schema_identifier_length = 64U;
static const uint64_t information_schema_table_type_length = 11U;
static const uint64_t information_schema_row_format_length = 10U;
static const uint64_t information_schema_create_options_length = 256U;
static const uint64_t information_schema_table_comment_length = 2048U;
static const uint64_t information_schema_temporal_length = 19U;
static const uint64_t information_schema_version_length = 3U;
static const uint64_t information_schema_tables_bigint_length = 21U;
static const uint64_t information_schema_constraint_type_length = 11U;
static const uint64_t information_schema_enforced_length = 3U;
static const uint64_t information_schema_match_option_length = 7U;
static const uint64_t information_schema_rule_length = 11U;
static const uint64_t information_schema_unsigned_int_length = 10U;
static const uint64_t information_schema_engine_support_length = 8U;
static const uint64_t information_schema_engine_comment_length = 80U;
static const uint64_t information_schema_yes_no_length = 3U;
static const uint64_t information_schema_collation_id_length = 20U;
static const uint64_t information_schema_pad_attribute_length = 9U;
static const uint64_t information_schema_keyword_word_length = 128U;
static const uint64_t information_schema_keyword_reserved_length = 11U;

static const char *information_schema_table_name(enum mylite_information_schema_table table);

static const char *information_schema_column_origin_schema(
    enum mylite_information_schema_table table,
    const char *name
);

static struct mylite_field_descriptor information_schema_constraint_column_descriptor(
    enum mylite_information_schema_table table,
    const char *name
);

static struct mylite_field_descriptor information_schema_tables_column_descriptor(const char *name);

static struct mylite_field_descriptor information_schema_table_constraints_column_descriptor(
    const char *name
);

static struct mylite_field_descriptor information_schema_key_column_usage_column_descriptor(
    const char *name
);

static struct mylite_field_descriptor information_schema_check_constraints_column_descriptor(
    const char *name
);

static struct mylite_field_descriptor information_schema_referential_constraints_column_descriptor(
    const char *name
);

static struct mylite_field_descriptor information_schema_engines_column_descriptor(
    const char *name
);

static struct mylite_field_descriptor information_schema_character_sets_column_descriptor(
    const char *name
);

static struct mylite_field_descriptor information_schema_collations_column_descriptor(
    const char *name
);

static struct mylite_field_descriptor information_schema_keywords_column_descriptor(
    const char *name
);

static struct mylite_field_descriptor information_schema_text_descriptor(
    uint64_t length,
    unsigned int flags,
    unsigned int decimals,
    bool nullable
);

static struct mylite_field_descriptor information_schema_enum_descriptor(uint64_t length);

static struct mylite_field_descriptor information_schema_integer_descriptor(
    unsigned int flags,
    bool nullable
);

static struct mylite_field_descriptor information_schema_bigint_descriptor(
    uint64_t length,
    unsigned int flags,
    bool nullable
);

static struct mylite_field_descriptor information_schema_temporal_descriptor(
    int type,
    unsigned int flags,
    bool nullable
);

static struct mylite_field_descriptor information_schema_generic_column_descriptor(
    const char *name
);

static int information_schema_column_is_integer(const char *name);

static int information_schema_column_is_not_null_text(const char *name);

static bool information_schema_tables_column_has_empty_origin_schema(const char *name);

int mylite_information_schema_attach_result_metadata(
    mylite_db *database,
    enum mylite_information_schema_table table,
    mylite_stmt *stmt
) {
    const char *table_name = information_schema_table_name(table);
    struct mylite_result_metadata metadata = {0};
    int column_count = 0;

    if (database == NULL || stmt == NULL || stmt->sqlite_stmt == NULL || table_name == NULL) {
        return MYLITE_MISUSE;
    }

    column_count = sqlite3_column_count(stmt->sqlite_stmt);
    metadata.columns = calloc((size_t)column_count, sizeof(*metadata.columns));
    if (metadata.columns == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    metadata.column_count = (size_t)column_count;

    for (int index = 0; index < column_count; ++index) {
        const char *column_name = sqlite3_column_name(stmt->sqlite_stmt, index);
        const char *origin_schema = information_schema_column_origin_schema(table, column_name);
        struct mylite_result_column_metadata *column = &metadata.columns[index];
        int status = mylite_result_metadata_copy_text(database, &column->name, column_name);

        if (status == MYLITE_OK) {
            status =
                mylite_result_metadata_copy_text(database, &column->schema_name, origin_schema);
        }
        if (status == MYLITE_OK) {
            status = mylite_result_metadata_copy_text(database, &column->table_name, table_name);
        }
        if (status == MYLITE_OK) {
            status = mylite_result_metadata_copy_text(
                database,
                &column->origin_schema_name,
                origin_schema
            );
        }
        if (status == MYLITE_OK) {
            status =
                mylite_result_metadata_copy_text(database, &column->origin_table_name, table_name);
        }
        if (status == MYLITE_OK) {
            status = mylite_result_metadata_copy_text(
                database,
                &column->origin_column_name,
                column_name
            );
        }
        if (status != MYLITE_OK) {
            mylite_result_metadata_deinit(&metadata);
            return status;
        }
        column->descriptor = mylite_information_schema_column_descriptor(table, column_name);
    }

    mylite_result_metadata_deinit(&stmt->result_metadata);
    stmt->result_metadata = metadata;
    return MYLITE_OK;
}

struct mylite_field_descriptor mylite_information_schema_column_descriptor(
    enum mylite_information_schema_table table,
    const char *name
) {
    struct mylite_field_descriptor descriptor =
        information_schema_constraint_column_descriptor(table, name);

    if (descriptor.type != MYLITE_FIELD_TYPE_INVALID) {
        return descriptor;
    }

    return information_schema_generic_column_descriptor(name);
}

static const char *information_schema_table_name(enum mylite_information_schema_table table) {
    switch (table) {
    case MYLITE_INFORMATION_SCHEMA_SCHEMATA:
        return "SCHEMATA";
    case MYLITE_INFORMATION_SCHEMA_TABLES:
        return "TABLES";
    case MYLITE_INFORMATION_SCHEMA_COLUMNS:
        return "COLUMNS";
    case MYLITE_INFORMATION_SCHEMA_STATISTICS:
        return "STATISTICS";
    case MYLITE_INFORMATION_SCHEMA_ENGINES:
        return "ENGINES";
    case MYLITE_INFORMATION_SCHEMA_CHARACTER_SETS:
        return "CHARACTER_SETS";
    case MYLITE_INFORMATION_SCHEMA_COLLATIONS:
        return "COLLATIONS";
    case MYLITE_INFORMATION_SCHEMA_COLLATION_CHARACTER_SET_APPLICABILITY:
        return "COLLATION_CHARACTER_SET_APPLICABILITY";
    case MYLITE_INFORMATION_SCHEMA_KEYWORDS:
        return "KEYWORDS";
    case MYLITE_INFORMATION_SCHEMA_TABLE_CONSTRAINTS:
        return "TABLE_CONSTRAINTS";
    case MYLITE_INFORMATION_SCHEMA_KEY_COLUMN_USAGE:
        return "KEY_COLUMN_USAGE";
    case MYLITE_INFORMATION_SCHEMA_CHECK_CONSTRAINTS:
        return "CHECK_CONSTRAINTS";
    case MYLITE_INFORMATION_SCHEMA_REFERENTIAL_CONSTRAINTS:
        return "REFERENTIAL_CONSTRAINTS";
    case MYLITE_INFORMATION_SCHEMA_NONE:
        return NULL;
    }
    return NULL;
}

static const char *information_schema_column_origin_schema(
    enum mylite_information_schema_table table,
    const char *name
) {
    if (table == MYLITE_INFORMATION_SCHEMA_TABLES &&
        information_schema_tables_column_has_empty_origin_schema(name)) {
        return "";
    }
    if (table == MYLITE_INFORMATION_SCHEMA_KEY_COLUMN_USAGE &&
        mylite_ascii_case_equal(name, "COLUMN_NAME")) {
        return "";
    }
    if (table == MYLITE_INFORMATION_SCHEMA_REFERENTIAL_CONSTRAINTS &&
        mylite_ascii_case_equal(name, "CONSTRAINT_NAME")) {
        return "";
    }
    return "information_schema";
}

static struct mylite_field_descriptor information_schema_constraint_column_descriptor(
    enum mylite_information_schema_table table,
    const char *name
) {
    switch (table) {
    case MYLITE_INFORMATION_SCHEMA_TABLES:
        return information_schema_tables_column_descriptor(name);
    case MYLITE_INFORMATION_SCHEMA_TABLE_CONSTRAINTS:
        return information_schema_table_constraints_column_descriptor(name);
    case MYLITE_INFORMATION_SCHEMA_KEY_COLUMN_USAGE:
        return information_schema_key_column_usage_column_descriptor(name);
    case MYLITE_INFORMATION_SCHEMA_CHECK_CONSTRAINTS:
        return information_schema_check_constraints_column_descriptor(name);
    case MYLITE_INFORMATION_SCHEMA_REFERENTIAL_CONSTRAINTS:
        return information_schema_referential_constraints_column_descriptor(name);
    case MYLITE_INFORMATION_SCHEMA_ENGINES:
        return information_schema_engines_column_descriptor(name);
    case MYLITE_INFORMATION_SCHEMA_CHARACTER_SETS:
        return information_schema_character_sets_column_descriptor(name);
    case MYLITE_INFORMATION_SCHEMA_COLLATIONS:
        return information_schema_collations_column_descriptor(name);
    case MYLITE_INFORMATION_SCHEMA_KEYWORDS:
        return information_schema_keywords_column_descriptor(name);
    case MYLITE_INFORMATION_SCHEMA_SCHEMATA:
    case MYLITE_INFORMATION_SCHEMA_COLUMNS:
    case MYLITE_INFORMATION_SCHEMA_STATISTICS:
    case MYLITE_INFORMATION_SCHEMA_COLLATION_CHARACTER_SET_APPLICABILITY:
    case MYLITE_INFORMATION_SCHEMA_NONE:
        break;
    }
    return (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_INVALID,
    };
}

static struct mylite_field_descriptor information_schema_tables_column_descriptor(
    const char *name
) {
    const unsigned int not_null_key_flags = MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY |
                                            MYLITE_FIELD_FLAG_NO_DEFAULT_VALUE |
                                            MYLITE_FIELD_FLAG_PART_KEY;

    if (mylite_ascii_case_equal(name, "TABLE_CATALOG")) {
        return information_schema_text_descriptor(
            information_schema_identifier_length,
            not_null_key_flags | MYLITE_FIELD_FLAG_UNIQUE_KEY,
            0U,
            false
        );
    }
    if (mylite_ascii_case_equal(name, "TABLE_SCHEMA") ||
        mylite_ascii_case_equal(name, "TABLE_NAME")) {
        return information_schema_text_descriptor(
            information_schema_identifier_length,
            not_null_key_flags,
            0U,
            false
        );
    }
    if (mylite_ascii_case_equal(name, "TABLE_TYPE")) {
        struct mylite_field_descriptor descriptor = information_schema_text_descriptor(
            information_schema_table_type_length,
            not_null_key_flags | MYLITE_FIELD_FLAG_MULTIPLE_KEY | MYLITE_FIELD_FLAG_ENUM,
            0U,
            false
        );

        descriptor.type = MYLITE_FIELD_TYPE_STRING;
        return descriptor;
    }
    if (mylite_ascii_case_equal(name, "ENGINE")) {
        return information_schema_text_descriptor(
            information_schema_identifier_length,
            0U,
            mylite_mysql_not_fixed_decimals,
            true
        );
    }
    if (mylite_ascii_case_equal(name, "VERSION")) {
        return information_schema_bigint_descriptor(
            information_schema_version_length,
            MYLITE_FIELD_FLAG_BINARY,
            true
        );
    }
    if (mylite_ascii_case_equal(name, "ROW_FORMAT")) {
        struct mylite_field_descriptor descriptor = information_schema_text_descriptor(
            information_schema_row_format_length,
            MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_ENUM,
            0U,
            true
        );

        descriptor.type = MYLITE_FIELD_TYPE_STRING;
        return descriptor;
    }
    if (mylite_ascii_case_equal(name, "TABLE_ROWS") ||
        mylite_ascii_case_equal(name, "AVG_ROW_LENGTH") ||
        mylite_ascii_case_equal(name, "DATA_LENGTH") ||
        mylite_ascii_case_equal(name, "MAX_DATA_LENGTH") ||
        mylite_ascii_case_equal(name, "INDEX_LENGTH") ||
        mylite_ascii_case_equal(name, "DATA_FREE") ||
        mylite_ascii_case_equal(name, "AUTO_INCREMENT")) {
        return information_schema_bigint_descriptor(
            information_schema_tables_bigint_length,
            MYLITE_FIELD_FLAG_UNSIGNED | MYLITE_FIELD_FLAG_BINARY,
            true
        );
    }
    if (mylite_ascii_case_equal(name, "CREATE_TIME")) {
        return information_schema_temporal_descriptor(
            MYLITE_FIELD_TYPE_TIMESTAMP,
            MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY |
                MYLITE_FIELD_FLAG_NO_DEFAULT_VALUE,
            false
        );
    }
    if (mylite_ascii_case_equal(name, "UPDATE_TIME") ||
        mylite_ascii_case_equal(name, "CHECK_TIME")) {
        return information_schema_temporal_descriptor(MYLITE_FIELD_TYPE_DATETIME, 0U, true);
    }
    if (mylite_ascii_case_equal(name, "TABLE_COLLATION")) {
        return information_schema_text_descriptor(
            information_schema_identifier_length,
            MYLITE_FIELD_FLAG_UNIQUE_KEY | MYLITE_FIELD_FLAG_NO_DEFAULT_VALUE |
                MYLITE_FIELD_FLAG_PART_KEY,
            0U,
            true
        );
    }
    if (mylite_ascii_case_equal(name, "CHECKSUM")) {
        return information_schema_bigint_descriptor(
            information_schema_tables_bigint_length,
            MYLITE_FIELD_FLAG_BINARY,
            true
        );
    }
    if (mylite_ascii_case_equal(name, "CREATE_OPTIONS")) {
        return information_schema_text_descriptor(
            information_schema_create_options_length,
            0U,
            mylite_mysql_not_fixed_decimals,
            true
        );
    }
    if (mylite_ascii_case_equal(name, "TABLE_COMMENT")) {
        return information_schema_text_descriptor(
            information_schema_table_comment_length,
            0U,
            mylite_mysql_not_fixed_decimals,
            true
        );
    }
    return (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_INVALID,
    };
}

static struct mylite_field_descriptor information_schema_table_constraints_column_descriptor(
    const char *name
) {
    const unsigned int not_null_key_flags = MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY |
                                            MYLITE_FIELD_FLAG_NO_DEFAULT_VALUE |
                                            MYLITE_FIELD_FLAG_PART_KEY;

    if (mylite_ascii_case_equal(name, "CONSTRAINT_CATALOG")) {
        return information_schema_text_descriptor(
            information_schema_identifier_length,
            not_null_key_flags | MYLITE_FIELD_FLAG_UNIQUE_KEY,
            0U,
            false
        );
    }
    if (mylite_ascii_case_equal(name, "CONSTRAINT_SCHEMA") ||
        mylite_ascii_case_equal(name, "TABLE_SCHEMA") ||
        mylite_ascii_case_equal(name, "TABLE_NAME")) {
        return information_schema_text_descriptor(
            information_schema_identifier_length,
            not_null_key_flags,
            0U,
            false
        );
    }
    if (mylite_ascii_case_equal(name, "CONSTRAINT_NAME")) {
        return information_schema_text_descriptor(
            information_schema_identifier_length,
            0U,
            0U,
            true
        );
    }
    if (mylite_ascii_case_equal(name, "CONSTRAINT_TYPE")) {
        return information_schema_text_descriptor(
            information_schema_constraint_type_length,
            MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY,
            0U,
            false
        );
    }
    if (mylite_ascii_case_equal(name, "ENFORCED")) {
        return information_schema_text_descriptor(
            information_schema_enforced_length,
            MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY,
            0U,
            false
        );
    }
    return (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_INVALID,
    };
}

static struct mylite_field_descriptor information_schema_key_column_usage_column_descriptor(
    const char *name
) {
    const unsigned int not_null_key_flags = MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY |
                                            MYLITE_FIELD_FLAG_NO_DEFAULT_VALUE |
                                            MYLITE_FIELD_FLAG_PART_KEY;

    if (mylite_ascii_case_equal(name, "CONSTRAINT_CATALOG") ||
        mylite_ascii_case_equal(name, "TABLE_CATALOG")) {
        return information_schema_text_descriptor(
            information_schema_identifier_length,
            not_null_key_flags | MYLITE_FIELD_FLAG_UNIQUE_KEY,
            0U,
            false
        );
    }
    if (mylite_ascii_case_equal(name, "CONSTRAINT_SCHEMA") ||
        mylite_ascii_case_equal(name, "TABLE_SCHEMA") ||
        mylite_ascii_case_equal(name, "TABLE_NAME")) {
        return information_schema_text_descriptor(
            information_schema_identifier_length,
            not_null_key_flags,
            0U,
            false
        );
    }
    if (mylite_ascii_case_equal(name, "CONSTRAINT_NAME") ||
        mylite_ascii_case_equal(name, "REFERENCED_COLUMN_NAME")) {
        return information_schema_text_descriptor(
            information_schema_identifier_length,
            0U,
            0U,
            true
        );
    }
    if (mylite_ascii_case_equal(name, "COLUMN_NAME")) {
        return information_schema_text_descriptor(
            information_schema_identifier_length,
            0U,
            mylite_mysql_not_fixed_decimals,
            true
        );
    }
    if (mylite_ascii_case_equal(name, "ORDINAL_POSITION")) {
        return information_schema_integer_descriptor(
            MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_UNSIGNED | MYLITE_FIELD_FLAG_BINARY,
            false
        );
    }
    if (mylite_ascii_case_equal(name, "POSITION_IN_UNIQUE_CONSTRAINT")) {
        return information_schema_integer_descriptor(
            MYLITE_FIELD_FLAG_UNSIGNED | MYLITE_FIELD_FLAG_BINARY,
            true
        );
    }
    if (mylite_ascii_case_equal(name, "REFERENCED_TABLE_SCHEMA") ||
        mylite_ascii_case_equal(name, "REFERENCED_TABLE_NAME")) {
        return information_schema_text_descriptor(
            information_schema_identifier_length,
            MYLITE_FIELD_FLAG_BINARY,
            0U,
            true
        );
    }
    return (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_INVALID,
    };
}

static struct mylite_field_descriptor information_schema_check_constraints_column_descriptor(
    const char *name
) {
    const unsigned int not_null_key_flags = MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY |
                                            MYLITE_FIELD_FLAG_NO_DEFAULT_VALUE |
                                            MYLITE_FIELD_FLAG_PART_KEY;

    if (mylite_ascii_case_equal(name, "CONSTRAINT_CATALOG")) {
        return information_schema_text_descriptor(
            information_schema_identifier_length,
            not_null_key_flags | MYLITE_FIELD_FLAG_UNIQUE_KEY,
            0U,
            false
        );
    }
    if (mylite_ascii_case_equal(name, "CONSTRAINT_SCHEMA")) {
        return information_schema_text_descriptor(
            information_schema_identifier_length,
            not_null_key_flags,
            0U,
            false
        );
    }
    if (mylite_ascii_case_equal(name, "CONSTRAINT_NAME")) {
        return information_schema_text_descriptor(
            information_schema_identifier_length,
            MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_NO_DEFAULT_VALUE |
                MYLITE_FIELD_FLAG_PART_KEY,
            0U,
            false
        );
    }
    if (mylite_ascii_case_equal(name, "CHECK_CLAUSE")) {
        struct mylite_field_descriptor descriptor = {
            .type = MYLITE_FIELD_TYPE_BLOB,
            .flags = MYLITE_FIELD_FLAG_BLOB | MYLITE_FIELD_FLAG_BINARY |
                     MYLITE_FIELD_FLAG_NO_DEFAULT_VALUE,
            .length = mylite_mysql_long_text_length,
            .charset_id = mylite_mysql_latin1_swedish_ci_charset_id,
            .nullable = false,
        };

        mylite_field_descriptor_set_nullable(&descriptor, false);
        return descriptor;
    }
    return (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_INVALID,
    };
}

static struct mylite_field_descriptor information_schema_referential_constraints_column_descriptor(
    const char *name
) {
    const unsigned int not_null_key_flags = MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY |
                                            MYLITE_FIELD_FLAG_NO_DEFAULT_VALUE |
                                            MYLITE_FIELD_FLAG_PART_KEY;

    if (mylite_ascii_case_equal(name, "CONSTRAINT_CATALOG")) {
        return information_schema_text_descriptor(
            information_schema_identifier_length,
            not_null_key_flags | MYLITE_FIELD_FLAG_UNIQUE_KEY,
            0U,
            false
        );
    }
    if (mylite_ascii_case_equal(name, "UNIQUE_CONSTRAINT_CATALOG")) {
        return information_schema_text_descriptor(
            information_schema_identifier_length,
            not_null_key_flags | MYLITE_FIELD_FLAG_MULTIPLE_KEY,
            0U,
            false
        );
    }
    if (mylite_ascii_case_equal(name, "CONSTRAINT_SCHEMA") ||
        mylite_ascii_case_equal(name, "UNIQUE_CONSTRAINT_SCHEMA")) {
        return information_schema_text_descriptor(
            information_schema_identifier_length,
            not_null_key_flags,
            0U,
            false
        );
    }
    if (mylite_ascii_case_equal(name, "CONSTRAINT_NAME")) {
        return information_schema_text_descriptor(
            information_schema_identifier_length,
            0U,
            mylite_mysql_not_fixed_decimals,
            true
        );
    }
    if (mylite_ascii_case_equal(name, "UNIQUE_CONSTRAINT_NAME")) {
        return information_schema_text_descriptor(
            information_schema_identifier_length,
            0U,
            0U,
            true
        );
    }
    if (mylite_ascii_case_equal(name, "MATCH_OPTION")) {
        return information_schema_enum_descriptor(information_schema_match_option_length);
    }
    if (mylite_ascii_case_equal(name, "UPDATE_RULE") ||
        mylite_ascii_case_equal(name, "DELETE_RULE")) {
        return information_schema_enum_descriptor(information_schema_rule_length);
    }
    if (mylite_ascii_case_equal(name, "TABLE_NAME") ||
        mylite_ascii_case_equal(name, "REFERENCED_TABLE_NAME")) {
        return information_schema_text_descriptor(
            information_schema_identifier_length,
            not_null_key_flags,
            0U,
            false
        );
    }
    return (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_INVALID,
    };
}

static struct mylite_field_descriptor information_schema_engines_column_descriptor(
    const char *name
) {
    if (mylite_ascii_case_equal(name, "ENGINE")) {
        return information_schema_text_descriptor(
            information_schema_identifier_length,
            MYLITE_FIELD_FLAG_NOT_NULL,
            mylite_mysql_not_fixed_decimals,
            false
        );
    }
    if (mylite_ascii_case_equal(name, "SUPPORT")) {
        return information_schema_text_descriptor(
            information_schema_engine_support_length,
            MYLITE_FIELD_FLAG_NOT_NULL,
            mylite_mysql_not_fixed_decimals,
            false
        );
    }
    if (mylite_ascii_case_equal(name, "COMMENT")) {
        return information_schema_text_descriptor(
            information_schema_engine_comment_length,
            MYLITE_FIELD_FLAG_NOT_NULL,
            mylite_mysql_not_fixed_decimals,
            false
        );
    }
    if (mylite_ascii_case_equal(name, "TRANSACTIONS") || mylite_ascii_case_equal(name, "XA") ||
        mylite_ascii_case_equal(name, "SAVEPOINTS")) {
        return information_schema_text_descriptor(
            information_schema_yes_no_length,
            0U,
            mylite_mysql_not_fixed_decimals,
            true
        );
    }
    return (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_INVALID,
    };
}

static struct mylite_field_descriptor information_schema_character_sets_column_descriptor(
    const char *name
) {
    if (mylite_ascii_case_equal(name, "CHARACTER_SET_NAME")) {
        return information_schema_text_descriptor(
            information_schema_identifier_length,
            MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_UNIQUE_KEY |
                MYLITE_FIELD_FLAG_NO_DEFAULT_VALUE | MYLITE_FIELD_FLAG_PART_KEY,
            0U,
            false
        );
    }
    if (mylite_ascii_case_equal(name, "DESCRIPTION")) {
        return information_schema_text_descriptor(
            information_schema_table_comment_length,
            MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_NO_DEFAULT_VALUE,
            0U,
            false
        );
    }
    if (mylite_ascii_case_equal(name, "DEFAULT_COLLATE_NAME")) {
        return information_schema_text_descriptor(
            information_schema_identifier_length,
            MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_UNIQUE_KEY |
                MYLITE_FIELD_FLAG_NO_DEFAULT_VALUE | MYLITE_FIELD_FLAG_PART_KEY,
            0U,
            false
        );
    }
    if (mylite_ascii_case_equal(name, "MAXLEN")) {
        return information_schema_integer_descriptor(
            MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_UNSIGNED |
                MYLITE_FIELD_FLAG_NO_DEFAULT_VALUE,
            false
        );
    }
    return (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_INVALID,
    };
}

static struct mylite_field_descriptor information_schema_collations_column_descriptor(
    const char *name
) {
    if (mylite_ascii_case_equal(name, "COLLATION_NAME") ||
        mylite_ascii_case_equal(name, "CHARACTER_SET_NAME")) {
        return information_schema_text_descriptor(
            information_schema_identifier_length,
            MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_NO_DEFAULT_VALUE,
            0U,
            false
        );
    }
    if (mylite_ascii_case_equal(name, "ID")) {
        return information_schema_bigint_descriptor(
            information_schema_collation_id_length,
            MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_UNSIGNED,
            false
        );
    }
    if (mylite_ascii_case_equal(name, "IS_DEFAULT") ||
        mylite_ascii_case_equal(name, "IS_COMPILED")) {
        return information_schema_text_descriptor(
            information_schema_yes_no_length,
            MYLITE_FIELD_FLAG_NOT_NULL,
            0U,
            false
        );
    }
    if (mylite_ascii_case_equal(name, "SORTLEN")) {
        return information_schema_integer_descriptor(
            MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_UNSIGNED |
                MYLITE_FIELD_FLAG_NO_DEFAULT_VALUE,
            false
        );
    }
    if (mylite_ascii_case_equal(name, "PAD_ATTRIBUTE")) {
        return information_schema_enum_descriptor(information_schema_pad_attribute_length);
    }
    return (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_INVALID,
    };
}

static struct mylite_field_descriptor information_schema_keywords_column_descriptor(
    const char *name
) {
    if (mylite_ascii_case_equal(name, "WORD")) {
        return information_schema_text_descriptor(
            information_schema_keyword_word_length,
            0U,
            0U,
            true
        );
    }
    if (mylite_ascii_case_equal(name, "RESERVED")) {
        struct mylite_field_descriptor descriptor = {
            .type = MYLITE_FIELD_TYPE_LONG,
            .flags = MYLITE_FIELD_FLAG_NUM,
            .length = information_schema_keyword_reserved_length,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = true,
        };

        mylite_field_descriptor_set_nullable(&descriptor, true);
        return descriptor;
    }
    return (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_INVALID,
    };
}

static struct mylite_field_descriptor information_schema_text_descriptor(
    uint64_t length,
    unsigned int flags,
    unsigned int decimals,
    bool nullable
) {
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_VAR_STRING,
        .flags = flags,
        .length = length,
        .decimals = decimals,
        .charset_id = mylite_mysql_latin1_swedish_ci_charset_id,
        .nullable = nullable,
    };

    mylite_field_descriptor_set_nullable(&descriptor, nullable);
    return descriptor;
}

static struct mylite_field_descriptor information_schema_enum_descriptor(uint64_t length) {
    struct mylite_field_descriptor descriptor = information_schema_text_descriptor(
        length,
        MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_ENUM |
            MYLITE_FIELD_FLAG_NO_DEFAULT_VALUE,
        0U,
        false
    );

    descriptor.type = MYLITE_FIELD_TYPE_STRING;
    return descriptor;
}

static struct mylite_field_descriptor information_schema_integer_descriptor(
    unsigned int flags,
    bool nullable
) {
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_LONG,
        .flags = flags | MYLITE_FIELD_FLAG_NUM,
        .length = information_schema_unsigned_int_length,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = nullable,
    };

    mylite_field_descriptor_set_nullable(&descriptor, nullable);
    return descriptor;
}

static struct mylite_field_descriptor information_schema_bigint_descriptor(
    uint64_t length,
    unsigned int flags,
    bool nullable
) {
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_LONGLONG,
        .flags = flags | MYLITE_FIELD_FLAG_NUM,
        .length = length,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = nullable,
    };

    mylite_field_descriptor_set_nullable(&descriptor, nullable);
    return descriptor;
}

static struct mylite_field_descriptor information_schema_temporal_descriptor(
    int type,
    unsigned int flags,
    bool nullable
) {
    struct mylite_field_descriptor descriptor = {
        .type = type,
        .flags = flags,
        .length = information_schema_temporal_length,
        .charset_id = mylite_mysql_latin1_swedish_ci_charset_id,
        .nullable = nullable,
    };

    mylite_field_descriptor_set_nullable(&descriptor, nullable);
    return descriptor;
}

static struct mylite_field_descriptor information_schema_generic_column_descriptor(
    const char *name
) {
    bool nullable = true;
    struct mylite_field_descriptor descriptor = {0};

    if (information_schema_column_is_not_null_text(name) != 0) {
        nullable = false;
    }

    if (information_schema_column_is_integer(name) != 0) {
        descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_LONGLONG,
            .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
            .length = mylite_mysql_signed_longlong_display_length,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = nullable,
        };
    } else {
        descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_VAR_STRING,
            .length = mylite_mysql_text_length,
            .decimals = mylite_mysql_not_fixed_decimals,
            .charset_id = mylite_mysql_utf8mb4_0900_ai_ci_charset_id,
            .nullable = nullable,
        };
    }

    mylite_field_descriptor_set_nullable(&descriptor, nullable);
    return descriptor;
}

static int information_schema_column_is_integer(const char *name) {
    static const char *const names[] = {
        "VERSION",
        "TABLE_ROWS",
        "AVG_ROW_LENGTH",
        "DATA_LENGTH",
        "MAX_DATA_LENGTH",
        "INDEX_LENGTH",
        "DATA_FREE",
        "AUTO_INCREMENT",
        "CHECKSUM",
        "ORDINAL_POSITION",
        "CHARACTER_MAXIMUM_LENGTH",
        "CHARACTER_OCTET_LENGTH",
        "NUMERIC_PRECISION",
        "NUMERIC_SCALE",
        "DATETIME_PRECISION",
        "SRS_ID",
        "NON_UNIQUE",
        "SEQ_IN_INDEX",
        "CARDINALITY",
        "SUB_PART",
        "POSITION_IN_UNIQUE_CONSTRAINT",
    };

    for (size_t index = 0U; index < sizeof(names) / sizeof(names[0]); ++index) {
        if (mylite_ascii_case_equal(name, names[index])) {
            return 1;
        }
    }
    return 0;
}

static bool information_schema_tables_column_has_empty_origin_schema(const char *name) {
    static const char *const names[] = {
        "ENGINE",
        "VERSION",
        "TABLE_ROWS",
        "AVG_ROW_LENGTH",
        "DATA_LENGTH",
        "MAX_DATA_LENGTH",
        "INDEX_LENGTH",
        "DATA_FREE",
        "AUTO_INCREMENT",
        "UPDATE_TIME",
        "CHECK_TIME",
        "CHECKSUM",
        "CREATE_OPTIONS",
        "TABLE_COMMENT",
    };

    for (size_t index = 0U; index < sizeof(names) / sizeof(names[0]); ++index) {
        if (mylite_ascii_case_equal(name, names[index])) {
            return true;
        }
    }
    return false;
}

static int information_schema_column_is_not_null_text(const char *name) {
    static const char *const names[] = {
        "TABLE_CATALOG",
        "TABLE_SCHEMA",
        "TABLE_NAME",
        "TABLE_TYPE",
        "CATALOG_NAME",
        "SCHEMA_NAME",
        "COLUMN_NAME",
        "ORDINAL_POSITION",
        "CONSTRAINT_CATALOG",
        "CONSTRAINT_SCHEMA",
        "CONSTRAINT_NAME",
        "INDEX_SCHEMA",
        "INDEX_NAME",
        "SEQ_IN_INDEX",
    };

    for (size_t index = 0U; index < sizeof(names) / sizeof(names[0]); ++index) {
        if (mylite_ascii_case_equal(name, names[index])) {
            return 1;
        }
    }
    return 0;
}
