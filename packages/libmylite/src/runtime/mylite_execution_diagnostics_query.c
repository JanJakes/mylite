#include "mylite_execution_diagnostics_internal.h"

void mylite_execution_diagnostics_set_no_tables_used_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_no_tables_used,
        "HY000",
        "No tables used"
    );
}

void mylite_execution_diagnostics_set_in_subquery_limit_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_not_supported_yet,
        "42000",
        "This version of MySQL doesn't yet support 'LIMIT & IN/ALL/ANY/SOME subquery'"
    );
}

void mylite_execution_diagnostics_set_scalar_subquery_column_count_error(struct mylite_db *database
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_operand_should_contain_one_column,
        "21000",
        "Operand should contain 1 column(s)"
    );
}

void mylite_execution_diagnostics_set_scalar_subquery_row_count_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_subquery_returns_more_than_one_row,
        "21000",
        "Subquery returns more than 1 row"
    );
}

void mylite_execution_diagnostics_set_union_column_count_mismatch_error(struct mylite_db *database
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_select_reduced,
        "21000",
        "The used SELECT statements have a different number of columns"
    );
}

void mylite_execution_diagnostics_set_update_table_used_error(
    struct mylite_db *database,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "You can't specify target table '%s' for update in FROM clause",
        table_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_update_table_used,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_safe_update_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_safe_update,
        "HY000",
        "You are using safe update mode and you tried to update a table without a WHERE that "
        "uses a KEY column."
    );
}

void mylite_execution_diagnostics_set_duplicate_table_alias_error(
    struct mylite_db *database,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Not unique table/alias: '%s'", table_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_not_unique_table_alias,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_cant_drop_field_or_key_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Can't DROP '%s'; check that column/key exists",
        column_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_cant_drop_field_or_key,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_cant_remove_all_fields_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_cant_remove_all_fields,
        "42000",
        "You can't delete all columns with ALTER TABLE; use DROP TABLE instead"
    );
}

void mylite_execution_diagnostics_set_must_have_visible_column_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_must_have_visible_column,
        "HY000",
        "A table must have at least one visible column."
    );
}

void mylite_execution_diagnostics_set_unknown_column_in_table_error(
    struct mylite_db *database,
    const char *column_name,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Unknown column '%s' in '%s'", column_name, table_name);

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

void mylite_execution_diagnostics_set_unknown_information_schema_table_error(
    struct mylite_db *database,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Unknown table '%s' in information_schema", table_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_table_in_schema,
        "42S02",
        message
    );
}

void mylite_execution_diagnostics_set_unknown_column_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Unknown column '%s' in 'field list'", column_name);

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

void mylite_execution_diagnostics_set_unknown_where_column_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Unknown column '%s' in 'where clause'", column_name);

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

void mylite_execution_diagnostics_set_unknown_order_column_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Unknown column '%s' in 'order clause'", column_name);

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

void mylite_execution_diagnostics_set_unknown_group_column_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Unknown column '%s' in 'group statement'", column_name);

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

void mylite_execution_diagnostics_set_unknown_having_column_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Unknown column '%s' in 'having clause'", column_name);

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

void mylite_execution_diagnostics_set_unknown_on_column_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Unknown column '%s' in 'on clause'", column_name);

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

void mylite_execution_diagnostics_set_ambiguous_column_reference_error(
    struct mylite_db *database,
    enum column_reference_diagnostic_context context,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    const char *context_text = "field list";
    int written = 0;

    if (context == COLUMN_REFERENCE_WHERE) {
        context_text = "where clause";
    } else if (context == COLUMN_REFERENCE_ORDER) {
        context_text = "order clause";
    } else if (context == COLUMN_REFERENCE_ON) {
        context_text = "on clause";
    } else if (context == COLUMN_REFERENCE_GROUP) {
        context_text = "group statement";
    } else if (context == COLUMN_REFERENCE_HAVING) {
        context_text = "having clause";
    }

    written = snprintf(
        message,
        sizeof(message),
        "Column '%s' in %s is ambiguous",
        column_name,
        context_text
    );
    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_column_ambiguous,
        "23000",
        message
    );
}

void mylite_execution_diagnostics_set_ambiguous_order_column_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Column '%s' in order clause is ambiguous", column_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_column_ambiguous,
        "23000",
        message
    );
}

void mylite_execution_diagnostics_set_not_unique_table_alias_error(
    struct mylite_db *database,
    const char *alias
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Not unique table/alias: '%s'", alias);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_not_unique_table_alias,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_only_full_group_by_error(
    struct mylite_db *database,
    size_t expression_index,
    const char *clause_name,
    const struct table_name_resolution *source,
    const struct mylite_catalog_column_descriptor *column
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Expression #%zu of %s is not in GROUP BY clause and contains nonaggregated "
        "column '%s.%s.%s' which is not functionally dependent on columns in GROUP BY clause; "
        "this is incompatible with sql_mode=only_full_group_by",
        expression_index,
        clause_name,
        source->schema.name,
        source->table_name,
        column->name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_not_group_by,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_column_specified_twice_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Column '%s' specified twice", column_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_column_specified_twice,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_column_count_mismatch_error(
    struct mylite_db *database,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Column count doesn't match value count at row %zu",
        row_number
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_column_count_mismatch,
        "21S01",
        message
    );
}

void mylite_execution_diagnostics_set_values_empty_row_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_empty_values_row,
        "HY000",
        "Each row of a VALUES clause must have at least one column, unless when used as source in "
        "an INSERT statement."
    );
}

void mylite_execution_diagnostics_set_values_default_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_values_default,
        "HY000",
        "A VALUES clause cannot use DEFAULT values, unless used as a source in an INSERT statement."
    );
}

void mylite_execution_diagnostics_set_values_integer_out_of_range_error(struct mylite_db *database
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_data_out_of_range,
        "22003",
        "VALUES integer literal is outside the supported signed 64-bit range"
    );
}
