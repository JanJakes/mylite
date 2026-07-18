#include "mylite_execution_diagnostics_internal.h"

void mylite_execution_diagnostics_set_duplicate_column_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Duplicate column name '%s'", column_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_duplicate_column,
        "42S21",
        message
    );
}

void mylite_execution_diagnostics_set_duplicated_enum_value_error(
    struct mylite_db *database,
    const char *column_name,
    const char *value,
    size_t value_length
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int display_length = value_length > (size_t)INT_MAX ? INT_MAX : (int)value_length;
    int written = snprintf(
        message,
        sizeof(message),
        "Column '%s' has duplicated value '%.*s' in ENUM",
        column_name,
        display_length,
        value == NULL ? "" : value
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_duplicated_value_in_enum,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_duplicated_set_value_error(
    struct mylite_db *database,
    const char *column_name,
    const char *value,
    size_t value_length
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int display_length = value_length > (size_t)INT_MAX ? INT_MAX : (int)value_length;
    int written = snprintf(
        message,
        sizeof(message),
        "Column '%s' has duplicated value '%.*s' in SET",
        column_name,
        display_length,
        value == NULL ? "" : value
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_duplicated_value_in_set,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_illegal_set_value_error(
    struct mylite_db *database,
    const char *value,
    size_t value_length
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int display_length = value_length > (size_t)INT_MAX ? INT_MAX : (int)value_length;
    int written = snprintf(
        message,
        sizeof(message),
        "Illegal set '%.*s' value found during parsing",
        display_length,
        value == NULL ? "" : value
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_illegal_set_value,
        "22007",
        message
    );
}

void mylite_execution_diagnostics_set_multiple_primary_key_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_multiple_primary_key,
        "42000",
        "Multiple primary key defined"
    );
}

void mylite_execution_diagnostics_set_sql_require_primary_key_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_primary_key_required,
        "HY000",
        "Unable to create or change a table without a primary key, when the system variable "
        "'sql_require_primary_key' is set. Add a primary key to the table or unset this variable "
        "to avoid this message. Note that tables without a primary key can cause performance "
        "problems in row-based replication, so please consult your DBA before changing this "
        "setting."
    );
}

void mylite_execution_diagnostics_set_wrong_auto_key_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_wrong_auto_key,
        "42000",
        "Incorrect table definition; there can be only one auto column and it must be defined as a "
        "key"
    );
}

void mylite_execution_diagnostics_set_column_length_too_big_error(
    struct mylite_db *database,
    const char *column_name,
    uint64_t maximum_length
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Column length too big for column '%s' (max = %" PRIu64 "); use BLOB or TEXT instead",
        column_name,
        maximum_length
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_column_length_too_big,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_row_size_too_large_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_row_size_too_large,
        "42000",
        "Row size too large. The maximum row size for the used table type, not counting BLOBs, is "
        "65535. This includes storage overhead, check the manual. You have to change some columns "
        "to TEXT or BLOBs"
    );
}

void mylite_execution_diagnostics_set_incorrect_column_specifier_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Incorrect column specifier for column '%s'",
        column_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_incorrect_column_specifier,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_key_column_missing_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Key column '%s' doesn't exist in table", column_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_key_column_does_not_exist,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_invalid_use_of_null_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_invalid_use_of_null,
        "22004",
        "Invalid use of NULL value"
    );
}

void mylite_execution_diagnostics_set_primary_key_part_null_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_primary_key_part_null,
        "42000",
        "All parts of a PRIMARY KEY must be NOT NULL; if you need NULL in a key, use UNIQUE instead"
    );
}

void mylite_execution_diagnostics_set_duplicate_key_name_error(
    struct mylite_db *database,
    const char *index_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Duplicate key name '%s'", index_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_duplicate_key_name,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_incorrect_index_name_error(
    struct mylite_db *database,
    const char *index_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Incorrect index name '%s'", index_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_incorrect_index_name,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_index_hint_use_force_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_wrong_usage,
        "HY000",
        "Incorrect usage of USE INDEX and FORCE INDEX"
    );
}

void mylite_execution_diagnostics_set_key_does_not_exist_in_table_error(
    struct mylite_db *database,
    const char *index_name,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Key '%s' doesn't exist in table '%s'",
        index_name,
        table_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_key_does_not_exist,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_primary_key_index_invisible_error(struct mylite_db *database
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_primary_key_index_invisible,
        "HY000",
        "A primary key index cannot be invisible."
    );
}

void mylite_execution_diagnostics_set_storage_engine_cant_index_column_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "The used storage engine can't index column '%s'",
        column_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_storage_engine_cant_index_column,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_fulltext_column_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Column '%s' cannot be part of FULLTEXT index",
        column_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_fulltext_column,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_fulltext_explicit_order_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_wrong_usage,
        "HY000",
        "Incorrect usage of spatial/fulltext/hash index and explicit index order"
    );
}

void mylite_execution_diagnostics_set_temporary_fulltext_index_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_temporary_fulltext_index,
        "HY000",
        "Cannot create FULLTEXT index on temporary InnoDB table"
    );
}

void mylite_execution_diagnostics_set_spatial_index_non_geometric_error(struct mylite_db *database
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_spatial_index_non_geometric,
        "42000",
        "A SPATIAL index may only contain a geometrical type column"
    );
}

void mylite_execution_diagnostics_set_spatial_index_must_be_not_null_error(
    struct mylite_db *database
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_spatial_must_be_not_null,
        "42000",
        "All parts of a SPATIAL index must be NOT NULL"
    );
}

void mylite_execution_diagnostics_set_spatial_unique_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_spatial_unique,
        "HY000",
        "Spatial indexes can't be primary or unique indexes."
    );
}

void mylite_execution_diagnostics_set_spatial_index_type_not_supported_error(
    struct mylite_db *database,
    const char *index_type
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "The index type %s is not supported for spatial indexes.",
        index_type
    );

    if (written < 0 || (size_t)written >= sizeof(message)) {
        mylite_execution_diagnostics_set_runtime_error(
            database,
            "spatial index type diagnostic is too long"
        );
        return;
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_spatial_index_type_not_supported,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_spatial_too_many_key_parts_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_too_many_key_parts,
        "42000",
        "Too many key parts specified; max 1 parts allowed"
    );
}

void mylite_execution_diagnostics_set_blob_key_without_length_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "BLOB/TEXT column '%s' used in key specification without a key length",
        column_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_blob_key_without_length,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_json_key_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "JSON column '%s' supports indexing only via generated columns on a specified JSON path.",
        column_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_json_used_as_key,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_incorrect_prefix_key_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_incorrect_prefix_key,
        "HY000",
        "Incorrect prefix key; the used key part isn't a string, the used length is longer than "
        "the key part, or the storage engine doesn't support unique prefix keys"
    );
}

void mylite_execution_diagnostics_set_key_part_length_cannot_be_zero_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Key part '%s' length cannot be 0", column_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_key_part_length_cannot_be_zero,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_key_too_long_error(
    struct mylite_db *database,
    uint64_t maximum_key_length_bytes
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Specified key was too long; max key length is %" PRIu64 " bytes",
        maximum_key_length_bytes
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_key_too_long,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_table_comment_too_long_error(
    struct mylite_db *database,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Comment for table '%s' is too long (max = 2048)",
        table_name == NULL ? "" : table_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_table_comment_too_long,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_column_comment_too_long_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Comment for field '%s' is too long (max = 1024)",
        column_name == NULL ? "" : column_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_column_comment_too_long,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_index_comment_too_long_error(
    struct mylite_db *database,
    const char *index_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Comment for index '%s' is too long (max = 1024)",
        index_name == NULL ? "" : index_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_index_comment_too_long,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_duplicate_key_error(
    struct mylite_db *database,
    const char *table_name,
    const char *index_name,
    const char *value
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Duplicate entry '%s' for key '%s.%s'",
        value,
        table_name,
        index_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_duplicate_key,
        "23000",
        message
    );
}

void mylite_execution_diagnostics_set_no_referenced_row_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_no_referenced_row,
        "23000",
        "Cannot add or update a child row: a foreign key constraint fails"
    );
}

void mylite_execution_diagnostics_set_row_is_referenced_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_row_is_referenced,
        "23000",
        "Cannot delete or update a parent row: a foreign key constraint fails"
    );
}

void mylite_execution_diagnostics_set_cannot_drop_index_needed_foreign_key_error(
    struct mylite_db *database,
    const char *index_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Cannot drop index '%s': needed in a foreign key constraint",
        index_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_cannot_drop_index_needed_fk,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_failed_to_open_referenced_table_error(
    struct mylite_db *database,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Failed to open the referenced table '%s'", table_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_failed_to_open_referenced_table,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_incorrect_foreign_key_definition_error(
    struct mylite_db *database
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_incorrect_foreign_key_definition,
        "42000",
        "Key reference and table reference don't match"
    );
}

void mylite_execution_diagnostics_set_foreign_key_column_incompatible_error(
    struct mylite_db *database
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_foreign_key_column_incompatible,
        "HY000",
        "Referencing column and referenced column in foreign key constraint are incompatible"
    );
}

void mylite_execution_diagnostics_set_foreign_key_missing_unique_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_foreign_key_missing_unique,
        "HY000",
        "Missing unique key for constraint in the referenced table"
    );
}

void mylite_execution_diagnostics_set_duplicate_foreign_key_error(
    struct mylite_db *database,
    const char *foreign_key_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Duplicate foreign key constraint name '%s'",
        foreign_key_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_duplicate_foreign_key,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_drop_column_foreign_key_child_error(
    struct mylite_db *database,
    const char *column_name,
    const char *foreign_key_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Cannot drop column '%s': needed in a foreign key constraint '%s'",
        column_name,
        foreign_key_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_drop_column_fk_child,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_drop_column_foreign_key_parent_error(
    struct mylite_db *database,
    const char *column_name,
    const char *foreign_key_name,
    const char *child_table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Cannot drop column '%s': needed in a foreign key constraint '%s' of table '%s'",
        column_name,
        foreign_key_name,
        child_table_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_drop_column_fk_parent,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_foreign_key_set_null_not_nullable_error(
    struct mylite_db *database,
    const char *column_name,
    const char *foreign_key_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Column '%s' cannot be NOT NULL: needed in a foreign key constraint '%s' SET NULL",
        column_name,
        foreign_key_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_column_not_null_for_set_null,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_foreign_key_cascade_duplicate_error(
    struct mylite_db *database,
    const char *parent_table_name,
    const char *record_value,
    const char *child_table_name,
    const char *index_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Foreign key constraint for table '%s', record '%s' would lead to a duplicate "
        "entry in table '%s', key '%s'",
        parent_table_name,
        record_value,
        child_table_name,
        index_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_foreign_key_cascade_duplicate,
        "23000",
        message
    );
}

void mylite_execution_diagnostics_set_check_constraint_non_boolean_error(struct mylite_db *database
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_check_constraint_non_boolean,
        "HY000",
        "An expression of a check constraint is not boolean"
    );
}

void mylite_execution_diagnostics_set_check_constraint_column_ref_error(
    struct mylite_db *database,
    const char *constraint_name,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Column check constraint '%s' references other column '%s'",
        constraint_name,
        column_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_check_constraint_column_ref,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_check_constraint_function_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_check_constraint_function,
        "HY000",
        "An expression of a check constraint contains disallowed function"
    );
}

void mylite_execution_diagnostics_set_check_constraint_subquery_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_check_constraint_subquery,
        "HY000",
        "An expression of a check constraint contains a disallowed subquery"
    );
}

void mylite_execution_diagnostics_set_check_constraint_variable_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_check_constraint_variable,
        "HY000",
        "An expression of a check constraint contains disallowed variable"
    );
}

void mylite_execution_diagnostics_set_check_constraint_auto_increment_error(
    struct mylite_db *database
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_check_constraint_auto_increment,
        "HY000",
        "An expression of a check constraint cannot refer to an AUTO_INCREMENT column"
    );
}

void mylite_execution_diagnostics_set_check_constraint_violated_error(
    struct mylite_db *database,
    const char *constraint_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Check constraint '%s' is violated.", constraint_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_check_constraint_violated,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_check_constraint_not_found_error(
    struct mylite_db *database,
    const char *constraint_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Check constraint '%s' is not found in the table.",
        constraint_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_check_constraint_not_found,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_drop_constraint_ambiguous_error(
    struct mylite_db *database,
    const char *constraint_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Table has multiple constraints with the name '%s'. "
        "Please use constraint specific 'DROP' clause.",
        constraint_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_drop_constraint_ambiguous,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_constraint_does_not_exist_error(
    struct mylite_db *database,
    const char *constraint_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Constraint '%s' does not exist.", constraint_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_constraint_does_not_exist,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_check_constraint_unknown_column_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Check constraint contains column '%s' that does not exist",
        column_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_check_constraint_unknown_column,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_alter_check_constraint_unknown_column_error(
    struct mylite_db *database,
    const char *column_name,
    const char *constraint_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Unknown column '%s' in 'check constraint %s expression'",
        column_name,
        constraint_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_column,
        "42S22",
        message
    );
}

void mylite_execution_diagnostics_set_duplicate_check_constraint_error(
    struct mylite_db *database,
    const char *check_constraint_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Duplicate check constraint name '%s'",
        check_constraint_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_duplicate_check_constraint,
        "HY000",
        message
    );
}

int mylite_execution_diagnostics_append_check_constraint_warning(
    struct mylite_db *database,
    const char *constraint_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Check constraint '%s' is violated.", constraint_name);
    int rc = MYLITE_OK;

    if (written < 0) {
        message[0] = '\0';
    }
    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_error_check_constraint_violated,
        "HY000",
        message
    );
    if (rc == MYLITE_NOMEM) {
        mylite_execution_diagnostics_set_nomem_error(database);
    }
    return rc;
}

int mylite_execution_diagnostics_append_duplicate_key_warning(
    struct mylite_db *database,
    const char *table_name,
    const char *index_name,
    const char *value
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Duplicate entry '%s' for key '%s.%s'",
        value,
        table_name,
        index_name
    );
    int rc = MYLITE_OK;

    if (written < 0) {
        message[0] = '\0';
    }
    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_error_duplicate_key,
        "23000",
        message
    );
    if (rc == MYLITE_NOMEM) {
        mylite_execution_diagnostics_set_nomem_error(database);
    }
    return rc;
}

int mylite_execution_diagnostics_append_no_referenced_row_warning(struct mylite_db *database) {
    int rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_error_no_referenced_row,
        "23000",
        "Cannot add or update a child row: a foreign key constraint fails"
    );

    if (rc == MYLITE_NOMEM) {
        mylite_execution_diagnostics_set_nomem_error(database);
    }
    return rc;
}
