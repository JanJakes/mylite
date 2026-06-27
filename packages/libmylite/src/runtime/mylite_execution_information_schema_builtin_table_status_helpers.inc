static int append_information_schema_builtin_table_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    struct table_status_values status = {0};
    const char *table_type = builtin_schema_table_type(directory, table_name);
    int rc = load_builtin_table_status_values(database, directory, table_name, &status);

    if (rc != MYLITE_OK) {
        return rc;
    }

    const char *values[information_schema_tables_column_count] = {
        "def",
        directory->schema_name,
        table_name,
        table_type,
        builtin_schema_table_engine(directory, table_name),
        builtin_schema_table_version(directory, table_name),
        builtin_schema_table_row_format(directory, table_name),
        builtin_schema_table_rows(directory, table_name),
        builtin_schema_table_average_row_length(directory, table_name),
        builtin_schema_table_data_length(directory, table_name),
        table_type == NULL || strcmp(table_type, "VIEW") == 0 ? NULL : "0",
        builtin_schema_table_index_length(directory, table_name),
        builtin_schema_table_data_free(directory, table_name),
        builtin_schema_table_auto_increment(directory, table_name),
        status.create_time,
        status.update_time,
        NULL,
        builtin_schema_table_collation(directory, table_name),
        NULL,
        builtin_schema_table_create_options(directory, table_name),
        builtin_schema_table_comment(directory, table_name),
    };

    return append_information_schema_row(database, rows, values);
}

static int load_builtin_table_status_values(
    struct mylite_db *database,
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name,
    struct table_status_values *status
) {
    int rc = format_builtin_table_status_timestamp(database, status);

    if (rc == MYLITE_OK && builtin_schema_table_has_update_time(directory, table_name)) {
        memcpy(
            status->update_time_text,
            status->create_time_text,
            sizeof(status->update_time_text)
        );
        status->update_time = status->update_time_text;
    }
    return rc;
}

static const struct mylite_execution_catalog_builtin_schema_table_directory *find_builtin_schema_table_directory(
    const char *schema_name
) {
    return mylite_execution_catalog_builtin_schema_table_directory_by_name(schema_name);
}

static const char *builtin_schema_table_type(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    if (directory == NULL || table_name == NULL) {
        return NULL;
    }
    if (strcmp(directory->schema_name, "information_schema") == 0) {
        return "SYSTEM VIEW";
    }
    if (strcmp(directory->schema_name, "sys") == 0 && strcmp(table_name, "sys_config") != 0) {
        return "VIEW";
    }
    return "BASE TABLE";
}

static const char *builtin_schema_table_engine(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    if (directory == NULL || table_name == NULL) {
        return NULL;
    }
    if (strcmp(directory->schema_name, "information_schema") == 0) {
        return NULL;
    }
    if (strcmp(directory->schema_name, "sys") == 0) {
        return strcmp(table_name, "sys_config") == 0 ? "InnoDB" : NULL;
    }
    if (strcmp(directory->schema_name, "performance_schema") == 0) {
        return "PERFORMANCE_SCHEMA";
    }
    if (strcmp(table_name, "general_log") == 0 || strcmp(table_name, "slow_log") == 0) {
        return "CSV";
    }
    return "InnoDB";
}

static const char *builtin_schema_table_version(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    const char *table_type = builtin_schema_table_type(directory, table_name);

    if (table_type == NULL || strcmp(table_type, "VIEW") == 0) {
        return NULL;
    }
    return "10";
}

static const char *builtin_schema_table_row_format(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    if (directory == NULL || table_name == NULL) {
        return NULL;
    }
    if (strcmp(directory->schema_name, "information_schema") == 0) {
        return NULL;
    }
    if (strcmp(directory->schema_name, "sys") == 0) {
        return strcmp(table_name, "sys_config") == 0 ? "Dynamic" : NULL;
    }
    if (strcmp(directory->schema_name, "performance_schema") == 0) {
        if (strcmp(table_name, "accounts") == 0 ||
            strcmp(table_name, "events_statements_histogram_global") == 0 ||
            strcmp(table_name, "hosts") == 0 || strcmp(table_name, "performance_timers") == 0 ||
            strcmp(table_name, "replication_applier_status") == 0 ||
            strcmp(table_name, "replication_asynchronous_connection_failover") == 0 ||
            strcmp(table_name, "replication_group_members") == 0 ||
            strcmp(table_name, "setup_actors") == 0 || strcmp(table_name, "users") == 0) {
            return "Fixed";
        }
    }
    return "Dynamic";
}

static const char *builtin_schema_table_rows(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    const char *table_type = builtin_schema_table_type(directory, table_name);

    if (table_type == NULL || strcmp(table_type, "VIEW") == 0) {
        return NULL;
    }
    if (strcmp(directory->schema_name, "mysql") == 0) {
        const char *rows = builtin_mysql_table_rows(table_name);

        if (rows != NULL) {
            return rows;
        }
    }
    if (strcmp(directory->schema_name, "performance_schema") == 0 &&
        strcmp(table_name, "setup_actors") == 0) {
        return "128";
    }
    if (strcmp(directory->schema_name, "performance_schema") == 0 &&
        strcmp(table_name, "setup_consumers") == 0) {
        return "16";
    }
    if (strcmp(directory->schema_name, "performance_schema") == 0 &&
        strcmp(table_name, "performance_timers") == 0) {
        return "5";
    }
    if (strcmp(directory->schema_name, "sys") == 0 && strcmp(table_name, "sys_config") == 0) {
        return "6";
    }
    return "0";
}

static const char *builtin_mysql_table_rows(const char *table_name) {
    static const struct mysql_table_row_count {
        const char *table_name;
        const char *rows;
    } row_counts[] = {
        {"db", "2"},
        {"engine_cost", "2"},
        {"general_log", "2"},
        {"global_grants", "103"},
        {"help_category", "53"},
        {"help_keyword", "1142"},
        {"help_relation", "1608"},
        {"help_topic", "902"},
        {"innodb_index_stats", "6"},
        {"innodb_table_stats", "2"},
        {"plugin", "2"},
        {"proxies_priv", "1"},
        {"server_cost", "6"},
        {"slow_log", "2"},
        {"tables_priv", "2"},
        {"time_zone", "1457"},
        {"time_zone_leap_second", "0"},
        {"time_zone_name", "1712"},
        {"time_zone_transition", "119074"},
        {"time_zone_transition_type", "10529"},
        {"user", "5"},
    };

    for (size_t index = 0U; index < sizeof(row_counts) / sizeof(row_counts[0]); ++index) {
        if (strcmp(table_name, row_counts[index].table_name) == 0) {
            return row_counts[index].rows;
        }
    }
    return NULL;
}

static const char *builtin_schema_table_average_row_length(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    const char *table_type = builtin_schema_table_type(directory, table_name);

    if (table_type == NULL || strcmp(table_type, "VIEW") == 0) {
        return NULL;
    }
    if (builtin_schema_table_is_mysql_stats(directory, table_name)) {
        return strcmp(table_name, "innodb_index_stats") == 0 ? "2730" : "8192";
    }
    if (builtin_schema_table_is_mysql_cost(directory, table_name)) {
        return strcmp(table_name, "server_cost") == 0 ? "2730" : "8192";
    }
    if (builtin_schema_table_is_mysql_plugin(directory, table_name)) {
        return "8192";
    }
    if (strcmp(directory->schema_name, "mysql") == 0 &&
        (strcmp(table_name, "db") == 0 || strcmp(table_name, "tables_priv") == 0)) {
        return "8192";
    }
    if (strcmp(directory->schema_name, "mysql") == 0 && strcmp(table_name, "proxies_priv") == 0) {
        return "16384";
    }
    if (strcmp(directory->schema_name, "mysql") == 0 && strcmp(table_name, "global_grants") == 0) {
        return "795";
    }
    if (strcmp(directory->schema_name, "mysql") == 0 && strcmp(table_name, "user") == 0) {
        return "3276";
    }
    if (builtin_schema_table_is_mysql_help(directory, table_name)) {
        return builtin_schema_mysql_help_average_row_length(table_name);
    }
    if (builtin_schema_table_is_mysql_time_zone(directory, table_name)) {
        return builtin_schema_mysql_time_zone_average_row_length(table_name);
    }
    if (strcmp(directory->schema_name, "sys") == 0 && strcmp(table_name, "sys_config") == 0) {
        return "2730";
    }
    return "0";
}

static const char *builtin_schema_mysql_help_average_row_length(const char *table_name) {
    if (strcmp(table_name, "help_category") == 0) {
        return "309";
    }
    if (strcmp(table_name, "help_keyword") == 0) {
        return "114";
    }
    if (strcmp(table_name, "help_relation") == 0) {
        return "50";
    }
    return "1761";
}

static const char *builtin_schema_mysql_time_zone_average_row_length(const char *table_name) {
    if (strcmp(table_name, "time_zone") == 0) {
        return "56";
    }
    if (strcmp(table_name, "time_zone_name") == 0) {
        return "153";
    }
    if (strcmp(table_name, "time_zone_transition") == 0) {
        return "39";
    }
    if (strcmp(table_name, "time_zone_transition_type") == 0) {
        return "45";
    }
    return "0";
}

static const char *builtin_schema_table_data_length(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    const char *table_type = builtin_schema_table_type(directory, table_name);

    if (table_type == NULL || strcmp(table_type, "VIEW") == 0) {
        return NULL;
    }
    if (strcmp(directory->schema_name, "mysql") == 0 &&
        (strcmp(table_name, "general_log") == 0 || strcmp(table_name, "slow_log") == 0)) {
        return "0";
    }
    if (strcmp(directory->schema_name, "mysql") == 0 && strcmp(table_name, "global_grants") == 0) {
        return "81920";
    }
    if (builtin_schema_table_is_mysql_time_zone(directory, table_name)) {
        if (strcmp(table_name, "time_zone") == 0) {
            return "81920";
        }
        if (strcmp(table_name, "time_zone_name") == 0) {
            return "262144";
        }
        if (strcmp(table_name, "time_zone_transition") == 0) {
            return "4734976";
        }
        if (strcmp(table_name, "time_zone_transition_type") == 0) {
            return "475136";
        }
    }
    if (builtin_schema_table_is_mysql_help(directory, table_name)) {
        if (strcmp(table_name, "help_category") == 0) {
            return "16384";
        }
        if (strcmp(table_name, "help_keyword") == 0) {
            return "131072";
        }
        if (strcmp(table_name, "help_relation") == 0) {
            return "81920";
        }
        return "1589248";
    }
    if (strcmp(directory->schema_name, "performance_schema") == 0 ||
        strcmp(directory->schema_name, "information_schema") == 0) {
        return "0";
    }
    return "16384";
}

static const char *builtin_schema_table_index_length(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    const char *table_type = builtin_schema_table_type(directory, table_name);

    if (table_type == NULL || strcmp(table_type, "VIEW") == 0) {
        return NULL;
    }
    if (!builtin_schema_table_is_mysql_help(directory, table_name)) {
        return strcmp(directory->schema_name, "mysql") == 0 &&
                       (strcmp(table_name, "db") == 0 || strcmp(table_name, "procs_priv") == 0 ||
                        strcmp(table_name, "proxies_priv") == 0 ||
                        strcmp(table_name, "tables_priv") == 0)
                   ? "16384"
                   : "0";
    }
    if (strcmp(table_name, "help_category") == 0) {
        return "16384";
    }
    if (strcmp(table_name, "help_keyword") == 0) {
        return "147456";
    }
    if (strcmp(table_name, "help_topic") == 0) {
        return "98304";
    }
    return "0";
}

static const char *builtin_schema_table_data_free(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    const char *table_type = builtin_schema_table_type(directory, table_name);

    if (table_type == NULL || strcmp(table_type, "VIEW") == 0) {
        return NULL;
    }
    return builtin_schema_table_is_mysql_stats(directory, table_name) ||
                   builtin_schema_table_is_mysql_cost(directory, table_name) ||
                   builtin_schema_table_is_mysql_component(directory, table_name) ||
                   builtin_schema_table_is_mysql_func(directory, table_name) ||
                   builtin_schema_table_is_mysql_gtid_executed(directory, table_name) ||
                   builtin_schema_table_is_mysql_help(directory, table_name) ||
                   builtin_schema_table_is_mysql_ndb_binlog_index(directory, table_name) ||
                   builtin_schema_table_is_mysql_plugin(directory, table_name) ||
                   builtin_schema_table_is_mysql_replication_metadata(directory, table_name) ||
                   builtin_schema_table_is_mysql_servers(directory, table_name) ||
                   builtin_schema_table_is_mysql_time_zone(directory, table_name) ||
                   (strcmp(directory->schema_name, "mysql") == 0 &&
                    strcmp(table_name, "columns_priv") == 0) ||
                   (strcmp(directory->schema_name, "mysql") == 0 &&
                    strcmp(table_name, "default_roles") == 0) ||
                   (strcmp(directory->schema_name, "mysql") == 0 &&
                    strcmp(table_name, "password_history") == 0) ||
                   (strcmp(directory->schema_name, "mysql") == 0 &&
                    strcmp(table_name, "role_edges") == 0) ||
                   (strcmp(directory->schema_name, "mysql") == 0 &&
                    strcmp(table_name, "procs_priv") == 0) ||
                   (strcmp(directory->schema_name, "mysql") == 0 &&
                    strcmp(table_name, "proxies_priv") == 0) ||
                   (strcmp(directory->schema_name, "mysql") == 0 &&
                    (strcmp(table_name, "db") == 0 || strcmp(table_name, "tables_priv") == 0)) ||
                   (strcmp(directory->schema_name, "mysql") == 0 &&
                    strcmp(table_name, "global_grants") == 0) ||
                   (strcmp(directory->schema_name, "mysql") == 0 && strcmp(table_name, "user") == 0)
               ? "4194304"
               : "0";
}

static const char *builtin_schema_table_auto_increment(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    if (builtin_schema_table_is_mysql_component(directory, table_name)) {
        return "1";
    }
    return builtin_schema_table_is_mysql_time_zone(directory, table_name) &&
                   strcmp(table_name, "time_zone") == 0
               ? "1796"
               : NULL;
}

static const char *builtin_schema_table_collation(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    const char *table_type = builtin_schema_table_type(directory, table_name);

    if (table_type == NULL || strcmp(table_type, "VIEW") == 0 ||
        strcmp(directory->schema_name, "information_schema") == 0) {
        return NULL;
    }
    if (strcmp(directory->schema_name, "sys") == 0) {
        return "utf8mb4_0900_ai_ci";
    }
    if (strcmp(directory->schema_name, "performance_schema") == 0) {
        if (strcmp(table_name, "keyring_keys") == 0 ||
            strcmp(table_name, "session_account_connect_attrs") == 0 ||
            strcmp(table_name, "session_connect_attrs") == 0) {
            return "utf8mb4_bin";
        }
        return "utf8mb4_0900_ai_ci";
    }
    if (strcmp(table_name, "ndb_binlog_index") == 0) {
        return "latin1_swedish_ci";
    }
    if (strcmp(table_name, "gtid_executed") == 0 ||
        strcmp(table_name, "replication_group_configuration_version") == 0 ||
        strcmp(table_name, "replication_group_member_actions") == 0) {
        return "utf8mb4_0900_ai_ci";
    }
    if (strcmp(table_name, "columns_priv") == 0 || strcmp(table_name, "db") == 0 ||
        strcmp(table_name, "default_roles") == 0 || strcmp(table_name, "func") == 0 ||
        strcmp(table_name, "global_grants") == 0 || strcmp(table_name, "innodb_index_stats") == 0 ||
        strcmp(table_name, "innodb_table_stats") == 0 ||
        strcmp(table_name, "password_history") == 0 || strcmp(table_name, "procs_priv") == 0 ||
        strcmp(table_name, "proxies_priv") == 0 || strcmp(table_name, "role_edges") == 0 ||
        strcmp(table_name, "tables_priv") == 0 || strcmp(table_name, "user") == 0) {
        return "utf8mb3_bin";
    }
    return "utf8mb3_general_ci";
}

static const char *builtin_schema_table_create_options(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    if (directory == NULL || table_name == NULL) {
        return NULL;
    }
    if (strcmp(directory->schema_name, "mysql") == 0) {
        if (strcmp(table_name, "general_log") == 0 || strcmp(table_name, "slow_log") == 0) {
            return "";
        }
        if (strcmp(table_name, "component") == 0) {
            return "row_format=DYNAMIC";
        }
        return "row_format=DYNAMIC stats_persistent=0";
    }
    return strcmp(builtin_schema_table_type(directory, table_name), "VIEW") == 0 ? NULL : "";
}

static const char *builtin_schema_table_comment(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    if (directory == NULL || table_name == NULL) {
        return NULL;
    }
    if (strcmp(builtin_schema_table_type(directory, table_name), "VIEW") == 0) {
        return "VIEW";
    }
    if (strcmp(directory->schema_name, "mysql") == 0) {
        const char *comment = builtin_mysql_table_comment(table_name);

        if (comment != NULL) {
            return comment;
        }
    }
    return "";
}

static const char *builtin_mysql_table_comment(const char *table_name) {
    static const struct mysql_table_comment {
        const char *table_name;
        const char *comment;
    } comments[] = {
        {"columns_priv", "Column privileges"},
        {"component", "Components"},
        {"db", "Database privileges"},
        {"default_roles", "Default roles"},
        {"func", "User defined functions"},
        {"general_log", "General log"},
        {"global_grants", "Extended global grants"},
        {"help_category", "help categories"},
        {"help_keyword", "help keywords"},
        {"help_relation", "keyword-topic relation"},
        {"help_topic", "help topics"},
        {"password_history", "Password history for user accounts"},
        {"plugin", "MySQL plugins"},
        {"procs_priv", "Procedure privileges"},
        {"proxies_priv", "User proxy privileges"},
        {"role_edges", "Role hierarchy and role grants"},
        {"servers", "MySQL Foreign Servers table"},
        {"slave_master_info", "Master Information"},
        {"slave_relay_log_info", "Relay Log Information"},
        {"slave_worker_info", "Worker Information"},
        {"slow_log", "Slow log"},
        {"tables_priv", "Table privileges"},
        {"time_zone", "Time zones"},
        {"time_zone_leap_second", "Leap seconds information for time zones"},
        {"time_zone_name", "Time zone names"},
        {"time_zone_transition", "Time zone transitions"},
        {"time_zone_transition_type", "Time zone transition types"},
        {"user", "Users and global privileges"},
    };

    for (size_t index = 0U; index < sizeof(comments) / sizeof(comments[0]); ++index) {
        if (strcmp(table_name, comments[index].table_name) == 0) {
            return comments[index].comment;
        }
    }
    return NULL;
}

static bool builtin_schema_table_has_update_time(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    return builtin_schema_table_is_mysql_stats(directory, table_name) ||
           builtin_schema_table_is_mysql_plugin(directory, table_name);
}

static bool builtin_schema_table_is_mysql_stats(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    return directory != NULL && table_name != NULL &&
           strcmp(directory->schema_name, "mysql") == 0 &&
           (strcmp(table_name, "innodb_index_stats") == 0 ||
            strcmp(table_name, "innodb_table_stats") == 0);
}

static bool builtin_schema_table_is_mysql_cost(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    return directory != NULL && table_name != NULL &&
           strcmp(directory->schema_name, "mysql") == 0 &&
           (strcmp(table_name, "engine_cost") == 0 || strcmp(table_name, "server_cost") == 0);
}

static bool builtin_schema_table_is_mysql_component(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    return directory != NULL && table_name != NULL &&
           strcmp(directory->schema_name, "mysql") == 0 && strcmp(table_name, "component") == 0;
}

static bool builtin_schema_table_is_mysql_func(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    return directory != NULL && table_name != NULL &&
           strcmp(directory->schema_name, "mysql") == 0 && strcmp(table_name, "func") == 0;
}

static bool builtin_schema_table_is_mysql_gtid_executed(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    return directory != NULL && table_name != NULL &&
           strcmp(directory->schema_name, "mysql") == 0 && strcmp(table_name, "gtid_executed") == 0;
}

static bool builtin_schema_table_is_mysql_help(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    return directory != NULL && table_name != NULL &&
           strcmp(directory->schema_name, "mysql") == 0 &&
           (strcmp(table_name, "help_category") == 0 || strcmp(table_name, "help_keyword") == 0 ||
            strcmp(table_name, "help_relation") == 0 || strcmp(table_name, "help_topic") == 0);
}

static bool builtin_schema_table_is_mysql_ndb_binlog_index(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    return directory != NULL && table_name != NULL &&
           strcmp(directory->schema_name, "mysql") == 0 &&
           strcmp(table_name, "ndb_binlog_index") == 0;
}

static bool builtin_schema_table_is_mysql_replication_metadata(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    return directory != NULL && table_name != NULL &&
           strcmp(directory->schema_name, "mysql") == 0 &&
           (strcmp(table_name, "slave_master_info") == 0 ||
            strcmp(table_name, "slave_relay_log_info") == 0 ||
            strcmp(table_name, "slave_worker_info") == 0);
}

static bool builtin_schema_table_is_mysql_plugin(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    return directory != NULL && table_name != NULL &&
           strcmp(directory->schema_name, "mysql") == 0 && strcmp(table_name, "plugin") == 0;
}

static bool builtin_schema_table_is_mysql_servers(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    return directory != NULL && table_name != NULL &&
           strcmp(directory->schema_name, "mysql") == 0 && strcmp(table_name, "servers") == 0;
}

static bool builtin_schema_table_is_mysql_time_zone(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    return directory != NULL && table_name != NULL &&
           strcmp(directory->schema_name, "mysql") == 0 &&
           (strcmp(table_name, "time_zone") == 0 ||
            strcmp(table_name, "time_zone_leap_second") == 0 ||
            strcmp(table_name, "time_zone_name") == 0 ||
            strcmp(table_name, "time_zone_transition") == 0 ||
            strcmp(table_name, "time_zone_transition_type") == 0);
}
