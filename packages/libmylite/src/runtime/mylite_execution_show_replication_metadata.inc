
static int execute_show_binary_log_status_statement(
    struct mylite_db *database,
    mylite_result **out_result
) {
    static const char *const result_columns[show_binary_log_status_result_column_count] = {
        "File",
        "Position",
        "Binlog_Do_DB",
        "Binlog_Ignore_DB",
        "Executed_Gtid_Set",
    };
    static const char *const values[show_binary_log_status_result_column_count] = {
        show_binary_log_file_name,
        show_binary_log_placeholder_position,
        "",
        "",
        "",
    };
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_binary_log_status_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_show_binary_logs_statement(
    struct mylite_db *database,
    mylite_result **out_result
) {
    static const char *const result_columns[show_binary_logs_result_column_count] = {
        "Log_name",
        "File_size",
        "Encrypted",
    };
    static const char *const values[show_binary_logs_result_column_count] = {
        show_binary_log_file_name,
        show_binary_log_placeholder_file_size,
        "No",
    };
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_binary_logs_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_show_binlog_events_statement(
    struct mylite_db *database,
    mylite_result **out_result
) {
    static const char *const result_columns[show_binlog_events_result_column_count] = {
        "Log_name",
        "Pos",
        "Event_type",
        "Server_id",
        "End_log_pos",
        "Info",
    };
    static const char *const values[show_binlog_events_result_column_count] = {
        show_binary_log_file_name,
        show_binary_log_placeholder_position,
        "Format_desc",
        show_binlog_events_placeholder_server_id,
        show_binlog_events_placeholder_end_log_pos,
        show_binlog_events_placeholder_info,
    };
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_binlog_events_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_show_relaylog_events_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[show_relaylog_events_result_column_count] = {
        "Log_name",
        "Pos",
        "Event_type",
        "Server_id",
        "End_log_pos",
        "Info",
    };
    mylite_result *result = NULL;
    int rc = validate_show_relaylog_events_channel(database, statement);

    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_result_create(&result);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_relaylog_events_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int validate_show_relaylog_events_channel(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *channel = child_at(statement, 0U);
    char *channel_name = NULL;
    size_t channel_name_length = 0U;
    int rc = MYLITE_OK;

    if (channel == NULL) {
        return MYLITE_OK;
    }

    rc = decode_sql_string_literal(
        database,
        channel,
        "SHOW RELAYLOG EVENTS FOR CHANNEL supports only string literals",
        "SHOW RELAYLOG EVENTS channel names do not support NUL bytes",
        &channel_name,
        &channel_name_length
    );
    if (rc != MYLITE_OK) {
        free(channel_name);
        return rc;
    }

    if (channel_name_length != 0U) {
        set_replica_channel_does_not_exist_error(database, channel_name);
        free(channel_name);
        return MYLITE_ERROR;
    }

    free(channel_name);
    return MYLITE_OK;
}

static void set_replica_channel_does_not_exist_error(
    struct mylite_db *database,
    const char *channel_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Replica channel '%s' does not exist.",
        channel_name == NULL ? "" : channel_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_replica_channel_does_not_exist,
        "HY000",
        message
    );
}

static int execute_show_replica_status_statement(
    struct mylite_db *database,
    mylite_result **out_result
) {
    static const char *const result_columns[show_replica_status_result_column_count] = {
        "Replica_IO_State",
        "Source_Host",
        "Source_User",
        "Source_Port",
        "Connect_Retry",
        "Source_Log_File",
        "Read_Source_Log_Pos",
        "Relay_Log_File",
        "Relay_Log_Pos",
        "Relay_Source_Log_File",
        "Replica_IO_Running",
        "Replica_SQL_Running",
        "Replicate_Do_DB",
        "Replicate_Ignore_DB",
        "Replicate_Do_Table",
        "Replicate_Ignore_Table",
        "Replicate_Wild_Do_Table",
        "Replicate_Wild_Ignore_Table",
        "Last_Errno",
        "Last_Error",
        "Skip_Counter",
        "Exec_Source_Log_Pos",
        "Relay_Log_Space",
        "Until_Condition",
        "Until_Log_File",
        "Until_Log_Pos",
        "Source_SSL_Allowed",
        "Source_SSL_CA_File",
        "Source_SSL_CA_Path",
        "Source_SSL_Cert",
        "Source_SSL_Cipher",
        "Source_SSL_Key",
        "Seconds_Behind_Source",
        "Source_SSL_Verify_Server_Cert",
        "Last_IO_Errno",
        "Last_IO_Error",
        "Last_SQL_Errno",
        "Last_SQL_Error",
        "Replicate_Ignore_Server_Ids",
        "Source_Server_Id",
        "Source_UUID",
        "Source_Info_File",
        "SQL_Delay",
        "SQL_Remaining_Delay",
        "Replica_SQL_Running_State",
        "Source_Retry_Count",
        "Source_Bind",
        "Last_IO_Error_Timestamp",
        "Last_SQL_Error_Timestamp",
        "Source_SSL_Crl",
        "Source_SSL_Crlpath",
        "Retrieved_Gtid_Set",
        "Executed_Gtid_Set",
        "Auto_Position",
        "Replicate_Rewrite_DB",
        "Channel_Name",
        "Source_TLS_Version",
        "Source_public_key_path",
        "Get_Source_public_key",
        "Network_Namespace",
    };
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_replica_status_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_show_replicas_statement(struct mylite_db *database, mylite_result **out_result) {
    static const char *const result_columns[show_replicas_result_column_count] = {
        "Server_Id",
        "Host",
        "Port",
        "Source_Id",
        "Replica_UUID",
    };
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_replicas_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}
