
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
