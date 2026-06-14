#include "mylite_execution_diagnostics_internal.h"

void mylite_execution_diagnostics_set_no_database_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_no_database_selected,
        "3D000",
        "No database selected"
    );
}

void mylite_execution_diagnostics_set_database_access_denied_error(
    struct mylite_db *database,
    const char *schema_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Access denied for user 'root'@'%%' to database '%s'",
        schema_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_database_access_denied,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_system_schema_access_error(
    struct mylite_db *database,
    const char *schema_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Access to system schema '%s' is rejected.",
        schema_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_system_schema_access,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_mysql_data_dictionary_table_access_error(
    struct mylite_db *database,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    const char *table_kind = strcmp(table_name, "innodb_ddl_log") == 0 ||
                                     strcmp(table_name, "innodb_dynamic_metadata") == 0
                                 ? "system"
                                 : "data dictionary";
    int written = snprintf(
        message,
        sizeof(message),
        "Access to %s table 'mysql.%s' is rejected.",
        table_kind,
        table_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_data_dictionary_access,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_database_exists_error(
    struct mylite_db *database,
    const char *schema_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Can't create database '%s'; database exists",
        schema_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_database_exists,
        "HY000",
        message
    );
}

int mylite_execution_diagnostics_append_database_exists_note(
    struct mylite_db *database,
    const char *schema_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Can't create database '%s'; database exists",
        schema_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    return mylite_diagnostics_append_note(
        mylite_connection_diagnostics(database),
        mysql_error_database_exists,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_cant_drop_database_error(
    struct mylite_db *database,
    const char *schema_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Can't drop database '%s'; database doesn't exist",
        schema_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_cant_drop_database,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_unknown_database_error(
    struct mylite_db *database,
    const char *schema_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Unknown database '%s'", schema_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_database,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_database_does_not_exist_error(
    struct mylite_db *database,
    const char *schema_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Database '%s' doesn't exist", schema_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_database_does_not_exist,
        "42Y07",
        message
    );
}

void mylite_execution_diagnostics_set_table_exists_error(
    struct mylite_db *database,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Table '%s' already exists", table_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_table_exists,
        "42S01",
        message
    );
}

int mylite_execution_diagnostics_append_table_exists_note(
    struct mylite_db *database,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Table '%s' already exists", table_name);

    if (written < 0) {
        message[0] = '\0';
    }
    return mylite_diagnostics_append_note(
        mylite_connection_diagnostics(database),
        mysql_error_table_exists,
        "42S01",
        message
    );
}

void mylite_execution_diagnostics_set_create_table_select_locking_clause_error(
    struct mylite_db *database,
    const char *source_table_name,
    const char *target_table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Can't update table '%s' while '%s' is being created.",
        source_table_name,
        target_table_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_cannot_update_table_while_creating,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_unknown_table_error(
    struct mylite_db *database,
    const char *schema_name,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Unknown table '%s.%s'", schema_name, table_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_table,
        "42S02",
        message
    );
}

void mylite_execution_diagnostics_set_not_view_error(
    struct mylite_db *database,
    const char *schema_name,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "'%s.%s' is not VIEW", schema_name, table_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_not_view,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_unknown_table_name_error(
    struct mylite_db *database,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Unknown table '%s'", table_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_table,
        "42S02",
        message
    );
}

void mylite_execution_diagnostics_set_unknown_multi_delete_table_error(
    struct mylite_db *database,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Unknown table '%s' in MULTI DELETE", table_name);

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

void mylite_execution_diagnostics_set_wrong_usage_error(
    struct mylite_db *database,
    const char *left,
    const char *right
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Incorrect usage of %s and %s", left, right);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_wrong_usage,
        "HY000",
        message
    );
}

int mylite_execution_diagnostics_set_unknown_drop_tables_error(
    struct mylite_db *database,
    const struct planned_drop_table *plan
) {
    struct mylite_dynamic_string message;
    char *owned_message = NULL;
    size_t missing_index = 0U;
    int rc = MYLITE_OK;

    mylite_dynamic_string_init(&message);
    rc = mylite_dynamic_string_append(&message, "Unknown table '");
    for (size_t target_index = 0U; rc == MYLITE_OK && target_index < plan->target_count;
         ++target_index) {
        const struct planned_drop_table_target *target = &plan->targets[target_index];

        if (!target->missing) {
            continue;
        }
        if (missing_index != 0U) {
            rc = mylite_dynamic_string_append_char(&message, ',');
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(&message, target->target.schema.name);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(&message, '.');
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(&message, target->target.table_name);
        }
        ++missing_index;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(&message, '\'');
    }
    if (rc == MYLITE_OK) {
        owned_message = mylite_dynamic_string_take(&message);
        if (owned_message == NULL) {
            rc = MYLITE_NOMEM;
        }
    }
    if (rc != MYLITE_OK) {
        mylite_dynamic_string_deinit(&message);
        mylite_execution_diagnostics_set_nomem_error(database);
        return rc;
    }

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_table,
        "42S02",
        owned_message
    );
    free(owned_message);

    return MYLITE_ERROR;
}

int mylite_execution_diagnostics_append_unknown_table_note(
    struct mylite_db *database,
    const char *schema_name,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Unknown table '%s.%s'", schema_name, table_name);

    if (written < 0) {
        message[0] = '\0';
    }
    return mylite_diagnostics_append_note(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_table,
        "42S02",
        message
    );
}

void mylite_execution_diagnostics_set_table_does_not_exist_error(
    struct mylite_db *database,
    const char *schema_name,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Table '%s.%s' doesn't exist", schema_name, table_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_table_does_not_exist,
        "42S02",
        message
    );
}

void mylite_execution_diagnostics_set_unknown_storage_engine_error(
    struct mylite_db *database,
    const char *engine_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Unknown storage engine '%s'", engine_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_storage_engine,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_table_storage_engine_option_error(
    struct mylite_db *database,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Table storage engine for '%s' doesn't have this option",
        table_name == NULL ? "" : table_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_table_storage_engine_option,
        "HY000",
        message
    );
}

int mylite_execution_diagnostics_append_table_storage_engine_option_note(
    struct mylite_db *database,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Table storage engine for '%s' doesn't have this option",
        table_name == NULL ? "" : table_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    return mylite_diagnostics_append_note(
        mylite_connection_diagnostics(database),
        mysql_error_table_storage_engine_option,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_failed_read_auto_increment_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_failed_read_auto_increment,
        "HY000",
        "Failed to read auto-increment value from storage engine"
    );
}

void mylite_execution_diagnostics_set_unknown_character_set_error(
    struct mylite_db *database,
    const char *charset_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Unknown character set: '%s'", charset_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_character_set,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_unknown_collation_error(
    struct mylite_db *database,
    const char *collation_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Unknown collation: '%s'", collation_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_collation,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_savepoint_does_not_exist_error(
    struct mylite_db *database,
    const char *savepoint_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "SAVEPOINT %s does not exist", savepoint_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_savepoint_does_not_exist,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_collation_not_valid_for_charset_error(
    struct mylite_db *database,
    const char *collation_name,
    const char *charset_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "COLLATION '%s' is not valid for CHARACTER SET '%s'",
        collation_name,
        charset_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_collation_not_valid_for_character_set,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_illegal_mix_of_collations_error(
    struct mylite_db *database,
    const char *first_collation,
    const char *second_collation,
    const char *operation
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Illegal mix of collations (%s,EXPLICIT) and (%s,EXPLICIT) for operation '%s'",
        first_collation,
        second_collation,
        operation
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_illegal_mix_of_collations,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_conflicting_character_set_declarations_error(
    struct mylite_db *database,
    const char *first_charset,
    const char *second_charset
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Conflicting declarations: 'CHARACTER SET %s' and 'CHARACTER SET %s'",
        first_charset,
        second_charset
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_conflicting_declarations,
        "HY000",
        message
    );
}
