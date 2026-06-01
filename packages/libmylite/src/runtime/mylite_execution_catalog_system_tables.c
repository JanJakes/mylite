#include "mylite_execution_catalog.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

static const struct mylite_execution_catalog_column_definition mysql_user_columns[] = {
    {"Host",
     "",
     "NO",
     "char",
     "255",
     "255",
     NULL,
     NULL,
     NULL,
     "ascii",
     "ascii_general_ci",
     "char(255)"},
    {"User", "", "NO", "char", "32", "96", NULL, NULL, NULL, "utf8mb3", "utf8mb3_bin", "char(32)"},
    {"Select_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Insert_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Update_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Delete_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Create_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Drop_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Reload_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Shutdown_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Process_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"File_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Grant_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"References_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Index_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Alter_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Show_db_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Super_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Create_tmp_table_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Lock_tables_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Execute_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Repl_slave_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Repl_client_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Create_view_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Show_view_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Create_routine_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Alter_routine_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Create_user_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Event_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Trigger_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Create_tablespace_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"ssl_type",
     "",
     "NO",
     "enum",
     "9",
     "27",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('','ANY','X509','SPECIFIED')"},
    {"ssl_cipher", NULL, "NO", "blob", "65535", "65535", NULL, NULL, NULL, NULL, NULL, "blob"},
    {"x509_issuer", NULL, "NO", "blob", "65535", "65535", NULL, NULL, NULL, NULL, NULL, "blob"},
    {"x509_subject", NULL, "NO", "blob", "65535", "65535", NULL, NULL, NULL, NULL, NULL, "blob"},
    {"max_questions", "0", "NO", "int", NULL, NULL, "10", "0", NULL, NULL, NULL, "int unsigned"},
    {"max_updates", "0", "NO", "int", NULL, NULL, "10", "0", NULL, NULL, NULL, "int unsigned"},
    {"max_connections", "0", "NO", "int", NULL, NULL, "10", "0", NULL, NULL, NULL, "int unsigned"},
    {"max_user_connections",
     "0",
     "NO",
     "int",
     NULL,
     NULL,
     "10",
     "0",
     NULL,
     NULL,
     NULL,
     "int unsigned"},
    {"plugin",
     "caching_sha2_password",
     "NO",
     "char",
     "64",
     "192",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_bin",
     "char(64)"},
    {"authentication_string",
     NULL,
     "YES",
     "text",
     "65535",
     "65535",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_bin",
     "text"},
    {"password_expired",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"password_last_changed",
     NULL,
     "YES",
     "timestamp",
     NULL,
     NULL,
     NULL,
     NULL,
     "0",
     NULL,
     NULL,
     "timestamp"},
    {"password_lifetime",
     NULL,
     "YES",
     "smallint",
     NULL,
     NULL,
     "5",
     "0",
     NULL,
     NULL,
     NULL,
     "smallint unsigned"},
    {"account_locked",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Create_role_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Drop_role_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Password_reuse_history",
     NULL,
     "YES",
     "smallint",
     NULL,
     NULL,
     "5",
     "0",
     NULL,
     NULL,
     NULL,
     "smallint unsigned"},
    {"Password_reuse_time",
     NULL,
     "YES",
     "smallint",
     NULL,
     NULL,
     "5",
     "0",
     NULL,
     NULL,
     NULL,
     "smallint unsigned"},
    {"Password_require_current",
     NULL,
     "YES",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"User_attributes", NULL, "YES", "json", NULL, NULL, NULL, NULL, NULL, NULL, NULL, "json"},
};

static const char *const mysql_user_column_keys[] = {
    "PRI", "PRI", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "",    "",    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "",    "",    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
};

static const char *const mysql_user_column_extras[] = {
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
};

static const char *const mysql_user_column_privileges[] = {
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_column_definition mysql_global_grants_columns[] = {
    {"USER", "", "NO", "char", "32", "96", NULL, NULL, NULL, "utf8mb3", "utf8mb3_bin", "char(32)"},
    {"HOST",
     "",
     "NO",
     "char",
     "255",
     "255",
     NULL,
     NULL,
     NULL,
     "ascii",
     "ascii_general_ci",
     "char(255)"},
    {"PRIV",
     "",
     "NO",
     "char",
     "32",
     "96",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "char(32)"},
    {"WITH_GRANT_OPTION",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
};

static const char *const mysql_global_grants_column_keys[] = {
    "PRI",
    "PRI",
    "PRI",
    "",
};

static const char *const mysql_global_grants_column_extras[] = {
    "",
    "",
    "",
    "",
};

static const char *const mysql_global_grants_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_column_definition mysql_db_columns[] = {
    {"Host",
     "",
     "NO",
     "char",
     "255",
     "255",
     NULL,
     NULL,
     NULL,
     "ascii",
     "ascii_general_ci",
     "char(255)"},
    {"Db", "", "NO", "char", "64", "192", NULL, NULL, NULL, "utf8mb3", "utf8mb3_bin", "char(64)"},
    {"User", "", "NO", "char", "32", "96", NULL, NULL, NULL, "utf8mb3", "utf8mb3_bin", "char(32)"},
    {"Select_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Insert_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Update_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Delete_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Create_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Drop_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Grant_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"References_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Index_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Alter_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Create_tmp_table_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Lock_tables_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Create_view_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Show_view_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Create_routine_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Alter_routine_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Execute_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Event_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
    {"Trigger_priv",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
};

static const char *const mysql_db_column_keys[] = {
    "PRI", "PRI", "PRI", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
};

static const char *const mysql_db_column_extras[] = {
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
};

static const char *const mysql_db_column_privileges[] = {
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
};

static const size_t mysql_db_primary_key_column_indexes[] = {
    0U,
    2U,
    1U,
};

static const struct mylite_execution_catalog_mysql_system_secondary_index
    mysql_db_secondary_indexes[] = {
        {"User", 2U, "2", "1", false},
};

static const struct mylite_execution_catalog_column_definition mysql_tables_priv_columns[] = {
    {"Host",
     "",
     "NO",
     "char",
     "255",
     "255",
     NULL,
     NULL,
     NULL,
     "ascii",
     "ascii_general_ci",
     "char(255)"},
    {"Db", "", "NO", "char", "64", "192", NULL, NULL, NULL, "utf8mb3", "utf8mb3_bin", "char(64)"},
    {"User", "", "NO", "char", "32", "96", NULL, NULL, NULL, "utf8mb3", "utf8mb3_bin", "char(32)"},
    {"Table_name",
     "",
     "NO",
     "char",
     "64",
     "192",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_bin",
     "char(64)"},
    {"Grantor",
     "",
     "NO",
     "varchar",
     "288",
     "864",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_bin",
     "varchar(288)"},
    {"Timestamp",
     "CURRENT_TIMESTAMP",
     "NO",
     "timestamp",
     NULL,
     NULL,
     NULL,
     NULL,
     "0",
     NULL,
     NULL,
     "timestamp"},
    {"Table_priv",
     "",
     "NO",
     "set",
     "98",
     "294",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "set('Select','Insert','Update','Delete','Create','Drop','Grant','References','Index','Alter',"
     "'Create View','Show view','Trigger')"},
    {"Column_priv",
     "",
     "NO",
     "set",
     "31",
     "93",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "set('Select','Insert','Update','References')"},
};

static const char *const mysql_tables_priv_column_keys[] = {
    "PRI",
    "PRI",
    "PRI",
    "PRI",
    "MUL",
    "",
    "",
    "",
};

static const char *const mysql_tables_priv_column_extras[] = {
    "",
    "",
    "",
    "",
    "",
    "DEFAULT_GENERATED on update CURRENT_TIMESTAMP",
    "",
    "",
};

static const char *const mysql_tables_priv_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const size_t mysql_tables_priv_primary_key_column_indexes[] = {
    0U,
    2U,
    1U,
    3U,
};

static const struct mylite_execution_catalog_mysql_system_secondary_index
    mysql_tables_priv_secondary_indexes[] = {
        {"Grantor", 4U, "2", "1", false},
};

static const struct mylite_execution_catalog_column_definition mysql_columns_priv_columns[] = {
    {"Host",
     "",
     "NO",
     "char",
     "255",
     "255",
     NULL,
     NULL,
     NULL,
     "ascii",
     "ascii_general_ci",
     "char(255)"},
    {"Db", "", "NO", "char", "64", "192", NULL, NULL, NULL, "utf8mb3", "utf8mb3_bin", "char(64)"},
    {"User", "", "NO", "char", "32", "96", NULL, NULL, NULL, "utf8mb3", "utf8mb3_bin", "char(32)"},
    {"Table_name",
     "",
     "NO",
     "char",
     "64",
     "192",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_bin",
     "char(64)"},
    {"Column_name",
     "",
     "NO",
     "char",
     "64",
     "192",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_bin",
     "char(64)"},
    {"Timestamp",
     "CURRENT_TIMESTAMP",
     "NO",
     "timestamp",
     NULL,
     NULL,
     NULL,
     NULL,
     "0",
     NULL,
     NULL,
     "timestamp"},
    {"Column_priv",
     "",
     "NO",
     "set",
     "31",
     "93",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "set('Select','Insert','Update','References')"},
};

static const char *const mysql_columns_priv_column_keys[] = {
    "PRI",
    "PRI",
    "PRI",
    "PRI",
    "PRI",
    "",
    "",
};

static const char *const mysql_columns_priv_column_extras[] = {
    "",
    "",
    "",
    "",
    "",
    "DEFAULT_GENERATED on update CURRENT_TIMESTAMP",
    "",
};

static const char *const mysql_columns_priv_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const size_t mysql_columns_priv_primary_key_column_indexes[] = {
    0U,
    2U,
    1U,
    3U,
    4U,
};

static const struct mylite_execution_catalog_column_definition mysql_procs_priv_columns[] = {
    {"Host",
     "",
     "NO",
     "char",
     "255",
     "255",
     NULL,
     NULL,
     NULL,
     "ascii",
     "ascii_general_ci",
     "char(255)"},
    {"Db", "", "NO", "char", "64", "192", NULL, NULL, NULL, "utf8mb3", "utf8mb3_bin", "char(64)"},
    {"User", "", "NO", "char", "32", "96", NULL, NULL, NULL, "utf8mb3", "utf8mb3_bin", "char(32)"},
    {"Routine_name",
     "",
     "NO",
     "char",
     "64",
     "192",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "char(64)"},
    {"Routine_type",
     NULL,
     "NO",
     "enum",
     "9",
     "27",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_bin",
     "enum('FUNCTION','PROCEDURE')"},
    {"Grantor",
     "",
     "NO",
     "varchar",
     "288",
     "864",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_bin",
     "varchar(288)"},
    {"Proc_priv",
     "",
     "NO",
     "set",
     "27",
     "81",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "set('Execute','Alter Routine','Grant')"},
    {"Timestamp",
     "CURRENT_TIMESTAMP",
     "NO",
     "timestamp",
     NULL,
     NULL,
     NULL,
     NULL,
     "0",
     NULL,
     NULL,
     "timestamp"},
};

static const char *const mysql_procs_priv_column_keys[] = {
    "PRI",
    "PRI",
    "PRI",
    "PRI",
    "PRI",
    "MUL",
    "",
    "",
};

static const char *const mysql_procs_priv_column_extras[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "DEFAULT_GENERATED on update CURRENT_TIMESTAMP",
};

static const char *const mysql_procs_priv_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const size_t mysql_procs_priv_primary_key_column_indexes[] = {
    0U,
    2U,
    1U,
    3U,
    4U,
};

static const struct mylite_execution_catalog_mysql_system_secondary_index
    mysql_procs_priv_secondary_indexes[] = {
        {"Grantor", 5U, "0", "1", false},
};

static const struct mylite_execution_catalog_column_definition mysql_proxies_priv_columns[] = {
    {"Host",
     "",
     "NO",
     "char",
     "255",
     "255",
     NULL,
     NULL,
     NULL,
     "ascii",
     "ascii_general_ci",
     "char(255)"},
    {"User", "", "NO", "char", "32", "96", NULL, NULL, NULL, "utf8mb3", "utf8mb3_bin", "char(32)"},
    {"Proxied_host",
     "",
     "NO",
     "char",
     "255",
     "255",
     NULL,
     NULL,
     NULL,
     "ascii",
     "ascii_general_ci",
     "char(255)"},
    {"Proxied_user",
     "",
     "NO",
     "char",
     "32",
     "96",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_bin",
     "char(32)"},
    {"With_grant", "0", "NO", "tinyint", NULL, NULL, "3", "0", NULL, NULL, NULL, "tinyint(1)"},
    {"Grantor",
     "",
     "NO",
     "varchar",
     "288",
     "864",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_bin",
     "varchar(288)"},
    {"Timestamp",
     "CURRENT_TIMESTAMP",
     "NO",
     "timestamp",
     NULL,
     NULL,
     NULL,
     NULL,
     "0",
     NULL,
     NULL,
     "timestamp"},
};

static const char *const mysql_proxies_priv_column_keys[] = {
    "PRI",
    "PRI",
    "PRI",
    "PRI",
    "",
    "MUL",
    "",
};

static const char *const mysql_proxies_priv_column_extras[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "DEFAULT_GENERATED on update CURRENT_TIMESTAMP",
};

static const char *const mysql_proxies_priv_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const size_t mysql_proxies_priv_primary_key_column_indexes[] = {
    0U,
    1U,
    2U,
    3U,
};

static const struct mylite_execution_catalog_mysql_system_secondary_index
    mysql_proxies_priv_secondary_indexes[] = {
        {"Grantor", 5U, "1", "1", false},
};

static const struct mylite_execution_catalog_column_definition mysql_default_roles_columns[] = {
    {"HOST",
     "",
     "NO",
     "char",
     "255",
     "255",
     NULL,
     NULL,
     NULL,
     "ascii",
     "ascii_general_ci",
     "char(255)"},
    {"USER", "", "NO", "char", "32", "96", NULL, NULL, NULL, "utf8mb3", "utf8mb3_bin", "char(32)"},
    {"DEFAULT_ROLE_HOST",
     "%",
     "NO",
     "char",
     "255",
     "255",
     NULL,
     NULL,
     NULL,
     "ascii",
     "ascii_general_ci",
     "char(255)"},
    {"DEFAULT_ROLE_USER",
     "",
     "NO",
     "char",
     "32",
     "96",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_bin",
     "char(32)"},
};

static const char *const mysql_default_roles_column_keys[] = {
    "PRI",
    "PRI",
    "PRI",
    "PRI",
};

static const char *const mysql_default_roles_column_extras[] = {
    "",
    "",
    "",
    "",
};

static const char *const mysql_default_roles_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const size_t mysql_default_roles_primary_key_column_indexes[] = {
    0U,
    1U,
    2U,
    3U,
};

static const struct mylite_execution_catalog_column_definition mysql_role_edges_columns[] = {
    {"FROM_HOST",
     "",
     "NO",
     "char",
     "255",
     "255",
     NULL,
     NULL,
     NULL,
     "ascii",
     "ascii_general_ci",
     "char(255)"},
    {"FROM_USER",
     "",
     "NO",
     "char",
     "32",
     "96",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_bin",
     "char(32)"},
    {"TO_HOST",
     "",
     "NO",
     "char",
     "255",
     "255",
     NULL,
     NULL,
     NULL,
     "ascii",
     "ascii_general_ci",
     "char(255)"},
    {"TO_USER",
     "",
     "NO",
     "char",
     "32",
     "96",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_bin",
     "char(32)"},
    {"WITH_ADMIN_OPTION",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('N','Y')"},
};

static const char *const mysql_role_edges_column_keys[] = {
    "PRI",
    "PRI",
    "PRI",
    "PRI",
    "",
};

static const char *const mysql_role_edges_column_extras[] = {
    "",
    "",
    "",
    "",
    "",
};

static const char *const mysql_role_edges_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const size_t mysql_role_edges_primary_key_column_indexes[] = {
    0U,
    1U,
    2U,
    3U,
};

static const struct mylite_execution_catalog_column_definition mysql_password_history_columns[] = {
    {"Host",
     "",
     "NO",
     "char",
     "255",
     "255",
     NULL,
     NULL,
     NULL,
     "ascii",
     "ascii_general_ci",
     "char(255)"},
    {"User", "", "NO", "char", "32", "96", NULL, NULL, NULL, "utf8mb3", "utf8mb3_bin", "char(32)"},
    {"Password_timestamp",
     "CURRENT_TIMESTAMP(6)",
     "NO",
     "timestamp",
     NULL,
     NULL,
     NULL,
     NULL,
     "6",
     NULL,
     NULL,
     "timestamp(6)"},
    {"Password",
     NULL,
     "YES",
     "text",
     "65535",
     "65535",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_bin",
     "text"},
};

static const char *const mysql_password_history_column_keys[] = {
    "PRI",
    "PRI",
    "PRI",
    "",
};

static const char *const mysql_password_history_column_extras[] = {
    "",
    "",
    "DEFAULT_GENERATED",
    "",
};

static const char *const mysql_password_history_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const size_t mysql_password_history_primary_key_column_indexes[] = {
    0U,
    1U,
    2U,
};

static const struct mylite_execution_catalog_column_definition sys_sys_config_columns[] = {
    {"variable",
     NULL,
     "NO",
     "varchar",
     "128",
     "512",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "varchar(128)"},
    {"value",
     NULL,
     "YES",
     "varchar",
     "128",
     "512",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "varchar(128)"},
    {"set_time",
     "CURRENT_TIMESTAMP",
     "YES",
     "timestamp",
     NULL,
     NULL,
     NULL,
     NULL,
     "0",
     NULL,
     NULL,
     "timestamp"},
    {"set_by",
     NULL,
     "YES",
     "varchar",
     "128",
     "512",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "varchar(128)"},
};

static const char *const sys_sys_config_column_keys[] = {
    "PRI",
    "",
    "",
    "",
};

static const char *const sys_sys_config_column_extras[] = {
    "",
    "",
    "DEFAULT_GENERATED on update CURRENT_TIMESTAMP",
    "",
};

static const char *const sys_sys_config_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const char sys_sys_config_trigger_action_statement[] =
    "BEGIN\n"
    "    IF @sys.ignore_sys_config_triggers != true AND NEW.set_by IS NULL THEN\n"
    "        SET NEW.set_by = USER();\n"
    "    END IF;\n"
    "END";

static const char sys_sys_config_trigger_sql_mode[] =
    "ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,"
    "ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION";

static const struct mylite_execution_catalog_sys_config_trigger sys_sys_config_triggers[] = {
    {"sys_config_insert_set_user", "INSERT"},
    {"sys_config_update_set_user", "UPDATE"},
};

static const struct mylite_execution_catalog_column_definition sys_version_columns[] = {
    {"sys_version",
     "",
     "NO",
     "varchar",
     "5",
     "20",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "varchar(5)"},
    {"mysql_version",
     "",
     "NO",
     "varchar",
     "5",
     "15",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "varchar(5)"},
};

static const char *const sys_version_column_keys[] = {
    "",
    "",
};

static const char *const sys_version_column_extras[] = {
    "",
    "",
};

static const char *const sys_version_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_column_definition sys_host_summary_columns[] = {
    {"host",
     NULL,
     "YES",
     "varchar",
     "255",
     "255",
     NULL,
     NULL,
     NULL,
     "ascii",
     "ascii_general_ci",
     "varchar(255)"},
    {"statements",
     NULL,
     "YES",
     "decimal",
     NULL,
     NULL,
     "64",
     "0",
     NULL,
     NULL,
     NULL,
     "decimal(64,0)"},
    {"statement_latency",
     NULL,
     "YES",
     "varchar",
     "11",
     "33",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "varchar(11)"},
    {"statement_avg_latency",
     NULL,
     "YES",
     "varchar",
     "11",
     "33",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "varchar(11)"},
    {"table_scans",
     NULL,
     "YES",
     "decimal",
     NULL,
     NULL,
     "65",
     "0",
     NULL,
     NULL,
     NULL,
     "decimal(65,0)"},
    {"file_ios", NULL, "YES", "decimal", NULL, NULL, "64", "0", NULL, NULL, NULL, "decimal(64,0)"},
    {"file_io_latency",
     NULL,
     "YES",
     "varchar",
     "11",
     "33",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "varchar(11)"},
    {"current_connections",
     NULL,
     "YES",
     "decimal",
     NULL,
     NULL,
     "41",
     "0",
     NULL,
     NULL,
     NULL,
     "decimal(41,0)"},
    {"total_connections",
     NULL,
     "YES",
     "decimal",
     NULL,
     NULL,
     "41",
     "0",
     NULL,
     NULL,
     NULL,
     "decimal(41,0)"},
    {"unique_users", "0", "NO", "bigint", NULL, NULL, "19", "0", NULL, NULL, NULL, "bigint"},
    {"current_memory",
     NULL,
     "YES",
     "varchar",
     "11",
     "33",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "varchar(11)"},
    {"total_memory_allocated",
     NULL,
     "YES",
     "varchar",
     "11",
     "33",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "varchar(11)"},
};

static const struct mylite_execution_catalog_column_definition sys_x_host_summary_columns[] = {
    {"host",
     NULL,
     "YES",
     "varchar",
     "255",
     "255",
     NULL,
     NULL,
     NULL,
     "ascii",
     "ascii_general_ci",
     "varchar(255)"},
    {"statements",
     NULL,
     "YES",
     "decimal",
     NULL,
     NULL,
     "64",
     "0",
     NULL,
     NULL,
     NULL,
     "decimal(64,0)"},
    {"statement_latency",
     NULL,
     "YES",
     "decimal",
     NULL,
     NULL,
     "64",
     "0",
     NULL,
     NULL,
     NULL,
     "decimal(64,0)"},
    {"statement_avg_latency",
     NULL,
     "YES",
     "decimal",
     NULL,
     NULL,
     "65",
     "4",
     NULL,
     NULL,
     NULL,
     "decimal(65,4)"},
    {"table_scans",
     NULL,
     "YES",
     "decimal",
     NULL,
     NULL,
     "65",
     "0",
     NULL,
     NULL,
     NULL,
     "decimal(65,0)"},
    {"file_ios", NULL, "YES", "decimal", NULL, NULL, "64", "0", NULL, NULL, NULL, "decimal(64,0)"},
    {"file_io_latency",
     NULL,
     "YES",
     "decimal",
     NULL,
     NULL,
     "64",
     "0",
     NULL,
     NULL,
     NULL,
     "decimal(64,0)"},
    {"current_connections",
     NULL,
     "YES",
     "decimal",
     NULL,
     NULL,
     "41",
     "0",
     NULL,
     NULL,
     NULL,
     "decimal(41,0)"},
    {"total_connections",
     NULL,
     "YES",
     "decimal",
     NULL,
     NULL,
     "41",
     "0",
     NULL,
     NULL,
     NULL,
     "decimal(41,0)"},
    {"unique_users", "0", "NO", "bigint", NULL, NULL, "19", "0", NULL, NULL, NULL, "bigint"},
    {"current_memory",
     NULL,
     "YES",
     "decimal",
     NULL,
     NULL,
     "63",
     "0",
     NULL,
     NULL,
     NULL,
     "decimal(63,0)"},
    {"total_memory_allocated",
     NULL,
     "YES",
     "decimal",
     NULL,
     NULL,
     "64",
     "0",
     NULL,
     NULL,
     NULL,
     "decimal(64,0)"},
};

static const char *const sys_host_summary_column_keys[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_host_summary_column_extras[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_host_summary_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_column_definition
    sys_host_summary_by_file_io_columns[] = {
        {"host",
         NULL,
         "YES",
         "varchar",
         "255",
         "255",
         NULL,
         NULL,
         NULL,
         "ascii",
         "ascii_general_ci",
         "varchar(255)"},
        {"ios", NULL, "YES", "decimal", NULL, NULL, "42", "0", NULL, NULL, NULL, "decimal(42,0)"},
        {"io_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
};

static const struct mylite_execution_catalog_column_definition
    sys_x_host_summary_by_file_io_columns[] = {
        {"host",
         NULL,
         "YES",
         "varchar",
         "255",
         "255",
         NULL,
         NULL,
         NULL,
         "ascii",
         "ascii_general_ci",
         "varchar(255)"},
        {"ios", NULL, "YES", "decimal", NULL, NULL, "42", "0", NULL, NULL, NULL, "decimal(42,0)"},
        {"io_latency",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "42",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(42,0)"},
};

static const char *const sys_host_summary_by_file_io_column_keys[] = {
    "",
    "",
    "",
};

static const char *const sys_host_summary_by_file_io_column_extras[] = {
    "",
    "",
    "",
};

static const char *const sys_host_summary_by_file_io_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_column_definition
    sys_host_summary_by_file_io_type_columns[] = {
        {"host",
         NULL,
         "YES",
         "varchar",
         "255",
         "255",
         NULL,
         NULL,
         NULL,
         "ascii",
         "ascii_general_ci",
         "varchar(255)"},
        {"event_name",
         NULL,
         "NO",
         "varchar",
         "128",
         "512",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(128)"},
        {"total", NULL, "NO", "bigint", NULL, NULL, "20", "0", NULL, NULL, NULL, "bigint unsigned"},
        {"total_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"max_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
};

static const struct mylite_execution_catalog_column_definition
    sys_x_host_summary_by_file_io_type_columns[] = {
        {"host",
         NULL,
         "YES",
         "varchar",
         "255",
         "255",
         NULL,
         NULL,
         NULL,
         "ascii",
         "ascii_general_ci",
         "varchar(255)"},
        {"event_name",
         NULL,
         "NO",
         "varchar",
         "128",
         "512",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(128)"},
        {"total", NULL, "NO", "bigint", NULL, NULL, "20", "0", NULL, NULL, NULL, "bigint unsigned"},
        {"total_latency",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"max_latency",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
};

static const char *const sys_host_summary_by_file_io_type_column_keys[] = {
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_host_summary_by_file_io_type_column_extras[] = {
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_host_summary_by_file_io_type_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_column_definition
    sys_host_summary_by_stages_columns[] = {
        {"host",
         NULL,
         "YES",
         "varchar",
         "255",
         "255",
         NULL,
         NULL,
         NULL,
         "ascii",
         "ascii_general_ci",
         "varchar(255)"},
        {"event_name",
         NULL,
         "NO",
         "varchar",
         "128",
         "512",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(128)"},
        {"total", NULL, "NO", "bigint", NULL, NULL, "20", "0", NULL, NULL, NULL, "bigint unsigned"},
        {"total_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"avg_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
};

static const struct mylite_execution_catalog_column_definition
    sys_x_host_summary_by_stages_columns[] = {
        {"host",
         NULL,
         "YES",
         "varchar",
         "255",
         "255",
         NULL,
         NULL,
         NULL,
         "ascii",
         "ascii_general_ci",
         "varchar(255)"},
        {"event_name",
         NULL,
         "NO",
         "varchar",
         "128",
         "512",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(128)"},
        {"total", NULL, "NO", "bigint", NULL, NULL, "20", "0", NULL, NULL, NULL, "bigint unsigned"},
        {"total_latency",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"avg_latency",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
};

static const char *const sys_host_summary_by_stages_column_keys[] = {
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_host_summary_by_stages_column_extras[] = {
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_host_summary_by_stages_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_column_definition
    sys_host_summary_by_statement_latency_columns[] = {
        {"host",
         NULL,
         "YES",
         "varchar",
         "255",
         "255",
         NULL,
         NULL,
         NULL,
         "ascii",
         "ascii_general_ci",
         "varchar(255)"},
        {"total", NULL, "YES", "decimal", NULL, NULL, "42", "0", NULL, NULL, NULL, "decimal(42,0)"},
        {"total_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"max_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"lock_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"cpu_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"rows_sent",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "42",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(42,0)"},
        {"rows_examined",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "42",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(42,0)"},
        {"rows_affected",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "42",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(42,0)"},
        {"full_scans",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "43",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(43,0)"},
};

static const struct mylite_execution_catalog_column_definition
    sys_x_host_summary_by_statement_latency_columns[] = {
        {"host",
         NULL,
         "YES",
         "varchar",
         "255",
         "255",
         NULL,
         NULL,
         NULL,
         "ascii",
         "ascii_general_ci",
         "varchar(255)"},
        {"total", NULL, "YES", "decimal", NULL, NULL, "42", "0", NULL, NULL, NULL, "decimal(42,0)"},
        {"total_latency",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "42",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(42,0)"},
        {"max_latency",
         NULL,
         "YES",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"lock_latency",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "42",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(42,0)"},
        {"cpu_latency",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "42",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(42,0)"},
        {"rows_sent",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "42",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(42,0)"},
        {"rows_examined",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "42",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(42,0)"},
        {"rows_affected",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "42",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(42,0)"},
        {"full_scans",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "43",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(43,0)"},
};

static const char *const sys_host_summary_by_statement_latency_column_keys[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_host_summary_by_statement_latency_column_extras[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_host_summary_by_statement_latency_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_column_definition
    sys_host_summary_by_statement_type_columns[] = {
        {"host",
         NULL,
         "YES",
         "varchar",
         "255",
         "255",
         NULL,
         NULL,
         NULL,
         "ascii",
         "ascii_general_ci",
         "varchar(255)"},
        {"statement",
         NULL,
         "YES",
         "varchar",
         "128",
         "512",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(128)"},
        {"total", NULL, "NO", "bigint", NULL, NULL, "20", "0", NULL, NULL, NULL, "bigint unsigned"},
        {"total_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"max_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"lock_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"cpu_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"rows_sent",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"rows_examined",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"rows_affected",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"full_scans",
         "0",
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
};

static const struct mylite_execution_catalog_column_definition
    sys_x_host_summary_by_statement_type_columns[] = {
        {"host",
         NULL,
         "YES",
         "varchar",
         "255",
         "255",
         NULL,
         NULL,
         NULL,
         "ascii",
         "ascii_general_ci",
         "varchar(255)"},
        {"statement",
         NULL,
         "YES",
         "varchar",
         "128",
         "512",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(128)"},
        {"total", NULL, "NO", "bigint", NULL, NULL, "20", "0", NULL, NULL, NULL, "bigint unsigned"},
        {"total_latency",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"max_latency",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"lock_latency",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"cpu_latency",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"rows_sent",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"rows_examined",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"rows_affected",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"full_scans",
         "0",
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
};

static const char *const sys_host_summary_by_statement_type_column_keys[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_host_summary_by_statement_type_column_extras[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_host_summary_by_statement_type_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_column_definition
    sys_memory_by_host_by_current_bytes_columns[] = {
        {"host",
         NULL,
         "YES",
         "varchar",
         "255",
         "255",
         NULL,
         NULL,
         NULL,
         "ascii",
         "ascii_general_ci",
         "varchar(255)"},
        {"current_count_used",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "41",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(41,0)"},
        {"current_allocated",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"current_avg_alloc",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"current_max_alloc",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"total_allocated",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
};

static const struct mylite_execution_catalog_column_definition
    sys_x_memory_by_host_by_current_bytes_columns[] = {
        {"host",
         NULL,
         "YES",
         "varchar",
         "255",
         "255",
         NULL,
         NULL,
         NULL,
         "ascii",
         "ascii_general_ci",
         "varchar(255)"},
        {"current_count_used",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "41",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(41,0)"},
        {"current_allocated",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "41",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(41,0)"},
        {"current_avg_alloc",
         "0.0000",
         "NO",
         "decimal",
         NULL,
         NULL,
         "45",
         "4",
         NULL,
         NULL,
         NULL,
         "decimal(45,4)"},
        {"current_max_alloc",
         NULL,
         "YES",
         "bigint",
         NULL,
         NULL,
         "19",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint"},
        {"total_allocated",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "42",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(42,0)"},
};

static const char *const sys_memory_by_host_by_current_bytes_column_keys[] = {
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_memory_by_host_by_current_bytes_column_extras[] = {
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_memory_by_host_by_current_bytes_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_column_definition
    sys_memory_by_thread_by_current_bytes_columns[] = {
        {"thread_id",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"user",
         NULL,
         "YES",
         "varchar",
         "288",
         "1152",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(288)"},
        {"current_count_used",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "41",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(41,0)"},
        {"current_allocated",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"current_avg_alloc",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"current_max_alloc",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"total_allocated",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
};

static const struct mylite_execution_catalog_column_definition
    sys_x_memory_by_thread_by_current_bytes_columns[] = {
        {"thread_id",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"user",
         NULL,
         "YES",
         "varchar",
         "288",
         "1152",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(288)"},
        {"current_count_used",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "41",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(41,0)"},
        {"current_allocated",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "41",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(41,0)"},
        {"current_avg_alloc",
         "0.0000",
         "NO",
         "decimal",
         NULL,
         NULL,
         "45",
         "4",
         NULL,
         NULL,
         NULL,
         "decimal(45,4)"},
        {"current_max_alloc",
         NULL,
         "YES",
         "bigint",
         NULL,
         NULL,
         "19",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint"},
        {"total_allocated",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "42",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(42,0)"},
};

static const char *const sys_memory_by_thread_by_current_bytes_column_keys[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_memory_by_thread_by_current_bytes_column_extras[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_memory_by_thread_by_current_bytes_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_column_definition
    sys_memory_by_user_by_current_bytes_columns[] = {
        {"user",
         NULL,
         "YES",
         "varchar",
         "32",
         "128",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_bin",
         "varchar(32)"},
        {"current_count_used",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "41",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(41,0)"},
        {"current_allocated",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"current_avg_alloc",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"current_max_alloc",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"total_allocated",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
};

static const struct mylite_execution_catalog_column_definition
    sys_x_memory_by_user_by_current_bytes_columns[] = {
        {"user",
         NULL,
         "YES",
         "varchar",
         "32",
         "128",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_bin",
         "varchar(32)"},
        {"current_count_used",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "41",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(41,0)"},
        {"current_allocated",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "41",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(41,0)"},
        {"current_avg_alloc",
         "0.0000",
         "NO",
         "decimal",
         NULL,
         NULL,
         "45",
         "4",
         NULL,
         NULL,
         NULL,
         "decimal(45,4)"},
        {"current_max_alloc",
         NULL,
         "YES",
         "bigint",
         NULL,
         NULL,
         "19",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint"},
        {"total_allocated",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "42",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(42,0)"},
};

static const char *const sys_memory_by_user_by_current_bytes_column_keys[] = {
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_memory_by_user_by_current_bytes_column_extras[] = {
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_memory_by_user_by_current_bytes_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_column_definition
    sys_innodb_buffer_stats_by_schema_columns[] = {
        {"object_schema",
         NULL,
         "YES",
         "text",
         "65535",
         "65535",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "text"},
        {"allocated",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"data",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"pages", "0", "NO", "bigint", NULL, NULL, "19", "0", NULL, NULL, NULL, "bigint"},
        {"pages_hashed", "0", "NO", "bigint", NULL, NULL, "19", "0", NULL, NULL, NULL, "bigint"},
        {"pages_old", "0", "NO", "bigint", NULL, NULL, "19", "0", NULL, NULL, NULL, "bigint"},
        {"rows_cached",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "45",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(45,0)"},
};

static const struct mylite_execution_catalog_column_definition
    sys_x_innodb_buffer_stats_by_schema_columns[] = {
        {"object_schema",
         NULL,
         "YES",
         "text",
         "65535",
         "65535",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "text"},
        {"allocated",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "44",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(44,0)"},
        {"data", NULL, "YES", "decimal", NULL, NULL, "44", "0", NULL, NULL, NULL, "decimal(44,0)"},
        {"pages", "0", "NO", "bigint", NULL, NULL, "19", "0", NULL, NULL, NULL, "bigint"},
        {"pages_hashed", "0", "NO", "bigint", NULL, NULL, "19", "0", NULL, NULL, NULL, "bigint"},
        {"pages_old", "0", "NO", "bigint", NULL, NULL, "19", "0", NULL, NULL, NULL, "bigint"},
        {"rows_cached",
         "0",
         "NO",
         "decimal",
         NULL,
         NULL,
         "45",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(45,0)"},
};

static const struct mylite_execution_catalog_column_definition
    sys_innodb_buffer_stats_by_table_columns[] = {
        {"object_schema",
         NULL,
         "YES",
         "text",
         "65535",
         "65535",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "text"},
        {"object_name",
         NULL,
         "YES",
         "text",
         "65535",
         "65535",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "text"},
        {"allocated",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"data",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"pages", "0", "NO", "bigint", NULL, NULL, "19", "0", NULL, NULL, NULL, "bigint"},
        {"pages_hashed", "0", "NO", "bigint", NULL, NULL, "19", "0", NULL, NULL, NULL, "bigint"},
        {"pages_old", "0", "NO", "bigint", NULL, NULL, "19", "0", NULL, NULL, NULL, "bigint"},
        {"rows_cached",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "45",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(45,0)"},
};

static const struct mylite_execution_catalog_column_definition
    sys_x_innodb_buffer_stats_by_table_columns[] = {
        {"object_schema",
         NULL,
         "YES",
         "text",
         "65535",
         "65535",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "text"},
        {"object_name",
         NULL,
         "YES",
         "text",
         "65535",
         "65535",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "text"},
        {"allocated",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "44",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(44,0)"},
        {"data", NULL, "YES", "decimal", NULL, NULL, "44", "0", NULL, NULL, NULL, "decimal(44,0)"},
        {"pages", "0", "NO", "bigint", NULL, NULL, "19", "0", NULL, NULL, NULL, "bigint"},
        {"pages_hashed", "0", "NO", "bigint", NULL, NULL, "19", "0", NULL, NULL, NULL, "bigint"},
        {"pages_old", "0", "NO", "bigint", NULL, NULL, "19", "0", NULL, NULL, NULL, "bigint"},
        {"rows_cached",
         "0",
         "NO",
         "decimal",
         NULL,
         NULL,
         "45",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(45,0)"},
};

static const char *const sys_innodb_buffer_stats_by_schema_column_keys[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_innodb_buffer_stats_by_schema_column_extras[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_innodb_buffer_stats_by_schema_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const char *const sys_innodb_buffer_stats_by_table_column_keys[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_innodb_buffer_stats_by_table_column_extras[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_innodb_buffer_stats_by_table_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_column_definition sys_innodb_lock_waits_columns[] = {
    {"wait_started", NULL, "YES", "datetime", NULL, NULL, NULL, NULL, "0", NULL, NULL, "datetime"},
    {"wait_age", NULL, "YES", "time", NULL, NULL, NULL, NULL, "0", NULL, NULL, "time"},
    {"wait_age_secs", NULL, "YES", "bigint", NULL, NULL, "19", "0", NULL, NULL, NULL, "bigint"},
    {"locked_table",
     NULL,
     "YES",
     "mediumtext",
     "16777215",
     "16777215",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "mediumtext"},
    {"locked_table_schema",
     NULL,
     "YES",
     "varchar",
     "64",
     "256",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "varchar(64)"},
    {"locked_table_name",
     NULL,
     "YES",
     "varchar",
     "64",
     "256",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "varchar(64)"},
    {"locked_table_partition",
     NULL,
     "YES",
     "varchar",
     "64",
     "256",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "varchar(64)"},
    {"locked_table_subpartition",
     NULL,
     "YES",
     "varchar",
     "64",
     "256",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "varchar(64)"},
    {"locked_index",
     NULL,
     "YES",
     "varchar",
     "64",
     "256",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "varchar(64)"},
    {"locked_type",
     NULL,
     "NO",
     "varchar",
     "32",
     "128",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "varchar(32)"},
    {"waiting_trx_id",
     "0",
     "NO",
     "bigint",
     NULL,
     NULL,
     "20",
     "0",
     NULL,
     NULL,
     NULL,
     "bigint unsigned"},
    {"waiting_trx_started",
     "0000-00-00 00:00:00",
     "NO",
     "datetime",
     NULL,
     NULL,
     NULL,
     NULL,
     "0",
     NULL,
     NULL,
     "datetime"},
    {"waiting_trx_age", NULL, "YES", "time", NULL, NULL, NULL, NULL, "0", NULL, NULL, "time"},
    {"waiting_trx_rows_locked",
     "0",
     "NO",
     "bigint",
     NULL,
     NULL,
     "20",
     "0",
     NULL,
     NULL,
     NULL,
     "bigint unsigned"},
    {"waiting_trx_rows_modified",
     "0",
     "NO",
     "bigint",
     NULL,
     NULL,
     "20",
     "0",
     NULL,
     NULL,
     NULL,
     "bigint unsigned"},
    {"waiting_pid",
     "0",
     "NO",
     "bigint",
     NULL,
     NULL,
     "20",
     "0",
     NULL,
     NULL,
     NULL,
     "bigint unsigned"},
    {"waiting_query",
     NULL,
     "YES",
     "longtext",
     "4294967295",
     "4294967295",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "longtext"},
    {"waiting_lock_id",
     NULL,
     "NO",
     "varchar",
     "128",
     "512",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "varchar(128)"},
    {"waiting_lock_mode",
     NULL,
     "NO",
     "varchar",
     "32",
     "128",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "varchar(32)"},
    {"blocking_trx_id",
     "0",
     "NO",
     "bigint",
     NULL,
     NULL,
     "20",
     "0",
     NULL,
     NULL,
     NULL,
     "bigint unsigned"},
    {"blocking_pid",
     "0",
     "NO",
     "bigint",
     NULL,
     NULL,
     "20",
     "0",
     NULL,
     NULL,
     NULL,
     "bigint unsigned"},
    {"blocking_query",
     NULL,
     "YES",
     "longtext",
     "4294967295",
     "4294967295",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "longtext"},
    {"blocking_lock_id",
     NULL,
     "NO",
     "varchar",
     "128",
     "512",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "varchar(128)"},
    {"blocking_lock_mode",
     NULL,
     "NO",
     "varchar",
     "32",
     "128",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "varchar(32)"},
    {"blocking_trx_started",
     "0000-00-00 00:00:00",
     "NO",
     "datetime",
     NULL,
     NULL,
     NULL,
     NULL,
     "0",
     NULL,
     NULL,
     "datetime"},
    {"blocking_trx_age", NULL, "YES", "time", NULL, NULL, NULL, NULL, "0", NULL, NULL, "time"},
    {"blocking_trx_rows_locked",
     "0",
     "NO",
     "bigint",
     NULL,
     NULL,
     "20",
     "0",
     NULL,
     NULL,
     NULL,
     "bigint unsigned"},
    {"blocking_trx_rows_modified",
     "0",
     "NO",
     "bigint",
     NULL,
     NULL,
     "20",
     "0",
     NULL,
     NULL,
     NULL,
     "bigint unsigned"},
    {"sql_kill_blocking_query",
     "",
     "NO",
     "varchar",
     "33",
     "132",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "varchar(33)"},
    {"sql_kill_blocking_connection",
     "",
     "NO",
     "varchar",
     "27",
     "108",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "varchar(27)"},
};

static const struct mylite_execution_catalog_column_definition sys_x_innodb_lock_waits_columns[] = {
    {"wait_started", NULL, "YES", "datetime", NULL, NULL, NULL, NULL, "0", NULL, NULL, "datetime"},
    {"wait_age", NULL, "YES", "time", NULL, NULL, NULL, NULL, "0", NULL, NULL, "time"},
    {"wait_age_secs", NULL, "YES", "bigint", NULL, NULL, "19", "0", NULL, NULL, NULL, "bigint"},
    {"locked_table",
     NULL,
     "YES",
     "mediumtext",
     "16777215",
     "16777215",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "mediumtext"},
    {"locked_table_schema",
     NULL,
     "YES",
     "varchar",
     "64",
     "256",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "varchar(64)"},
    {"locked_table_name",
     NULL,
     "YES",
     "varchar",
     "64",
     "256",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "varchar(64)"},
    {"locked_table_partition",
     NULL,
     "YES",
     "varchar",
     "64",
     "256",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "varchar(64)"},
    {"locked_table_subpartition",
     NULL,
     "YES",
     "varchar",
     "64",
     "256",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "varchar(64)"},
    {"locked_index",
     NULL,
     "YES",
     "varchar",
     "64",
     "256",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "varchar(64)"},
    {"locked_type",
     NULL,
     "NO",
     "varchar",
     "32",
     "128",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "varchar(32)"},
    {"waiting_trx_id",
     "0",
     "NO",
     "bigint",
     NULL,
     NULL,
     "20",
     "0",
     NULL,
     NULL,
     NULL,
     "bigint unsigned"},
    {"waiting_trx_started",
     "0000-00-00 00:00:00",
     "NO",
     "datetime",
     NULL,
     NULL,
     NULL,
     NULL,
     "0",
     NULL,
     NULL,
     "datetime"},
    {"waiting_trx_age", NULL, "YES", "time", NULL, NULL, NULL, NULL, "0", NULL, NULL, "time"},
    {"waiting_trx_rows_locked",
     "0",
     "NO",
     "bigint",
     NULL,
     NULL,
     "20",
     "0",
     NULL,
     NULL,
     NULL,
     "bigint unsigned"},
    {"waiting_trx_rows_modified",
     "0",
     "NO",
     "bigint",
     NULL,
     NULL,
     "20",
     "0",
     NULL,
     NULL,
     NULL,
     "bigint unsigned"},
    {"waiting_pid",
     "0",
     "NO",
     "bigint",
     NULL,
     NULL,
     "20",
     "0",
     NULL,
     NULL,
     NULL,
     "bigint unsigned"},
    {"waiting_query",
     NULL,
     "YES",
     "varchar",
     "1024",
     "3072",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "varchar(1024)"},
    {"waiting_lock_id",
     NULL,
     "NO",
     "varchar",
     "128",
     "512",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "varchar(128)"},
    {"waiting_lock_mode",
     NULL,
     "NO",
     "varchar",
     "32",
     "128",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "varchar(32)"},
    {"blocking_trx_id",
     "0",
     "NO",
     "bigint",
     NULL,
     NULL,
     "20",
     "0",
     NULL,
     NULL,
     NULL,
     "bigint unsigned"},
    {"blocking_pid",
     "0",
     "NO",
     "bigint",
     NULL,
     NULL,
     "20",
     "0",
     NULL,
     NULL,
     NULL,
     "bigint unsigned"},
    {"blocking_query",
     NULL,
     "YES",
     "varchar",
     "1024",
     "3072",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "varchar(1024)"},
    {"blocking_lock_id",
     NULL,
     "NO",
     "varchar",
     "128",
     "512",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "varchar(128)"},
    {"blocking_lock_mode",
     NULL,
     "NO",
     "varchar",
     "32",
     "128",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "varchar(32)"},
    {"blocking_trx_started",
     "0000-00-00 00:00:00",
     "NO",
     "datetime",
     NULL,
     NULL,
     NULL,
     NULL,
     "0",
     NULL,
     NULL,
     "datetime"},
    {"blocking_trx_age", NULL, "YES", "time", NULL, NULL, NULL, NULL, "0", NULL, NULL, "time"},
    {"blocking_trx_rows_locked",
     "0",
     "NO",
     "bigint",
     NULL,
     NULL,
     "20",
     "0",
     NULL,
     NULL,
     NULL,
     "bigint unsigned"},
    {"blocking_trx_rows_modified",
     "0",
     "NO",
     "bigint",
     NULL,
     NULL,
     "20",
     "0",
     NULL,
     NULL,
     NULL,
     "bigint unsigned"},
    {"sql_kill_blocking_query",
     "",
     "NO",
     "varchar",
     "33",
     "132",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "varchar(33)"},
    {"sql_kill_blocking_connection",
     "",
     "NO",
     "varchar",
     "27",
     "108",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "varchar(27)"},
};

static const char *const sys_innodb_lock_waits_column_keys[] = {
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
};

static const char *const sys_innodb_lock_waits_column_extras[] = {
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
};

static const char *const sys_innodb_lock_waits_column_privileges[] = {
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
};

static const struct mylite_execution_catalog_column_definition
    sys_io_by_thread_by_latency_columns[] = {
        {"user",
         NULL,
         "YES",
         "varchar",
         "288",
         "1152",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(288)"},
        {"total", NULL, "YES", "decimal", NULL, NULL, "42", "0", NULL, NULL, NULL, "decimal(42,0)"},
        {"total_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"min_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"avg_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"max_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"thread_id",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"processlist_id",
         NULL,
         "YES",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
};

static const struct mylite_execution_catalog_column_definition
    sys_x_io_by_thread_by_latency_columns[] = {
        {"user",
         NULL,
         "YES",
         "varchar",
         "288",
         "1152",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(288)"},
        {"total", NULL, "YES", "decimal", NULL, NULL, "42", "0", NULL, NULL, NULL, "decimal(42,0)"},
        {"total_latency",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "42",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(42,0)"},
        {"min_latency",
         NULL,
         "YES",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"avg_latency",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "24",
         "4",
         NULL,
         NULL,
         NULL,
         "decimal(24,4)"},
        {"max_latency",
         NULL,
         "YES",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"thread_id",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"processlist_id",
         NULL,
         "YES",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
};

static const char *const sys_io_by_thread_by_latency_column_keys[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_io_by_thread_by_latency_column_extras[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_io_by_thread_by_latency_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_column_definition
    sys_io_global_by_file_by_bytes_columns[] = {
        {"file",
         NULL,
         "YES",
         "varchar",
         "512",
         "2048",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(512)"},
        {"count_read",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"total_read",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"avg_read",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"count_write",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"total_written",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"avg_write",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"total",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"write_pct",
         "0.00",
         "NO",
         "decimal",
         NULL,
         NULL,
         "26",
         "2",
         NULL,
         NULL,
         NULL,
         "decimal(26,2)"},
};

static const struct mylite_execution_catalog_column_definition
    sys_x_io_global_by_file_by_bytes_columns[] = {
        {"file",
         NULL,
         "NO",
         "varchar",
         "512",
         "2048",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(512)"},
        {"count_read",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"total_read", NULL, "NO", "bigint", NULL, NULL, "19", "0", NULL, NULL, NULL, "bigint"},
        {"avg_read",
         "0.0000",
         "NO",
         "decimal",
         NULL,
         NULL,
         "23",
         "4",
         NULL,
         NULL,
         NULL,
         "decimal(23,4)"},
        {"count_write",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"total_written", NULL, "NO", "bigint", NULL, NULL, "19", "0", NULL, NULL, NULL, "bigint"},
        {"avg_write",
         "0.0000",
         "NO",
         "decimal",
         NULL,
         NULL,
         "23",
         "4",
         NULL,
         NULL,
         NULL,
         "decimal(23,4)"},
        {"total", "0", "NO", "bigint", NULL, NULL, "19", "0", NULL, NULL, NULL, "bigint"},
        {"write_pct",
         "0.00",
         "NO",
         "decimal",
         NULL,
         NULL,
         "26",
         "2",
         NULL,
         NULL,
         NULL,
         "decimal(26,2)"},
};

static const char *const sys_io_global_by_file_by_bytes_column_keys[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_io_global_by_file_by_bytes_column_extras[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_io_global_by_file_by_bytes_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_column_definition
    sys_io_global_by_file_by_latency_columns[] = {
        {"file",
         NULL,
         "YES",
         "varchar",
         "512",
         "2048",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(512)"},
        {"total", NULL, "NO", "bigint", NULL, NULL, "20", "0", NULL, NULL, NULL, "bigint unsigned"},
        {"total_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"count_read",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"read_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"count_write",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"write_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"count_misc",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"misc_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
};

static const struct mylite_execution_catalog_column_definition
    sys_x_io_global_by_file_by_latency_columns[] = {
        {"file",
         NULL,
         "NO",
         "varchar",
         "512",
         "2048",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(512)"},
        {"total", NULL, "NO", "bigint", NULL, NULL, "20", "0", NULL, NULL, NULL, "bigint unsigned"},
        {"total_latency",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"count_read",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"read_latency",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"count_write",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"write_latency",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"count_misc",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"misc_latency",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
};

static const char *const sys_io_global_by_file_by_latency_column_keys[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_io_global_by_file_by_latency_column_extras[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_io_global_by_file_by_latency_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_column_definition
    sys_io_global_by_wait_by_bytes_columns[] = {
        {"event_name",
         NULL,
         "YES",
         "varchar",
         "128",
         "512",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(128)"},
        {"total", NULL, "NO", "bigint", NULL, NULL, "20", "0", NULL, NULL, NULL, "bigint unsigned"},
        {"total_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"min_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"avg_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"max_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"count_read",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"total_read",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"avg_read",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"count_write",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"total_written",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"avg_written",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"total_requested",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
};

static const struct mylite_execution_catalog_column_definition
    sys_x_io_global_by_wait_by_bytes_columns[] = {
        {"event_name",
         NULL,
         "YES",
         "varchar",
         "128",
         "512",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(128)"},
        {"total", NULL, "NO", "bigint", NULL, NULL, "20", "0", NULL, NULL, NULL, "bigint unsigned"},
        {"total_latency",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"min_latency",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"avg_latency",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"max_latency",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"count_read",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"total_read", NULL, "NO", "bigint", NULL, NULL, "19", "0", NULL, NULL, NULL, "bigint"},
        {"avg_read",
         "0.0000",
         "NO",
         "decimal",
         NULL,
         NULL,
         "23",
         "4",
         NULL,
         NULL,
         NULL,
         "decimal(23,4)"},
        {"count_write",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"total_written", NULL, "NO", "bigint", NULL, NULL, "19", "0", NULL, NULL, NULL, "bigint"},
        {"avg_written",
         "0.0000",
         "NO",
         "decimal",
         NULL,
         NULL,
         "23",
         "4",
         NULL,
         NULL,
         NULL,
         "decimal(23,4)"},
        {"total_requested", "0", "NO", "bigint", NULL, NULL, "19", "0", NULL, NULL, NULL, "bigint"},
};

static const char *const sys_io_global_by_wait_by_bytes_column_keys[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_io_global_by_wait_by_bytes_column_extras[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_io_global_by_wait_by_bytes_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_column_definition
    sys_io_global_by_wait_by_latency_columns[] = {
        {"event_name",
         NULL,
         "YES",
         "varchar",
         "128",
         "512",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(128)"},
        {"total", NULL, "NO", "bigint", NULL, NULL, "20", "0", NULL, NULL, NULL, "bigint unsigned"},
        {"total_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"avg_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"max_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"read_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"write_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"misc_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"count_read",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"total_read",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"avg_read",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"count_write",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"total_written",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"avg_written",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
};

static const struct mylite_execution_catalog_column_definition
    sys_x_io_global_by_wait_by_latency_columns[] = {
        {"event_name",
         NULL,
         "YES",
         "varchar",
         "128",
         "512",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(128)"},
        {"total", NULL, "NO", "bigint", NULL, NULL, "20", "0", NULL, NULL, NULL, "bigint unsigned"},
        {"total_latency",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"avg_latency",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"max_latency",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"read_latency",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"write_latency",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"misc_latency",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"count_read",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"total_read", NULL, "NO", "bigint", NULL, NULL, "19", "0", NULL, NULL, NULL, "bigint"},
        {"avg_read",
         "0.0000",
         "NO",
         "decimal",
         NULL,
         NULL,
         "23",
         "4",
         NULL,
         NULL,
         NULL,
         "decimal(23,4)"},
        {"count_write",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"total_written", NULL, "NO", "bigint", NULL, NULL, "19", "0", NULL, NULL, NULL, "bigint"},
        {"avg_written",
         "0.0000",
         "NO",
         "decimal",
         NULL,
         NULL,
         "23",
         "4",
         NULL,
         NULL,
         NULL,
         "decimal(23,4)"},
};

static const char *const sys_io_global_by_wait_by_latency_column_keys[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_io_global_by_wait_by_latency_column_extras[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_io_global_by_wait_by_latency_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_column_definition sys_latest_file_io_columns[] = {
    {"thread",
     NULL,
     "YES",
     "varchar",
     "317",
     "1268",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "varchar(317)"},
    {"file",
     NULL,
     "YES",
     "varchar",
     "512",
     "2048",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "varchar(512)"},
    {"latency",
     NULL,
     "YES",
     "varchar",
     "11",
     "33",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "varchar(11)"},
    {"operation",
     NULL,
     "NO",
     "varchar",
     "32",
     "128",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "varchar(32)"},
    {"requested",
     NULL,
     "YES",
     "varchar",
     "11",
     "33",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "varchar(11)"},
};

static const struct mylite_execution_catalog_column_definition sys_x_latest_file_io_columns[] = {
    {"thread",
     NULL,
     "YES",
     "varchar",
     "317",
     "1268",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "varchar(317)"},
    {"file",
     NULL,
     "YES",
     "varchar",
     "512",
     "2048",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "varchar(512)"},
    {"latency", NULL, "YES", "bigint", NULL, NULL, "20", "0", NULL, NULL, NULL, "bigint unsigned"},
    {"operation",
     NULL,
     "NO",
     "varchar",
     "32",
     "128",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "varchar(32)"},
    {"requested", NULL, "YES", "bigint", NULL, NULL, "19", "0", NULL, NULL, NULL, "bigint"},
};

static const char *const sys_latest_file_io_column_keys[] = {
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_latest_file_io_column_extras[] = {
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_latest_file_io_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_column_definition
    sys_ps_check_lost_instrumentation_columns[] = {
        {"variable_name",
         NULL,
         "NO",
         "varchar",
         "64",
         "256",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(64)"},
        {"variable_value",
         NULL,
         "YES",
         "varchar",
         "1024",
         "4096",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(1024)"},
};

static const char *const sys_ps_check_lost_instrumentation_column_keys[] = {
    "",
    "",
};

static const char *const sys_ps_check_lost_instrumentation_column_extras[] = {
    "",
    "",
};

static const char *const sys_ps_check_lost_instrumentation_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_column_definition sys_schema_auto_increment_columns[] =
    {
        {"table_schema",
         NULL,
         "NO",
         "varchar",
         "64",
         "192",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_bin",
         "varchar(64)"},
        {"table_name",
         NULL,
         "NO",
         "varchar",
         "64",
         "192",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_bin",
         "varchar(64)"},
        {"column_name",
         NULL,
         "YES",
         "varchar",
         "64",
         "192",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_tolower_ci",
         "varchar(64)"},
        {"data_type",
         NULL,
         "YES",
         "longtext",
         "4294967295",
         "4294967295",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_bin",
         "longtext"},
        {"column_type",
         NULL,
         "NO",
         "mediumtext",
         "16777215",
         "16777215",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_bin",
         "mediumtext"},
        {"is_signed", "0", "NO", "int", NULL, NULL, "10", "0", NULL, NULL, NULL, "int"},
        {"is_unsigned", "0", "NO", "int", NULL, NULL, "10", "0", NULL, NULL, NULL, "int"},
        {"max_value",
         NULL,
         "YES",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"auto_increment",
         NULL,
         "YES",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"auto_increment_ratio",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "25",
         "4",
         NULL,
         NULL,
         NULL,
         "decimal(25,4) unsigned"},
};

static const char *const sys_schema_auto_increment_columns_column_keys[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_schema_auto_increment_columns_column_extras[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_schema_auto_increment_columns_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_column_definition
    sys_schema_index_statistics_columns[] = {
        {"table_schema",
         NULL,
         "YES",
         "varchar",
         "64",
         "256",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(64)"},
        {"table_name",
         NULL,
         "YES",
         "varchar",
         "64",
         "256",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(64)"},
        {"index_name",
         NULL,
         "YES",
         "varchar",
         "64",
         "256",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(64)"},
        {"rows_selected",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"select_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"rows_inserted",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"insert_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"rows_updated",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"update_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"rows_deleted",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"delete_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
};

static const struct mylite_execution_catalog_column_definition
    sys_x_schema_index_statistics_columns[] = {
        {"table_schema",
         NULL,
         "YES",
         "varchar",
         "64",
         "256",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(64)"},
        {"table_name",
         NULL,
         "YES",
         "varchar",
         "64",
         "256",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(64)"},
        {"index_name",
         NULL,
         "YES",
         "varchar",
         "64",
         "256",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(64)"},
        {"rows_selected",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"select_latency",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"rows_inserted",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"insert_latency",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"rows_updated",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"update_latency",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"rows_deleted",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"delete_latency",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
};

static const char *const sys_schema_index_statistics_column_keys[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_schema_index_statistics_column_extras[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_schema_index_statistics_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_column_definition
    sys_x_ps_schema_table_statistics_io_columns[] = {
        {"table_schema",
         NULL,
         "YES",
         "varchar",
         "64",
         "256",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(64)"},
        {"table_name",
         NULL,
         "YES",
         "varchar",
         "64",
         "256",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(64)"},
        {"count_read",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "42",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(42,0)"},
        {"sum_number_of_bytes_read",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "41",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(41,0)"},
        {"sum_timer_read",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "42",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(42,0)"},
        {"count_write",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "42",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(42,0)"},
        {"sum_number_of_bytes_write",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "41",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(41,0)"},
        {"sum_timer_write",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "42",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(42,0)"},
        {"count_misc",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "42",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(42,0)"},
        {"sum_timer_misc",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "42",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(42,0)"},
};

static const char *const sys_x_ps_schema_table_statistics_io_column_keys[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_x_ps_schema_table_statistics_io_column_extras[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_x_ps_schema_table_statistics_io_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_column_definition
    sys_schema_table_statistics_columns[] = {
        {"table_schema",
         NULL,
         "YES",
         "varchar",
         "64",
         "256",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(64)"},
        {"table_name",
         NULL,
         "YES",
         "varchar",
         "64",
         "256",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(64)"},
        {"total_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"rows_fetched",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"fetch_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"rows_inserted",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"insert_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"rows_updated",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"update_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"rows_deleted",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"delete_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"io_read_requests",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "42",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(42,0)"},
        {"io_read",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"io_read_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"io_write_requests",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "42",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(42,0)"},
        {"io_write",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"io_write_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"io_misc_requests",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "42",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(42,0)"},
        {"io_misc_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
};

static const struct mylite_execution_catalog_column_definition
    sys_x_schema_table_statistics_columns[] = {
        {"table_schema",
         NULL,
         "YES",
         "varchar",
         "64",
         "256",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(64)"},
        {"table_name",
         NULL,
         "YES",
         "varchar",
         "64",
         "256",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(64)"},
        {"total_latency",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"rows_fetched",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"fetch_latency",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"rows_inserted",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"insert_latency",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"rows_updated",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"update_latency",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"rows_deleted",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"delete_latency",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"io_read_requests",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "42",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(42,0)"},
        {"io_read",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "41",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(41,0)"},
        {"io_read_latency",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "42",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(42,0)"},
        {"io_write_requests",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "42",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(42,0)"},
        {"io_write",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "41",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(41,0)"},
        {"io_write_latency",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "42",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(42,0)"},
        {"io_misc_requests",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "42",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(42,0)"},
        {"io_misc_latency",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "42",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(42,0)"},
};

static const struct mylite_execution_catalog_column_definition
    sys_schema_table_statistics_with_buffer_columns[] = {
        {"table_schema",
         NULL,
         "YES",
         "varchar",
         "64",
         "256",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(64)"},
        {"table_name",
         NULL,
         "YES",
         "varchar",
         "64",
         "256",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(64)"},
        {"rows_fetched",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"fetch_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"rows_inserted",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"insert_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"rows_updated",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"update_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"rows_deleted",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"delete_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"io_read_requests",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "42",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(42,0)"},
        {"io_read",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"io_read_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"io_write_requests",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "42",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(42,0)"},
        {"io_write",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"io_write_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"io_misc_requests",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "42",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(42,0)"},
        {"io_misc_latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"innodb_buffer_allocated",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"innodb_buffer_data",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"innodb_buffer_free",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
        {"innodb_buffer_pages",
         "0",
         "YES",
         "bigint",
         NULL,
         NULL,
         "19",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint"},
        {"innodb_buffer_pages_hashed",
         "0",
         "YES",
         "bigint",
         NULL,
         NULL,
         "19",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint"},
        {"innodb_buffer_pages_old",
         "0",
         "YES",
         "bigint",
         NULL,
         NULL,
         "19",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint"},
        {"innodb_buffer_rows_cached",
         "0",
         "YES",
         "decimal",
         NULL,
         NULL,
         "45",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(45,0)"},
};

static const struct mylite_execution_catalog_column_definition
    sys_x_schema_table_statistics_with_buffer_columns[] = {
        {"table_schema",
         NULL,
         "YES",
         "varchar",
         "64",
         "256",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(64)"},
        {"table_name",
         NULL,
         "YES",
         "varchar",
         "64",
         "256",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(64)"},
        {"rows_fetched",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"fetch_latency",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"rows_inserted",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"insert_latency",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"rows_updated",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"update_latency",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"rows_deleted",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"delete_latency",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"io_read_requests",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "42",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(42,0)"},
        {"io_read",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "41",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(41,0)"},
        {"io_read_latency",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "42",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(42,0)"},
        {"io_write_requests",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "42",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(42,0)"},
        {"io_write",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "41",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(41,0)"},
        {"io_write_latency",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "42",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(42,0)"},
        {"io_misc_requests",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "42",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(42,0)"},
        {"io_misc_latency",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "42",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(42,0)"},
        {"innodb_buffer_allocated",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "44",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(44,0)"},
        {"innodb_buffer_data",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "44",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(44,0)"},
        {"innodb_buffer_free",
         NULL,
         "YES",
         "decimal",
         NULL,
         NULL,
         "45",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(45,0)"},
        {"innodb_buffer_pages",
         "0",
         "YES",
         "bigint",
         NULL,
         NULL,
         "19",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint"},
        {"innodb_buffer_pages_hashed",
         "0",
         "YES",
         "bigint",
         NULL,
         NULL,
         "19",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint"},
        {"innodb_buffer_pages_old",
         "0",
         "YES",
         "bigint",
         NULL,
         NULL,
         "19",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint"},
        {"innodb_buffer_rows_cached",
         "0",
         "YES",
         "decimal",
         NULL,
         NULL,
         "45",
         "0",
         NULL,
         NULL,
         NULL,
         "decimal(45,0)"},
};

static const struct mylite_execution_catalog_column_definition
    sys_schema_tables_with_full_table_scans_columns[] = {
        {"object_schema",
         NULL,
         "YES",
         "varchar",
         "64",
         "256",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(64)"},
        {"object_name",
         NULL,
         "YES",
         "varchar",
         "64",
         "256",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(64)"},
        {"rows_full_scanned",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"latency",
         NULL,
         "YES",
         "varchar",
         "11",
         "33",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(11)"},
};

static const struct mylite_execution_catalog_column_definition
    sys_x_schema_tables_with_full_table_scans_columns[] = {
        {"object_schema",
         NULL,
         "YES",
         "varchar",
         "64",
         "256",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(64)"},
        {"object_name",
         NULL,
         "YES",
         "varchar",
         "64",
         "256",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(64)"},
        {"rows_full_scanned",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"latency",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
};

static const char *const sys_schema_table_statistics_column_keys[] = {
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
};

static const char *const sys_schema_table_statistics_column_extras[] = {
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
};

static const char *const sys_schema_table_statistics_column_privileges[] = {
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references",
};

static const char *const sys_schema_table_statistics_with_buffer_column_keys[] = {
    "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "",
};

static const char *const sys_schema_table_statistics_with_buffer_column_extras[] = {
    "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "",
};

static const char *const sys_schema_table_statistics_with_buffer_column_privileges[] = {
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references", "select,insert,update,references",
    "select,insert,update,references",
};

static const char *const sys_schema_tables_with_full_table_scans_column_keys[] = {
    "",
    "",
    "",
    "",
};

static const char *const sys_schema_tables_with_full_table_scans_column_extras[] = {
    "",
    "",
    "",
    "",
};

static const char *const sys_schema_tables_with_full_table_scans_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_column_definition sys_schema_unused_indexes_columns[] =
    {
        {"object_schema",
         NULL,
         "YES",
         "varchar",
         "64",
         "256",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(64)"},
        {"object_name",
         NULL,
         "YES",
         "varchar",
         "64",
         "256",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(64)"},
        {"index_name",
         NULL,
         "YES",
         "varchar",
         "64",
         "256",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(64)"},
};

static const char *const sys_schema_unused_indexes_column_keys[] = {
    "",
    "",
    "",
};

static const char *const sys_schema_unused_indexes_column_extras[] = {
    "",
    "",
    "",
};

static const char *const sys_schema_unused_indexes_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_column_definition
    sys_schema_redundant_indexes_columns[] = {
        {"table_schema",
         NULL,
         "NO",
         "varchar",
         "64",
         "192",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_bin",
         "varchar(64)"},
        {"table_name",
         NULL,
         "NO",
         "varchar",
         "64",
         "192",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_bin",
         "varchar(64)"},
        {"redundant_index_name",
         NULL,
         "YES",
         "varchar",
         "64",
         "192",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_tolower_ci",
         "varchar(64)"},
        {"redundant_index_columns",
         NULL,
         "YES",
         "text",
         "65535",
         "65535",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_tolower_ci",
         "text"},
        {"redundant_index_non_unique",
         NULL,
         "YES",
         "int",
         NULL,
         NULL,
         "10",
         "0",
         NULL,
         NULL,
         NULL,
         "int"},
        {"dominant_index_name",
         NULL,
         "YES",
         "varchar",
         "64",
         "192",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_tolower_ci",
         "varchar(64)"},
        {"dominant_index_columns",
         NULL,
         "YES",
         "text",
         "65535",
         "65535",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_tolower_ci",
         "text"},
        {"dominant_index_non_unique",
         NULL,
         "YES",
         "int",
         NULL,
         NULL,
         "10",
         "0",
         NULL,
         NULL,
         NULL,
         "int"},
        {"subpart_exists", "0", "NO", "int", NULL, NULL, "10", "0", NULL, NULL, NULL, "int"},
        {"sql_drop_index",
         NULL,
         "YES",
         "varchar",
         "223",
         "669",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_tolower_ci",
         "varchar(223)"},
};

static const struct mylite_execution_catalog_column_definition
    sys_x_schema_flattened_keys_columns[] = {
        {"table_schema",
         NULL,
         "NO",
         "varchar",
         "64",
         "192",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_bin",
         "varchar(64)"},
        {"table_name",
         NULL,
         "NO",
         "varchar",
         "64",
         "192",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_bin",
         "varchar(64)"},
        {"index_name",
         NULL,
         "YES",
         "varchar",
         "64",
         "192",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_tolower_ci",
         "varchar(64)"},
        {"non_unique", NULL, "YES", "int", NULL, NULL, "10", "0", NULL, NULL, NULL, "int"},
        {"subpart_exists",
         NULL,
         "YES",
         "bigint",
         NULL,
         NULL,
         "19",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint"},
        {"index_columns",
         NULL,
         "YES",
         "text",
         "65535",
         "65535",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_tolower_ci",
         "text"},
};

static const char *const sys_schema_redundant_indexes_column_keys[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_schema_redundant_indexes_column_extras[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_schema_redundant_indexes_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const char *const sys_x_schema_flattened_keys_column_keys[] = {
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_x_schema_flattened_keys_column_extras[] = {
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_x_schema_flattened_keys_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_column_definition
    sys_schema_table_lock_waits_columns[] = {
        {"object_schema",
         NULL,
         "YES",
         "varchar",
         "64",
         "256",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(64)"},
        {"object_name",
         NULL,
         "YES",
         "varchar",
         "64",
         "256",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(64)"},
        {"waiting_thread_id",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"waiting_pid",
         NULL,
         "YES",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"waiting_account",
         NULL,
         "YES",
         "text",
         "65535",
         "65535",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "text"},
        {"waiting_lock_type",
         NULL,
         "NO",
         "varchar",
         "32",
         "128",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(32)"},
        {"waiting_lock_duration",
         NULL,
         "NO",
         "varchar",
         "32",
         "128",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(32)"},
        {"waiting_query",
         NULL,
         "YES",
         "longtext",
         "4294967295",
         "4294967295",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "longtext"},
        {"waiting_query_secs",
         NULL,
         "YES",
         "bigint",
         NULL,
         NULL,
         "19",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint"},
        {"waiting_query_rows_affected",
         NULL,
         "YES",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"waiting_query_rows_examined",
         NULL,
         "YES",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"blocking_thread_id",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"blocking_pid",
         NULL,
         "YES",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"blocking_account",
         NULL,
         "YES",
         "text",
         "65535",
         "65535",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "text"},
        {"blocking_lock_type",
         NULL,
         "NO",
         "varchar",
         "32",
         "128",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(32)"},
        {"blocking_lock_duration",
         NULL,
         "NO",
         "varchar",
         "32",
         "128",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(32)"},
        {"sql_kill_blocking_query",
         NULL,
         "YES",
         "varchar",
         "31",
         "124",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(31)"},
        {"sql_kill_blocking_connection",
         NULL,
         "YES",
         "varchar",
         "25",
         "100",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(25)"},
};

static const char *const sys_schema_table_lock_waits_column_keys[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_schema_table_lock_waits_column_extras[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const sys_schema_table_lock_waits_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_column_definition
    sys_schema_object_overview_columns[] = {
        {"db",
         "",
         "NO",
         "varchar",
         "64",
         "192",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_bin",
         "varchar(64)"},
        {"object_type",
         NULL,
         "YES",
         "varchar",
         "19",
         "57",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_bin",
         "varchar(19)"},
        {"count", "0", "NO", "bigint", NULL, NULL, "19", "0", NULL, NULL, NULL, "bigint"},
};

static const char *const sys_schema_object_overview_column_keys[] = {
    "",
    "",
    "",
};

static const char *const sys_schema_object_overview_column_extras[] = {
    "",
    "",
    "",
};

static const char *const sys_schema_object_overview_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const char sys_version_view_definition[] =
    "select '2.1.3' AS `sys_version`,version() AS `mysql_version`";

static const char sys_version_show_create_view_sql[] =
    "CREATE ALGORITHM=UNDEFINED DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`version` (`sys_version`,`mysql_version`) AS select '2.1.3' AS "
    "`sys_version`,version() AS `mysql_version`";

static const char sys_version_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=UNDEFINED DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`version` (`sys_version`,`mysql_version`) AS select '2.1.3' AS "
    "`sys_version`,version() AS `mysql_version`";

#define SYS_HOST_SUMMARY_VIEW_COLUMNS                                                              \
    "(`host`,`statements`,`statement_latency`,`statement_avg_latency`,`table_scans`,`file_ios`,"   \
    "`file_io_latency`,`current_connections`,`total_connections`,`unique_users`,`current_memory`," \
    "`total_memory_allocated`)"

#define SYS_HOST_SUMMARY_QUALIFIED_SELECT_SUFFIX                                                   \
    "sum(`performance_schema`.`accounts`.`CURRENT_CONNECTIONS`) AS `current_connections`,"         \
    "sum(`performance_schema`.`accounts`.`TOTAL_CONNECTIONS`) AS `total_connections`,count("       \
    "distinct `performance_schema`.`accounts`.`USER`) AS `unique_users`,"

#define SYS_HOST_SUMMARY_QUALIFIED_FROM                                                            \
    " from (((`performance_schema`.`accounts` join `sys`."                                         \
    "`x$host_summary_by_statement_latency` `stmt` on((`performance_schema`.`accounts`.`HOST` "     \
    "= `sys`.`stmt`.`host`))) join `sys`.`x$host_summary_by_file_io` `io` on(("                    \
    "`performance_schema`.`accounts`.`HOST` = `sys`.`io`.`host`))) join `sys`."                    \
    "`x$memory_by_host_by_current_bytes` `mem` on((`performance_schema`.`accounts`.`HOST` = "      \
    "`sys`.`mem`.`host`))) group by if((`performance_schema`.`accounts`.`HOST` is null),"          \
    "'background',`performance_schema`.`accounts`.`HOST`)"

#define SYS_HOST_SUMMARY_UNQUALIFIED_SELECT_SUFFIX                                                 \
    "sum(`performance_schema`.`accounts`.`CURRENT_CONNECTIONS`) AS `current_connections`,"         \
    "sum(`performance_schema`.`accounts`.`TOTAL_CONNECTIONS`) AS `total_connections`,count("       \
    "distinct `performance_schema`.`accounts`.`USER`) AS `unique_users`,"

#define SYS_HOST_SUMMARY_UNQUALIFIED_FROM                                                          \
    " from (((`performance_schema`.`accounts` join `x$host_summary_by_statement_latency` "         \
    "`stmt` on((`performance_schema`.`accounts`.`HOST` = `stmt`.`host`))) join "                   \
    "`x$host_summary_by_file_io` `io` on((`performance_schema`.`accounts`.`HOST` = "               \
    "`io`.`host`))) join `x$memory_by_host_by_current_bytes` `mem` on(("                           \
    "`performance_schema`.`accounts`.`HOST` = `mem`.`host`))) group by if(("                       \
    "`performance_schema`.`accounts`.`HOST` is null),'background',"                                \
    "`performance_schema`.`accounts`.`HOST`)"

#define SYS_HOST_SUMMARY_QUALIFIED_VIEW_DEFINITION                                                 \
    "select if((`performance_schema`.`accounts`.`HOST` is null),'background',"                     \
    "`performance_schema`.`accounts`.`HOST`) AS `host`,sum(`sys`.`stmt`.`total`) AS "              \
    "`statements`,format_pico_time(sum(`sys`.`stmt`.`total_latency`)) AS "                         \
    "`statement_latency`,format_pico_time(ifnull((sum(`sys`.`stmt`.`total_latency`) / "            \
    "nullif(sum(`sys`.`stmt`.`total`),0)),0)) AS `statement_avg_latency`,sum("                     \
    "`sys`.`stmt`.`full_scans`) AS `table_scans`,sum(`sys`.`io`.`ios`) AS "                        \
    "`file_ios`,format_pico_time(sum(`sys`.`io`.`io_latency`)) AS "                                \
    "`file_io_latency`," SYS_HOST_SUMMARY_QUALIFIED_SELECT_SUFFIX                                  \
    "format_bytes(sum(`sys`.`mem`.`current_allocated`)) AS `current_memory`,format_bytes("         \
    "sum(`sys`.`mem`.`total_allocated`)) AS "                                                      \
    "`total_memory_allocated`" SYS_HOST_SUMMARY_QUALIFIED_FROM

#define SYS_X_HOST_SUMMARY_QUALIFIED_VIEW_DEFINITION                                               \
    "select if((`performance_schema`.`accounts`.`HOST` is null),'background',"                     \
    "`performance_schema`.`accounts`.`HOST`) AS `host`,sum(`sys`.`stmt`.`total`) AS "              \
    "`statements`,sum(`sys`.`stmt`.`total_latency`) AS `statement_latency`,("                      \
    "sum(`sys`.`stmt`.`total_latency`) / sum(`sys`.`stmt`.`total`)) AS "                           \
    "`statement_avg_latency`,sum(`sys`.`stmt`.`full_scans`) AS `table_scans`,sum("                 \
    "`sys`.`io`.`ios`) AS `file_ios`,sum(`sys`.`io`.`io_latency`) AS "                             \
    "`file_io_latency`," SYS_HOST_SUMMARY_QUALIFIED_SELECT_SUFFIX                                  \
    "sum(`sys`.`mem`.`current_allocated`) AS `current_memory`,sum("                                \
    "`sys`.`mem`.`total_allocated`) AS `total_memory_allocated`" SYS_HOST_SUMMARY_QUALIFIED_FROM

#define SYS_HOST_SUMMARY_UNQUALIFIED_VIEW_DEFINITION                                               \
    "select if((`performance_schema`.`accounts`.`HOST` is null),'background',"                     \
    "`performance_schema`.`accounts`.`HOST`) AS `host`,sum(`stmt`.`total`) AS "                    \
    "`statements`,format_pico_time(sum(`stmt`.`total_latency`)) AS "                               \
    "`statement_latency`,format_pico_time(ifnull((sum(`stmt`.`total_latency`) / nullif(sum("       \
    "`stmt`.`total`),0)),0)) AS `statement_avg_latency`,sum(`stmt`.`full_scans`) AS "              \
    "`table_scans`,sum(`io`.`ios`) AS `file_ios`,format_pico_time(sum(`io`."                       \
    "`io_latency`)) AS `file_io_latency`," SYS_HOST_SUMMARY_UNQUALIFIED_SELECT_SUFFIX              \
    "format_bytes(sum(`mem`.`current_allocated`)) AS `current_memory`,format_bytes(sum("           \
    "`mem`.`total_allocated`)) AS `total_memory_allocated`" SYS_HOST_SUMMARY_UNQUALIFIED_FROM

#define SYS_X_HOST_SUMMARY_UNQUALIFIED_VIEW_DEFINITION                                             \
    "select if((`performance_schema`.`accounts`.`HOST` is null),'background',"                     \
    "`performance_schema`.`accounts`.`HOST`) AS `host`,sum(`stmt`.`total`) AS "                    \
    "`statements`,sum(`stmt`.`total_latency`) AS `statement_latency`,(sum("                        \
    "`stmt`.`total_latency`) / sum(`stmt`.`total`)) AS `statement_avg_latency`,sum("               \
    "`stmt`.`full_scans`) AS `table_scans`,sum(`io`.`ios`) AS `file_ios`,sum("                     \
    "`io`.`io_latency`) AS `file_io_latency`," SYS_HOST_SUMMARY_UNQUALIFIED_SELECT_SUFFIX          \
    "sum(`mem`.`current_allocated`) AS `current_memory`,sum(`mem`.`total_allocated`) AS "          \
    "`total_memory_allocated`" SYS_HOST_SUMMARY_UNQUALIFIED_FROM

static const char sys_host_summary_view_definition[] = SYS_HOST_SUMMARY_QUALIFIED_VIEW_DEFINITION;

static const char sys_host_summary_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`host_summary` " SYS_HOST_SUMMARY_VIEW_COLUMNS
    " AS " SYS_HOST_SUMMARY_UNQUALIFIED_VIEW_DEFINITION;

static const char sys_host_summary_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`host_summary` " SYS_HOST_SUMMARY_VIEW_COLUMNS
    " AS " SYS_HOST_SUMMARY_QUALIFIED_VIEW_DEFINITION;

static const char sys_x_host_summary_view_definition[] =
    SYS_X_HOST_SUMMARY_QUALIFIED_VIEW_DEFINITION;

static const char sys_x_host_summary_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$host_summary` " SYS_HOST_SUMMARY_VIEW_COLUMNS
    " AS " SYS_X_HOST_SUMMARY_UNQUALIFIED_VIEW_DEFINITION;

static const char sys_x_host_summary_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$host_summary` " SYS_HOST_SUMMARY_VIEW_COLUMNS
    " AS " SYS_X_HOST_SUMMARY_QUALIFIED_VIEW_DEFINITION;

#undef SYS_HOST_SUMMARY_VIEW_COLUMNS
#undef SYS_HOST_SUMMARY_QUALIFIED_SELECT_SUFFIX
#undef SYS_HOST_SUMMARY_QUALIFIED_FROM
#undef SYS_HOST_SUMMARY_UNQUALIFIED_SELECT_SUFFIX
#undef SYS_HOST_SUMMARY_UNQUALIFIED_FROM
#undef SYS_HOST_SUMMARY_QUALIFIED_VIEW_DEFINITION
#undef SYS_X_HOST_SUMMARY_QUALIFIED_VIEW_DEFINITION
#undef SYS_HOST_SUMMARY_UNQUALIFIED_VIEW_DEFINITION
#undef SYS_X_HOST_SUMMARY_UNQUALIFIED_VIEW_DEFINITION

#define SYS_HOST_SUMMARY_BY_FILE_IO_VIEW_COLUMNS "(`host`,`ios`,`io_latency`)"

#define SYS_HOST_SUMMARY_BY_FILE_IO_SELECT_PREFIX                                                  \
    "select if((`performance_schema`.`events_waits_summary_by_host_by_event_name`.`HOST` is "      \
    "null),'background',`performance_schema`.`events_waits_summary_by_host_by_event_name`."        \
    "`HOST`) AS `host`,sum(`performance_schema`.`events_waits_summary_by_host_by_event_name`."     \
    "`COUNT_STAR`) AS `ios`,"

#define SYS_HOST_SUMMARY_BY_FILE_IO_SELECT_SUFFIX                                                  \
    " from `performance_schema`.`events_waits_summary_by_host_by_event_name` where ("              \
    "`performance_schema`.`events_waits_summary_by_host_by_event_name`.`EVENT_NAME` like "         \
    "'wait/io/file/%') group by if((`performance_schema`."                                         \
    "`events_waits_summary_by_host_by_event_name`.`HOST` is null),'background',"                   \
    "`performance_schema`.`events_waits_summary_by_host_by_event_name`.`HOST`) order by sum("      \
    "`performance_schema`.`events_waits_summary_by_host_by_event_name`.`SUM_TIMER_WAIT`) desc"

#define SYS_HOST_SUMMARY_BY_FILE_IO_VIEW_DEFINITION                                                \
    SYS_HOST_SUMMARY_BY_FILE_IO_SELECT_PREFIX                                                      \
    "format_pico_time(sum(`performance_schema`.`events_waits_summary_by_host_by_event_name`."      \
    "`SUM_TIMER_WAIT`)) AS `io_latency`" SYS_HOST_SUMMARY_BY_FILE_IO_SELECT_SUFFIX

#define SYS_X_HOST_SUMMARY_BY_FILE_IO_VIEW_DEFINITION                                              \
    SYS_HOST_SUMMARY_BY_FILE_IO_SELECT_PREFIX                                                      \
    "sum(`performance_schema`.`events_waits_summary_by_host_by_event_name`.`SUM_TIMER_WAIT`) AS "  \
    "`io_latency`" SYS_HOST_SUMMARY_BY_FILE_IO_SELECT_SUFFIX

static const char sys_host_summary_by_file_io_view_definition[] =
    SYS_HOST_SUMMARY_BY_FILE_IO_VIEW_DEFINITION;

static const char sys_host_summary_by_file_io_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`host_summary_by_file_io` " SYS_HOST_SUMMARY_BY_FILE_IO_VIEW_COLUMNS
    " AS " SYS_HOST_SUMMARY_BY_FILE_IO_VIEW_DEFINITION;

static const char sys_host_summary_by_file_io_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`host_summary_by_file_io` " SYS_HOST_SUMMARY_BY_FILE_IO_VIEW_COLUMNS
    " AS " SYS_HOST_SUMMARY_BY_FILE_IO_VIEW_DEFINITION;

static const char sys_x_host_summary_by_file_io_view_definition[] =
    SYS_X_HOST_SUMMARY_BY_FILE_IO_VIEW_DEFINITION;

static const char sys_x_host_summary_by_file_io_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$host_summary_by_file_io` " SYS_HOST_SUMMARY_BY_FILE_IO_VIEW_COLUMNS
    " AS " SYS_X_HOST_SUMMARY_BY_FILE_IO_VIEW_DEFINITION;

static const char sys_x_host_summary_by_file_io_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$host_summary_by_file_io` " SYS_HOST_SUMMARY_BY_FILE_IO_VIEW_COLUMNS
    " AS " SYS_X_HOST_SUMMARY_BY_FILE_IO_VIEW_DEFINITION;

#undef SYS_HOST_SUMMARY_BY_FILE_IO_VIEW_COLUMNS
#undef SYS_HOST_SUMMARY_BY_FILE_IO_SELECT_PREFIX
#undef SYS_HOST_SUMMARY_BY_FILE_IO_SELECT_SUFFIX
#undef SYS_HOST_SUMMARY_BY_FILE_IO_VIEW_DEFINITION
#undef SYS_X_HOST_SUMMARY_BY_FILE_IO_VIEW_DEFINITION

#define SYS_HOST_SUMMARY_BY_FILE_IO_TYPE_VIEW_COLUMNS                                              \
    "(`host`,`event_name`,`total`,`total_latency`,`max_latency`)"

#define SYS_HOST_SUMMARY_BY_FILE_IO_TYPE_SELECT_PREFIX                                             \
    "select if((`performance_schema`.`events_waits_summary_by_host_by_event_name`.`HOST` is "      \
    "null),'background',`performance_schema`.`events_waits_summary_by_host_by_event_name`."        \
    "`HOST`) AS `host`,`performance_schema`.`events_waits_summary_by_host_by_event_name`."         \
    "`EVENT_NAME` AS `event_name`,`performance_schema`."                                           \
    "`events_waits_summary_by_host_by_event_name`.`COUNT_STAR` AS `total`,"

#define SYS_HOST_SUMMARY_BY_FILE_IO_TYPE_SELECT_SUFFIX                                             \
    " from `performance_schema`.`events_waits_summary_by_host_by_event_name` where (("             \
    "`performance_schema`.`events_waits_summary_by_host_by_event_name`.`EVENT_NAME` like "         \
    "'wait/io/file%') and (`performance_schema`."                                                  \
    "`events_waits_summary_by_host_by_event_name`.`COUNT_STAR` > 0)) order by if(("                \
    "`performance_schema`.`events_waits_summary_by_host_by_event_name`.`HOST` is null),"           \
    "'background',`performance_schema`.`events_waits_summary_by_host_by_event_name`.`HOST`),"      \
    "`performance_schema`.`events_waits_summary_by_host_by_event_name`.`SUM_TIMER_WAIT` desc"

#define SYS_HOST_SUMMARY_BY_FILE_IO_TYPE_VIEW_DEFINITION                                           \
    SYS_HOST_SUMMARY_BY_FILE_IO_TYPE_SELECT_PREFIX                                                 \
    "format_pico_time(`performance_schema`.`events_waits_summary_by_host_by_event_name`."          \
    "`SUM_TIMER_WAIT`) AS `total_latency`,format_pico_time(`performance_schema`."                  \
    "`events_waits_summary_by_host_by_event_name`.`MAX_TIMER_WAIT`) AS "                           \
    "`max_latency`" SYS_HOST_SUMMARY_BY_FILE_IO_TYPE_SELECT_SUFFIX

#define SYS_X_HOST_SUMMARY_BY_FILE_IO_TYPE_VIEW_DEFINITION                                         \
    SYS_HOST_SUMMARY_BY_FILE_IO_TYPE_SELECT_PREFIX                                                 \
    "`performance_schema`.`events_waits_summary_by_host_by_event_name`.`SUM_TIMER_WAIT` AS "       \
    "`total_latency`,`performance_schema`.`events_waits_summary_by_host_by_event_name`."           \
    "`MAX_TIMER_WAIT` AS `max_latency`" SYS_HOST_SUMMARY_BY_FILE_IO_TYPE_SELECT_SUFFIX

static const char sys_host_summary_by_file_io_type_view_definition[] =
    SYS_HOST_SUMMARY_BY_FILE_IO_TYPE_VIEW_DEFINITION;

static const char sys_host_summary_by_file_io_type_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`host_summary_by_file_io_type` " SYS_HOST_SUMMARY_BY_FILE_IO_TYPE_VIEW_COLUMNS
    " AS " SYS_HOST_SUMMARY_BY_FILE_IO_TYPE_VIEW_DEFINITION;

static const char sys_host_summary_by_file_io_type_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`host_summary_by_file_io_type` " SYS_HOST_SUMMARY_BY_FILE_IO_TYPE_VIEW_COLUMNS
    " AS " SYS_HOST_SUMMARY_BY_FILE_IO_TYPE_VIEW_DEFINITION;

static const char sys_x_host_summary_by_file_io_type_view_definition[] =
    SYS_X_HOST_SUMMARY_BY_FILE_IO_TYPE_VIEW_DEFINITION;

static const char sys_x_host_summary_by_file_io_type_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$host_summary_by_file_io_type` " SYS_HOST_SUMMARY_BY_FILE_IO_TYPE_VIEW_COLUMNS
    " AS " SYS_X_HOST_SUMMARY_BY_FILE_IO_TYPE_VIEW_DEFINITION;

static const char sys_x_host_summary_by_file_io_type_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$host_summary_by_file_io_type` " SYS_HOST_SUMMARY_BY_FILE_IO_TYPE_VIEW_COLUMNS
    " AS " SYS_X_HOST_SUMMARY_BY_FILE_IO_TYPE_VIEW_DEFINITION;

#undef SYS_HOST_SUMMARY_BY_FILE_IO_TYPE_VIEW_COLUMNS
#undef SYS_HOST_SUMMARY_BY_FILE_IO_TYPE_SELECT_PREFIX
#undef SYS_HOST_SUMMARY_BY_FILE_IO_TYPE_SELECT_SUFFIX
#undef SYS_HOST_SUMMARY_BY_FILE_IO_TYPE_VIEW_DEFINITION
#undef SYS_X_HOST_SUMMARY_BY_FILE_IO_TYPE_VIEW_DEFINITION

#define SYS_HOST_SUMMARY_BY_STAGES_VIEW_COLUMNS                                                    \
    "(`host`,`event_name`,`total`,`total_latency`,`avg_latency`)"

#define SYS_HOST_SUMMARY_BY_STAGES_SELECT_PREFIX                                                   \
    "select if((`performance_schema`.`events_stages_summary_by_host_by_event_name`.`HOST` is "     \
    "null),'background',`performance_schema`.`events_stages_summary_by_host_by_event_name`."       \
    "`HOST`) AS `host`,`performance_schema`.`events_stages_summary_by_host_by_event_name`."        \
    "`EVENT_NAME` AS `event_name`,`performance_schema`."                                           \
    "`events_stages_summary_by_host_by_event_name`.`COUNT_STAR` AS `total`,"

#define SYS_HOST_SUMMARY_BY_STAGES_SELECT_SUFFIX                                                   \
    " from `performance_schema`.`events_stages_summary_by_host_by_event_name` where ("             \
    "`performance_schema`.`events_stages_summary_by_host_by_event_name`.`SUM_TIMER_WAIT` <> "      \
    "0) order by if((`performance_schema`.`events_stages_summary_by_host_by_event_name`."          \
    "`HOST` is null),'background',`performance_schema`."                                           \
    "`events_stages_summary_by_host_by_event_name`.`HOST`),`performance_schema`."                  \
    "`events_stages_summary_by_host_by_event_name`.`SUM_TIMER_WAIT` desc"

#define SYS_HOST_SUMMARY_BY_STAGES_VIEW_DEFINITION                                                 \
    SYS_HOST_SUMMARY_BY_STAGES_SELECT_PREFIX                                                       \
    "format_pico_time(`performance_schema`.`events_stages_summary_by_host_by_event_name`."         \
    "`SUM_TIMER_WAIT`) AS `total_latency`,format_pico_time(`performance_schema`."                  \
    "`events_stages_summary_by_host_by_event_name`.`AVG_TIMER_WAIT`) AS "                          \
    "`avg_latency`" SYS_HOST_SUMMARY_BY_STAGES_SELECT_SUFFIX

#define SYS_X_HOST_SUMMARY_BY_STAGES_VIEW_DEFINITION                                               \
    SYS_HOST_SUMMARY_BY_STAGES_SELECT_PREFIX                                                       \
    "`performance_schema`.`events_stages_summary_by_host_by_event_name`.`SUM_TIMER_WAIT` AS "      \
    "`total_latency`,`performance_schema`.`events_stages_summary_by_host_by_event_name`."          \
    "`AVG_TIMER_WAIT` AS `avg_latency`" SYS_HOST_SUMMARY_BY_STAGES_SELECT_SUFFIX

static const char sys_host_summary_by_stages_view_definition[] =
    SYS_HOST_SUMMARY_BY_STAGES_VIEW_DEFINITION;

static const char sys_host_summary_by_stages_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`host_summary_by_stages` " SYS_HOST_SUMMARY_BY_STAGES_VIEW_COLUMNS
    " AS " SYS_HOST_SUMMARY_BY_STAGES_VIEW_DEFINITION;

static const char sys_host_summary_by_stages_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`host_summary_by_stages` " SYS_HOST_SUMMARY_BY_STAGES_VIEW_COLUMNS
    " AS " SYS_HOST_SUMMARY_BY_STAGES_VIEW_DEFINITION;

static const char sys_x_host_summary_by_stages_view_definition[] =
    SYS_X_HOST_SUMMARY_BY_STAGES_VIEW_DEFINITION;

static const char sys_x_host_summary_by_stages_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$host_summary_by_stages` " SYS_HOST_SUMMARY_BY_STAGES_VIEW_COLUMNS
    " AS " SYS_X_HOST_SUMMARY_BY_STAGES_VIEW_DEFINITION;

static const char sys_x_host_summary_by_stages_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$host_summary_by_stages` " SYS_HOST_SUMMARY_BY_STAGES_VIEW_COLUMNS
    " AS " SYS_X_HOST_SUMMARY_BY_STAGES_VIEW_DEFINITION;

#undef SYS_HOST_SUMMARY_BY_STAGES_VIEW_COLUMNS
#undef SYS_HOST_SUMMARY_BY_STAGES_SELECT_PREFIX
#undef SYS_HOST_SUMMARY_BY_STAGES_SELECT_SUFFIX
#undef SYS_HOST_SUMMARY_BY_STAGES_VIEW_DEFINITION
#undef SYS_X_HOST_SUMMARY_BY_STAGES_VIEW_DEFINITION

#define SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_VIEW_COLUMNS                                         \
    "(`host`,`total`,`total_latency`,`max_latency`,`lock_latency`,`cpu_latency`,`rows_sent`,"      \
    "`rows_examined`,`rows_affected`,`full_scans`)"

#define SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SOURCE                                               \
    "`performance_schema`.`events_statements_summary_by_host_by_event_name`"

#define SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_HOST_EXPR                                            \
    "if((" SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SOURCE                                            \
    ".`HOST` is null),'background'," SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SOURCE ".`HOST`)"

#define SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SELECT_PREFIX                                        \
    "select " SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_HOST_EXPR                                      \
    " AS `host`,sum(" SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SOURCE ".`COUNT_STAR`) AS `total`,"

#define SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SELECT_SUFFIX                                        \
    "sum(" SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SOURCE ".`SUM_ROWS_SENT`) AS `rows_sent`,"        \
    "sum(" SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SOURCE ".`SUM_ROWS_EXAMINED`) AS "                \
    "`rows_examined`,sum(" SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SOURCE                            \
    ".`SUM_ROWS_AFFECTED`) AS `rows_affected`,(sum(" SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SOURCE  \
    ".`SUM_NO_INDEX_USED`) + sum(" SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SOURCE                    \
    ".`SUM_NO_GOOD_INDEX_USED`)) AS `full_scans` "                                                 \
    "from " SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SOURCE                                           \
    " group by " SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_HOST_EXPR                                   \
    " order by sum(" SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SOURCE ".`SUM_TIMER_WAIT`) desc"

#define SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_VIEW_DEFINITION                                      \
    SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SELECT_PREFIX                                            \
    "format_pico_time(sum(" SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SOURCE ".`SUM_TIMER_WAIT`)) AS " \
    "`total_latency`,format_pico_time(max(" SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SOURCE           \
    ".`MAX_TIMER_WAIT`)) AS "                                                                      \
    "`max_latency`,format_pico_time(sum(" SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SOURCE             \
    ".`SUM_LOCK_TIME`)) AS "                                                                       \
    "`lock_latency`,format_pico_time(sum(" SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SOURCE            \
    ".`SUM_CPU_TIME`)) AS `cpu_latency`," SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SELECT_SUFFIX

#define SYS_X_HOST_SUMMARY_BY_STATEMENT_LATENCY_VIEW_DEFINITION                                    \
    SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SELECT_PREFIX                                            \
    "sum(" SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SOURCE                                            \
    ".`SUM_TIMER_WAIT`) AS `total_latency`,max(" SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SOURCE      \
    ".`MAX_TIMER_WAIT`) AS `max_latency`,sum(" SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SOURCE        \
    ".`SUM_LOCK_TIME`) AS `lock_latency`,sum(" SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SOURCE        \
    ".`SUM_CPU_TIME`) AS `cpu_latency`," SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SELECT_SUFFIX

static const char sys_host_summary_by_statement_latency_view_definition[] =
    SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_VIEW_DEFINITION;

static const char sys_host_summary_by_statement_latency_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`host_summary_by_statement_latency` " SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_VIEW_COLUMNS
    " AS " SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_VIEW_DEFINITION;

static const char sys_host_summary_by_statement_latency_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`host_summary_by_statement_latency` " SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_VIEW_COLUMNS
    " AS " SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_VIEW_DEFINITION;

static const char sys_x_host_summary_by_statement_latency_view_definition[] =
    SYS_X_HOST_SUMMARY_BY_STATEMENT_LATENCY_VIEW_DEFINITION;

static const char sys_x_host_summary_by_statement_latency_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$host_summary_by_statement_latency` " SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_VIEW_COLUMNS
    " AS " SYS_X_HOST_SUMMARY_BY_STATEMENT_LATENCY_VIEW_DEFINITION;

static const char sys_x_host_summary_by_statement_latency_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$host_summary_by_statement_latency`"
    " " SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_VIEW_COLUMNS
    " AS " SYS_X_HOST_SUMMARY_BY_STATEMENT_LATENCY_VIEW_DEFINITION;

#undef SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_VIEW_COLUMNS
#undef SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SOURCE
#undef SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_HOST_EXPR
#undef SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SELECT_PREFIX
#undef SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SELECT_SUFFIX
#undef SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_VIEW_DEFINITION
#undef SYS_X_HOST_SUMMARY_BY_STATEMENT_LATENCY_VIEW_DEFINITION

#define SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_VIEW_COLUMNS                                            \
    "(`host`,`statement`,`total`,`total_latency`,`max_latency`,`lock_latency`,`cpu_latency`,"      \
    "`rows_sent`,`rows_examined`,`rows_affected`,`full_scans`)"

#define SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE                                                  \
    "`performance_schema`.`events_statements_summary_by_host_by_event_name`"

#define SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_HOST_EXPR                                               \
    "if((" SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE                                               \
    ".`HOST` is null),'background'," SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE ".`HOST`)"

#define SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_STATEMENT_EXPR                                          \
    "substring_index(" SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE ".`EVENT_NAME`,'/',-(1))"

#define SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SELECT_PREFIX                                           \
    "select " SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_HOST_EXPR                                         \
    " AS `host`," SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_STATEMENT_EXPR                                \
    " AS `statement`," SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE ".`COUNT_STAR` AS `total`,"

#define SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SELECT_SUFFIX                                           \
    SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE                                                      \
    ".`SUM_ROWS_SENT` AS `rows_sent`," SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE                   \
    ".`SUM_ROWS_EXAMINED` AS `rows_examined`," SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE           \
    ".`SUM_ROWS_AFFECTED` AS `rows_affected`,(" SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE          \
    ".`SUM_NO_INDEX_USED` + " SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE                            \
    ".`SUM_NO_GOOD_INDEX_USED`) AS `full_scans` "                                                  \
    "from " SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE                                              \
    " where (" SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE                                           \
    ".`SUM_TIMER_WAIT` <> 0) order by " SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_HOST_EXPR               \
    "," SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE ".`SUM_TIMER_WAIT` desc"

#define SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_VIEW_DEFINITION                                         \
    SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SELECT_PREFIX                                               \
    "format_pico_time(" SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE ".`SUM_TIMER_WAIT`) AS "         \
    "`total_latency`,format_pico_time(" SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE                  \
    ".`MAX_TIMER_WAIT`) AS "                                                                       \
    "`max_latency`,format_pico_time(" SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE                    \
    ".`SUM_LOCK_TIME`) AS `lock_latency`,"                                                         \
    "format_pico_time(" SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE                                  \
    ".`SUM_CPU_TIME`) AS `cpu_latency`," SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SELECT_SUFFIX

#define SYS_X_HOST_SUMMARY_BY_STATEMENT_TYPE_VIEW_DEFINITION                                       \
    SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SELECT_PREFIX                                               \
    SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE                                                      \
    ".`SUM_TIMER_WAIT` AS `total_latency`," SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE              \
    ".`MAX_TIMER_WAIT` AS `max_latency`," SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE                \
    ".`SUM_LOCK_TIME` AS `lock_latency`," SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE                \
    ".`SUM_CPU_TIME` AS `cpu_latency`," SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SELECT_SUFFIX

static const char sys_host_summary_by_statement_type_view_definition[] =
    SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_VIEW_DEFINITION;

static const char sys_host_summary_by_statement_type_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`host_summary_by_statement_type` " SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_VIEW_COLUMNS
    " AS " SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_VIEW_DEFINITION;

static const char sys_host_summary_by_statement_type_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`host_summary_by_statement_type` " SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_VIEW_COLUMNS
    " AS " SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_VIEW_DEFINITION;

static const char sys_x_host_summary_by_statement_type_view_definition[] =
    SYS_X_HOST_SUMMARY_BY_STATEMENT_TYPE_VIEW_DEFINITION;

static const char sys_x_host_summary_by_statement_type_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$host_summary_by_statement_type` " SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_VIEW_COLUMNS
    " AS " SYS_X_HOST_SUMMARY_BY_STATEMENT_TYPE_VIEW_DEFINITION;

static const char sys_x_host_summary_by_statement_type_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$host_summary_by_statement_type` " SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_VIEW_COLUMNS
    " AS " SYS_X_HOST_SUMMARY_BY_STATEMENT_TYPE_VIEW_DEFINITION;

#undef SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_VIEW_COLUMNS
#undef SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE
#undef SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_HOST_EXPR
#undef SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_STATEMENT_EXPR
#undef SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SELECT_PREFIX
#undef SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SELECT_SUFFIX
#undef SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_VIEW_DEFINITION
#undef SYS_X_HOST_SUMMARY_BY_STATEMENT_TYPE_VIEW_DEFINITION

#define SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_VIEW_COLUMNS                                           \
    "(`host`,`current_count_used`,`current_allocated`,`current_avg_alloc`,`current_max_alloc`,"    \
    "`total_allocated`)"

#define SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_SOURCE                                                 \
    "`performance_schema`.`memory_summary_by_host_by_event_name`"

#define SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_HOST_EXPR                                              \
    "if((" SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_SOURCE                                              \
    ".`HOST` is null),'background'," SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_SOURCE ".`HOST`)"

#define SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_CURRENT_BYTES_EXPR                                     \
    "sum(" SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_SOURCE ".`CURRENT_NUMBER_OF_BYTES_USED`)"

#define SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_COUNT_EXPR                                             \
    "sum(" SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_SOURCE ".`CURRENT_COUNT_USED`)"

#define SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_SELECT_PREFIX                                          \
    "select " SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_HOST_EXPR                                        \
    " AS `host`," SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_COUNT_EXPR " AS `current_count_used`,"

#define SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_AVG_EXPR                                               \
    "ifnull((" SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_CURRENT_BYTES_EXPR                              \
    " / nullif(" SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_COUNT_EXPR ",0)),0)"

#define SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_SELECT_SUFFIX                                          \
    "max(" SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_SOURCE ".`CURRENT_NUMBER_OF_BYTES_USED`) AS "       \
    "`current_max_alloc`,sum(" SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_SOURCE                          \
    ".`SUM_NUMBER_OF_BYTES_ALLOC`) AS `total_allocated` "                                          \
    "from " SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_SOURCE                                             \
    " group by " SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_HOST_EXPR                                     \
    " order by " SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_CURRENT_BYTES_EXPR " desc"

#define SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_VIEW_DEFINITION                                        \
    SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_SELECT_PREFIX                                              \
    "format_bytes(" SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_CURRENT_BYTES_EXPR                         \
    ") AS `current_allocated`,format_bytes(" SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_AVG_EXPR          \
    ") AS `current_avg_alloc`,format_bytes(max(" SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_SOURCE        \
    ".`CURRENT_NUMBER_OF_BYTES_USED`)) AS "                                                        \
    "`current_max_alloc`,format_bytes(sum(" SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_SOURCE             \
    ".`SUM_NUMBER_OF_BYTES_ALLOC`)) AS `total_allocated` "                                         \
    "from " SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_SOURCE                                             \
    " group by " SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_HOST_EXPR                                     \
    " order by " SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_CURRENT_BYTES_EXPR " desc"

#define SYS_X_MEMORY_BY_HOST_BY_CURRENT_BYTES_VIEW_DEFINITION                                      \
    SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_SELECT_PREFIX                                              \
    SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_CURRENT_BYTES_EXPR                                         \
    " AS `current_allocated`," SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_AVG_EXPR                        \
    " AS `current_avg_alloc`," SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_SELECT_SUFFIX

static const char sys_memory_by_host_by_current_bytes_view_definition[] =
    SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_VIEW_DEFINITION;

static const char sys_memory_by_host_by_current_bytes_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`memory_by_host_by_current_bytes` " SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_VIEW_COLUMNS
    " AS " SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_VIEW_DEFINITION;

static const char sys_memory_by_host_by_current_bytes_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`memory_by_host_by_current_bytes` " SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_VIEW_COLUMNS
    " AS " SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_VIEW_DEFINITION;

static const char sys_x_memory_by_host_by_current_bytes_view_definition[] =
    SYS_X_MEMORY_BY_HOST_BY_CURRENT_BYTES_VIEW_DEFINITION;

static const char sys_x_memory_by_host_by_current_bytes_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$memory_by_host_by_current_bytes` " SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_VIEW_COLUMNS
    " AS " SYS_X_MEMORY_BY_HOST_BY_CURRENT_BYTES_VIEW_DEFINITION;

static const char sys_x_memory_by_host_by_current_bytes_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$memory_by_host_by_current_bytes` " SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_VIEW_COLUMNS
    " AS " SYS_X_MEMORY_BY_HOST_BY_CURRENT_BYTES_VIEW_DEFINITION;

#undef SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_VIEW_COLUMNS
#undef SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_SOURCE
#undef SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_HOST_EXPR
#undef SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_CURRENT_BYTES_EXPR
#undef SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_COUNT_EXPR
#undef SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_SELECT_PREFIX
#undef SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_AVG_EXPR
#undef SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_SELECT_SUFFIX
#undef SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_VIEW_DEFINITION
#undef SYS_X_MEMORY_BY_HOST_BY_CURRENT_BYTES_VIEW_DEFINITION

#define SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_VIEW_COLUMNS                                         \
    "(`thread_id`,`user`,`current_count_used`,`current_allocated`,`current_avg_alloc`,"            \
    "`current_max_alloc`,`total_allocated`)"

#define SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_MEMORY_SOURCE                                        \
    "`performance_schema`.`memory_summary_by_thread_by_event_name` `mt`"

#define SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_THREAD_SOURCE "`performance_schema`.`threads` `t`"

#define SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_FROM_EXPR                                            \
    "(" SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_MEMORY_SOURCE                                        \
    " join " SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_THREAD_SOURCE                                   \
    " on((`mt`.`THREAD_ID` = `t`.`THREAD_ID`)))"

#define SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_USER_EXPR                                            \
    "if((`t`.`NAME` = 'thread/sql/one_connection'),concat(`t`.`PROCESSLIST_USER`,'@',"             \
    "convert(`t`.`PROCESSLIST_HOST` using utf8mb4)),replace(`t`.`NAME`,'thread/',''))"

#define SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_CURRENT_BYTES_EXPR                                   \
    "sum(`mt`.`CURRENT_NUMBER_OF_BYTES_USED`)"

#define SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_COUNT_EXPR "sum(`mt`.`CURRENT_COUNT_USED`)"

#define SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_AVG_EXPR                                             \
    "ifnull((" SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_CURRENT_BYTES_EXPR                            \
    " / nullif(" SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_COUNT_EXPR ",0)),0)"

#define SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_SELECT_SUFFIX                                        \
    "max(`mt`.`CURRENT_NUMBER_OF_BYTES_USED`) AS `current_max_alloc`,"                             \
    "sum(`mt`.`SUM_NUMBER_OF_BYTES_ALLOC`) AS `total_allocated` "                                  \
    "from " SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_FROM_EXPR

#define SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_VIEW_DEFINITION                                      \
    "select `mt`.`THREAD_ID` AS `thread_id`," SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_USER_EXPR      \
    " AS `user`," SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_COUNT_EXPR " AS `current_count_used`,"     \
    "format_bytes(" SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_CURRENT_BYTES_EXPR                       \
    ") AS `current_allocated`,format_bytes(" SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_AVG_EXPR        \
    ") AS `current_avg_alloc`,format_bytes(max(`mt`.`CURRENT_NUMBER_OF_BYTES_USED`)) AS "          \
    "`current_max_alloc`,format_bytes(sum(`mt`.`SUM_NUMBER_OF_BYTES_ALLOC`)) AS "                  \
    "`total_allocated` from " SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_FROM_EXPR                      \
    " group by `mt`.`THREAD_ID`," SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_USER_EXPR                  \
    " order by " SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_CURRENT_BYTES_EXPR " desc"

#define SYS_X_MEMORY_BY_THREAD_BY_CURRENT_BYTES_VIEW_DEFINITION                                    \
    "select `t`.`THREAD_ID` AS `thread_id`," SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_USER_EXPR       \
    " AS `user`," SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_COUNT_EXPR                                 \
    " AS `current_count_used`," SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_CURRENT_BYTES_EXPR           \
    " AS `current_allocated`," SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_AVG_EXPR                      \
    " AS `current_avg_alloc`," SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_SELECT_SUFFIX                 \
    " group by `t`.`THREAD_ID`," SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_USER_EXPR                   \
    " order by " SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_CURRENT_BYTES_EXPR " desc"

static const char sys_memory_by_thread_by_current_bytes_view_definition[] =
    SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_VIEW_DEFINITION;

static const char sys_memory_by_thread_by_current_bytes_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`memory_by_thread_by_current_bytes` " SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_VIEW_COLUMNS
    " AS " SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_VIEW_DEFINITION;

static const char sys_memory_by_thread_by_current_bytes_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`memory_by_thread_by_current_bytes` " SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_VIEW_COLUMNS
    " AS " SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_VIEW_DEFINITION;

static const char sys_x_memory_by_thread_by_current_bytes_view_definition[] =
    SYS_X_MEMORY_BY_THREAD_BY_CURRENT_BYTES_VIEW_DEFINITION;

static const char sys_x_memory_by_thread_by_current_bytes_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$memory_by_thread_by_current_bytes` " SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_VIEW_COLUMNS
    " AS " SYS_X_MEMORY_BY_THREAD_BY_CURRENT_BYTES_VIEW_DEFINITION;

static const char sys_x_memory_by_thread_by_current_bytes_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$memory_by_thread_by_current_bytes`"
    " " SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_VIEW_COLUMNS
    " AS " SYS_X_MEMORY_BY_THREAD_BY_CURRENT_BYTES_VIEW_DEFINITION;

#undef SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_VIEW_COLUMNS
#undef SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_MEMORY_SOURCE
#undef SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_THREAD_SOURCE
#undef SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_FROM_EXPR
#undef SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_USER_EXPR
#undef SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_CURRENT_BYTES_EXPR
#undef SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_COUNT_EXPR
#undef SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_AVG_EXPR
#undef SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_SELECT_SUFFIX
#undef SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_VIEW_DEFINITION
#undef SYS_X_MEMORY_BY_THREAD_BY_CURRENT_BYTES_VIEW_DEFINITION

#define SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_VIEW_COLUMNS                                           \
    "(`user`,`current_count_used`,`current_allocated`,`current_avg_alloc`,`current_max_alloc`,"    \
    "`total_allocated`)"

#define SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_SOURCE                                                 \
    "`performance_schema`.`memory_summary_by_user_by_event_name`"

#define SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_USER_EXPR                                              \
    "if((" SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_SOURCE                                              \
    ".`USER` is null),'background'," SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_SOURCE ".`USER`)"

#define SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_CURRENT_BYTES_EXPR                                     \
    "sum(" SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_SOURCE ".`CURRENT_NUMBER_OF_BYTES_USED`)"

#define SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_COUNT_EXPR                                             \
    "sum(" SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_SOURCE ".`CURRENT_COUNT_USED`)"

#define SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_SELECT_PREFIX                                          \
    "select " SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_USER_EXPR                                        \
    " AS `user`," SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_COUNT_EXPR " AS `current_count_used`,"

#define SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_AVG_EXPR                                               \
    "ifnull((" SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_CURRENT_BYTES_EXPR                              \
    " / nullif(" SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_COUNT_EXPR ",0)),0)"

#define SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_SELECT_SUFFIX                                          \
    "max(" SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_SOURCE ".`CURRENT_NUMBER_OF_BYTES_USED`) AS "       \
    "`current_max_alloc`,sum(" SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_SOURCE                          \
    ".`SUM_NUMBER_OF_BYTES_ALLOC`) AS `total_allocated` "                                          \
    "from " SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_SOURCE                                             \
    " group by " SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_USER_EXPR                                     \
    " order by " SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_CURRENT_BYTES_EXPR " desc"

#define SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_VIEW_DEFINITION                                        \
    SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_SELECT_PREFIX                                              \
    "format_bytes(" SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_CURRENT_BYTES_EXPR                         \
    ") AS `current_allocated`,format_bytes(" SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_AVG_EXPR          \
    ") AS `current_avg_alloc`,format_bytes(max(" SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_SOURCE        \
    ".`CURRENT_NUMBER_OF_BYTES_USED`)) AS "                                                        \
    "`current_max_alloc`,format_bytes(sum(" SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_SOURCE             \
    ".`SUM_NUMBER_OF_BYTES_ALLOC`)) AS `total_allocated` "                                         \
    "from " SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_SOURCE                                             \
    " group by " SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_USER_EXPR                                     \
    " order by " SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_CURRENT_BYTES_EXPR " desc"

#define SYS_X_MEMORY_BY_USER_BY_CURRENT_BYTES_VIEW_DEFINITION                                      \
    SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_SELECT_PREFIX                                              \
    SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_CURRENT_BYTES_EXPR                                         \
    " AS `current_allocated`," SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_AVG_EXPR                        \
    " AS `current_avg_alloc`," SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_SELECT_SUFFIX

static const char sys_memory_by_user_by_current_bytes_view_definition[] =
    SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_VIEW_DEFINITION;

static const char sys_memory_by_user_by_current_bytes_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`memory_by_user_by_current_bytes` " SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_VIEW_COLUMNS
    " AS " SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_VIEW_DEFINITION;

static const char sys_memory_by_user_by_current_bytes_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`memory_by_user_by_current_bytes` " SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_VIEW_COLUMNS
    " AS " SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_VIEW_DEFINITION;

static const char sys_x_memory_by_user_by_current_bytes_view_definition[] =
    SYS_X_MEMORY_BY_USER_BY_CURRENT_BYTES_VIEW_DEFINITION;

static const char sys_x_memory_by_user_by_current_bytes_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$memory_by_user_by_current_bytes` " SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_VIEW_COLUMNS
    " AS " SYS_X_MEMORY_BY_USER_BY_CURRENT_BYTES_VIEW_DEFINITION;

static const char sys_x_memory_by_user_by_current_bytes_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$memory_by_user_by_current_bytes` " SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_VIEW_COLUMNS
    " AS " SYS_X_MEMORY_BY_USER_BY_CURRENT_BYTES_VIEW_DEFINITION;

#undef SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_VIEW_COLUMNS
#undef SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_SOURCE
#undef SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_USER_EXPR
#undef SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_CURRENT_BYTES_EXPR
#undef SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_COUNT_EXPR
#undef SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_SELECT_PREFIX
#undef SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_AVG_EXPR
#undef SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_SELECT_SUFFIX
#undef SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_VIEW_DEFINITION
#undef SYS_X_MEMORY_BY_USER_BY_CURRENT_BYTES_VIEW_DEFINITION

#define SYS_INNODB_BUFFER_STATS_BY_SCHEMA_VIEW_COLUMNS                                             \
    "(`object_schema`,`allocated`,`data`,`pages`,`pages_hashed`,`pages_old`,`rows_cached`)"

#define SYS_INNODB_BUFFER_STATS_BY_SCHEMA_OBJECT_EXPR                                              \
    "if((locate('.',`ibp`.`TABLE_NAME`) = 0),'InnoDB System',"                                     \
    "replace(substring_index(`ibp`.`TABLE_NAME`,'.',1),'`',''))"

#define SYS_INNODB_BUFFER_STATS_BY_SCHEMA_ALLOCATED_EXPR                                           \
    "sum(if((`ibp`.`COMPRESSED_SIZE` = 0),16384,`ibp`.`COMPRESSED_SIZE`))"

#define SYS_INNODB_BUFFER_STATS_BY_SCHEMA_SELECT_PREFIX                                            \
    "select " SYS_INNODB_BUFFER_STATS_BY_SCHEMA_OBJECT_EXPR " AS `object_schema`,"

#define SYS_INNODB_BUFFER_STATS_BY_SCHEMA_SELECT_SUFFIX                                            \
    "`ibp`.`DATA_SIZE`)) AS `data`,count(`ibp`.`PAGE_NUMBER`) AS `pages`,"                         \
    "count(if((`ibp`.`IS_HASHED` = 'YES'),1,NULL)) AS `pages_hashed`,"                             \
    "count(if((`ibp`.`IS_OLD` = 'YES'),1,NULL)) AS `pages_old`,"

#define SYS_INNODB_BUFFER_STATS_BY_SCHEMA_FROM_SUFFIX                                              \
    " from `information_schema`.`INNODB_BUFFER_PAGE` `ibp` where (`ibp`.`TABLE_NAME` is not null)" \
    " group by `object_schema` order by " SYS_INNODB_BUFFER_STATS_BY_SCHEMA_ALLOCATED_EXPR " desc"

#define SYS_INNODB_BUFFER_STATS_BY_SCHEMA_VIEW_DEFINITION                                          \
    SYS_INNODB_BUFFER_STATS_BY_SCHEMA_SELECT_PREFIX                                                \
    "format_bytes(" SYS_INNODB_BUFFER_STATS_BY_SCHEMA_ALLOCATED_EXPR ") AS `allocated`,"           \
    "format_bytes(sum(" SYS_INNODB_BUFFER_STATS_BY_SCHEMA_SELECT_SUFFIX                            \
    "round((sum(`ibp`.`NUMBER_RECORDS`) / count(distinct `ibp`.`INDEX_NAME`)),0) AS "              \
    "`rows_cached`" SYS_INNODB_BUFFER_STATS_BY_SCHEMA_FROM_SUFFIX

#define SYS_X_INNODB_BUFFER_STATS_BY_SCHEMA_VIEW_DEFINITION                                        \
    SYS_INNODB_BUFFER_STATS_BY_SCHEMA_SELECT_PREFIX                                                \
    SYS_INNODB_BUFFER_STATS_BY_SCHEMA_ALLOCATED_EXPR                                               \
    " AS `allocated`,sum(" SYS_INNODB_BUFFER_STATS_BY_SCHEMA_SELECT_SUFFIX                         \
    "round(ifnull((sum(`ibp`.`NUMBER_RECORDS`) / nullif(count(distinct "                           \
    "`ibp`.`INDEX_NAME`),0)),0),"                                                                  \
    "0) AS `rows_cached`" SYS_INNODB_BUFFER_STATS_BY_SCHEMA_FROM_SUFFIX

static const char sys_innodb_buffer_stats_by_schema_view_definition[] =
    SYS_INNODB_BUFFER_STATS_BY_SCHEMA_VIEW_DEFINITION;

static const char sys_innodb_buffer_stats_by_schema_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`innodb_buffer_stats_by_schema` " SYS_INNODB_BUFFER_STATS_BY_SCHEMA_VIEW_COLUMNS
    " AS " SYS_INNODB_BUFFER_STATS_BY_SCHEMA_VIEW_DEFINITION;

static const char sys_innodb_buffer_stats_by_schema_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`innodb_buffer_stats_by_schema` " SYS_INNODB_BUFFER_STATS_BY_SCHEMA_VIEW_COLUMNS
    " AS " SYS_INNODB_BUFFER_STATS_BY_SCHEMA_VIEW_DEFINITION;

static const char sys_x_innodb_buffer_stats_by_schema_view_definition[] =
    SYS_X_INNODB_BUFFER_STATS_BY_SCHEMA_VIEW_DEFINITION;

static const char sys_x_innodb_buffer_stats_by_schema_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$innodb_buffer_stats_by_schema` " SYS_INNODB_BUFFER_STATS_BY_SCHEMA_VIEW_COLUMNS
    " AS " SYS_X_INNODB_BUFFER_STATS_BY_SCHEMA_VIEW_DEFINITION;

static const char sys_x_innodb_buffer_stats_by_schema_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$innodb_buffer_stats_by_schema` " SYS_INNODB_BUFFER_STATS_BY_SCHEMA_VIEW_COLUMNS
    " AS " SYS_X_INNODB_BUFFER_STATS_BY_SCHEMA_VIEW_DEFINITION;

#undef SYS_INNODB_BUFFER_STATS_BY_SCHEMA_VIEW_COLUMNS
#undef SYS_INNODB_BUFFER_STATS_BY_SCHEMA_OBJECT_EXPR
#undef SYS_INNODB_BUFFER_STATS_BY_SCHEMA_ALLOCATED_EXPR
#undef SYS_INNODB_BUFFER_STATS_BY_SCHEMA_SELECT_PREFIX
#undef SYS_INNODB_BUFFER_STATS_BY_SCHEMA_SELECT_SUFFIX
#undef SYS_INNODB_BUFFER_STATS_BY_SCHEMA_FROM_SUFFIX
#undef SYS_INNODB_BUFFER_STATS_BY_SCHEMA_VIEW_DEFINITION
#undef SYS_X_INNODB_BUFFER_STATS_BY_SCHEMA_VIEW_DEFINITION

#define SYS_INNODB_BUFFER_STATS_BY_TABLE_VIEW_COLUMNS                                              \
    "(`object_schema`,`object_name`,`allocated`,`data`,`pages`,`pages_hashed`,`pages_old`,"        \
    "`rows_cached`)"

#define SYS_INNODB_BUFFER_STATS_BY_TABLE_OBJECT_SCHEMA_EXPR                                        \
    "if((locate('.',`ibp`.`TABLE_NAME`) = 0),'InnoDB System',"                                     \
    "replace(substring_index(`ibp`.`TABLE_NAME`,'.',1),'`',''))"

#define SYS_INNODB_BUFFER_STATS_BY_TABLE_OBJECT_NAME_EXPR                                          \
    "replace(substring_index(`ibp`.`TABLE_NAME`,'.',-(1)),'`','')"

#define SYS_INNODB_BUFFER_STATS_BY_TABLE_ALLOCATED_EXPR                                            \
    "sum(if((`ibp`.`COMPRESSED_SIZE` = 0),16384,`ibp`.`COMPRESSED_SIZE`))"

#define SYS_INNODB_BUFFER_STATS_BY_TABLE_SELECT_PREFIX                                             \
    "select " SYS_INNODB_BUFFER_STATS_BY_TABLE_OBJECT_SCHEMA_EXPR                                  \
    " AS `object_schema`," SYS_INNODB_BUFFER_STATS_BY_TABLE_OBJECT_NAME_EXPR " AS `object_name`,"

#define SYS_INNODB_BUFFER_STATS_BY_TABLE_SELECT_SUFFIX                                             \
    "`ibp`.`DATA_SIZE`)) AS `data`,count(`ibp`.`PAGE_NUMBER`) AS `pages`,"                         \
    "count(if((`ibp`.`IS_HASHED` = 'YES'),1,NULL)) AS `pages_hashed`,"                             \
    "count(if((`ibp`.`IS_OLD` = 'YES'),1,NULL)) AS `pages_old`,"

#define SYS_INNODB_BUFFER_STATS_BY_TABLE_FROM_SUFFIX                                               \
    " from `information_schema`.`INNODB_BUFFER_PAGE` `ibp` where (`ibp`.`TABLE_NAME` is not null)" \
    " group by `object_schema`,`object_name` order "                                               \
    "by " SYS_INNODB_BUFFER_STATS_BY_TABLE_ALLOCATED_EXPR " desc"

#define SYS_INNODB_BUFFER_STATS_BY_TABLE_VIEW_DEFINITION                                           \
    SYS_INNODB_BUFFER_STATS_BY_TABLE_SELECT_PREFIX                                                 \
    "format_bytes(" SYS_INNODB_BUFFER_STATS_BY_TABLE_ALLOCATED_EXPR ") AS `allocated`,"            \
    "format_bytes(sum(" SYS_INNODB_BUFFER_STATS_BY_TABLE_SELECT_SUFFIX                             \
    "round((sum(`ibp`.`NUMBER_RECORDS`) / count(distinct `ibp`.`INDEX_NAME`)),0) AS "              \
    "`rows_cached`" SYS_INNODB_BUFFER_STATS_BY_TABLE_FROM_SUFFIX

#define SYS_X_INNODB_BUFFER_STATS_BY_TABLE_VIEW_DEFINITION                                         \
    SYS_INNODB_BUFFER_STATS_BY_TABLE_SELECT_PREFIX                                                 \
    SYS_INNODB_BUFFER_STATS_BY_TABLE_ALLOCATED_EXPR                                                \
    " AS `allocated`,sum(" SYS_INNODB_BUFFER_STATS_BY_TABLE_SELECT_SUFFIX                          \
    "round(ifnull((sum(`ibp`.`NUMBER_RECORDS`) / nullif(count(distinct "                           \
    "`ibp`.`INDEX_NAME`),0)),0),"                                                                  \
    "0) AS `rows_cached`" SYS_INNODB_BUFFER_STATS_BY_TABLE_FROM_SUFFIX

static const char sys_innodb_buffer_stats_by_table_view_definition[] =
    SYS_INNODB_BUFFER_STATS_BY_TABLE_VIEW_DEFINITION;

static const char sys_innodb_buffer_stats_by_table_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`innodb_buffer_stats_by_table` " SYS_INNODB_BUFFER_STATS_BY_TABLE_VIEW_COLUMNS
    " AS " SYS_INNODB_BUFFER_STATS_BY_TABLE_VIEW_DEFINITION;

static const char sys_innodb_buffer_stats_by_table_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`innodb_buffer_stats_by_table` " SYS_INNODB_BUFFER_STATS_BY_TABLE_VIEW_COLUMNS
    " AS " SYS_INNODB_BUFFER_STATS_BY_TABLE_VIEW_DEFINITION;

static const char sys_x_innodb_buffer_stats_by_table_view_definition[] =
    SYS_X_INNODB_BUFFER_STATS_BY_TABLE_VIEW_DEFINITION;

static const char sys_x_innodb_buffer_stats_by_table_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$innodb_buffer_stats_by_table` " SYS_INNODB_BUFFER_STATS_BY_TABLE_VIEW_COLUMNS
    " AS " SYS_X_INNODB_BUFFER_STATS_BY_TABLE_VIEW_DEFINITION;

static const char sys_x_innodb_buffer_stats_by_table_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$innodb_buffer_stats_by_table` " SYS_INNODB_BUFFER_STATS_BY_TABLE_VIEW_COLUMNS
    " AS " SYS_X_INNODB_BUFFER_STATS_BY_TABLE_VIEW_DEFINITION;

#undef SYS_INNODB_BUFFER_STATS_BY_TABLE_VIEW_COLUMNS
#undef SYS_INNODB_BUFFER_STATS_BY_TABLE_OBJECT_SCHEMA_EXPR
#undef SYS_INNODB_BUFFER_STATS_BY_TABLE_OBJECT_NAME_EXPR
#undef SYS_INNODB_BUFFER_STATS_BY_TABLE_ALLOCATED_EXPR
#undef SYS_INNODB_BUFFER_STATS_BY_TABLE_SELECT_PREFIX
#undef SYS_INNODB_BUFFER_STATS_BY_TABLE_SELECT_SUFFIX
#undef SYS_INNODB_BUFFER_STATS_BY_TABLE_FROM_SUFFIX
#undef SYS_INNODB_BUFFER_STATS_BY_TABLE_VIEW_DEFINITION
#undef SYS_X_INNODB_BUFFER_STATS_BY_TABLE_VIEW_DEFINITION

#define SYS_INNODB_LOCK_WAITS_VIEW_COLUMNS                                                         \
    "(`wait_started`,`wait_age`,`wait_age_secs`,`locked_table`,`locked_table_schema`,"             \
    "`locked_table_name`,`locked_table_partition`,`locked_table_subpartition`,`locked_index`,"     \
    "`locked_type`,`waiting_trx_id`,`waiting_trx_started`,`waiting_trx_age`,"                      \
    "`waiting_trx_rows_locked`,`waiting_trx_rows_modified`,`waiting_pid`,`waiting_query`,"         \
    "`waiting_lock_id`,`waiting_lock_mode`,`blocking_trx_id`,`blocking_pid`,`blocking_query`,"     \
    "`blocking_lock_id`,`blocking_lock_mode`,`blocking_trx_started`,`blocking_trx_age`,"           \
    "`blocking_trx_rows_locked`,`blocking_trx_rows_modified`,`sql_kill_blocking_query`,"           \
    "`sql_kill_blocking_connection`)"

#define SYS_INNODB_LOCK_WAITS_SELECT_PREFIX                                                        \
    "select `r`.`trx_wait_started` AS `wait_started`,timediff(now(),"                              \
    "`r`.`trx_wait_started`) AS `wait_age`,timestampdiff(SECOND,`r`.`trx_wait_started`,now()) "    \
    "AS `wait_age_secs`,concat(`sys`.`quote_identifier`(`rl`.`OBJECT_SCHEMA`),'.',"                \
    "`sys`.`quote_identifier`(`rl`.`OBJECT_NAME`)) AS `locked_table`,`rl`.`OBJECT_SCHEMA` AS "     \
    "`locked_table_schema`,`rl`.`OBJECT_NAME` AS `locked_table_name`,`rl`.`PARTITION_NAME` AS "    \
    "`locked_table_partition`,`rl`.`SUBPARTITION_NAME` AS `locked_table_subpartition`,"            \
    "`rl`.`INDEX_NAME` AS `locked_index`,`rl`.`LOCK_TYPE` AS `locked_type`,`r`.`trx_id` AS "       \
    "`waiting_trx_id`,`r`.`trx_started` AS `waiting_trx_started`,timediff(now(),"                  \
    "`r`.`trx_started`) AS `waiting_trx_age`,`r`.`trx_rows_locked` AS "                            \
    "`waiting_trx_rows_locked`,`r`.`trx_rows_modified` AS `waiting_trx_rows_modified`,"            \
    "`r`.`trx_mysql_thread_id` AS `waiting_pid`,"

#define SYS_INNODB_LOCK_WAITS_SELECT_BRIDGE                                                        \
    " AS `waiting_query`,`rl`.`ENGINE_LOCK_ID` AS `waiting_lock_id`,`rl`.`LOCK_MODE` AS "          \
    "`waiting_lock_mode`,`b`.`trx_id` AS `blocking_trx_id`,`b`.`trx_mysql_thread_id` AS "          \
    "`blocking_pid`,"

#define SYS_INNODB_LOCK_WAITS_SELECT_SUFFIX                                                        \
    " AS `blocking_query`,`bl`.`ENGINE_LOCK_ID` AS `blocking_lock_id`,`bl`.`LOCK_MODE` AS "        \
    "`blocking_lock_mode`,`b`.`trx_started` AS `blocking_trx_started`,timediff(now(),"             \
    "`b`.`trx_started`) AS `blocking_trx_age`,`b`.`trx_rows_locked` AS "                           \
    "`blocking_trx_rows_locked`,`b`.`trx_rows_modified` AS `blocking_trx_rows_modified`,"          \
    "concat('KILL QUERY ',`b`.`trx_mysql_thread_id`) AS `sql_kill_blocking_query`,"                \
    "concat('KILL ',`b`.`trx_mysql_thread_id`) AS `sql_kill_blocking_connection` from "            \
    "((((`performance_schema`.`data_lock_waits` `w` join `information_schema`.`INNODB_TRX` "       \
    "`b` on((`b`.`trx_id` = cast(`w`.`BLOCKING_ENGINE_TRANSACTION_ID` as char charset "            \
    "utf8mb4)))) join `information_schema`.`INNODB_TRX` `r` on((`r`.`trx_id` = cast("              \
    "`w`.`REQUESTING_ENGINE_TRANSACTION_ID` as char charset utf8mb4)))) join "                     \
    "`performance_schema`.`data_locks` `bl` on(((`bl`.`ENGINE_LOCK_ID` = "                         \
    "`w`.`BLOCKING_ENGINE_LOCK_ID`) and (`bl`.`ENGINE` = `w`.`ENGINE`)))) join "                   \
    "`performance_schema`.`data_locks` `rl` on(((`rl`.`ENGINE_LOCK_ID` = "                         \
    "`w`.`REQUESTING_ENGINE_LOCK_ID`) and (`rl`.`ENGINE` = `w`.`ENGINE`)))) order by "             \
    "`r`.`trx_wait_started`"

#define SYS_INNODB_LOCK_WAITS_VIEW_DEFINITION                                                      \
    SYS_INNODB_LOCK_WAITS_SELECT_PREFIX                                                            \
    "`sys`.`format_statement`(`r`.`trx_query`)" SYS_INNODB_LOCK_WAITS_SELECT_BRIDGE                \
    "`sys`.`format_statement`(`b`.`trx_query`)" SYS_INNODB_LOCK_WAITS_SELECT_SUFFIX

#define SYS_X_INNODB_LOCK_WAITS_VIEW_DEFINITION                                                    \
    SYS_INNODB_LOCK_WAITS_SELECT_PREFIX "`r`.`trx_query`" SYS_INNODB_LOCK_WAITS_SELECT_BRIDGE      \
                                        "`b`.`trx_query`" SYS_INNODB_LOCK_WAITS_SELECT_SUFFIX

static const char sys_innodb_lock_waits_view_definition[] = SYS_INNODB_LOCK_WAITS_VIEW_DEFINITION;

static const char sys_innodb_lock_waits_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`innodb_lock_waits` " SYS_INNODB_LOCK_WAITS_VIEW_COLUMNS
    " AS " SYS_INNODB_LOCK_WAITS_VIEW_DEFINITION;

static const char sys_innodb_lock_waits_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`innodb_lock_waits` " SYS_INNODB_LOCK_WAITS_VIEW_COLUMNS
    " AS " SYS_INNODB_LOCK_WAITS_VIEW_DEFINITION;

static const char sys_x_innodb_lock_waits_view_definition[] =
    SYS_X_INNODB_LOCK_WAITS_VIEW_DEFINITION;

static const char sys_x_innodb_lock_waits_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$innodb_lock_waits` " SYS_INNODB_LOCK_WAITS_VIEW_COLUMNS
    " AS " SYS_X_INNODB_LOCK_WAITS_VIEW_DEFINITION;

static const char sys_x_innodb_lock_waits_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$innodb_lock_waits` " SYS_INNODB_LOCK_WAITS_VIEW_COLUMNS
    " AS " SYS_X_INNODB_LOCK_WAITS_VIEW_DEFINITION;

#undef SYS_INNODB_LOCK_WAITS_VIEW_COLUMNS
#undef SYS_INNODB_LOCK_WAITS_SELECT_PREFIX
#undef SYS_INNODB_LOCK_WAITS_SELECT_BRIDGE
#undef SYS_INNODB_LOCK_WAITS_SELECT_SUFFIX
#undef SYS_INNODB_LOCK_WAITS_VIEW_DEFINITION
#undef SYS_X_INNODB_LOCK_WAITS_VIEW_DEFINITION

#define SYS_IO_BY_THREAD_BY_LATENCY_VIEW_COLUMNS                                                   \
    "(`user`,`total`,`total_latency`,`min_latency`,`avg_latency`,`max_latency`,`thread_id`,"       \
    "`processlist_id`)"

#define SYS_IO_BY_THREAD_BY_LATENCY_SELECT_PREFIX                                                  \
    "select if((`performance_schema`.`threads`.`PROCESSLIST_ID` is null),substring_index("         \
    "`performance_schema`.`threads`.`NAME`,'/',-(1)),concat(`performance_schema`.`threads`."       \
    "`PROCESSLIST_USER`,'@',convert(`performance_schema`.`threads`.`PROCESSLIST_HOST` using "      \
    "utf8mb4))) AS `user`,sum(`performance_schema`."                                               \
    "`events_waits_summary_by_thread_by_event_name`.`COUNT_STAR`) AS `total`,"

#define SYS_IO_BY_THREAD_BY_LATENCY_SELECT_SUFFIX                                                  \
    "`performance_schema`.`events_waits_summary_by_thread_by_event_name`.`THREAD_ID` AS "          \
    "`thread_id`,`performance_schema`.`threads`.`PROCESSLIST_ID` AS `processlist_id` from "        \
    "(`performance_schema`.`events_waits_summary_by_thread_by_event_name` left join "              \
    "`performance_schema`.`threads` on((`performance_schema`."                                     \
    "`events_waits_summary_by_thread_by_event_name`.`THREAD_ID` = `performance_schema`."           \
    "`threads`.`THREAD_ID`))) where ((`performance_schema`."                                       \
    "`events_waits_summary_by_thread_by_event_name`.`EVENT_NAME` like 'wait/io/file/%') and "      \
    "(`performance_schema`.`events_waits_summary_by_thread_by_event_name`.`SUM_TIMER_WAIT` > "     \
    "0)) group by `performance_schema`.`events_waits_summary_by_thread_by_event_name`."            \
    "`THREAD_ID`,`performance_schema`.`threads`.`PROCESSLIST_ID`,`user` order by sum("             \
    "`performance_schema`.`events_waits_summary_by_thread_by_event_name`.`SUM_TIMER_WAIT`) desc"

#define SYS_IO_BY_THREAD_BY_LATENCY_VIEW_DEFINITION                                                \
    SYS_IO_BY_THREAD_BY_LATENCY_SELECT_PREFIX                                                      \
    "format_pico_time(sum(`performance_schema`.`events_waits_summary_by_thread_by_event_name`."    \
    "`SUM_TIMER_WAIT`)) AS `total_latency`,format_pico_time(min(`performance_schema`."             \
    "`events_waits_summary_by_thread_by_event_name`.`MIN_TIMER_WAIT`)) AS "                        \
    "`min_latency`,format_pico_time(avg(`performance_schema`."                                     \
    "`events_waits_summary_by_thread_by_event_name`.`AVG_TIMER_WAIT`)) AS "                        \
    "`avg_latency`,format_pico_time(max(`performance_schema`."                                     \
    "`events_waits_summary_by_thread_by_event_name`.`MAX_TIMER_WAIT`)) AS "                        \
    "`max_latency`," SYS_IO_BY_THREAD_BY_LATENCY_SELECT_SUFFIX

#define SYS_X_IO_BY_THREAD_BY_LATENCY_VIEW_DEFINITION                                              \
    SYS_IO_BY_THREAD_BY_LATENCY_SELECT_PREFIX                                                      \
    "sum(`performance_schema`.`events_waits_summary_by_thread_by_event_name`.`SUM_TIMER_WAIT`) "   \
    "AS `total_latency`,min(`performance_schema`."                                                 \
    "`events_waits_summary_by_thread_by_event_name`.`MIN_TIMER_WAIT`) AS "                         \
    "`min_latency`,avg(`performance_schema`."                                                      \
    "`events_waits_summary_by_thread_by_event_name`.`AVG_TIMER_WAIT`) AS "                         \
    "`avg_latency`,max(`performance_schema`."                                                      \
    "`events_waits_summary_by_thread_by_event_name`.`MAX_TIMER_WAIT`) AS "                         \
    "`max_latency`," SYS_IO_BY_THREAD_BY_LATENCY_SELECT_SUFFIX

static const char sys_io_by_thread_by_latency_view_definition[] =
    SYS_IO_BY_THREAD_BY_LATENCY_VIEW_DEFINITION;

static const char sys_io_by_thread_by_latency_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`io_by_thread_by_latency` " SYS_IO_BY_THREAD_BY_LATENCY_VIEW_COLUMNS
    " AS " SYS_IO_BY_THREAD_BY_LATENCY_VIEW_DEFINITION;

static const char sys_io_by_thread_by_latency_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`io_by_thread_by_latency` " SYS_IO_BY_THREAD_BY_LATENCY_VIEW_COLUMNS
    " AS " SYS_IO_BY_THREAD_BY_LATENCY_VIEW_DEFINITION;

static const char sys_x_io_by_thread_by_latency_view_definition[] =
    SYS_X_IO_BY_THREAD_BY_LATENCY_VIEW_DEFINITION;

static const char sys_x_io_by_thread_by_latency_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$io_by_thread_by_latency` " SYS_IO_BY_THREAD_BY_LATENCY_VIEW_COLUMNS
    " AS " SYS_X_IO_BY_THREAD_BY_LATENCY_VIEW_DEFINITION;

static const char sys_x_io_by_thread_by_latency_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$io_by_thread_by_latency` " SYS_IO_BY_THREAD_BY_LATENCY_VIEW_COLUMNS
    " AS " SYS_X_IO_BY_THREAD_BY_LATENCY_VIEW_DEFINITION;

#undef SYS_IO_BY_THREAD_BY_LATENCY_VIEW_COLUMNS
#undef SYS_IO_BY_THREAD_BY_LATENCY_SELECT_PREFIX
#undef SYS_IO_BY_THREAD_BY_LATENCY_SELECT_SUFFIX
#undef SYS_IO_BY_THREAD_BY_LATENCY_VIEW_DEFINITION
#undef SYS_X_IO_BY_THREAD_BY_LATENCY_VIEW_DEFINITION

#define SYS_IO_GLOBAL_BY_FILE_BY_BYTES_VIEW_COLUMNS                                                \
    "(`file`,`count_read`,`total_read`,`avg_read`,`count_write`,`total_written`,`avg_write`,"      \
    "`total`,`write_pct`)"

#define SYS_IO_GLOBAL_BY_FILE_BY_BYTES_VIEW_DEFINITION                                             \
    "select `sys`.`format_path`(`performance_schema`.`file_summary_by_instance`.`FILE_NAME`) AS "  \
    "`file`,`performance_schema`.`file_summary_by_instance`.`COUNT_READ` AS "                      \
    "`count_read`,format_bytes(`performance_schema`.`file_summary_by_instance`."                   \
    "`SUM_NUMBER_OF_BYTES_READ`) AS `total_read`,format_bytes(ifnull(("                            \
    "`performance_schema`.`file_summary_by_instance`.`SUM_NUMBER_OF_BYTES_READ` / nullif("         \
    "`performance_schema`.`file_summary_by_instance`.`COUNT_READ`,0)),0)) AS "                     \
    "`avg_read`,`performance_schema`.`file_summary_by_instance`.`COUNT_WRITE` AS "                 \
    "`count_write`,format_bytes(`performance_schema`.`file_summary_by_instance`."                  \
    "`SUM_NUMBER_OF_BYTES_WRITE`) AS `total_written`,format_bytes(ifnull(("                        \
    "`performance_schema`.`file_summary_by_instance`.`SUM_NUMBER_OF_BYTES_WRITE` / nullif("        \
    "`performance_schema`.`file_summary_by_instance`.`COUNT_WRITE`,0)),0.00)) AS "                 \
    "`avg_write`,format_bytes((`performance_schema`.`file_summary_by_instance`."                   \
    "`SUM_NUMBER_OF_BYTES_READ` + `performance_schema`.`file_summary_by_instance`."                \
    "`SUM_NUMBER_OF_BYTES_WRITE`)) AS `total`,ifnull(round((100 - (("                              \
    "`performance_schema`.`file_summary_by_instance`.`SUM_NUMBER_OF_BYTES_READ` / nullif(("        \
    "`performance_schema`.`file_summary_by_instance`.`SUM_NUMBER_OF_BYTES_READ` + "                \
    "`performance_schema`.`file_summary_by_instance`.`SUM_NUMBER_OF_BYTES_WRITE`),0)) * "          \
    "100)),2),0.00) AS `write_pct` from `performance_schema`.`file_summary_by_instance` "          \
    "order by (`performance_schema`.`file_summary_by_instance`.`SUM_NUMBER_OF_BYTES_READ` + "      \
    "`performance_schema`.`file_summary_by_instance`.`SUM_NUMBER_OF_BYTES_WRITE`) desc"

#define SYS_X_IO_GLOBAL_BY_FILE_BY_BYTES_VIEW_DEFINITION                                           \
    "select `performance_schema`.`file_summary_by_instance`.`FILE_NAME` AS "                       \
    "`file`,`performance_schema`.`file_summary_by_instance`.`COUNT_READ` AS "                      \
    "`count_read`,`performance_schema`.`file_summary_by_instance`.`SUM_NUMBER_OF_BYTES_READ` AS "  \
    "`total_read`,ifnull((`performance_schema`.`file_summary_by_instance`."                        \
    "`SUM_NUMBER_OF_BYTES_READ` / nullif(`performance_schema`.`file_summary_by_instance`."         \
    "`COUNT_READ`,0)),0) AS `avg_read`,`performance_schema`.`file_summary_by_instance`."           \
    "`COUNT_WRITE` AS `count_write`,`performance_schema`.`file_summary_by_instance`."              \
    "`SUM_NUMBER_OF_BYTES_WRITE` AS `total_written`,ifnull(("                                      \
    "`performance_schema`.`file_summary_by_instance`.`SUM_NUMBER_OF_BYTES_WRITE` / nullif("        \
    "`performance_schema`.`file_summary_by_instance`.`COUNT_WRITE`,0)),0.00) AS "                  \
    "`avg_write`,(`performance_schema`.`file_summary_by_instance`.`SUM_NUMBER_OF_BYTES_READ` + "   \
    "`performance_schema`.`file_summary_by_instance`.`SUM_NUMBER_OF_BYTES_WRITE`) AS "             \
    "`total`,ifnull(round((100 - ((`performance_schema`.`file_summary_by_instance`."               \
    "`SUM_NUMBER_OF_BYTES_READ` / nullif((`performance_schema`.`file_summary_by_instance`."        \
    "`SUM_NUMBER_OF_BYTES_READ` + `performance_schema`.`file_summary_by_instance`."                \
    "`SUM_NUMBER_OF_BYTES_WRITE`),0)) * 100)),2),0.00) AS `write_pct` from "                       \
    "`performance_schema`.`file_summary_by_instance` order by ("                                   \
    "`performance_schema`.`file_summary_by_instance`.`SUM_NUMBER_OF_BYTES_READ` + "                \
    "`performance_schema`.`file_summary_by_instance`.`SUM_NUMBER_OF_BYTES_WRITE`) desc"

static const char sys_io_global_by_file_by_bytes_view_definition[] =
    SYS_IO_GLOBAL_BY_FILE_BY_BYTES_VIEW_DEFINITION;

static const char sys_io_global_by_file_by_bytes_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`io_global_by_file_by_bytes` " SYS_IO_GLOBAL_BY_FILE_BY_BYTES_VIEW_COLUMNS
    " AS " SYS_IO_GLOBAL_BY_FILE_BY_BYTES_VIEW_DEFINITION;

static const char sys_io_global_by_file_by_bytes_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`io_global_by_file_by_bytes` " SYS_IO_GLOBAL_BY_FILE_BY_BYTES_VIEW_COLUMNS
    " AS " SYS_IO_GLOBAL_BY_FILE_BY_BYTES_VIEW_DEFINITION;

static const char sys_x_io_global_by_file_by_bytes_view_definition[] =
    SYS_X_IO_GLOBAL_BY_FILE_BY_BYTES_VIEW_DEFINITION;

static const char sys_x_io_global_by_file_by_bytes_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$io_global_by_file_by_bytes` " SYS_IO_GLOBAL_BY_FILE_BY_BYTES_VIEW_COLUMNS
    " AS " SYS_X_IO_GLOBAL_BY_FILE_BY_BYTES_VIEW_DEFINITION;

static const char sys_x_io_global_by_file_by_bytes_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$io_global_by_file_by_bytes` " SYS_IO_GLOBAL_BY_FILE_BY_BYTES_VIEW_COLUMNS
    " AS " SYS_X_IO_GLOBAL_BY_FILE_BY_BYTES_VIEW_DEFINITION;

#undef SYS_IO_GLOBAL_BY_FILE_BY_BYTES_VIEW_COLUMNS
#undef SYS_IO_GLOBAL_BY_FILE_BY_BYTES_VIEW_DEFINITION
#undef SYS_X_IO_GLOBAL_BY_FILE_BY_BYTES_VIEW_DEFINITION

#define SYS_IO_GLOBAL_BY_FILE_BY_LATENCY_VIEW_COLUMNS                                              \
    "(`file`,`total`,`total_latency`,`count_read`,`read_latency`,`count_write`,`write_latency`,"   \
    "`count_misc`,`misc_latency`)"

#define SYS_IO_GLOBAL_BY_FILE_BY_LATENCY_VIEW_DEFINITION                                           \
    "select `sys`.`format_path`(`performance_schema`.`file_summary_by_instance`.`FILE_NAME`) AS "  \
    "`file`,`performance_schema`.`file_summary_by_instance`.`COUNT_STAR` AS "                      \
    "`total`,format_pico_time(`performance_schema`.`file_summary_by_instance`.`SUM_TIMER_WAIT`) "  \
    "AS `total_latency`,`performance_schema`.`file_summary_by_instance`.`COUNT_READ` AS "          \
    "`count_read`,format_pico_time(`performance_schema`.`file_summary_by_instance`."               \
    "`SUM_TIMER_READ`) AS `read_latency`,`performance_schema`.`file_summary_by_instance`."         \
    "`COUNT_WRITE` AS `count_write`,format_pico_time(`performance_schema`."                        \
    "`file_summary_by_instance`.`SUM_TIMER_WRITE`) AS `write_latency`,`performance_schema`."       \
    "`file_summary_by_instance`.`COUNT_MISC` AS `count_misc`,format_pico_time("                    \
    "`performance_schema`.`file_summary_by_instance`.`SUM_TIMER_MISC`) AS "                        \
    "`misc_latency` from `performance_schema`.`file_summary_by_instance` order by "                \
    "`performance_schema`.`file_summary_by_instance`.`SUM_TIMER_WAIT` desc"

#define SYS_X_IO_GLOBAL_BY_FILE_BY_LATENCY_VIEW_DEFINITION                                         \
    "select `performance_schema`.`file_summary_by_instance`.`FILE_NAME` AS "                       \
    "`file`,`performance_schema`.`file_summary_by_instance`.`COUNT_STAR` AS "                      \
    "`total`,`performance_schema`.`file_summary_by_instance`.`SUM_TIMER_WAIT` AS "                 \
    "`total_latency`,`performance_schema`.`file_summary_by_instance`.`COUNT_READ` AS "             \
    "`count_read`,`performance_schema`.`file_summary_by_instance`.`SUM_TIMER_READ` AS "            \
    "`read_latency`,`performance_schema`.`file_summary_by_instance`.`COUNT_WRITE` AS "             \
    "`count_write`,`performance_schema`.`file_summary_by_instance`.`SUM_TIMER_WRITE` AS "          \
    "`write_latency`,`performance_schema`.`file_summary_by_instance`.`COUNT_MISC` AS "             \
    "`count_misc`,`performance_schema`.`file_summary_by_instance`.`SUM_TIMER_MISC` AS "            \
    "`misc_latency` from `performance_schema`.`file_summary_by_instance` order by "                \
    "`performance_schema`.`file_summary_by_instance`.`SUM_TIMER_WAIT` desc"

static const char sys_io_global_by_file_by_latency_view_definition[] =
    SYS_IO_GLOBAL_BY_FILE_BY_LATENCY_VIEW_DEFINITION;

static const char sys_io_global_by_file_by_latency_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`io_global_by_file_by_latency` " SYS_IO_GLOBAL_BY_FILE_BY_LATENCY_VIEW_COLUMNS
    " AS " SYS_IO_GLOBAL_BY_FILE_BY_LATENCY_VIEW_DEFINITION;

static const char sys_io_global_by_file_by_latency_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`io_global_by_file_by_latency` " SYS_IO_GLOBAL_BY_FILE_BY_LATENCY_VIEW_COLUMNS
    " AS " SYS_IO_GLOBAL_BY_FILE_BY_LATENCY_VIEW_DEFINITION;

static const char sys_x_io_global_by_file_by_latency_view_definition[] =
    SYS_X_IO_GLOBAL_BY_FILE_BY_LATENCY_VIEW_DEFINITION;

static const char sys_x_io_global_by_file_by_latency_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$io_global_by_file_by_latency` " SYS_IO_GLOBAL_BY_FILE_BY_LATENCY_VIEW_COLUMNS
    " AS " SYS_X_IO_GLOBAL_BY_FILE_BY_LATENCY_VIEW_DEFINITION;

static const char sys_x_io_global_by_file_by_latency_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$io_global_by_file_by_latency` " SYS_IO_GLOBAL_BY_FILE_BY_LATENCY_VIEW_COLUMNS
    " AS " SYS_X_IO_GLOBAL_BY_FILE_BY_LATENCY_VIEW_DEFINITION;

#undef SYS_IO_GLOBAL_BY_FILE_BY_LATENCY_VIEW_COLUMNS
#undef SYS_IO_GLOBAL_BY_FILE_BY_LATENCY_VIEW_DEFINITION
#undef SYS_X_IO_GLOBAL_BY_FILE_BY_LATENCY_VIEW_DEFINITION

#define SYS_IO_GLOBAL_BY_WAIT_BY_BYTES_VIEW_COLUMNS                                                \
    "(`event_name`,`total`,`total_latency`,`min_latency`,`avg_latency`,`max_latency`,"             \
    "`count_read`,`total_read`,`avg_read`,`count_write`,`total_written`,`avg_written`,"            \
    "`total_requested`)"

#define SYS_IO_GLOBAL_BY_WAIT_BY_BYTES_VIEW_DEFINITION                                             \
    "select substring_index(`performance_schema`.`file_summary_by_event_name`.`EVENT_NAME`,"       \
    "'/',-(2)) AS `event_name`,`performance_schema`.`file_summary_by_event_name`.`COUNT_STAR` AS " \
    "`total`,format_pico_time(`performance_schema`.`file_summary_by_event_name`."                  \
    "`SUM_TIMER_WAIT`) AS `total_latency`,format_pico_time(`performance_schema`."                  \
    "`file_summary_by_event_name`.`MIN_TIMER_WAIT`) AS `min_latency`,format_pico_time("            \
    "`performance_schema`.`file_summary_by_event_name`.`AVG_TIMER_WAIT`) AS "                      \
    "`avg_latency`,format_pico_time(`performance_schema`.`file_summary_by_event_name`."            \
    "`MAX_TIMER_WAIT`) AS `max_latency`,`performance_schema`.`file_summary_by_event_name`."        \
    "`COUNT_READ` AS `count_read`,format_bytes(`performance_schema`.`file_summary_by_event_name`." \
    "`SUM_NUMBER_OF_BYTES_READ`) AS `total_read`,format_bytes(ifnull(("                            \
    "`performance_schema`.`file_summary_by_event_name`.`SUM_NUMBER_OF_BYTES_READ` / nullif("       \
    "`performance_schema`.`file_summary_by_event_name`.`COUNT_READ`,0)),0)) AS "                   \
    "`avg_read`,`performance_schema`.`file_summary_by_event_name`.`COUNT_WRITE` AS "               \
    "`count_write`,format_bytes(`performance_schema`.`file_summary_by_event_name`."                \
    "`SUM_NUMBER_OF_BYTES_WRITE`) AS `total_written`,format_bytes(ifnull(("                        \
    "`performance_schema`.`file_summary_by_event_name`.`SUM_NUMBER_OF_BYTES_WRITE` / nullif("      \
    "`performance_schema`.`file_summary_by_event_name`.`COUNT_WRITE`,0)),0)) AS "                  \
    "`avg_written`,format_bytes((`performance_schema`.`file_summary_by_event_name`."               \
    "`SUM_NUMBER_OF_BYTES_WRITE` + `performance_schema`.`file_summary_by_event_name`."             \
    "`SUM_NUMBER_OF_BYTES_READ`)) AS `total_requested` from "                                      \
    "`performance_schema`.`file_summary_by_event_name` where (("                                   \
    "`performance_schema`.`file_summary_by_event_name`.`EVENT_NAME` like 'wait/io/file/%') and "   \
    "(`performance_schema`.`file_summary_by_event_name`.`COUNT_STAR` > 0)) order by ("             \
    "`performance_schema`.`file_summary_by_event_name`.`SUM_NUMBER_OF_BYTES_WRITE` + "             \
    "`performance_schema`.`file_summary_by_event_name`.`SUM_NUMBER_OF_BYTES_READ`) desc"

#define SYS_X_IO_GLOBAL_BY_WAIT_BY_BYTES_VIEW_DEFINITION                                           \
    "select substring_index(`performance_schema`.`file_summary_by_event_name`.`EVENT_NAME`,"       \
    "'/',-(2)) AS `event_name`,`performance_schema`.`file_summary_by_event_name`.`COUNT_STAR` AS " \
    "`total`,`performance_schema`.`file_summary_by_event_name`.`SUM_TIMER_WAIT` AS "               \
    "`total_latency`,`performance_schema`.`file_summary_by_event_name`.`MIN_TIMER_WAIT` AS "       \
    "`min_latency`,`performance_schema`.`file_summary_by_event_name`.`AVG_TIMER_WAIT` AS "         \
    "`avg_latency`,`performance_schema`.`file_summary_by_event_name`.`MAX_TIMER_WAIT` AS "         \
    "`max_latency`,`performance_schema`.`file_summary_by_event_name`.`COUNT_READ` AS "             \
    "`count_read`,`performance_schema`.`file_summary_by_event_name`.`SUM_NUMBER_OF_BYTES_READ` "   \
    "AS `total_read`,ifnull((`performance_schema`.`file_summary_by_event_name`."                   \
    "`SUM_NUMBER_OF_BYTES_READ` / nullif(`performance_schema`.`file_summary_by_event_name`."       \
    "`COUNT_READ`,0)),0) AS `avg_read`,`performance_schema`.`file_summary_by_event_name`."         \
    "`COUNT_WRITE` AS `count_write`,`performance_schema`.`file_summary_by_event_name`."            \
    "`SUM_NUMBER_OF_BYTES_WRITE` AS `total_written`,ifnull((`performance_schema`."                 \
    "`file_summary_by_event_name`.`SUM_NUMBER_OF_BYTES_WRITE` / nullif(`performance_schema`."      \
    "`file_summary_by_event_name`.`COUNT_WRITE`,0)),0) AS `avg_written`,("                         \
    "`performance_schema`.`file_summary_by_event_name`.`SUM_NUMBER_OF_BYTES_WRITE` + "             \
    "`performance_schema`.`file_summary_by_event_name`.`SUM_NUMBER_OF_BYTES_READ`) AS "            \
    "`total_requested` from `performance_schema`.`file_summary_by_event_name` where (("            \
    "`performance_schema`.`file_summary_by_event_name`.`EVENT_NAME` like 'wait/io/file/%') and "   \
    "(`performance_schema`.`file_summary_by_event_name`.`COUNT_STAR` > 0)) order by ("             \
    "`performance_schema`.`file_summary_by_event_name`.`SUM_NUMBER_OF_BYTES_WRITE` + "             \
    "`performance_schema`.`file_summary_by_event_name`.`SUM_NUMBER_OF_BYTES_READ`) desc"

static const char sys_io_global_by_wait_by_bytes_view_definition[] =
    SYS_IO_GLOBAL_BY_WAIT_BY_BYTES_VIEW_DEFINITION;

static const char sys_io_global_by_wait_by_bytes_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`io_global_by_wait_by_bytes` " SYS_IO_GLOBAL_BY_WAIT_BY_BYTES_VIEW_COLUMNS
    " AS " SYS_IO_GLOBAL_BY_WAIT_BY_BYTES_VIEW_DEFINITION;

static const char sys_io_global_by_wait_by_bytes_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`io_global_by_wait_by_bytes` " SYS_IO_GLOBAL_BY_WAIT_BY_BYTES_VIEW_COLUMNS
    " AS " SYS_IO_GLOBAL_BY_WAIT_BY_BYTES_VIEW_DEFINITION;

static const char sys_x_io_global_by_wait_by_bytes_view_definition[] =
    SYS_X_IO_GLOBAL_BY_WAIT_BY_BYTES_VIEW_DEFINITION;

static const char sys_x_io_global_by_wait_by_bytes_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$io_global_by_wait_by_bytes` " SYS_IO_GLOBAL_BY_WAIT_BY_BYTES_VIEW_COLUMNS
    " AS " SYS_X_IO_GLOBAL_BY_WAIT_BY_BYTES_VIEW_DEFINITION;

static const char sys_x_io_global_by_wait_by_bytes_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$io_global_by_wait_by_bytes` " SYS_IO_GLOBAL_BY_WAIT_BY_BYTES_VIEW_COLUMNS
    " AS " SYS_X_IO_GLOBAL_BY_WAIT_BY_BYTES_VIEW_DEFINITION;

#undef SYS_IO_GLOBAL_BY_WAIT_BY_BYTES_VIEW_COLUMNS
#undef SYS_IO_GLOBAL_BY_WAIT_BY_BYTES_VIEW_DEFINITION
#undef SYS_X_IO_GLOBAL_BY_WAIT_BY_BYTES_VIEW_DEFINITION

#define SYS_IO_GLOBAL_BY_WAIT_BY_LATENCY_VIEW_COLUMNS                                              \
    "(`event_name`,`total`,`total_latency`,`avg_latency`,`max_latency`,`read_latency`,"            \
    "`write_latency`,`misc_latency`,`count_read`,`total_read`,`avg_read`,`count_write`,"           \
    "`total_written`,`avg_written`)"

#define SYS_IO_GLOBAL_BY_WAIT_BY_LATENCY_VIEW_DEFINITION                                           \
    "select substring_index(`performance_schema`.`file_summary_by_event_name`.`EVENT_NAME`,"       \
    "'/',-(2)) AS `event_name`,`performance_schema`.`file_summary_by_event_name`.`COUNT_STAR` AS " \
    "`total`,format_pico_time(`performance_schema`.`file_summary_by_event_name`."                  \
    "`SUM_TIMER_WAIT`) AS `total_latency`,format_pico_time(`performance_schema`."                  \
    "`file_summary_by_event_name`.`AVG_TIMER_WAIT`) AS `avg_latency`,format_pico_time("            \
    "`performance_schema`.`file_summary_by_event_name`.`MAX_TIMER_WAIT`) AS "                      \
    "`max_latency`,format_pico_time(`performance_schema`.`file_summary_by_event_name`."            \
    "`SUM_TIMER_READ`) AS `read_latency`,format_pico_time(`performance_schema`."                   \
    "`file_summary_by_event_name`.`SUM_TIMER_WRITE`) AS `write_latency`,format_pico_time("         \
    "`performance_schema`.`file_summary_by_event_name`.`SUM_TIMER_MISC`) AS "                      \
    "`misc_latency`,`performance_schema`.`file_summary_by_event_name`.`COUNT_READ` AS "            \
    "`count_read`,format_bytes(`performance_schema`.`file_summary_by_event_name`."                 \
    "`SUM_NUMBER_OF_BYTES_READ`) AS `total_read`,format_bytes(ifnull(("                            \
    "`performance_schema`.`file_summary_by_event_name`.`SUM_NUMBER_OF_BYTES_READ` / nullif("       \
    "`performance_schema`.`file_summary_by_event_name`.`COUNT_READ`,0)),0)) AS "                   \
    "`avg_read`,`performance_schema`.`file_summary_by_event_name`.`COUNT_WRITE` AS "               \
    "`count_write`,format_bytes(`performance_schema`.`file_summary_by_event_name`."                \
    "`SUM_NUMBER_OF_BYTES_WRITE`) AS `total_written`,format_bytes(ifnull(("                        \
    "`performance_schema`.`file_summary_by_event_name`.`SUM_NUMBER_OF_BYTES_WRITE` / nullif("      \
    "`performance_schema`.`file_summary_by_event_name`.`COUNT_WRITE`,0)),0)) AS "                  \
    "`avg_written` from `performance_schema`.`file_summary_by_event_name` where (("                \
    "`performance_schema`.`file_summary_by_event_name`.`EVENT_NAME` like 'wait/io/file/%') and "   \
    "(`performance_schema`.`file_summary_by_event_name`.`COUNT_STAR` > 0)) order by "              \
    "`performance_schema`.`file_summary_by_event_name`.`SUM_TIMER_WAIT` desc"

#define SYS_X_IO_GLOBAL_BY_WAIT_BY_LATENCY_VIEW_DEFINITION                                         \
    "select substring_index(`performance_schema`.`file_summary_by_event_name`.`EVENT_NAME`,"       \
    "'/',-(2)) AS `event_name`,`performance_schema`.`file_summary_by_event_name`.`COUNT_STAR` AS " \
    "`total`,`performance_schema`.`file_summary_by_event_name`.`SUM_TIMER_WAIT` AS "               \
    "`total_latency`,`performance_schema`.`file_summary_by_event_name`.`AVG_TIMER_WAIT` AS "       \
    "`avg_latency`,`performance_schema`.`file_summary_by_event_name`.`MAX_TIMER_WAIT` AS "         \
    "`max_latency`,`performance_schema`.`file_summary_by_event_name`.`SUM_TIMER_READ` AS "         \
    "`read_latency`,`performance_schema`.`file_summary_by_event_name`.`SUM_TIMER_WRITE` AS "       \
    "`write_latency`,`performance_schema`.`file_summary_by_event_name`.`SUM_TIMER_MISC` AS "       \
    "`misc_latency`,`performance_schema`.`file_summary_by_event_name`.`COUNT_READ` AS "            \
    "`count_read`,`performance_schema`.`file_summary_by_event_name`.`SUM_NUMBER_OF_BYTES_READ` "   \
    "AS `total_read`,ifnull((`performance_schema`.`file_summary_by_event_name`."                   \
    "`SUM_NUMBER_OF_BYTES_READ` / nullif(`performance_schema`.`file_summary_by_event_name`."       \
    "`COUNT_READ`,0)),0) AS `avg_read`,`performance_schema`.`file_summary_by_event_name`."         \
    "`COUNT_WRITE` AS `count_write`,`performance_schema`.`file_summary_by_event_name`."            \
    "`SUM_NUMBER_OF_BYTES_WRITE` AS `total_written`,ifnull((`performance_schema`."                 \
    "`file_summary_by_event_name`.`SUM_NUMBER_OF_BYTES_WRITE` / nullif(`performance_schema`."      \
    "`file_summary_by_event_name`.`COUNT_WRITE`,0)),0) AS `avg_written` from "                     \
    "`performance_schema`.`file_summary_by_event_name` where (("                                   \
    "`performance_schema`.`file_summary_by_event_name`.`EVENT_NAME` like 'wait/io/file/%') and "   \
    "(`performance_schema`.`file_summary_by_event_name`.`COUNT_STAR` > 0)) order by "              \
    "`performance_schema`.`file_summary_by_event_name`.`SUM_TIMER_WAIT` desc"

static const char sys_io_global_by_wait_by_latency_view_definition[] =
    SYS_IO_GLOBAL_BY_WAIT_BY_LATENCY_VIEW_DEFINITION;

static const char sys_io_global_by_wait_by_latency_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`io_global_by_wait_by_latency` " SYS_IO_GLOBAL_BY_WAIT_BY_LATENCY_VIEW_COLUMNS
    " AS " SYS_IO_GLOBAL_BY_WAIT_BY_LATENCY_VIEW_DEFINITION;

static const char sys_io_global_by_wait_by_latency_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`io_global_by_wait_by_latency` " SYS_IO_GLOBAL_BY_WAIT_BY_LATENCY_VIEW_COLUMNS
    " AS " SYS_IO_GLOBAL_BY_WAIT_BY_LATENCY_VIEW_DEFINITION;

static const char sys_x_io_global_by_wait_by_latency_view_definition[] =
    SYS_X_IO_GLOBAL_BY_WAIT_BY_LATENCY_VIEW_DEFINITION;

static const char sys_x_io_global_by_wait_by_latency_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$io_global_by_wait_by_latency` " SYS_IO_GLOBAL_BY_WAIT_BY_LATENCY_VIEW_COLUMNS
    " AS " SYS_X_IO_GLOBAL_BY_WAIT_BY_LATENCY_VIEW_DEFINITION;

static const char sys_x_io_global_by_wait_by_latency_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$io_global_by_wait_by_latency` " SYS_IO_GLOBAL_BY_WAIT_BY_LATENCY_VIEW_COLUMNS
    " AS " SYS_X_IO_GLOBAL_BY_WAIT_BY_LATENCY_VIEW_DEFINITION;

#undef SYS_IO_GLOBAL_BY_WAIT_BY_LATENCY_VIEW_COLUMNS
#undef SYS_IO_GLOBAL_BY_WAIT_BY_LATENCY_VIEW_DEFINITION
#undef SYS_X_IO_GLOBAL_BY_WAIT_BY_LATENCY_VIEW_DEFINITION

#define SYS_LATEST_FILE_IO_VIEW_COLUMNS "(`thread`,`file`,`latency`,`operation`,`requested`)"

#define SYS_LATEST_FILE_IO_SELECT_PREFIX                                                           \
    "select if((`processlist`.`ID` is null),concat(substring_index("                               \
    "`performance_schema`.`threads`.`NAME`,'/',-(1)),':',"                                         \
    "`performance_schema`.`events_waits_history_long`.`THREAD_ID`),convert(concat("                \
    "`processlist`.`USER`,'@',`processlist`.`HOST`,':',`processlist`.`ID`) using utf8mb4)) AS "    \
    "`thread`,"

#define SYS_LATEST_FILE_IO_SELECT_SUFFIX                                                           \
    " AS `operation`,format_bytes(`performance_schema`.`events_waits_history_long`."               \
    "`NUMBER_OF_BYTES`) AS `requested` from ((`performance_schema`.`events_waits_history_long` "   \
    "join "                                                                                        \
    "`performance_schema`.`threads` on((`performance_schema`.`events_waits_history_long`."         \
    "`THREAD_ID` = `performance_schema`.`threads`.`THREAD_ID`))) left join "                       \
    "`information_schema`.`PROCESSLIST` `processlist` on((`performance_schema`.`threads`."         \
    "`PROCESSLIST_ID` = `processlist`.`ID`))) where (("                                            \
    "`performance_schema`.`events_waits_history_long`.`OBJECT_NAME` is not null) and ("            \
    "`performance_schema`.`events_waits_history_long`.`EVENT_NAME` like 'wait/io/file/%')) order " \
    "by "                                                                                          \
    "`performance_schema`.`events_waits_history_long`.`TIMER_START`"

#define SYS_X_LATEST_FILE_IO_SELECT_SUFFIX                                                         \
    " AS `operation`,`performance_schema`.`events_waits_history_long`.`NUMBER_OF_BYTES` AS "       \
    "`requested` from ((`performance_schema`.`events_waits_history_long` join "                    \
    "`performance_schema`.`threads` on((`performance_schema`.`events_waits_history_long`."         \
    "`THREAD_ID` = `performance_schema`.`threads`.`THREAD_ID`))) left join "                       \
    "`information_schema`.`PROCESSLIST` `processlist` on((`performance_schema`.`threads`."         \
    "`PROCESSLIST_ID` = `processlist`.`ID`))) where (("                                            \
    "`performance_schema`.`events_waits_history_long`.`OBJECT_NAME` is not null) and ("            \
    "`performance_schema`.`events_waits_history_long`.`EVENT_NAME` like 'wait/io/file/%')) order " \
    "by "                                                                                          \
    "`performance_schema`.`events_waits_history_long`.`TIMER_START`"

#define SYS_LATEST_FILE_IO_VIEW_DEFINITION                                                         \
    SYS_LATEST_FILE_IO_SELECT_PREFIX                                                               \
    "`sys`.`format_path`(`performance_schema`.`events_waits_history_long`.`OBJECT_NAME`) AS "      \
    "`file`,format_pico_time(`performance_schema`.`events_waits_history_long`.`TIMER_WAIT`) AS "   \
    "`latency`,`performance_schema`.`events_waits_history_long`.`"                                 \
    "OPERATION`" SYS_LATEST_FILE_IO_SELECT_SUFFIX

#define SYS_X_LATEST_FILE_IO_VIEW_DEFINITION                                                       \
    SYS_LATEST_FILE_IO_SELECT_PREFIX                                                               \
    "`performance_schema`.`events_waits_history_long`.`OBJECT_NAME` AS `file`,"                    \
    "`performance_schema`.`events_waits_history_long`.`TIMER_WAIT` AS `latency`,"                  \
    "`performance_schema`.`events_waits_history_long`.`"                                           \
    "OPERATION`" SYS_X_LATEST_FILE_IO_SELECT_SUFFIX

static const char sys_latest_file_io_view_definition[] = SYS_LATEST_FILE_IO_VIEW_DEFINITION;

static const char sys_latest_file_io_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`latest_file_io` " SYS_LATEST_FILE_IO_VIEW_COLUMNS " AS " SYS_LATEST_FILE_IO_VIEW_DEFINITION;

static const char sys_latest_file_io_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`latest_file_io` " SYS_LATEST_FILE_IO_VIEW_COLUMNS
    " AS " SYS_LATEST_FILE_IO_VIEW_DEFINITION;

static const char sys_x_latest_file_io_view_definition[] = SYS_X_LATEST_FILE_IO_VIEW_DEFINITION;

static const char sys_x_latest_file_io_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$latest_file_io` " SYS_LATEST_FILE_IO_VIEW_COLUMNS
    " AS " SYS_X_LATEST_FILE_IO_VIEW_DEFINITION;

static const char sys_x_latest_file_io_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$latest_file_io` " SYS_LATEST_FILE_IO_VIEW_COLUMNS
    " AS " SYS_X_LATEST_FILE_IO_VIEW_DEFINITION;

#undef SYS_LATEST_FILE_IO_VIEW_COLUMNS
#undef SYS_LATEST_FILE_IO_SELECT_PREFIX
#undef SYS_LATEST_FILE_IO_SELECT_SUFFIX
#undef SYS_X_LATEST_FILE_IO_SELECT_SUFFIX
#undef SYS_LATEST_FILE_IO_VIEW_DEFINITION
#undef SYS_X_LATEST_FILE_IO_VIEW_DEFINITION

#define SYS_PS_CHECK_LOST_INSTRUMENTATION_VIEW_COLUMNS "(`variable_name`,`variable_value`)"

#define SYS_PS_CHECK_LOST_INSTRUMENTATION_VIEW_DEFINITION                                          \
    "select `performance_schema`.`global_status`.`VARIABLE_NAME` AS `variable_name`,"              \
    "`performance_schema`.`global_status`.`VARIABLE_VALUE` AS `variable_value` from "              \
    "`performance_schema`.`global_status` where (("                                                \
    "`performance_schema`.`global_status`.`VARIABLE_NAME` like 'perf%lost') and "                  \
    "(`performance_schema`.`global_status`.`VARIABLE_VALUE` > 0))"

static const char sys_ps_check_lost_instrumentation_view_definition[] =
    SYS_PS_CHECK_LOST_INSTRUMENTATION_VIEW_DEFINITION;

static const char sys_ps_check_lost_instrumentation_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`ps_check_lost_instrumentation` " SYS_PS_CHECK_LOST_INSTRUMENTATION_VIEW_COLUMNS
    " AS " SYS_PS_CHECK_LOST_INSTRUMENTATION_VIEW_DEFINITION;

static const char sys_ps_check_lost_instrumentation_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`ps_check_lost_instrumentation` " SYS_PS_CHECK_LOST_INSTRUMENTATION_VIEW_COLUMNS
    " AS " SYS_PS_CHECK_LOST_INSTRUMENTATION_VIEW_DEFINITION;

#undef SYS_PS_CHECK_LOST_INSTRUMENTATION_VIEW_COLUMNS
#undef SYS_PS_CHECK_LOST_INSTRUMENTATION_VIEW_DEFINITION

#define SYS_SCHEMA_AUTO_INCREMENT_COLUMNS_VIEW_COLUMNS                                             \
    "(`table_schema`,`table_name`,`column_name`,`data_type`,`column_type`,`is_signed`,"            \
    "`is_unsigned`,`max_value`,`auto_increment`,`auto_increment_ratio`)"

#define SYS_SCHEMA_AUTO_INCREMENT_COLUMNS_VIEW_DEFINITION                                          \
    "select `information_schema`.`COLUMNS`.`TABLE_SCHEMA` AS `TABLE_SCHEMA`,"                      \
    "`information_schema`.`COLUMNS`.`TABLE_NAME` AS `TABLE_NAME`,"                                 \
    "`information_schema`.`COLUMNS`.`COLUMN_NAME` AS `COLUMN_NAME`,"                               \
    "`information_schema`.`COLUMNS`.`DATA_TYPE` AS `DATA_TYPE`,"                                   \
    "`information_schema`.`COLUMNS`.`COLUMN_TYPE` AS `COLUMN_TYPE`,"                               \
    "(locate('unsigned',`information_schema`.`COLUMNS`.`COLUMN_TYPE`) = 0) AS `is_signed`,"        \
    "(locate('unsigned',`information_schema`.`COLUMNS`.`COLUMN_TYPE`) > 0) AS `is_unsigned`,"      \
    "((case `information_schema`.`COLUMNS`.`DATA_TYPE` when 'tinyint' then 255 when "              \
    "'smallint' then 65535 when 'mediumint' then 16777215 when 'int' then 4294967295 when "        \
    "'bigint' then 18446744073709551615 end) >> if((locate('unsigned',"                            \
    "`information_schema`.`COLUMNS`.`COLUMN_TYPE`) > 0),0,1)) AS `max_value`,"                     \
    "`information_schema`.`TABLES`.`AUTO_INCREMENT` AS `AUTO_INCREMENT`,"                          \
    "(`information_schema`.`TABLES`.`AUTO_INCREMENT` / ((case "                                    \
    "`information_schema`.`COLUMNS`.`DATA_TYPE` when 'tinyint' then 255 when 'smallint' then "     \
    "65535 when 'mediumint' then 16777215 when 'int' then 4294967295 when 'bigint' then "          \
    "18446744073709551615 end) >> if((locate('unsigned',"                                          \
    "`information_schema`.`COLUMNS`.`COLUMN_TYPE`) > 0),0,1))) AS `auto_increment_ratio` "         \
    "from (`information_schema`.`COLUMNS` join `information_schema`.`TABLES` on((("                \
    "`information_schema`.`COLUMNS`.`TABLE_SCHEMA` = "                                             \
    "`information_schema`.`TABLES`.`TABLE_SCHEMA`) and "                                           \
    "(`information_schema`.`COLUMNS`.`TABLE_NAME` = "                                              \
    "`information_schema`.`TABLES`.`TABLE_NAME`)))) where (("                                      \
    "`information_schema`.`COLUMNS`.`TABLE_SCHEMA` not in ('mysql','sys','INFORMATION_SCHEMA',"    \
    "'performance_schema')) and (`information_schema`.`TABLES`.`TABLE_TYPE` = 'BASE TABLE') "      \
    "and (`information_schema`.`COLUMNS`.`EXTRA` = 'auto_increment')) order by "                   \
    "(`information_schema`.`TABLES`.`AUTO_INCREMENT` / ((case "                                    \
    "`information_schema`.`COLUMNS`.`DATA_TYPE` when 'tinyint' then 255 when 'smallint' then "     \
    "65535 when 'mediumint' then 16777215 when 'int' then 4294967295 when 'bigint' then "          \
    "18446744073709551615 end) >> if((locate('unsigned',"                                          \
    "`information_schema`.`COLUMNS`.`COLUMN_TYPE`) > 0),0,1))) desc,((case "                       \
    "`information_schema`.`COLUMNS`.`DATA_TYPE` when 'tinyint' then 255 when 'smallint' then "     \
    "65535 when 'mediumint' then 16777215 when 'int' then 4294967295 when 'bigint' then "          \
    "18446744073709551615 end) >> if((locate('unsigned',"                                          \
    "`information_schema`.`COLUMNS`.`COLUMN_TYPE`) > 0),0,1))"

static const char sys_schema_auto_increment_columns_view_definition[] =
    SYS_SCHEMA_AUTO_INCREMENT_COLUMNS_VIEW_DEFINITION;

static const char sys_schema_auto_increment_columns_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`schema_auto_increment_columns` " SYS_SCHEMA_AUTO_INCREMENT_COLUMNS_VIEW_COLUMNS
    " AS " SYS_SCHEMA_AUTO_INCREMENT_COLUMNS_VIEW_DEFINITION;

static const char sys_schema_auto_increment_columns_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`schema_auto_increment_columns` " SYS_SCHEMA_AUTO_INCREMENT_COLUMNS_VIEW_COLUMNS
    " AS " SYS_SCHEMA_AUTO_INCREMENT_COLUMNS_VIEW_DEFINITION;

#undef SYS_SCHEMA_AUTO_INCREMENT_COLUMNS_VIEW_COLUMNS
#undef SYS_SCHEMA_AUTO_INCREMENT_COLUMNS_VIEW_DEFINITION

#define SYS_SCHEMA_INDEX_STATISTICS_VIEW_COLUMNS                                                   \
    "(`table_schema`,`table_name`,`index_name`,`rows_selected`,`select_latency`,"                  \
    "`rows_inserted`,`insert_latency`,`rows_updated`,`update_latency`,`rows_deleted`,"             \
    "`delete_latency`)"

#define SYS_SCHEMA_INDEX_STATISTICS_VIEW_DEFINITION                                                \
    "select `performance_schema`.`table_io_waits_summary_by_index_usage`.`OBJECT_SCHEMA` AS "      \
    "`table_schema`,`performance_schema`.`table_io_waits_summary_by_index_usage`.`OBJECT_NAME` "   \
    "AS `table_name`,`performance_schema`.`table_io_waits_summary_by_index_usage`.`INDEX_NAME` "   \
    "AS `index_name`,`performance_schema`.`table_io_waits_summary_by_index_usage`.`COUNT_FETCH` "  \
    "AS `rows_selected`,format_pico_time("                                                         \
    "`performance_schema`.`table_io_waits_summary_by_index_usage`.`SUM_TIMER_FETCH`) AS "          \
    "`select_latency`,"                                                                            \
    "`performance_schema`.`table_io_waits_summary_by_index_usage`.`COUNT_INSERT` AS "              \
    "`rows_inserted`,format_pico_time("                                                            \
    "`performance_schema`.`table_io_waits_summary_by_index_usage`.`SUM_TIMER_INSERT`) AS "         \
    "`insert_latency`,"                                                                            \
    "`performance_schema`.`table_io_waits_summary_by_index_usage`.`COUNT_UPDATE` AS "              \
    "`rows_updated`,format_pico_time("                                                             \
    "`performance_schema`.`table_io_waits_summary_by_index_usage`.`SUM_TIMER_UPDATE`) AS "         \
    "`update_latency`,"                                                                            \
    "`performance_schema`.`table_io_waits_summary_by_index_usage`.`COUNT_DELETE` AS "              \
    "`rows_deleted`,format_pico_time("                                                             \
    "`performance_schema`.`table_io_waits_summary_by_index_usage`.`SUM_TIMER_DELETE`) AS "         \
    "`delete_latency` from `performance_schema`.`table_io_waits_summary_by_index_usage` where "    \
    "(`performance_schema`.`table_io_waits_summary_by_index_usage`.`INDEX_NAME` is not null) "     \
    "order by `performance_schema`.`table_io_waits_summary_by_index_usage`.`SUM_TIMER_WAIT` desc"

#define SYS_X_SCHEMA_INDEX_STATISTICS_VIEW_DEFINITION                                              \
    "select `performance_schema`.`table_io_waits_summary_by_index_usage`.`OBJECT_SCHEMA` AS "      \
    "`table_schema`,`performance_schema`.`table_io_waits_summary_by_index_usage`.`OBJECT_NAME` "   \
    "AS `table_name`,`performance_schema`.`table_io_waits_summary_by_index_usage`.`INDEX_NAME` "   \
    "AS `index_name`,`performance_schema`.`table_io_waits_summary_by_index_usage`.`COUNT_FETCH` "  \
    "AS `rows_selected`,"                                                                          \
    "`performance_schema`.`table_io_waits_summary_by_index_usage`.`SUM_TIMER_FETCH` AS "           \
    "`select_latency`,"                                                                            \
    "`performance_schema`.`table_io_waits_summary_by_index_usage`.`COUNT_INSERT` AS "              \
    "`rows_inserted`,"                                                                             \
    "`performance_schema`.`table_io_waits_summary_by_index_usage`.`SUM_TIMER_INSERT` AS "          \
    "`insert_latency`,"                                                                            \
    "`performance_schema`.`table_io_waits_summary_by_index_usage`.`COUNT_UPDATE` AS "              \
    "`rows_updated`,"                                                                              \
    "`performance_schema`.`table_io_waits_summary_by_index_usage`.`SUM_TIMER_UPDATE` AS "          \
    "`update_latency`,"                                                                            \
    "`performance_schema`.`table_io_waits_summary_by_index_usage`.`COUNT_DELETE` AS "              \
    "`rows_deleted`,"                                                                              \
    "`performance_schema`.`table_io_waits_summary_by_index_usage`.`SUM_TIMER_DELETE` AS "          \
    "`delete_latency` from `performance_schema`.`table_io_waits_summary_by_index_usage` where "    \
    "(`performance_schema`.`table_io_waits_summary_by_index_usage`.`INDEX_NAME` is not null) "     \
    "order by `performance_schema`.`table_io_waits_summary_by_index_usage`.`SUM_TIMER_WAIT` desc"

static const char sys_schema_index_statistics_view_definition[] =
    SYS_SCHEMA_INDEX_STATISTICS_VIEW_DEFINITION;

static const char sys_schema_index_statistics_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`schema_index_statistics` " SYS_SCHEMA_INDEX_STATISTICS_VIEW_COLUMNS
    " AS " SYS_SCHEMA_INDEX_STATISTICS_VIEW_DEFINITION;

static const char sys_schema_index_statistics_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`schema_index_statistics` " SYS_SCHEMA_INDEX_STATISTICS_VIEW_COLUMNS
    " AS " SYS_SCHEMA_INDEX_STATISTICS_VIEW_DEFINITION;

static const char sys_x_schema_index_statistics_view_definition[] =
    SYS_X_SCHEMA_INDEX_STATISTICS_VIEW_DEFINITION;

static const char sys_x_schema_index_statistics_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$schema_index_statistics` " SYS_SCHEMA_INDEX_STATISTICS_VIEW_COLUMNS
    " AS " SYS_X_SCHEMA_INDEX_STATISTICS_VIEW_DEFINITION;

static const char sys_x_schema_index_statistics_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$schema_index_statistics` " SYS_SCHEMA_INDEX_STATISTICS_VIEW_COLUMNS
    " AS " SYS_X_SCHEMA_INDEX_STATISTICS_VIEW_DEFINITION;

#undef SYS_SCHEMA_INDEX_STATISTICS_VIEW_COLUMNS
#undef SYS_SCHEMA_INDEX_STATISTICS_VIEW_DEFINITION
#undef SYS_X_SCHEMA_INDEX_STATISTICS_VIEW_DEFINITION

#define SYS_SCHEMA_REDUNDANT_INDEXES_VIEW_COLUMNS                                                  \
    "(`table_schema`,`table_name`,`redundant_index_name`,`redundant_index_columns`,"               \
    "`redundant_index_non_unique`,`dominant_index_name`,`dominant_index_columns`,"                 \
    "`dominant_index_non_unique`,`subpart_exists`,`sql_drop_index`)"

#define SYS_SCHEMA_REDUNDANT_INDEXES_VIEW_DEFINITION                                               \
    "select `sys`.`redundant_keys`.`table_schema` AS `table_schema`,"                              \
    "`sys`.`redundant_keys`.`table_name` AS `table_name`,"                                         \
    "`sys`.`redundant_keys`.`index_name` AS `redundant_index_name`,"                               \
    "`sys`.`redundant_keys`.`index_columns` AS `redundant_index_columns`,"                         \
    "`sys`.`redundant_keys`.`non_unique` AS `redundant_index_non_unique`,"                         \
    "`sys`.`dominant_keys`.`index_name` AS `dominant_index_name`,"                                 \
    "`sys`.`dominant_keys`.`index_columns` AS `dominant_index_columns`,"                           \
    "`sys`.`dominant_keys`.`non_unique` AS `dominant_index_non_unique`,"                           \
    "if(((0 <> `sys`.`redundant_keys`.`subpart_exists`) or (0 <> "                                 \
    "`sys`.`dominant_keys`.`subpart_exists`)),1,0) AS `subpart_exists`,"                           \
    "concat('ALTER TABLE `',`sys`.`redundant_keys`.`table_schema`,'`.`',"                          \
    "`sys`.`redundant_keys`.`table_name`,'` DROP INDEX `',"                                        \
    "`sys`.`redundant_keys`.`index_name`,'`') AS `sql_drop_index` from "                           \
    "(`sys`.`x$schema_flattened_keys` `redundant_keys` join "                                      \
    "`sys`.`x$schema_flattened_keys` `dominant_keys` on((("                                        \
    "`sys`.`redundant_keys`.`table_schema` = `sys`.`dominant_keys`.`table_schema`) and ("          \
    "`sys`.`redundant_keys`.`table_name` = `sys`.`dominant_keys`.`table_name`)))) where (("        \
    "`sys`.`redundant_keys`.`index_name` <> `sys`.`dominant_keys`.`index_name`) and ((("           \
    "`sys`.`redundant_keys`.`index_columns` = `sys`.`dominant_keys`.`index_columns`) and (("       \
    "`sys`.`redundant_keys`.`non_unique` > `sys`.`dominant_keys`.`non_unique`) or (("              \
    "`sys`.`redundant_keys`.`non_unique` = `sys`.`dominant_keys`.`non_unique`) and (if(("          \
    "`sys`.`redundant_keys`.`index_name` = 'PRIMARY'),'',`sys`.`redundant_keys`.`index_name`) "    \
    "> if((`sys`.`dominant_keys`.`index_name` = 'PRIMARY'),'',"                                    \
    "`sys`.`dominant_keys`.`index_name`))))) or ((locate(concat("                                  \
    "`sys`.`redundant_keys`.`index_columns`,','),`sys`.`dominant_keys`.`index_columns`) = 1) "     \
    "and (`sys`.`redundant_keys`.`non_unique` = 1)) or ((locate(concat("                           \
    "`sys`.`dominant_keys`.`index_columns`,','),`sys`.`redundant_keys`.`index_columns`) = 1) "     \
    "and (`sys`.`dominant_keys`.`non_unique` = 0))))"

#define SYS_X_SCHEMA_FLATTENED_KEYS_VIEW_COLUMNS                                                   \
    "(`table_schema`,`table_name`,`index_name`,`non_unique`,`subpart_exists`,`index_columns`)"

#define SYS_X_SCHEMA_FLATTENED_KEYS_VIEW_DEFINITION                                                \
    "select `information_schema`.`STATISTICS`.`TABLE_SCHEMA` AS `TABLE_SCHEMA`,"                   \
    "`information_schema`.`STATISTICS`.`TABLE_NAME` AS `TABLE_NAME`,"                              \
    "`information_schema`.`STATISTICS`.`INDEX_NAME` AS `INDEX_NAME`,"                              \
    "max(`information_schema`.`STATISTICS`.`NON_UNIQUE`) AS `non_unique`,max(if(("                 \
    "`information_schema`.`STATISTICS`.`SUB_PART` is null),0,1)) AS `subpart_exists`,"             \
    "group_concat(`information_schema`.`STATISTICS`.`COLUMN_NAME` order by "                       \
    "`information_schema`.`STATISTICS`.`SEQ_IN_INDEX` ASC separator ',') AS `index_columns` "      \
    "from `information_schema`.`STATISTICS` where (("                                              \
    "`information_schema`.`STATISTICS`.`INDEX_TYPE` = 'BTREE') and ("                              \
    "`information_schema`.`STATISTICS`.`TABLE_SCHEMA` not in ('mysql','sys',"                      \
    "'INFORMATION_SCHEMA','PERFORMANCE_SCHEMA'))) group by "                                       \
    "`information_schema`.`STATISTICS`.`TABLE_SCHEMA`,"                                            \
    "`information_schema`.`STATISTICS`.`TABLE_NAME`,"                                              \
    "`information_schema`.`STATISTICS`.`INDEX_NAME`"

static const char sys_schema_redundant_indexes_view_definition[] =
    SYS_SCHEMA_REDUNDANT_INDEXES_VIEW_DEFINITION;

static const char sys_schema_redundant_indexes_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`schema_redundant_indexes` " SYS_SCHEMA_REDUNDANT_INDEXES_VIEW_COLUMNS
    " AS " SYS_SCHEMA_REDUNDANT_INDEXES_VIEW_DEFINITION;

static const char sys_schema_redundant_indexes_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`schema_redundant_indexes` " SYS_SCHEMA_REDUNDANT_INDEXES_VIEW_COLUMNS
    " AS " SYS_SCHEMA_REDUNDANT_INDEXES_VIEW_DEFINITION;

static const char sys_x_schema_flattened_keys_view_definition[] =
    SYS_X_SCHEMA_FLATTENED_KEYS_VIEW_DEFINITION;

static const char sys_x_schema_flattened_keys_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$schema_flattened_keys` " SYS_X_SCHEMA_FLATTENED_KEYS_VIEW_COLUMNS
    " AS " SYS_X_SCHEMA_FLATTENED_KEYS_VIEW_DEFINITION;

static const char sys_x_schema_flattened_keys_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$schema_flattened_keys` " SYS_X_SCHEMA_FLATTENED_KEYS_VIEW_COLUMNS
    " AS " SYS_X_SCHEMA_FLATTENED_KEYS_VIEW_DEFINITION;

#undef SYS_SCHEMA_REDUNDANT_INDEXES_VIEW_COLUMNS
#undef SYS_SCHEMA_REDUNDANT_INDEXES_VIEW_DEFINITION
#undef SYS_X_SCHEMA_FLATTENED_KEYS_VIEW_COLUMNS
#undef SYS_X_SCHEMA_FLATTENED_KEYS_VIEW_DEFINITION

#define SYS_SCHEMA_TABLE_LOCK_WAITS_VIEW_COLUMNS                                                   \
    "(`object_schema`,`object_name`,`waiting_thread_id`,`waiting_pid`,`waiting_account`,"          \
    "`waiting_lock_type`,`waiting_lock_duration`,`waiting_query`,`waiting_query_secs`,"            \
    "`waiting_query_rows_affected`,`waiting_query_rows_examined`,`blocking_thread_id`,"            \
    "`blocking_pid`,`blocking_account`,`blocking_lock_type`,`blocking_lock_duration`,"             \
    "`sql_kill_blocking_query`,`sql_kill_blocking_connection`)"

#define SYS_SCHEMA_TABLE_LOCK_WAITS_SELECT_PREFIX                                                  \
    "select `g`.`OBJECT_SCHEMA` AS `object_schema`,`g`.`OBJECT_NAME` AS `object_name`,"            \
    "`pt`.`THREAD_ID` AS `waiting_thread_id`,`pt`.`PROCESSLIST_ID` AS `waiting_pid`,"              \
    "`sys`.`ps_thread_account`(`p`.`OWNER_THREAD_ID`) AS `waiting_account`,"                       \
    "`p`.`LOCK_TYPE` AS `waiting_lock_type`,`p`.`LOCK_DURATION` AS "                               \
    "`waiting_lock_duration`,"

#define SYS_SCHEMA_TABLE_LOCK_WAITS_SELECT_SUFFIX                                                  \
    " AS `waiting_query`,`pt`.`PROCESSLIST_TIME` AS `waiting_query_secs`,"                         \
    "`ps`.`ROWS_AFFECTED` AS `waiting_query_rows_affected`,`ps`.`ROWS_EXAMINED` AS "               \
    "`waiting_query_rows_examined`,`gt`.`THREAD_ID` AS `blocking_thread_id`,"                      \
    "`gt`.`PROCESSLIST_ID` AS `blocking_pid`,`sys`.`ps_thread_account`("                           \
    "`g`.`OWNER_THREAD_ID`) AS `blocking_account`,`g`.`LOCK_TYPE` AS `blocking_lock_type`,"        \
    "`g`.`LOCK_DURATION` AS `blocking_lock_duration`,concat('KILL QUERY ',"                        \
    "`gt`.`PROCESSLIST_ID`) AS `sql_kill_blocking_query`,concat('KILL ',"                          \
    "`gt`.`PROCESSLIST_ID`) AS `sql_kill_blocking_connection` from "                               \
    "(((((`performance_schema`.`metadata_locks` `g` join "                                         \
    "`performance_schema`.`metadata_locks` `p` on(((`g`.`OBJECT_TYPE` = "                          \
    "`p`.`OBJECT_TYPE`) and (`g`.`OBJECT_SCHEMA` = `p`.`OBJECT_SCHEMA`) and ("                     \
    "`g`.`OBJECT_NAME` = `p`.`OBJECT_NAME`) and (`g`.`LOCK_STATUS` = 'GRANTED') and ("             \
    "`p`.`LOCK_STATUS` = 'PENDING')))) join `performance_schema`.`threads` `gt` on(("              \
    "`g`.`OWNER_THREAD_ID` = `gt`.`THREAD_ID`))) join `performance_schema`.`threads` `pt` "        \
    "on((`p`.`OWNER_THREAD_ID` = `pt`.`THREAD_ID`))) left join "                                   \
    "`performance_schema`.`events_statements_current` `gs` on((`g`.`OWNER_THREAD_ID` = "           \
    "`gs`.`THREAD_ID`))) left join `performance_schema`.`events_statements_current` `ps` on(("     \
    "`p`.`OWNER_THREAD_ID` = `ps`.`THREAD_ID`))) where (`g`.`OBJECT_TYPE` = 'TABLE')"

#define SYS_SCHEMA_TABLE_LOCK_WAITS_VIEW_DEFINITION                                                \
    SYS_SCHEMA_TABLE_LOCK_WAITS_SELECT_PREFIX                                                      \
    "`sys`.`format_statement`(`pt`.`PROCESSLIST_INFO`)" SYS_SCHEMA_TABLE_LOCK_WAITS_SELECT_SUFFIX

#define SYS_X_SCHEMA_TABLE_LOCK_WAITS_VIEW_DEFINITION                                              \
    SYS_SCHEMA_TABLE_LOCK_WAITS_SELECT_PREFIX                                                      \
    "`pt`.`PROCESSLIST_INFO`" SYS_SCHEMA_TABLE_LOCK_WAITS_SELECT_SUFFIX

static const char sys_schema_table_lock_waits_view_definition[] =
    SYS_SCHEMA_TABLE_LOCK_WAITS_VIEW_DEFINITION;

static const char sys_schema_table_lock_waits_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`schema_table_lock_waits` " SYS_SCHEMA_TABLE_LOCK_WAITS_VIEW_COLUMNS
    " AS " SYS_SCHEMA_TABLE_LOCK_WAITS_VIEW_DEFINITION;

static const char sys_schema_table_lock_waits_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`schema_table_lock_waits` " SYS_SCHEMA_TABLE_LOCK_WAITS_VIEW_COLUMNS
    " AS " SYS_SCHEMA_TABLE_LOCK_WAITS_VIEW_DEFINITION;

static const char sys_x_schema_table_lock_waits_view_definition[] =
    SYS_X_SCHEMA_TABLE_LOCK_WAITS_VIEW_DEFINITION;

static const char sys_x_schema_table_lock_waits_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$schema_table_lock_waits` " SYS_SCHEMA_TABLE_LOCK_WAITS_VIEW_COLUMNS
    " AS " SYS_X_SCHEMA_TABLE_LOCK_WAITS_VIEW_DEFINITION;

static const char sys_x_schema_table_lock_waits_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$schema_table_lock_waits` " SYS_SCHEMA_TABLE_LOCK_WAITS_VIEW_COLUMNS
    " AS " SYS_X_SCHEMA_TABLE_LOCK_WAITS_VIEW_DEFINITION;

#undef SYS_SCHEMA_TABLE_LOCK_WAITS_VIEW_COLUMNS
#undef SYS_SCHEMA_TABLE_LOCK_WAITS_SELECT_PREFIX
#undef SYS_SCHEMA_TABLE_LOCK_WAITS_SELECT_SUFFIX
#undef SYS_SCHEMA_TABLE_LOCK_WAITS_VIEW_DEFINITION
#undef SYS_X_SCHEMA_TABLE_LOCK_WAITS_VIEW_DEFINITION

#define SYS_X_PS_SCHEMA_TABLE_STATISTICS_IO_VIEW_COLUMNS                                           \
    "(`table_schema`,`table_name`,`count_read`,`sum_number_of_bytes_read`,"                        \
    "`sum_timer_read`,`count_write`,`sum_number_of_bytes_write`,`sum_timer_write`,"                \
    "`count_misc`,`sum_timer_misc`)"

#define SYS_X_PS_SCHEMA_TABLE_STATISTICS_IO_VIEW_DEFINITION                                        \
    "select `extract_schema_from_file_name`(`performance_schema`.`file_summary_by_instance`."      \
    "`FILE_NAME`) AS `table_schema`,`extract_table_from_file_name`(`performance_schema`."          \
    "`file_summary_by_instance`.`FILE_NAME`) AS `table_name`,sum(`performance_schema`."            \
    "`file_summary_by_instance`.`COUNT_READ`) AS `count_read`,sum(`performance_schema`."           \
    "`file_summary_by_instance`.`SUM_NUMBER_OF_BYTES_READ`) AS "                                   \
    "`sum_number_of_bytes_read`,sum(`performance_schema`.`file_summary_by_instance`."              \
    "`SUM_TIMER_READ`) AS `sum_timer_read`,sum(`performance_schema`."                              \
    "`file_summary_by_instance`.`COUNT_WRITE`) AS `count_write`,sum(`performance_schema`."         \
    "`file_summary_by_instance`.`SUM_NUMBER_OF_BYTES_WRITE`) AS "                                  \
    "`sum_number_of_bytes_write`,sum(`performance_schema`.`file_summary_by_instance`."             \
    "`SUM_TIMER_WRITE`) AS `sum_timer_write`,sum(`performance_schema`."                            \
    "`file_summary_by_instance`.`COUNT_MISC`) AS `count_misc`,sum(`performance_schema`."           \
    "`file_summary_by_instance`.`SUM_TIMER_MISC`) AS `sum_timer_misc` from "                       \
    "`performance_schema`.`file_summary_by_instance` group by `table_schema`,`table_name`"

static const char sys_x_ps_schema_table_statistics_io_view_definition[] =
    SYS_X_PS_SCHEMA_TABLE_STATISTICS_IO_VIEW_DEFINITION;

static const char sys_x_ps_schema_table_statistics_io_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$ps_schema_table_statistics_io` " SYS_X_PS_SCHEMA_TABLE_STATISTICS_IO_VIEW_COLUMNS
    " AS " SYS_X_PS_SCHEMA_TABLE_STATISTICS_IO_VIEW_DEFINITION;

static const char sys_x_ps_schema_table_statistics_io_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$ps_schema_table_statistics_io` " SYS_X_PS_SCHEMA_TABLE_STATISTICS_IO_VIEW_COLUMNS
    " AS " SYS_X_PS_SCHEMA_TABLE_STATISTICS_IO_VIEW_DEFINITION;

#undef SYS_X_PS_SCHEMA_TABLE_STATISTICS_IO_VIEW_COLUMNS
#undef SYS_X_PS_SCHEMA_TABLE_STATISTICS_IO_VIEW_DEFINITION

#define SYS_SCHEMA_TABLE_STATISTICS_VIEW_COLUMNS                                                   \
    "(`table_schema`,`table_name`,`total_latency`,`rows_fetched`,`fetch_latency`,"                 \
    "`rows_inserted`,`insert_latency`,`rows_updated`,`update_latency`,`rows_deleted`,"             \
    "`delete_latency`,`io_read_requests`,`io_read`,`io_read_latency`,`io_write_requests`,"         \
    "`io_write`,`io_write_latency`,`io_misc_requests`,`io_misc_latency`)"

#define SYS_SCHEMA_TABLE_STATISTICS_SELECT_SUFFIX                                                  \
    " from (`performance_schema`.`table_io_waits_summary_by_table` `pst` left join "               \
    "`sys`.`x$ps_schema_table_statistics_io` `fsbi` on(((`pst`.`OBJECT_SCHEMA` = "                 \
    "`sys`.`fsbi`.`table_schema`) and (`pst`.`OBJECT_NAME` = "                                     \
    "`sys`.`fsbi`.`table_name`)))) order by `pst`.`SUM_TIMER_WAIT` desc"

#define SYS_SCHEMA_TABLE_STATISTICS_VIEW_DEFINITION                                                \
    "select `pst`.`OBJECT_SCHEMA` AS `table_schema`,`pst`.`OBJECT_NAME` AS `table_name`,"          \
    "format_pico_time(`pst`.`SUM_TIMER_WAIT`) AS `total_latency`,`pst`.`COUNT_FETCH` AS "          \
    "`rows_fetched`,format_pico_time(`pst`.`SUM_TIMER_FETCH`) AS `fetch_latency`,"                 \
    "`pst`.`COUNT_INSERT` AS `rows_inserted`,format_pico_time(`pst`.`SUM_TIMER_INSERT`) AS "       \
    "`insert_latency`,`pst`.`COUNT_UPDATE` AS `rows_updated`,format_pico_time("                    \
    "`pst`.`SUM_TIMER_UPDATE`) AS `update_latency`,`pst`.`COUNT_DELETE` AS `rows_deleted`,"        \
    "format_pico_time(`pst`.`SUM_TIMER_DELETE`) AS `delete_latency`,"                              \
    "`sys`.`fsbi`.`count_read` AS `io_read_requests`,format_bytes("                                \
    "`sys`.`fsbi`.`sum_number_of_bytes_read`) AS `io_read`,format_pico_time("                      \
    "`sys`.`fsbi`.`sum_timer_read`) AS `io_read_latency`,`sys`.`fsbi`.`count_write` AS "           \
    "`io_write_requests`,format_bytes(`sys`.`fsbi`.`sum_number_of_bytes_write`) AS "               \
    "`io_write`,format_pico_time(`sys`.`fsbi`.`sum_timer_write`) AS `io_write_latency`,"           \
    "`sys`.`fsbi`.`count_misc` AS `io_misc_requests`,format_pico_time("                            \
    "`sys`.`fsbi`.`sum_timer_misc`) AS "                                                           \
    "`io_misc_latency`" SYS_SCHEMA_TABLE_STATISTICS_SELECT_SUFFIX

#define SYS_X_SCHEMA_TABLE_STATISTICS_VIEW_DEFINITION                                              \
    "select `pst`.`OBJECT_SCHEMA` AS `table_schema`,`pst`.`OBJECT_NAME` AS `table_name`,"          \
    "`pst`.`SUM_TIMER_WAIT` AS `total_latency`,`pst`.`COUNT_FETCH` AS `rows_fetched`,"             \
    "`pst`.`SUM_TIMER_FETCH` AS `fetch_latency`,`pst`.`COUNT_INSERT` AS `rows_inserted`,"          \
    "`pst`.`SUM_TIMER_INSERT` AS `insert_latency`,`pst`.`COUNT_UPDATE` AS `rows_updated`,"         \
    "`pst`.`SUM_TIMER_UPDATE` AS `update_latency`,`pst`.`COUNT_DELETE` AS `rows_deleted`,"         \
    "`pst`.`SUM_TIMER_DELETE` AS `delete_latency`,`sys`.`fsbi`.`count_read` AS "                   \
    "`io_read_requests`,`sys`.`fsbi`.`sum_number_of_bytes_read` AS `io_read`,"                     \
    "`sys`.`fsbi`.`sum_timer_read` AS `io_read_latency`,`sys`.`fsbi`.`count_write` AS "            \
    "`io_write_requests`,`sys`.`fsbi`.`sum_number_of_bytes_write` AS `io_write`,"                  \
    "`sys`.`fsbi`.`sum_timer_write` AS `io_write_latency`,`sys`.`fsbi`.`count_misc` AS "           \
    "`io_misc_requests`,`sys`.`fsbi`.`sum_timer_misc` AS "                                         \
    "`io_misc_latency`" SYS_SCHEMA_TABLE_STATISTICS_SELECT_SUFFIX

static const char sys_schema_table_statistics_view_definition[] =
    SYS_SCHEMA_TABLE_STATISTICS_VIEW_DEFINITION;

static const char sys_schema_table_statistics_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`schema_table_statistics` " SYS_SCHEMA_TABLE_STATISTICS_VIEW_COLUMNS
    " AS " SYS_SCHEMA_TABLE_STATISTICS_VIEW_DEFINITION;

static const char sys_schema_table_statistics_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`schema_table_statistics` " SYS_SCHEMA_TABLE_STATISTICS_VIEW_COLUMNS
    " AS " SYS_SCHEMA_TABLE_STATISTICS_VIEW_DEFINITION;

static const char sys_x_schema_table_statistics_view_definition[] =
    SYS_X_SCHEMA_TABLE_STATISTICS_VIEW_DEFINITION;

static const char sys_x_schema_table_statistics_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$schema_table_statistics` " SYS_SCHEMA_TABLE_STATISTICS_VIEW_COLUMNS
    " AS " SYS_X_SCHEMA_TABLE_STATISTICS_VIEW_DEFINITION;

static const char sys_x_schema_table_statistics_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$schema_table_statistics` " SYS_SCHEMA_TABLE_STATISTICS_VIEW_COLUMNS
    " AS " SYS_X_SCHEMA_TABLE_STATISTICS_VIEW_DEFINITION;

#undef SYS_SCHEMA_TABLE_STATISTICS_VIEW_COLUMNS
#undef SYS_SCHEMA_TABLE_STATISTICS_SELECT_SUFFIX
#undef SYS_SCHEMA_TABLE_STATISTICS_VIEW_DEFINITION
#undef SYS_X_SCHEMA_TABLE_STATISTICS_VIEW_DEFINITION

#define SYS_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_VIEW_COLUMNS                                       \
    "(`table_schema`,`table_name`,`rows_fetched`,`fetch_latency`,`rows_inserted`,"                 \
    "`insert_latency`,`rows_updated`,`update_latency`,`rows_deleted`,`delete_latency`,"            \
    "`io_read_requests`,`io_read`,`io_read_latency`,`io_write_requests`,`io_write`,"               \
    "`io_write_latency`,`io_misc_requests`,`io_misc_latency`,`innodb_buffer_allocated`,"           \
    "`innodb_buffer_data`,`innodb_buffer_free`,`innodb_buffer_pages`,"                             \
    "`innodb_buffer_pages_hashed`,`innodb_buffer_pages_old`,`innodb_buffer_rows_cached`)"

#define SYS_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_SELECT_SUFFIX                                      \
    " from ((`performance_schema`.`table_io_waits_summary_by_table` `pst` left join "              \
    "`sys`.`x$ps_schema_table_statistics_io` `fsbi` on(((`pst`.`OBJECT_SCHEMA` = "                 \
    "`sys`.`fsbi`.`table_schema`) and (`pst`.`OBJECT_NAME` = "                                     \
    "`sys`.`fsbi`.`table_name`)))) left join `sys`.`x$innodb_buffer_stats_by_table` `ibp` "        \
    "on(((`pst`.`OBJECT_SCHEMA` = `sys`.`ibp`.`object_schema`) and ("                              \
    "`pst`.`OBJECT_NAME` = `sys`.`ibp`.`object_name`)))) order by `pst`.`SUM_TIMER_WAIT` desc"

#define SYS_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_VIEW_DEFINITION                                    \
    "select `pst`.`OBJECT_SCHEMA` AS `table_schema`,`pst`.`OBJECT_NAME` AS `table_name`,"          \
    "`pst`.`COUNT_FETCH` AS `rows_fetched`,format_pico_time(`pst`.`SUM_TIMER_FETCH`) AS "          \
    "`fetch_latency`,`pst`.`COUNT_INSERT` AS `rows_inserted`,format_pico_time("                    \
    "`pst`.`SUM_TIMER_INSERT`) AS `insert_latency`,`pst`.`COUNT_UPDATE` AS `rows_updated`,"        \
    "format_pico_time(`pst`.`SUM_TIMER_UPDATE`) AS `update_latency`,`pst`.`COUNT_DELETE` AS "      \
    "`rows_deleted`,format_pico_time(`pst`.`SUM_TIMER_DELETE`) AS `delete_latency`,"               \
    "`sys`.`fsbi`.`count_read` AS `io_read_requests`,format_bytes("                                \
    "`sys`.`fsbi`.`sum_number_of_bytes_read`) AS `io_read`,format_pico_time("                      \
    "`sys`.`fsbi`.`sum_timer_read`) AS `io_read_latency`,`sys`.`fsbi`.`count_write` AS "           \
    "`io_write_requests`,format_bytes(`sys`.`fsbi`.`sum_number_of_bytes_write`) AS "               \
    "`io_write`,format_pico_time(`sys`.`fsbi`.`sum_timer_write`) AS `io_write_latency`,"           \
    "`sys`.`fsbi`.`count_misc` AS `io_misc_requests`,format_pico_time("                            \
    "`sys`.`fsbi`.`sum_timer_misc`) AS `io_misc_latency`,format_bytes("                            \
    "`sys`.`ibp`.`allocated`) AS `innodb_buffer_allocated`,format_bytes("                          \
    "`sys`.`ibp`.`data`) AS `innodb_buffer_data`,format_bytes((`sys`.`ibp`.`allocated` - "         \
    "`sys`.`ibp`.`data`)) AS `innodb_buffer_free`,`sys`.`ibp`.`pages` AS "                         \
    "`innodb_buffer_pages`,`sys`.`ibp`.`pages_hashed` AS `innodb_buffer_pages_hashed`,"            \
    "`sys`.`ibp`.`pages_old` AS `innodb_buffer_pages_old`,`sys`.`ibp`.`rows_cached` AS "           \
    "`innodb_buffer_rows_cached`" SYS_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_SELECT_SUFFIX

#define SYS_X_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_SELECT_SUFFIX                                    \
    " from ((`performance_schema`.`table_io_waits_summary_by_table` `pst` left join "              \
    "`x$ps_schema_table_statistics_io` `fsbi` on(((`pst`.`OBJECT_SCHEMA` = "                       \
    "`fsbi`.`table_schema`) and (`pst`.`OBJECT_NAME` = `fsbi`.`table_name`)))) left join "         \
    "`x$innodb_buffer_stats_by_table` `ibp` on(((`pst`.`OBJECT_SCHEMA` = "                         \
    "`ibp`.`object_schema`) and (`pst`.`OBJECT_NAME` = `ibp`.`object_name`)))) order by "          \
    "`pst`.`SUM_TIMER_WAIT` desc"

#define SYS_X_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_VIEW_DEFINITION                                  \
    "select `pst`.`OBJECT_SCHEMA` AS `table_schema`,`pst`.`OBJECT_NAME` AS `table_name`,"          \
    "`pst`.`COUNT_FETCH` AS `rows_fetched`,`pst`.`SUM_TIMER_FETCH` AS `fetch_latency`,"            \
    "`pst`.`COUNT_INSERT` AS `rows_inserted`,`pst`.`SUM_TIMER_INSERT` AS `insert_latency`,"        \
    "`pst`.`COUNT_UPDATE` AS `rows_updated`,`pst`.`SUM_TIMER_UPDATE` AS `update_latency`,"         \
    "`pst`.`COUNT_DELETE` AS `rows_deleted`,`pst`.`SUM_TIMER_DELETE` AS `delete_latency`,"         \
    "`fsbi`.`count_read` AS `io_read_requests`,`fsbi`.`sum_number_of_bytes_read` AS "              \
    "`io_read`,`fsbi`.`sum_timer_read` AS `io_read_latency`,`fsbi`.`count_write` AS "              \
    "`io_write_requests`,`fsbi`.`sum_number_of_bytes_write` AS `io_write`,"                        \
    "`fsbi`.`sum_timer_write` AS `io_write_latency`,`fsbi`.`count_misc` AS "                       \
    "`io_misc_requests`,`fsbi`.`sum_timer_misc` AS `io_misc_latency`,`ibp`.`allocated` AS "        \
    "`innodb_buffer_allocated`,`ibp`.`data` AS `innodb_buffer_data`,("                             \
    "`ibp`.`allocated` - `ibp`.`data`) AS `innodb_buffer_free`,`ibp`.`pages` AS "                  \
    "`innodb_buffer_pages`,`ibp`.`pages_hashed` AS `innodb_buffer_pages_hashed`,"                  \
    "`ibp`.`pages_old` AS `innodb_buffer_pages_old`,`ibp`.`rows_cached` AS "                       \
    "`innodb_buffer_rows_cached`" SYS_X_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_SELECT_SUFFIX

static const char sys_schema_table_statistics_with_buffer_view_definition[] =
    SYS_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_VIEW_DEFINITION;

static const char sys_schema_table_statistics_with_buffer_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`schema_table_statistics_with_buffer` " SYS_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_VIEW_COLUMNS
    " AS " SYS_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_VIEW_DEFINITION;

static const char sys_schema_table_statistics_with_buffer_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`schema_table_statistics_with_buffer`"
    " " SYS_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_VIEW_COLUMNS
    " AS " SYS_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_VIEW_DEFINITION;

static const char sys_x_schema_table_statistics_with_buffer_view_definition[] =
    SYS_X_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_VIEW_DEFINITION;

static const char sys_x_schema_table_statistics_with_buffer_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$schema_table_statistics_with_buffer` " SYS_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_VIEW_COLUMNS
    " AS " SYS_X_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_VIEW_DEFINITION;

static const char sys_x_schema_table_statistics_with_buffer_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$schema_table_statistics_with_buffer`"
    " " SYS_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_VIEW_COLUMNS
    " AS " SYS_X_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_VIEW_DEFINITION;

#undef SYS_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_VIEW_COLUMNS
#undef SYS_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_SELECT_SUFFIX
#undef SYS_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_VIEW_DEFINITION
#undef SYS_X_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_SELECT_SUFFIX
#undef SYS_X_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_VIEW_DEFINITION

#define SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_VIEW_COLUMNS                                       \
    "(`object_schema`,`object_name`,`rows_full_scanned`,`latency`)"

#define SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_SELECT_SUFFIX                                      \
    " from `performance_schema`.`table_io_waits_summary_by_index_usage` where (("                  \
    "`performance_schema`.`table_io_waits_summary_by_index_usage`.`INDEX_NAME` is null) and ("     \
    "`performance_schema`.`table_io_waits_summary_by_index_usage`.`COUNT_READ` > 0)) order by "    \
    "`performance_schema`.`table_io_waits_summary_by_index_usage`.`COUNT_READ` desc"

#define SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_SELECT_PREFIX                                      \
    "select `performance_schema`.`table_io_waits_summary_by_index_usage`.`OBJECT_SCHEMA` AS "      \
    "`object_schema`,`performance_schema`.`table_io_waits_summary_by_index_usage`.`OBJECT_NAME` "  \
    "AS `object_name`,`performance_schema`.`table_io_waits_summary_by_index_usage`.`COUNT_READ` "  \
    "AS `rows_full_scanned`,"

#define SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_VIEW_DEFINITION                                    \
    SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_SELECT_PREFIX                                          \
    "format_pico_time(`performance_schema`.`table_io_waits_summary_by_index_usage`."               \
    "`SUM_TIMER_WAIT`) AS `latency`" SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_SELECT_SUFFIX

#define SYS_X_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_VIEW_DEFINITION                                  \
    SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_SELECT_PREFIX                                          \
    "`performance_schema`.`table_io_waits_summary_by_index_usage`.`SUM_TIMER_WAIT` AS "            \
    "`latency`" SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_SELECT_SUFFIX

static const char sys_schema_tables_with_full_table_scans_view_definition[] =
    SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_VIEW_DEFINITION;

static const char sys_schema_tables_with_full_table_scans_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`schema_tables_with_full_table_scans` " SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_VIEW_COLUMNS
    " AS " SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_VIEW_DEFINITION;

static const char sys_schema_tables_with_full_table_scans_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`schema_tables_with_full_table_scans`"
    " " SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_VIEW_COLUMNS
    " AS " SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_VIEW_DEFINITION;

static const char sys_x_schema_tables_with_full_table_scans_view_definition[] =
    SYS_X_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_VIEW_DEFINITION;

static const char sys_x_schema_tables_with_full_table_scans_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$schema_tables_with_full_table_scans` " SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_VIEW_COLUMNS
    " AS " SYS_X_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_VIEW_DEFINITION;

static const char sys_x_schema_tables_with_full_table_scans_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$schema_tables_with_full_table_scans`"
    " " SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_VIEW_COLUMNS
    " AS " SYS_X_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_VIEW_DEFINITION;

#undef SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_VIEW_COLUMNS
#undef SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_SELECT_SUFFIX
#undef SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_SELECT_PREFIX
#undef SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_VIEW_DEFINITION
#undef SYS_X_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_VIEW_DEFINITION

#define SYS_SCHEMA_UNUSED_INDEXES_VIEW_COLUMNS "(`object_schema`,`object_name`,`index_name`)"

#define SYS_SCHEMA_UNUSED_INDEXES_VIEW_DEFINITION                                                  \
    "select `t`.`OBJECT_SCHEMA` AS `object_schema`,`t`.`OBJECT_NAME` AS `object_name`,"            \
    "`t`.`INDEX_NAME` AS `index_name` from ("                                                      \
    "`performance_schema`.`table_io_waits_summary_by_index_usage` `t` join "                       \
    "`information_schema`.`STATISTICS` `s` on(((`t`.`OBJECT_SCHEMA` = "                            \
    "`information_schema`.`s`.`TABLE_SCHEMA`) and (`t`.`OBJECT_NAME` = "                           \
    "`information_schema`.`s`.`TABLE_NAME`) and (`t`.`INDEX_NAME` = "                              \
    "`information_schema`.`s`.`INDEX_NAME`)))) where ((`t`.`INDEX_NAME` is not null) "             \
    "and (`t`.`COUNT_STAR` = 0) and (`t`.`OBJECT_SCHEMA` <> 'mysql') and "                         \
    "(`t`.`INDEX_NAME` <> 'PRIMARY') and (`information_schema`.`s`.`NON_UNIQUE` = 1) "             \
    "and (`information_schema`.`s`.`SEQ_IN_INDEX` = 1)) order by `t`.`OBJECT_SCHEMA`,"             \
    "`t`.`OBJECT_NAME`"

static const char sys_schema_unused_indexes_view_definition[] =
    SYS_SCHEMA_UNUSED_INDEXES_VIEW_DEFINITION;

static const char sys_schema_unused_indexes_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`schema_unused_indexes` " SYS_SCHEMA_UNUSED_INDEXES_VIEW_COLUMNS
    " AS " SYS_SCHEMA_UNUSED_INDEXES_VIEW_DEFINITION;

static const char sys_schema_unused_indexes_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`schema_unused_indexes` " SYS_SCHEMA_UNUSED_INDEXES_VIEW_COLUMNS
    " AS " SYS_SCHEMA_UNUSED_INDEXES_VIEW_DEFINITION;

#undef SYS_SCHEMA_UNUSED_INDEXES_VIEW_COLUMNS
#undef SYS_SCHEMA_UNUSED_INDEXES_VIEW_DEFINITION

#define SYS_SCHEMA_OBJECT_OVERVIEW_VIEW_COLUMNS "(`db`,`object_type`,`count`)"

#define SYS_SCHEMA_OBJECT_OVERVIEW_VIEW_DEFINITION                                                 \
    "select `information_schema`.`routines`.`ROUTINE_SCHEMA` AS `db`,"                             \
    "`information_schema`.`routines`.`ROUTINE_TYPE` AS `object_type`,count(0) AS `count` "         \
    "from `information_schema`.`ROUTINES` `routines` group by "                                    \
    "`information_schema`.`routines`.`ROUTINE_SCHEMA`,"                                            \
    "`information_schema`.`routines`.`ROUTINE_TYPE` union select "                                 \
    "`information_schema`.`tables`.`TABLE_SCHEMA` AS `TABLE_SCHEMA`,"                              \
    "`information_schema`.`tables`.`TABLE_TYPE` AS `TABLE_TYPE`,count(0) AS `COUNT(*)` "           \
    "from `information_schema`.`TABLES` `tables` group by "                                        \
    "`information_schema`.`tables`.`TABLE_SCHEMA`,"                                                \
    "`information_schema`.`tables`.`TABLE_TYPE` union select "                                     \
    "`information_schema`.`statistics`.`TABLE_SCHEMA` AS `TABLE_SCHEMA`,"                          \
    "concat('INDEX (',`information_schema`.`statistics`.`INDEX_TYPE`,')') AS "                     \
    "`CONCAT('INDEX (', INDEX_TYPE, ')')`,count(0) AS `COUNT(*)` from "                            \
    "`information_schema`.`STATISTICS` `statistics` group by "                                     \
    "`information_schema`.`statistics`.`TABLE_SCHEMA`,"                                            \
    "`information_schema`.`statistics`.`INDEX_TYPE` union select "                                 \
    "`information_schema`.`triggers`.`TRIGGER_SCHEMA` AS `TRIGGER_SCHEMA`,'TRIGGER' AS "           \
    "`TRIGGER`,count(0) AS `COUNT(*)` from `information_schema`.`TRIGGERS` `triggers` "            \
    "group by `information_schema`.`triggers`.`TRIGGER_SCHEMA` union select "                      \
    "`information_schema`.`events`.`EVENT_SCHEMA` AS `EVENT_SCHEMA`,'EVENT' AS `EVENT`,"           \
    "count(0) AS `COUNT(*)` from `information_schema`.`EVENTS` `events` group by "                 \
    "`information_schema`.`events`.`EVENT_SCHEMA` order by `db`,`object_type`"

static const char sys_schema_object_overview_view_definition[] =
    SYS_SCHEMA_OBJECT_OVERVIEW_VIEW_DEFINITION;

static const char sys_schema_object_overview_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`schema_object_overview` " SYS_SCHEMA_OBJECT_OVERVIEW_VIEW_COLUMNS
    " AS " SYS_SCHEMA_OBJECT_OVERVIEW_VIEW_DEFINITION;

static const char sys_schema_object_overview_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`schema_object_overview` " SYS_SCHEMA_OBJECT_OVERVIEW_VIEW_COLUMNS
    " AS " SYS_SCHEMA_OBJECT_OVERVIEW_VIEW_DEFINITION;

#undef SYS_SCHEMA_OBJECT_OVERVIEW_VIEW_COLUMNS
#undef SYS_SCHEMA_OBJECT_OVERVIEW_VIEW_DEFINITION

static const struct mylite_execution_catalog_builtin_sys_view builtin_sys_view_definitions[] = {
    {"version",
     sys_version_view_definition,
     sys_version_show_create_view_sql,
     sys_version_show_create_qualified_view_sql},
    {"host_summary",
     sys_host_summary_view_definition,
     sys_host_summary_show_create_view_sql,
     sys_host_summary_show_create_qualified_view_sql},
    {"host_summary_by_file_io",
     sys_host_summary_by_file_io_view_definition,
     sys_host_summary_by_file_io_show_create_view_sql,
     sys_host_summary_by_file_io_show_create_qualified_view_sql},
    {"host_summary_by_file_io_type",
     sys_host_summary_by_file_io_type_view_definition,
     sys_host_summary_by_file_io_type_show_create_view_sql,
     sys_host_summary_by_file_io_type_show_create_qualified_view_sql},
    {"host_summary_by_stages",
     sys_host_summary_by_stages_view_definition,
     sys_host_summary_by_stages_show_create_view_sql,
     sys_host_summary_by_stages_show_create_qualified_view_sql},
    {"host_summary_by_statement_latency",
     sys_host_summary_by_statement_latency_view_definition,
     sys_host_summary_by_statement_latency_show_create_view_sql,
     sys_host_summary_by_statement_latency_show_create_qualified_view_sql},
    {"host_summary_by_statement_type",
     sys_host_summary_by_statement_type_view_definition,
     sys_host_summary_by_statement_type_show_create_view_sql,
     sys_host_summary_by_statement_type_show_create_qualified_view_sql},
    {"innodb_buffer_stats_by_schema",
     sys_innodb_buffer_stats_by_schema_view_definition,
     sys_innodb_buffer_stats_by_schema_show_create_view_sql,
     sys_innodb_buffer_stats_by_schema_show_create_qualified_view_sql},
    {"innodb_buffer_stats_by_table",
     sys_innodb_buffer_stats_by_table_view_definition,
     sys_innodb_buffer_stats_by_table_show_create_view_sql,
     sys_innodb_buffer_stats_by_table_show_create_qualified_view_sql},
    {"innodb_lock_waits",
     sys_innodb_lock_waits_view_definition,
     sys_innodb_lock_waits_show_create_view_sql,
     sys_innodb_lock_waits_show_create_qualified_view_sql},
    {"io_by_thread_by_latency",
     sys_io_by_thread_by_latency_view_definition,
     sys_io_by_thread_by_latency_show_create_view_sql,
     sys_io_by_thread_by_latency_show_create_qualified_view_sql},
    {"io_global_by_file_by_bytes",
     sys_io_global_by_file_by_bytes_view_definition,
     sys_io_global_by_file_by_bytes_show_create_view_sql,
     sys_io_global_by_file_by_bytes_show_create_qualified_view_sql},
    {"io_global_by_file_by_latency",
     sys_io_global_by_file_by_latency_view_definition,
     sys_io_global_by_file_by_latency_show_create_view_sql,
     sys_io_global_by_file_by_latency_show_create_qualified_view_sql},
    {"io_global_by_wait_by_bytes",
     sys_io_global_by_wait_by_bytes_view_definition,
     sys_io_global_by_wait_by_bytes_show_create_view_sql,
     sys_io_global_by_wait_by_bytes_show_create_qualified_view_sql},
    {"io_global_by_wait_by_latency",
     sys_io_global_by_wait_by_latency_view_definition,
     sys_io_global_by_wait_by_latency_show_create_view_sql,
     sys_io_global_by_wait_by_latency_show_create_qualified_view_sql},
    {"latest_file_io",
     sys_latest_file_io_view_definition,
     sys_latest_file_io_show_create_view_sql,
     sys_latest_file_io_show_create_qualified_view_sql},
    {"memory_by_host_by_current_bytes",
     sys_memory_by_host_by_current_bytes_view_definition,
     sys_memory_by_host_by_current_bytes_show_create_view_sql,
     sys_memory_by_host_by_current_bytes_show_create_qualified_view_sql},
    {"memory_by_thread_by_current_bytes",
     sys_memory_by_thread_by_current_bytes_view_definition,
     sys_memory_by_thread_by_current_bytes_show_create_view_sql,
     sys_memory_by_thread_by_current_bytes_show_create_qualified_view_sql},
    {"memory_by_user_by_current_bytes",
     sys_memory_by_user_by_current_bytes_view_definition,
     sys_memory_by_user_by_current_bytes_show_create_view_sql,
     sys_memory_by_user_by_current_bytes_show_create_qualified_view_sql},
    {"ps_check_lost_instrumentation",
     sys_ps_check_lost_instrumentation_view_definition,
     sys_ps_check_lost_instrumentation_show_create_view_sql,
     sys_ps_check_lost_instrumentation_show_create_qualified_view_sql},
    {"schema_auto_increment_columns",
     sys_schema_auto_increment_columns_view_definition,
     sys_schema_auto_increment_columns_show_create_view_sql,
     sys_schema_auto_increment_columns_show_create_qualified_view_sql},
    {"schema_index_statistics",
     sys_schema_index_statistics_view_definition,
     sys_schema_index_statistics_show_create_view_sql,
     sys_schema_index_statistics_show_create_qualified_view_sql},
    {"schema_object_overview",
     sys_schema_object_overview_view_definition,
     sys_schema_object_overview_show_create_view_sql,
     sys_schema_object_overview_show_create_qualified_view_sql},
    {"schema_redundant_indexes",
     sys_schema_redundant_indexes_view_definition,
     sys_schema_redundant_indexes_show_create_view_sql,
     sys_schema_redundant_indexes_show_create_qualified_view_sql},
    {"schema_table_lock_waits",
     sys_schema_table_lock_waits_view_definition,
     sys_schema_table_lock_waits_show_create_view_sql,
     sys_schema_table_lock_waits_show_create_qualified_view_sql},
    {"schema_table_statistics",
     sys_schema_table_statistics_view_definition,
     sys_schema_table_statistics_show_create_view_sql,
     sys_schema_table_statistics_show_create_qualified_view_sql},
    {"schema_table_statistics_with_buffer",
     sys_schema_table_statistics_with_buffer_view_definition,
     sys_schema_table_statistics_with_buffer_show_create_view_sql,
     sys_schema_table_statistics_with_buffer_show_create_qualified_view_sql},
    {"schema_tables_with_full_table_scans",
     sys_schema_tables_with_full_table_scans_view_definition,
     sys_schema_tables_with_full_table_scans_show_create_view_sql,
     sys_schema_tables_with_full_table_scans_show_create_qualified_view_sql},
    {"schema_unused_indexes",
     sys_schema_unused_indexes_view_definition,
     sys_schema_unused_indexes_show_create_view_sql,
     sys_schema_unused_indexes_show_create_qualified_view_sql},
    {"x$schema_flattened_keys",
     sys_x_schema_flattened_keys_view_definition,
     sys_x_schema_flattened_keys_show_create_view_sql,
     sys_x_schema_flattened_keys_show_create_qualified_view_sql},
    {"x$host_summary",
     sys_x_host_summary_view_definition,
     sys_x_host_summary_show_create_view_sql,
     sys_x_host_summary_show_create_qualified_view_sql},
    {"x$host_summary_by_file_io",
     sys_x_host_summary_by_file_io_view_definition,
     sys_x_host_summary_by_file_io_show_create_view_sql,
     sys_x_host_summary_by_file_io_show_create_qualified_view_sql},
    {"x$host_summary_by_file_io_type",
     sys_x_host_summary_by_file_io_type_view_definition,
     sys_x_host_summary_by_file_io_type_show_create_view_sql,
     sys_x_host_summary_by_file_io_type_show_create_qualified_view_sql},
    {"x$host_summary_by_stages",
     sys_x_host_summary_by_stages_view_definition,
     sys_x_host_summary_by_stages_show_create_view_sql,
     sys_x_host_summary_by_stages_show_create_qualified_view_sql},
    {"x$host_summary_by_statement_latency",
     sys_x_host_summary_by_statement_latency_view_definition,
     sys_x_host_summary_by_statement_latency_show_create_view_sql,
     sys_x_host_summary_by_statement_latency_show_create_qualified_view_sql},
    {"x$host_summary_by_statement_type",
     sys_x_host_summary_by_statement_type_view_definition,
     sys_x_host_summary_by_statement_type_show_create_view_sql,
     sys_x_host_summary_by_statement_type_show_create_qualified_view_sql},
    {"x$innodb_buffer_stats_by_schema",
     sys_x_innodb_buffer_stats_by_schema_view_definition,
     sys_x_innodb_buffer_stats_by_schema_show_create_view_sql,
     sys_x_innodb_buffer_stats_by_schema_show_create_qualified_view_sql},
    {"x$innodb_buffer_stats_by_table",
     sys_x_innodb_buffer_stats_by_table_view_definition,
     sys_x_innodb_buffer_stats_by_table_show_create_view_sql,
     sys_x_innodb_buffer_stats_by_table_show_create_qualified_view_sql},
    {"x$innodb_lock_waits",
     sys_x_innodb_lock_waits_view_definition,
     sys_x_innodb_lock_waits_show_create_view_sql,
     sys_x_innodb_lock_waits_show_create_qualified_view_sql},
    {"x$io_by_thread_by_latency",
     sys_x_io_by_thread_by_latency_view_definition,
     sys_x_io_by_thread_by_latency_show_create_view_sql,
     sys_x_io_by_thread_by_latency_show_create_qualified_view_sql},
    {"x$io_global_by_file_by_bytes",
     sys_x_io_global_by_file_by_bytes_view_definition,
     sys_x_io_global_by_file_by_bytes_show_create_view_sql,
     sys_x_io_global_by_file_by_bytes_show_create_qualified_view_sql},
    {"x$io_global_by_file_by_latency",
     sys_x_io_global_by_file_by_latency_view_definition,
     sys_x_io_global_by_file_by_latency_show_create_view_sql,
     sys_x_io_global_by_file_by_latency_show_create_qualified_view_sql},
    {"x$io_global_by_wait_by_bytes",
     sys_x_io_global_by_wait_by_bytes_view_definition,
     sys_x_io_global_by_wait_by_bytes_show_create_view_sql,
     sys_x_io_global_by_wait_by_bytes_show_create_qualified_view_sql},
    {"x$io_global_by_wait_by_latency",
     sys_x_io_global_by_wait_by_latency_view_definition,
     sys_x_io_global_by_wait_by_latency_show_create_view_sql,
     sys_x_io_global_by_wait_by_latency_show_create_qualified_view_sql},
    {"x$latest_file_io",
     sys_x_latest_file_io_view_definition,
     sys_x_latest_file_io_show_create_view_sql,
     sys_x_latest_file_io_show_create_qualified_view_sql},
    {"x$memory_by_host_by_current_bytes",
     sys_x_memory_by_host_by_current_bytes_view_definition,
     sys_x_memory_by_host_by_current_bytes_show_create_view_sql,
     sys_x_memory_by_host_by_current_bytes_show_create_qualified_view_sql},
    {"x$memory_by_thread_by_current_bytes",
     sys_x_memory_by_thread_by_current_bytes_view_definition,
     sys_x_memory_by_thread_by_current_bytes_show_create_view_sql,
     sys_x_memory_by_thread_by_current_bytes_show_create_qualified_view_sql},
    {"x$memory_by_user_by_current_bytes",
     sys_x_memory_by_user_by_current_bytes_view_definition,
     sys_x_memory_by_user_by_current_bytes_show_create_view_sql,
     sys_x_memory_by_user_by_current_bytes_show_create_qualified_view_sql},
    {"x$ps_schema_table_statistics_io",
     sys_x_ps_schema_table_statistics_io_view_definition,
     sys_x_ps_schema_table_statistics_io_show_create_view_sql,
     sys_x_ps_schema_table_statistics_io_show_create_qualified_view_sql},
    {"x$schema_index_statistics",
     sys_x_schema_index_statistics_view_definition,
     sys_x_schema_index_statistics_show_create_view_sql,
     sys_x_schema_index_statistics_show_create_qualified_view_sql},
    {"x$schema_table_lock_waits",
     sys_x_schema_table_lock_waits_view_definition,
     sys_x_schema_table_lock_waits_show_create_view_sql,
     sys_x_schema_table_lock_waits_show_create_qualified_view_sql},
    {"x$schema_table_statistics",
     sys_x_schema_table_statistics_view_definition,
     sys_x_schema_table_statistics_show_create_view_sql,
     sys_x_schema_table_statistics_show_create_qualified_view_sql},
    {"x$schema_table_statistics_with_buffer",
     sys_x_schema_table_statistics_with_buffer_view_definition,
     sys_x_schema_table_statistics_with_buffer_show_create_view_sql,
     sys_x_schema_table_statistics_with_buffer_show_create_qualified_view_sql},
    {"x$schema_tables_with_full_table_scans",
     sys_x_schema_tables_with_full_table_scans_view_definition,
     sys_x_schema_tables_with_full_table_scans_show_create_view_sql,
     sys_x_schema_tables_with_full_table_scans_show_create_qualified_view_sql},
};

static const struct mylite_execution_catalog_column_definition mysql_component_columns[] = {
    {"component_id", NULL, "NO", "int", NULL, NULL, "10", "0", NULL, NULL, NULL, "int unsigned"},
    {"component_group_id",
     NULL,
     "NO",
     "int",
     NULL,
     NULL,
     "10",
     "0",
     NULL,
     NULL,
     NULL,
     "int unsigned"},
    {"component_urn",
     NULL,
     "NO",
     "text",
     "65535",
     "65535",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "text"},
};

static const char *const mysql_component_column_keys[] = {
    "PRI",
    "",
    "",
};

static const char *const mysql_component_column_extras[] = {
    "auto_increment",
    "",
    "",
};

static const char *const mysql_component_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_column_definition mysql_func_columns[] = {
    {"name", "", "NO", "char", "64", "192", NULL, NULL, NULL, "utf8mb3", "utf8mb3_bin", "char(64)"},
    {"ret", "0", "NO", "tinyint", NULL, NULL, "3", "0", NULL, NULL, NULL, "tinyint"},
    {"dl", "", "NO", "char", "128", "384", NULL, NULL, NULL, "utf8mb3", "utf8mb3_bin", "char(128)"},
    {"type",
     NULL,
     "NO",
     "enum",
     "9",
     "27",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('function','aggregate')"},
};

static const char *const mysql_func_column_keys[] = {
    "PRI",
    "",
    "",
    "",
};

static const char *const mysql_func_column_extras[] = {
    "",
    "",
    "",
    "",
};

static const char *const mysql_func_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_column_definition mysql_gtid_executed_columns[] = {
    {"source_uuid",
     NULL,
     "NO",
     "char",
     "36",
     "144",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "char(36)"},
    {"interval_start", NULL, "NO", "bigint", NULL, NULL, "19", "0", NULL, NULL, NULL, "bigint"},
    {"interval_end", NULL, "NO", "bigint", NULL, NULL, "19", "0", NULL, NULL, NULL, "bigint"},
    {"gtid_tag",
     NULL,
     "NO",
     "char",
     "32",
     "128",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "char(32)"},
};

static const char *const mysql_gtid_executed_column_keys[] = {
    "PRI",
    "PRI",
    "",
    "PRI",
};

static const char *const mysql_gtid_executed_column_extras[] = {
    "",
    "",
    "",
    "",
};

static const char *const mysql_gtid_executed_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const char *const mysql_gtid_executed_column_comments[] = {
    "uuid of the source where the transaction was originally executed.",
    "First number of interval.",
    "Last number of interval.",
    "GTID Tag.",
};

static const size_t mysql_gtid_executed_primary_key_column_indexes[] = {
    0U,
    3U,
    1U,
};

static const struct mylite_execution_catalog_column_definition mysql_general_log_columns[] = {
    {"event_time",
     "CURRENT_TIMESTAMP(6)",
     "NO",
     "timestamp",
     NULL,
     NULL,
     NULL,
     NULL,
     "6",
     NULL,
     NULL,
     "timestamp(6)"},
    {"user_host",
     NULL,
     "NO",
     "mediumtext",
     "16777215",
     "16777215",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "mediumtext"},
    {"thread_id", NULL, "NO", "bigint", NULL, NULL, "20", "0", NULL, NULL, NULL, "bigint unsigned"},
    {"server_id", NULL, "NO", "int", NULL, NULL, "10", "0", NULL, NULL, NULL, "int unsigned"},
    {"command_type",
     NULL,
     "NO",
     "varchar",
     "64",
     "192",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "varchar(64)"},
    {"argument",
     NULL,
     "NO",
     "mediumblob",
     "16777215",
     "16777215",
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     "mediumblob"},
};

static const char *const mysql_general_log_column_keys[] = {
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const mysql_general_log_column_extras[] = {
    "DEFAULT_GENERATED on update CURRENT_TIMESTAMP(6)",
    "",
    "",
    "",
    "",
    "",
};

static const char *const mysql_general_log_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_column_definition mysql_slow_log_columns[] = {
    {"start_time",
     "CURRENT_TIMESTAMP(6)",
     "NO",
     "timestamp",
     NULL,
     NULL,
     NULL,
     NULL,
     "6",
     NULL,
     NULL,
     "timestamp(6)"},
    {"user_host",
     NULL,
     "NO",
     "mediumtext",
     "16777215",
     "16777215",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "mediumtext"},
    {"query_time", NULL, "NO", "time", NULL, NULL, NULL, NULL, "6", NULL, NULL, "time(6)"},
    {"lock_time", NULL, "NO", "time", NULL, NULL, NULL, NULL, "6", NULL, NULL, "time(6)"},
    {"rows_sent", NULL, "NO", "int", NULL, NULL, "10", "0", NULL, NULL, NULL, "int"},
    {"rows_examined", NULL, "NO", "int", NULL, NULL, "10", "0", NULL, NULL, NULL, "int"},
    {"db",
     NULL,
     "NO",
     "varchar",
     "512",
     "1536",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "varchar(512)"},
    {"last_insert_id", NULL, "NO", "int", NULL, NULL, "10", "0", NULL, NULL, NULL, "int"},
    {"insert_id", NULL, "NO", "int", NULL, NULL, "10", "0", NULL, NULL, NULL, "int"},
    {"server_id", NULL, "NO", "int", NULL, NULL, "10", "0", NULL, NULL, NULL, "int unsigned"},
    {"sql_text",
     NULL,
     "NO",
     "mediumblob",
     "16777215",
     "16777215",
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     "mediumblob"},
    {"thread_id", NULL, "NO", "bigint", NULL, NULL, "20", "0", NULL, NULL, NULL, "bigint unsigned"},
};

static const char *const mysql_slow_log_column_keys[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const mysql_slow_log_column_extras[] = {
    "DEFAULT_GENERATED on update CURRENT_TIMESTAMP(6)",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const mysql_slow_log_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_column_definition mysql_help_category_columns[] = {
    {"help_category_id",
     NULL,
     "NO",
     "smallint",
     NULL,
     NULL,
     "5",
     "0",
     NULL,
     NULL,
     NULL,
     "smallint unsigned"},
    {"name",
     NULL,
     "NO",
     "char",
     "64",
     "192",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "char(64)"},
    {"parent_category_id",
     NULL,
     "YES",
     "smallint",
     NULL,
     NULL,
     "5",
     "0",
     NULL,
     NULL,
     NULL,
     "smallint unsigned"},
    {"url",
     NULL,
     "NO",
     "text",
     "65535",
     "65535",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "text"},
};

static const char *const mysql_help_category_column_keys[] = {
    "PRI",
    "UNI",
    "",
    "",
};

static const char *const mysql_help_category_column_extras[] = {
    "",
    "",
    "",
    "",
};

static const char *const mysql_help_category_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_mysql_system_secondary_index
    mysql_help_category_secondary_indexes[] = {
        {"name", 1U, "53", "0", true},
};

static const struct mylite_execution_catalog_column_definition mysql_help_keyword_columns[] = {
    {"help_keyword_id", NULL, "NO", "int", NULL, NULL, "10", "0", NULL, NULL, NULL, "int unsigned"},
    {"name",
     NULL,
     "NO",
     "char",
     "64",
     "192",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "char(64)"},
};

static const char *const mysql_help_keyword_column_keys[] = {
    "PRI",
    "UNI",
};

static const char *const mysql_help_keyword_column_extras[] = {
    "",
    "",
};

static const char *const mysql_help_keyword_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_mysql_system_secondary_index
    mysql_help_keyword_secondary_indexes[] = {
        {"name", 1U, "551", "0", true},
};

static const struct mylite_execution_catalog_column_definition mysql_help_relation_columns[] = {
    {"help_topic_id", NULL, "NO", "int", NULL, NULL, "10", "0", NULL, NULL, NULL, "int unsigned"},
    {"help_keyword_id", NULL, "NO", "int", NULL, NULL, "10", "0", NULL, NULL, NULL, "int unsigned"},
};

static const char *const mysql_help_relation_column_keys[] = {
    "PRI",
    "PRI",
};

static const char *const mysql_help_relation_column_extras[] = {
    "",
    "",
};

static const char *const mysql_help_relation_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
};

static const size_t mysql_help_relation_primary_key_column_indexes[] = {
    1U,
    0U,
};

static const struct mylite_execution_catalog_column_definition mysql_help_topic_columns[] = {
    {"help_topic_id", NULL, "NO", "int", NULL, NULL, "10", "0", NULL, NULL, NULL, "int unsigned"},
    {"name",
     NULL,
     "NO",
     "char",
     "64",
     "192",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "char(64)"},
    {"help_category_id",
     NULL,
     "NO",
     "smallint",
     NULL,
     NULL,
     "5",
     "0",
     NULL,
     NULL,
     NULL,
     "smallint unsigned"},
    {"description",
     NULL,
     "NO",
     "text",
     "65535",
     "65535",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "text"},
    {"example",
     NULL,
     "NO",
     "text",
     "65535",
     "65535",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "text"},
    {"url",
     NULL,
     "NO",
     "text",
     "65535",
     "65535",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "text"},
};

static const char *const mysql_help_topic_column_keys[] = {
    "PRI",
    "UNI",
    "",
    "",
    "",
    "",
};

static const char *const mysql_help_topic_column_extras[] = {
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const mysql_help_topic_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_mysql_system_secondary_index
    mysql_help_topic_secondary_indexes[] = {
        {"name", 1U, "596", "0", true},
};

static const struct mylite_execution_catalog_column_definition mysql_ndb_binlog_index_columns[] = {
    {"Position", NULL, "NO", "bigint", NULL, NULL, "20", "0", NULL, NULL, NULL, "bigint unsigned"},
    {"File",
     NULL,
     "NO",
     "varchar",
     "255",
     "255",
     NULL,
     NULL,
     NULL,
     "latin1",
     "latin1_swedish_ci",
     "varchar(255)"},
    {"epoch", NULL, "NO", "bigint", NULL, NULL, "20", "0", NULL, NULL, NULL, "bigint unsigned"},
    {"inserts", NULL, "NO", "int", NULL, NULL, "10", "0", NULL, NULL, NULL, "int unsigned"},
    {"updates", NULL, "NO", "int", NULL, NULL, "10", "0", NULL, NULL, NULL, "int unsigned"},
    {"deletes", NULL, "NO", "int", NULL, NULL, "10", "0", NULL, NULL, NULL, "int unsigned"},
    {"schemaops", NULL, "NO", "int", NULL, NULL, "10", "0", NULL, NULL, NULL, "int unsigned"},
    {"orig_server_id", NULL, "NO", "int", NULL, NULL, "10", "0", NULL, NULL, NULL, "int unsigned"},
    {"orig_epoch",
     NULL,
     "NO",
     "bigint",
     NULL,
     NULL,
     "20",
     "0",
     NULL,
     NULL,
     NULL,
     "bigint unsigned"},
    {"gci", NULL, "NO", "int", NULL, NULL, "10", "0", NULL, NULL, NULL, "int unsigned"},
    {"next_position",
     NULL,
     "NO",
     "bigint",
     NULL,
     NULL,
     "20",
     "0",
     NULL,
     NULL,
     NULL,
     "bigint unsigned"},
    {"next_file",
     NULL,
     "NO",
     "varchar",
     "255",
     "255",
     NULL,
     NULL,
     NULL,
     "latin1",
     "latin1_swedish_ci",
     "varchar(255)"},
};

static const char *const mysql_ndb_binlog_index_column_keys[] = {
    "",
    "",
    "PRI",
    "",
    "",
    "",
    "",
    "PRI",
    "PRI",
    "",
    "",
    "",
};

static const char *const mysql_ndb_binlog_index_column_extras[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const mysql_ndb_binlog_index_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const char mysql_system_table_default_column_privileges[] =
    "select,insert,update,references";

static const struct mylite_execution_catalog_column_definition mysql_slave_master_info_columns[] = {
    {"Number_of_lines", NULL, "NO", "int", NULL, NULL, "10", "0", NULL, NULL, NULL, "int unsigned"},
    {"Master_log_name",
     NULL,
     "NO",
     "text",
     "65535",
     "65535",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_bin",
     "text"},
    {"Master_log_pos",
     NULL,
     "NO",
     "bigint",
     NULL,
     NULL,
     "20",
     "0",
     NULL,
     NULL,
     NULL,
     "bigint unsigned"},
    {"Host",
     NULL,
     "YES",
     "varchar",
     "255",
     "255",
     NULL,
     NULL,
     NULL,
     "ascii",
     "ascii_general_ci",
     "varchar(255)"},
    {"User_name",
     NULL,
     "YES",
     "text",
     "65535",
     "65535",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_bin",
     "text"},
    {"User_password",
     NULL,
     "YES",
     "text",
     "65535",
     "65535",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_bin",
     "text"},
    {"Port", NULL, "NO", "int", NULL, NULL, "10", "0", NULL, NULL, NULL, "int unsigned"},
    {"Connect_retry", NULL, "NO", "int", NULL, NULL, "10", "0", NULL, NULL, NULL, "int unsigned"},
    {"Enabled_ssl", NULL, "NO", "tinyint", NULL, NULL, "3", "0", NULL, NULL, NULL, "tinyint(1)"},
    {"Ssl_ca",
     NULL,
     "YES",
     "text",
     "65535",
     "65535",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_bin",
     "text"},
    {"Ssl_capath",
     NULL,
     "YES",
     "text",
     "65535",
     "65535",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_bin",
     "text"},
    {"Ssl_cert",
     NULL,
     "YES",
     "text",
     "65535",
     "65535",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_bin",
     "text"},
    {"Ssl_cipher",
     NULL,
     "YES",
     "text",
     "65535",
     "65535",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_bin",
     "text"},
    {"Ssl_key",
     NULL,
     "YES",
     "text",
     "65535",
     "65535",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_bin",
     "text"},
    {"Ssl_verify_server_cert",
     NULL,
     "NO",
     "tinyint",
     NULL,
     NULL,
     "3",
     "0",
     NULL,
     NULL,
     NULL,
     "tinyint(1)"},
    {"Heartbeat", NULL, "NO", "float", NULL, NULL, "12", NULL, NULL, NULL, NULL, "float"},
    {"Bind",
     NULL,
     "YES",
     "text",
     "65535",
     "65535",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_bin",
     "text"},
    {"Ignored_server_ids",
     NULL,
     "YES",
     "text",
     "65535",
     "65535",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_bin",
     "text"},
    {"Uuid",
     NULL,
     "YES",
     "text",
     "65535",
     "65535",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_bin",
     "text"},
    {"Retry_count",
     NULL,
     "NO",
     "bigint",
     NULL,
     NULL,
     "20",
     "0",
     NULL,
     NULL,
     NULL,
     "bigint unsigned"},
    {"Ssl_crl",
     NULL,
     "YES",
     "text",
     "65535",
     "65535",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_bin",
     "text"},
    {"Ssl_crlpath",
     NULL,
     "YES",
     "text",
     "65535",
     "65535",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_bin",
     "text"},
    {"Enabled_auto_position",
     NULL,
     "NO",
     "tinyint",
     NULL,
     NULL,
     "3",
     "0",
     NULL,
     NULL,
     NULL,
     "tinyint(1)"},
    {"Channel_name",
     NULL,
     "NO",
     "varchar",
     "64",
     "192",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "varchar(64)"},
    {"Tls_version",
     NULL,
     "YES",
     "text",
     "65535",
     "65535",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_bin",
     "text"},
    {"Public_key_path",
     NULL,
     "YES",
     "text",
     "65535",
     "65535",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_bin",
     "text"},
    {"Get_public_key", NULL, "NO", "tinyint", NULL, NULL, "3", "0", NULL, NULL, NULL, "tinyint(1)"},
    {"Network_namespace",
     NULL,
     "YES",
     "text",
     "65535",
     "65535",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_bin",
     "text"},
    {"Master_compression_algorithm",
     NULL,
     "NO",
     "varchar",
     "64",
     "192",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_bin",
     "varchar(64)"},
    {"Master_zstd_compression_level",
     NULL,
     "NO",
     "int",
     NULL,
     NULL,
     "10",
     "0",
     NULL,
     NULL,
     NULL,
     "int unsigned"},
    {"Tls_ciphersuites",
     NULL,
     "YES",
     "text",
     "65535",
     "65535",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_bin",
     "text"},
    {"Source_connection_auto_failover",
     "0",
     "NO",
     "tinyint",
     NULL,
     NULL,
     "3",
     "0",
     NULL,
     NULL,
     NULL,
     "tinyint(1)"},
    {"Gtid_only", "0", "NO", "tinyint", NULL, NULL, "3", "0", NULL, NULL, NULL, "tinyint(1)"},
};

static const char *const mysql_slave_master_info_column_keys[] = {
    "", "", "", "", "", "", "",    "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "PRI", "", "", "", "", "", "", "", "", "",
};

static const char *const mysql_slave_master_info_column_extras[] = {
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
};

static const char *const mysql_slave_master_info_column_privileges[] = {
    mysql_system_table_default_column_privileges, mysql_system_table_default_column_privileges,
    mysql_system_table_default_column_privileges, mysql_system_table_default_column_privileges,
    mysql_system_table_default_column_privileges, mysql_system_table_default_column_privileges,
    mysql_system_table_default_column_privileges, mysql_system_table_default_column_privileges,
    mysql_system_table_default_column_privileges, mysql_system_table_default_column_privileges,
    mysql_system_table_default_column_privileges, mysql_system_table_default_column_privileges,
    mysql_system_table_default_column_privileges, mysql_system_table_default_column_privileges,
    mysql_system_table_default_column_privileges, mysql_system_table_default_column_privileges,
    mysql_system_table_default_column_privileges, mysql_system_table_default_column_privileges,
    mysql_system_table_default_column_privileges, mysql_system_table_default_column_privileges,
    mysql_system_table_default_column_privileges, mysql_system_table_default_column_privileges,
    mysql_system_table_default_column_privileges, mysql_system_table_default_column_privileges,
    mysql_system_table_default_column_privileges, mysql_system_table_default_column_privileges,
    mysql_system_table_default_column_privileges, mysql_system_table_default_column_privileges,
    mysql_system_table_default_column_privileges, mysql_system_table_default_column_privileges,
    mysql_system_table_default_column_privileges, mysql_system_table_default_column_privileges,
    mysql_system_table_default_column_privileges,
};

static const char *const mysql_slave_master_info_column_comments[] = {
    "Number of lines in the file.",
    "The name of the master binary log currently being read from the master.",
    "The master log position of the last read event.",
    "The host name of the source.",
    "The user name used to connect to the master.",
    "The password used to connect to the master.",
    "The network port used to connect to the master.",
    "The period (in seconds) that the slave will wait before trying to reconnect to the master.",
    "Indicates whether the server supports SSL connections.",
    "The file used for the Certificate Authority (CA) certificate.",
    "The path to the Certificate Authority (CA) certificates.",
    "The name of the SSL certificate file.",
    "The name of the cipher in use for the SSL connection.",
    "The name of the SSL key file.",
    "Whether to verify the server certificate.",
    "",
    "Displays which interface is employed when connecting to the MySQL server",
    "The number of server IDs to be ignored, followed by the actual server IDs",
    "The master server uuid.",
    "Number of reconnect attempts, to the master, before giving up.",
    "The file used for the Certificate Revocation List (CRL)",
    "The path used for Certificate Revocation List (CRL) files",
    "Indicates whether GTIDs will be used to retrieve events from the master.",
    "The channel on which the replica is connected to a source. Used in Multisource Replication",
    "Tls version",
    "The file containing public key of master server.",
    "Preference to get public key from master.",
    "Network namespace used for communication with the master server.",
    "Compression algorithm supported for data transfer between source and replica.",
    "Compression level associated with zstd compression algorithm.",
    "Ciphersuites used for TLS 1.3 communication with the master server.",
    "Indicates whether the channel connection failover is enabled.",
    "Indicates if this channel only uses GTIDs and does not persist positions.",
};

static const struct mylite_execution_catalog_column_definition
    mysql_slave_relay_log_info_columns[] = {
        {"Number_of_lines",
         NULL,
         "NO",
         "int",
         NULL,
         NULL,
         "10",
         "0",
         NULL,
         NULL,
         NULL,
         "int unsigned"},
        {"Relay_log_name",
         NULL,
         "YES",
         "text",
         "65535",
         "65535",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_bin",
         "text"},
        {"Relay_log_pos",
         NULL,
         "YES",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"Master_log_name",
         NULL,
         "YES",
         "text",
         "65535",
         "65535",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_bin",
         "text"},
        {"Master_log_pos",
         NULL,
         "YES",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"Sql_delay", NULL, "YES", "int", NULL, NULL, "10", "0", NULL, NULL, NULL, "int"},
        {"Number_of_workers",
         NULL,
         "YES",
         "int",
         NULL,
         NULL,
         "10",
         "0",
         NULL,
         NULL,
         NULL,
         "int unsigned"},
        {"Id", NULL, "YES", "int", NULL, NULL, "10", "0", NULL, NULL, NULL, "int unsigned"},
        {"Channel_name",
         NULL,
         "NO",
         "varchar",
         "64",
         "192",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "varchar(64)"},
        {"Privilege_checks_username",
         NULL,
         "YES",
         "varchar",
         "32",
         "96",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_bin",
         "varchar(32)"},
        {"Privilege_checks_hostname",
         NULL,
         "YES",
         "varchar",
         "255",
         "255",
         NULL,
         NULL,
         NULL,
         "ascii",
         "ascii_general_ci",
         "varchar(255)"},
        {"Require_row_format",
         NULL,
         "NO",
         "tinyint",
         NULL,
         NULL,
         "3",
         "0",
         NULL,
         NULL,
         NULL,
         "tinyint(1)"},
        {"Require_table_primary_key_check",
         "STREAM",
         "NO",
         "enum",
         "8",
         "24",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "enum('STREAM','ON','OFF','GENERATE')"},
        {"Assign_gtids_to_anonymous_transactions_type",
         "OFF",
         "NO",
         "enum",
         "5",
         "15",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "enum('OFF','LOCAL','UUID')"},
        {"Assign_gtids_to_anonymous_transactions_value",
         NULL,
         "YES",
         "text",
         "65535",
         "65535",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_bin",
         "text"},
};

static const char *const mysql_slave_relay_log_info_column_keys[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "PRI",
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const mysql_slave_relay_log_info_column_extras[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const mysql_slave_relay_log_info_column_privileges[] = {
    mysql_system_table_default_column_privileges,
    mysql_system_table_default_column_privileges,
    mysql_system_table_default_column_privileges,
    mysql_system_table_default_column_privileges,
    mysql_system_table_default_column_privileges,
    mysql_system_table_default_column_privileges,
    mysql_system_table_default_column_privileges,
    mysql_system_table_default_column_privileges,
    mysql_system_table_default_column_privileges,
    mysql_system_table_default_column_privileges,
    mysql_system_table_default_column_privileges,
    mysql_system_table_default_column_privileges,
    mysql_system_table_default_column_privileges,
    mysql_system_table_default_column_privileges,
    mysql_system_table_default_column_privileges,
};

static const char *const mysql_slave_relay_log_info_column_comments[] = {
    "Number of lines in the file or rows in the table. Used to version table definitions.",
    "The name of the current relay log file.",
    "The relay log position of the last executed event.",
    "The name of the master binary log file from which the events in the relay log file were read.",
    "The master log position of the last executed event.",
    "The number of seconds that the slave must lag behind the master.",
    "",
    "Internal Id that uniquely identifies this record.",
    "The channel on which the replica is connected to a source. Used in Multisource Replication",
    "Username part of PRIVILEGE_CHECKS_USER.",
    "Hostname part of PRIVILEGE_CHECKS_USER.",
    "Indicates whether the channel shall only accept row based events.",
    ("Indicates what is the channel policy regarding tables without primary keys on create and "
     "alter table queries"),
    ("Indicates whether the channel will generate a new GTID for anonymous transactions. OFF means "
     "that anonymous transactions will remain anonymous. LOCAL means that anonymous transactions "
     "will be assigned a newly generated GTID based on server_uuid. UUID indicates that anonymous "
     "transactions will be assigned a newly generated GTID based on "
     "Assign_gtids_to_anonymous_transactions_value"),
    "Indicates the UUID used while generating GTIDs for anonymous transactions",
};

static const struct mylite_execution_catalog_column_definition mysql_slave_worker_info_columns[] = {
    {"Id", NULL, "NO", "int", NULL, NULL, "10", "0", NULL, NULL, NULL, "int unsigned"},
    {"Relay_log_name",
     NULL,
     "NO",
     "text",
     "65535",
     "65535",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_bin",
     "text"},
    {"Relay_log_pos",
     NULL,
     "NO",
     "bigint",
     NULL,
     NULL,
     "20",
     "0",
     NULL,
     NULL,
     NULL,
     "bigint unsigned"},
    {"Master_log_name",
     NULL,
     "NO",
     "text",
     "65535",
     "65535",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_bin",
     "text"},
    {"Master_log_pos",
     NULL,
     "NO",
     "bigint",
     NULL,
     NULL,
     "20",
     "0",
     NULL,
     NULL,
     NULL,
     "bigint unsigned"},
    {"Checkpoint_relay_log_name",
     NULL,
     "NO",
     "text",
     "65535",
     "65535",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_bin",
     "text"},
    {"Checkpoint_relay_log_pos",
     NULL,
     "NO",
     "bigint",
     NULL,
     NULL,
     "20",
     "0",
     NULL,
     NULL,
     NULL,
     "bigint unsigned"},
    {"Checkpoint_master_log_name",
     NULL,
     "NO",
     "text",
     "65535",
     "65535",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_bin",
     "text"},
    {"Checkpoint_master_log_pos",
     NULL,
     "NO",
     "bigint",
     NULL,
     NULL,
     "20",
     "0",
     NULL,
     NULL,
     NULL,
     "bigint unsigned"},
    {"Checkpoint_seqno",
     NULL,
     "NO",
     "int",
     NULL,
     NULL,
     "10",
     "0",
     NULL,
     NULL,
     NULL,
     "int unsigned"},
    {"Checkpoint_group_size",
     NULL,
     "NO",
     "int",
     NULL,
     NULL,
     "10",
     "0",
     NULL,
     NULL,
     NULL,
     "int unsigned"},
    {"Checkpoint_group_bitmap",
     NULL,
     "NO",
     "blob",
     "65535",
     "65535",
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     "blob"},
    {"Channel_name",
     NULL,
     "NO",
     "varchar",
     "64",
     "192",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "varchar(64)"},
};

static const char *const mysql_slave_worker_info_column_keys[] = {
    "PRI",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "PRI",
};

static const char *const mysql_slave_worker_info_column_extras[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const mysql_slave_worker_info_column_privileges[] = {
    mysql_system_table_default_column_privileges,
    mysql_system_table_default_column_privileges,
    mysql_system_table_default_column_privileges,
    mysql_system_table_default_column_privileges,
    mysql_system_table_default_column_privileges,
    mysql_system_table_default_column_privileges,
    mysql_system_table_default_column_privileges,
    mysql_system_table_default_column_privileges,
    mysql_system_table_default_column_privileges,
    mysql_system_table_default_column_privileges,
    mysql_system_table_default_column_privileges,
    mysql_system_table_default_column_privileges,
    mysql_system_table_default_column_privileges,
};

static const char *const mysql_slave_worker_info_column_comments[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "The channel on which the replica is connected to a source. Used in Multisource Replication",
};

static const size_t mysql_slave_worker_info_primary_key_column_indexes[] = {
    12U,
    0U,
};

static const struct mylite_execution_catalog_column_definition mysql_plugin_columns[] = {
    {"name",
     "",
     "NO",
     "varchar",
     "64",
     "192",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "varchar(64)"},
    {"dl",
     "",
     "NO",
     "varchar",
     "128",
     "384",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "varchar(128)"},
};

static const char *const mysql_plugin_column_keys[] = {
    "PRI",
    "",
};

static const char *const mysql_plugin_column_extras[] = {
    "",
    "",
};

static const char *const mysql_plugin_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_column_definition mysql_engine_cost_columns[] = {
    {"engine_name",
     NULL,
     "NO",
     "varchar",
     "64",
     "192",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "varchar(64)"},
    {"device_type", NULL, "NO", "int", NULL, NULL, "10", "0", NULL, NULL, NULL, "int"},
    {"cost_name",
     NULL,
     "NO",
     "varchar",
     "64",
     "192",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "varchar(64)"},
    {"cost_value", NULL, "YES", "float", NULL, NULL, "12", NULL, NULL, NULL, NULL, "float"},
    {"last_update",
     "CURRENT_TIMESTAMP",
     "NO",
     "timestamp",
     NULL,
     NULL,
     NULL,
     NULL,
     "0",
     NULL,
     NULL,
     "timestamp"},
    {"comment",
     NULL,
     "YES",
     "varchar",
     "1024",
     "3072",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "varchar(1024)"},
    {"default_value", NULL, "YES", "float", NULL, NULL, "12", NULL, NULL, NULL, NULL, "float"},
};

static const char *const mysql_engine_cost_column_keys[] = {
    "PRI",
    "PRI",
    "PRI",
    "",
    "",
    "",
    "",
};

static const char *const mysql_engine_cost_column_extras[] = {
    "",
    "",
    "",
    "",
    "DEFAULT_GENERATED on update CURRENT_TIMESTAMP",
    "",
    "VIRTUAL GENERATED",
};

static const char *const mysql_engine_cost_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const char *const mysql_engine_cost_column_generation_expressions[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    ("(case `cost_name` when _utf8mb4\\'io_block_read_cost\\' then 1.0 when "
     "_utf8mb4\\'memory_block_read_cost\\' then 0.25 else NULL end)"),
};

static const size_t mysql_engine_cost_primary_key_column_indexes[] = {
    2U,
    0U,
    1U,
};

static const struct mylite_execution_catalog_column_definition mysql_server_cost_columns[] = {
    {"cost_name",
     NULL,
     "NO",
     "varchar",
     "64",
     "192",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "varchar(64)"},
    {"cost_value", NULL, "YES", "float", NULL, NULL, "12", NULL, NULL, NULL, NULL, "float"},
    {"last_update",
     "CURRENT_TIMESTAMP",
     "NO",
     "timestamp",
     NULL,
     NULL,
     NULL,
     NULL,
     "0",
     NULL,
     NULL,
     "timestamp"},
    {"comment",
     NULL,
     "YES",
     "varchar",
     "1024",
     "3072",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "varchar(1024)"},
    {"default_value", NULL, "YES", "float", NULL, NULL, "12", NULL, NULL, NULL, NULL, "float"},
};

static const char *const mysql_server_cost_column_keys[] = {
    "PRI",
    "",
    "",
    "",
    "",
};

static const char *const mysql_server_cost_column_extras[] = {
    "",
    "",
    "DEFAULT_GENERATED on update CURRENT_TIMESTAMP",
    "",
    "VIRTUAL GENERATED",
};

static const char *const mysql_server_cost_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const char *const mysql_server_cost_column_generation_expressions[] = {
    "",
    "",
    "",
    "",
    ("(case `cost_name` when _utf8mb4\\'disk_temptable_create_cost\\' then 20.0 "
     "when _utf8mb4\\'disk_temptable_row_cost\\' then 0.5 when "
     "_utf8mb4\\'key_compare_cost\\' then 0.05 when "
     "_utf8mb4\\'memory_temptable_create_cost\\' then 1.0 when "
     "_utf8mb4\\'memory_temptable_row_cost\\' then 0.1 when "
     "_utf8mb4\\'row_evaluate_cost\\' then 0.1 else NULL end)"),
};

static const struct mylite_execution_catalog_column_definition mysql_servers_columns[] = {
    {"Server_name",
     "",
     "NO",
     "char",
     "64",
     "192",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "char(64)"},
    {"Host",
     "",
     "NO",
     "char",
     "255",
     "255",
     NULL,
     NULL,
     NULL,
     "ascii",
     "ascii_general_ci",
     "char(255)"},
    {"Db",
     "",
     "NO",
     "char",
     "64",
     "192",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "char(64)"},
    {"Username",
     "",
     "NO",
     "char",
     "64",
     "192",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "char(64)"},
    {"Password",
     "",
     "NO",
     "char",
     "64",
     "192",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "char(64)"},
    {"Port", "0", "NO", "int", NULL, NULL, "10", "0", NULL, NULL, NULL, "int"},
    {"Socket",
     "",
     "NO",
     "char",
     "64",
     "192",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "char(64)"},
    {"Wrapper",
     "",
     "NO",
     "char",
     "64",
     "192",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "char(64)"},
    {"Owner",
     "",
     "NO",
     "char",
     "64",
     "192",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "char(64)"},
};

static const char *const mysql_servers_column_keys[] = {
    "PRI",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const mysql_servers_column_extras[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const mysql_servers_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_column_definition mysql_time_zone_columns[] = {
    {"Time_zone_id", NULL, "NO", "int", NULL, NULL, "10", "0", NULL, NULL, NULL, "int unsigned"},
    {"Use_leap_seconds",
     "N",
     "NO",
     "enum",
     "1",
     "3",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "enum('Y','N')"},
};

static const char *const mysql_time_zone_column_keys[] = {
    "PRI",
    "",
};

static const char *const mysql_time_zone_column_extras[] = {
    "auto_increment",
    "",
};

static const char *const mysql_time_zone_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_column_definition
    mysql_time_zone_leap_second_columns[] = {
        {"Transition_time",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "19",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint"},
        {"Correction", NULL, "NO", "int", NULL, NULL, "10", "0", NULL, NULL, NULL, "int"},
};

static const char *const mysql_time_zone_leap_second_column_keys[] = {
    "PRI",
    "",
};

static const char *const mysql_time_zone_leap_second_column_extras[] = {
    "",
    "",
};

static const char *const mysql_time_zone_leap_second_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_column_definition mysql_time_zone_name_columns[] = {
    {"Name",
     NULL,
     "NO",
     "char",
     "64",
     "192",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "char(64)"},
    {"Time_zone_id", NULL, "NO", "int", NULL, NULL, "10", "0", NULL, NULL, NULL, "int unsigned"},
};

static const char *const mysql_time_zone_name_column_keys[] = {
    "PRI",
    "",
};

static const char *const mysql_time_zone_name_column_extras[] = {
    "",
    "",
};

static const char *const mysql_time_zone_name_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_column_definition
    mysql_time_zone_transition_columns[] = {
        {"Time_zone_id",
         NULL,
         "NO",
         "int",
         NULL,
         NULL,
         "10",
         "0",
         NULL,
         NULL,
         NULL,
         "int unsigned"},
        {"Transition_time",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "19",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint"},
        {"Transition_type_id",
         NULL,
         "NO",
         "int",
         NULL,
         NULL,
         "10",
         "0",
         NULL,
         NULL,
         NULL,
         "int unsigned"},
};

static const char *const mysql_time_zone_transition_column_keys[] = {
    "PRI",
    "PRI",
    "",
};

static const char *const mysql_time_zone_transition_column_extras[] = {
    "",
    "",
    "",
};

static const char *const mysql_time_zone_transition_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_column_definition
    mysql_time_zone_transition_type_columns[] = {
        {"Time_zone_id",
         NULL,
         "NO",
         "int",
         NULL,
         NULL,
         "10",
         "0",
         NULL,
         NULL,
         NULL,
         "int unsigned"},
        {"Transition_type_id",
         NULL,
         "NO",
         "int",
         NULL,
         NULL,
         "10",
         "0",
         NULL,
         NULL,
         NULL,
         "int unsigned"},
        {"Offset", "0", "NO", "int", NULL, NULL, "10", "0", NULL, NULL, NULL, "int"},
        {"Is_DST",
         "0",
         "NO",
         "tinyint",
         NULL,
         NULL,
         "3",
         "0",
         NULL,
         NULL,
         NULL,
         "tinyint unsigned"},
        {"Abbreviation",
         "",
         "NO",
         "char",
         "8",
         "24",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_general_ci",
         "char(8)"},
};

static const char *const mysql_time_zone_transition_type_column_keys[] = {
    "PRI",
    "PRI",
    "",
    "",
    "",
};

static const char *const mysql_time_zone_transition_type_column_extras[] = {
    "",
    "",
    "",
    "",
    "",
};

static const char *const mysql_time_zone_transition_type_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_column_definition mysql_innodb_index_stats_columns[] =
    {
        {"database_name",
         NULL,
         "NO",
         "varchar",
         "64",
         "192",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_bin",
         "varchar(64)"},
        {"table_name",
         NULL,
         "NO",
         "varchar",
         "199",
         "597",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_bin",
         "varchar(199)"},
        {"index_name",
         NULL,
         "NO",
         "varchar",
         "64",
         "192",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_bin",
         "varchar(64)"},
        {"last_update",
         "CURRENT_TIMESTAMP",
         "NO",
         "timestamp",
         NULL,
         NULL,
         NULL,
         NULL,
         "0",
         NULL,
         NULL,
         "timestamp"},
        {"stat_name",
         NULL,
         "NO",
         "varchar",
         "64",
         "192",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_bin",
         "varchar(64)"},
        {"stat_value",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"sample_size",
         NULL,
         "YES",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"stat_description",
         NULL,
         "NO",
         "varchar",
         "1024",
         "3072",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_bin",
         "varchar(1024)"},
};

static const char *const mysql_innodb_index_stats_column_keys[] = {
    "PRI",
    "PRI",
    "PRI",
    "",
    "PRI",
    "",
    "",
    "",
};

static const char *const mysql_innodb_index_stats_column_extras[] = {
    "",
    "",
    "",
    "DEFAULT_GENERATED on update CURRENT_TIMESTAMP",
    "",
    "",
    "",
    "",
};

static const char *const mysql_innodb_index_stats_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_column_definition mysql_innodb_table_stats_columns[] =
    {
        {"database_name",
         NULL,
         "NO",
         "varchar",
         "64",
         "192",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_bin",
         "varchar(64)"},
        {"table_name",
         NULL,
         "NO",
         "varchar",
         "199",
         "597",
         NULL,
         NULL,
         NULL,
         "utf8mb3",
         "utf8mb3_bin",
         "varchar(199)"},
        {"last_update",
         "CURRENT_TIMESTAMP",
         "NO",
         "timestamp",
         NULL,
         NULL,
         NULL,
         NULL,
         "0",
         NULL,
         NULL,
         "timestamp"},
        {"n_rows",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"clustered_index_size",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
        {"sum_of_other_index_sizes",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
};

static const char *const mysql_innodb_table_stats_column_keys[] = {
    "PRI",
    "PRI",
    "",
    "",
    "",
    "",
};

static const char *const mysql_innodb_table_stats_column_extras[] = {
    "",
    "",
    "DEFAULT_GENERATED on update CURRENT_TIMESTAMP",
    "",
    "",
    "",
};

static const char *const mysql_innodb_table_stats_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_mysql_system_table mysql_system_table_definitions[] = {
    {"mysql",
     {MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_USER,
      "user",
      mysql_user_columns,
      sizeof(mysql_user_columns) / sizeof(mysql_user_columns[0])},
     mysql_user_column_keys,
     mysql_user_column_extras,
     mysql_user_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"mysql",
     {MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_GLOBAL_GRANTS,
      "global_grants",
      mysql_global_grants_columns,
      sizeof(mysql_global_grants_columns) / sizeof(mysql_global_grants_columns[0])},
     mysql_global_grants_column_keys,
     mysql_global_grants_column_extras,
     mysql_global_grants_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"mysql",
     {MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_DB,
      "db",
      mysql_db_columns,
      sizeof(mysql_db_columns) / sizeof(mysql_db_columns[0])},
     mysql_db_column_keys,
     mysql_db_column_extras,
     mysql_db_column_privileges,
     NULL,
     mysql_db_primary_key_column_indexes,
     sizeof(mysql_db_primary_key_column_indexes) / sizeof(mysql_db_primary_key_column_indexes[0]),
     NULL,
     mysql_db_secondary_indexes,
     sizeof(mysql_db_secondary_indexes) / sizeof(mysql_db_secondary_indexes[0])},
    {"mysql",
     {MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_TABLES_PRIV,
      "tables_priv",
      mysql_tables_priv_columns,
      sizeof(mysql_tables_priv_columns) / sizeof(mysql_tables_priv_columns[0])},
     mysql_tables_priv_column_keys,
     mysql_tables_priv_column_extras,
     mysql_tables_priv_column_privileges,
     NULL,
     mysql_tables_priv_primary_key_column_indexes,
     sizeof(mysql_tables_priv_primary_key_column_indexes) /
         sizeof(mysql_tables_priv_primary_key_column_indexes[0]),
     NULL,
     mysql_tables_priv_secondary_indexes,
     sizeof(mysql_tables_priv_secondary_indexes) / sizeof(mysql_tables_priv_secondary_indexes[0])},
    {"mysql",
     {MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_COLUMNS_PRIV,
      "columns_priv",
      mysql_columns_priv_columns,
      sizeof(mysql_columns_priv_columns) / sizeof(mysql_columns_priv_columns[0])},
     mysql_columns_priv_column_keys,
     mysql_columns_priv_column_extras,
     mysql_columns_priv_column_privileges,
     NULL,
     mysql_columns_priv_primary_key_column_indexes,
     sizeof(mysql_columns_priv_primary_key_column_indexes) /
         sizeof(mysql_columns_priv_primary_key_column_indexes[0]),
     NULL,
     NULL,
     0U},
    {"mysql",
     {MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_PROCS_PRIV,
      "procs_priv",
      mysql_procs_priv_columns,
      sizeof(mysql_procs_priv_columns) / sizeof(mysql_procs_priv_columns[0])},
     mysql_procs_priv_column_keys,
     mysql_procs_priv_column_extras,
     mysql_procs_priv_column_privileges,
     NULL,
     mysql_procs_priv_primary_key_column_indexes,
     sizeof(mysql_procs_priv_primary_key_column_indexes) /
         sizeof(mysql_procs_priv_primary_key_column_indexes[0]),
     NULL,
     mysql_procs_priv_secondary_indexes,
     sizeof(mysql_procs_priv_secondary_indexes) / sizeof(mysql_procs_priv_secondary_indexes[0])},
    {"mysql",
     {MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_PROXIES_PRIV,
      "proxies_priv",
      mysql_proxies_priv_columns,
      sizeof(mysql_proxies_priv_columns) / sizeof(mysql_proxies_priv_columns[0])},
     mysql_proxies_priv_column_keys,
     mysql_proxies_priv_column_extras,
     mysql_proxies_priv_column_privileges,
     NULL,
     mysql_proxies_priv_primary_key_column_indexes,
     sizeof(mysql_proxies_priv_primary_key_column_indexes) /
         sizeof(mysql_proxies_priv_primary_key_column_indexes[0]),
     NULL,
     mysql_proxies_priv_secondary_indexes,
     sizeof(mysql_proxies_priv_secondary_indexes) /
         sizeof(mysql_proxies_priv_secondary_indexes[0])},
    {"mysql",
     {MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_DEFAULT_ROLES,
      "default_roles",
      mysql_default_roles_columns,
      sizeof(mysql_default_roles_columns) / sizeof(mysql_default_roles_columns[0])},
     mysql_default_roles_column_keys,
     mysql_default_roles_column_extras,
     mysql_default_roles_column_privileges,
     NULL,
     mysql_default_roles_primary_key_column_indexes,
     sizeof(mysql_default_roles_primary_key_column_indexes) /
         sizeof(mysql_default_roles_primary_key_column_indexes[0]),
     NULL,
     NULL,
     0U},
    {"mysql",
     {MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_ROLE_EDGES,
      "role_edges",
      mysql_role_edges_columns,
      sizeof(mysql_role_edges_columns) / sizeof(mysql_role_edges_columns[0])},
     mysql_role_edges_column_keys,
     mysql_role_edges_column_extras,
     mysql_role_edges_column_privileges,
     NULL,
     mysql_role_edges_primary_key_column_indexes,
     sizeof(mysql_role_edges_primary_key_column_indexes) /
         sizeof(mysql_role_edges_primary_key_column_indexes[0]),
     NULL,
     NULL,
     0U},
    {"mysql",
     {MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_PASSWORD_HISTORY,
      "password_history",
      mysql_password_history_columns,
      sizeof(mysql_password_history_columns) / sizeof(mysql_password_history_columns[0])},
     mysql_password_history_column_keys,
     mysql_password_history_column_extras,
     mysql_password_history_column_privileges,
     NULL,
     mysql_password_history_primary_key_column_indexes,
     sizeof(mysql_password_history_primary_key_column_indexes) /
         sizeof(mysql_password_history_primary_key_column_indexes[0]),
     NULL,
     NULL,
     0U},
    {"mysql",
     {MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_COMPONENT,
      "component",
      mysql_component_columns,
      sizeof(mysql_component_columns) / sizeof(mysql_component_columns[0])},
     mysql_component_column_keys,
     mysql_component_column_extras,
     mysql_component_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"mysql",
     {MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_ENGINE_COST,
      "engine_cost",
      mysql_engine_cost_columns,
      sizeof(mysql_engine_cost_columns) / sizeof(mysql_engine_cost_columns[0])},
     mysql_engine_cost_column_keys,
     mysql_engine_cost_column_extras,
     mysql_engine_cost_column_privileges,
     NULL,
     mysql_engine_cost_primary_key_column_indexes,
     sizeof(mysql_engine_cost_primary_key_column_indexes) /
         sizeof(mysql_engine_cost_primary_key_column_indexes[0]),
     mysql_engine_cost_column_generation_expressions,
     NULL,
     0U},
    {"mysql",
     {MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_FUNC,
      "func",
      mysql_func_columns,
      sizeof(mysql_func_columns) / sizeof(mysql_func_columns[0])},
     mysql_func_column_keys,
     mysql_func_column_extras,
     mysql_func_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"mysql",
     {MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_GTID_EXECUTED,
      "gtid_executed",
      mysql_gtid_executed_columns,
      sizeof(mysql_gtid_executed_columns) / sizeof(mysql_gtid_executed_columns[0])},
     mysql_gtid_executed_column_keys,
     mysql_gtid_executed_column_extras,
     mysql_gtid_executed_column_privileges,
     mysql_gtid_executed_column_comments,
     mysql_gtid_executed_primary_key_column_indexes,
     sizeof(mysql_gtid_executed_primary_key_column_indexes) /
         sizeof(mysql_gtid_executed_primary_key_column_indexes[0]),
     NULL,
     NULL,
     0U},
    {"mysql",
     {MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_GENERAL_LOG,
      "general_log",
      mysql_general_log_columns,
      sizeof(mysql_general_log_columns) / sizeof(mysql_general_log_columns[0])},
     mysql_general_log_column_keys,
     mysql_general_log_column_extras,
     mysql_general_log_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"mysql",
     {MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_SLOW_LOG,
      "slow_log",
      mysql_slow_log_columns,
      sizeof(mysql_slow_log_columns) / sizeof(mysql_slow_log_columns[0])},
     mysql_slow_log_column_keys,
     mysql_slow_log_column_extras,
     mysql_slow_log_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"mysql",
     {MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_HELP_CATEGORY,
      "help_category",
      mysql_help_category_columns,
      sizeof(mysql_help_category_columns) / sizeof(mysql_help_category_columns[0])},
     mysql_help_category_column_keys,
     mysql_help_category_column_extras,
     mysql_help_category_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     mysql_help_category_secondary_indexes,
     sizeof(mysql_help_category_secondary_indexes) /
         sizeof(mysql_help_category_secondary_indexes[0])},
    {"mysql",
     {MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_HELP_KEYWORD,
      "help_keyword",
      mysql_help_keyword_columns,
      sizeof(mysql_help_keyword_columns) / sizeof(mysql_help_keyword_columns[0])},
     mysql_help_keyword_column_keys,
     mysql_help_keyword_column_extras,
     mysql_help_keyword_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     mysql_help_keyword_secondary_indexes,
     sizeof(mysql_help_keyword_secondary_indexes) /
         sizeof(mysql_help_keyword_secondary_indexes[0])},
    {"mysql",
     {MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_HELP_RELATION,
      "help_relation",
      mysql_help_relation_columns,
      sizeof(mysql_help_relation_columns) / sizeof(mysql_help_relation_columns[0])},
     mysql_help_relation_column_keys,
     mysql_help_relation_column_extras,
     mysql_help_relation_column_privileges,
     NULL,
     mysql_help_relation_primary_key_column_indexes,
     sizeof(mysql_help_relation_primary_key_column_indexes) /
         sizeof(mysql_help_relation_primary_key_column_indexes[0]),
     NULL,
     NULL,
     0U},
    {"mysql",
     {MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_HELP_TOPIC,
      "help_topic",
      mysql_help_topic_columns,
      sizeof(mysql_help_topic_columns) / sizeof(mysql_help_topic_columns[0])},
     mysql_help_topic_column_keys,
     mysql_help_topic_column_extras,
     mysql_help_topic_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     mysql_help_topic_secondary_indexes,
     sizeof(mysql_help_topic_secondary_indexes) / sizeof(mysql_help_topic_secondary_indexes[0])},
    {"mysql",
     {MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_NDB_BINLOG_INDEX,
      "ndb_binlog_index",
      mysql_ndb_binlog_index_columns,
      sizeof(mysql_ndb_binlog_index_columns) / sizeof(mysql_ndb_binlog_index_columns[0])},
     mysql_ndb_binlog_index_column_keys,
     mysql_ndb_binlog_index_column_extras,
     mysql_ndb_binlog_index_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"mysql",
     {MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_SLAVE_MASTER_INFO,
      "slave_master_info",
      mysql_slave_master_info_columns,
      sizeof(mysql_slave_master_info_columns) / sizeof(mysql_slave_master_info_columns[0])},
     mysql_slave_master_info_column_keys,
     mysql_slave_master_info_column_extras,
     mysql_slave_master_info_column_privileges,
     mysql_slave_master_info_column_comments,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"mysql",
     {MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_SLAVE_RELAY_LOG_INFO,
      "slave_relay_log_info",
      mysql_slave_relay_log_info_columns,
      sizeof(mysql_slave_relay_log_info_columns) / sizeof(mysql_slave_relay_log_info_columns[0])},
     mysql_slave_relay_log_info_column_keys,
     mysql_slave_relay_log_info_column_extras,
     mysql_slave_relay_log_info_column_privileges,
     mysql_slave_relay_log_info_column_comments,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"mysql",
     {MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_SLAVE_WORKER_INFO,
      "slave_worker_info",
      mysql_slave_worker_info_columns,
      sizeof(mysql_slave_worker_info_columns) / sizeof(mysql_slave_worker_info_columns[0])},
     mysql_slave_worker_info_column_keys,
     mysql_slave_worker_info_column_extras,
     mysql_slave_worker_info_column_privileges,
     mysql_slave_worker_info_column_comments,
     mysql_slave_worker_info_primary_key_column_indexes,
     sizeof(mysql_slave_worker_info_primary_key_column_indexes) /
         sizeof(mysql_slave_worker_info_primary_key_column_indexes[0]),
     NULL,
     NULL,
     0U},
    {"mysql",
     {MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_PLUGIN,
      "plugin",
      mysql_plugin_columns,
      sizeof(mysql_plugin_columns) / sizeof(mysql_plugin_columns[0])},
     mysql_plugin_column_keys,
     mysql_plugin_column_extras,
     mysql_plugin_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"mysql",
     {MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_SERVER_COST,
      "server_cost",
      mysql_server_cost_columns,
      sizeof(mysql_server_cost_columns) / sizeof(mysql_server_cost_columns[0])},
     mysql_server_cost_column_keys,
     mysql_server_cost_column_extras,
     mysql_server_cost_column_privileges,
     NULL,
     NULL,
     0U,
     mysql_server_cost_column_generation_expressions,
     NULL,
     0U},
    {"mysql",
     {MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_SERVERS,
      "servers",
      mysql_servers_columns,
      sizeof(mysql_servers_columns) / sizeof(mysql_servers_columns[0])},
     mysql_servers_column_keys,
     mysql_servers_column_extras,
     mysql_servers_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"mysql",
     {MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_TIME_ZONE,
      "time_zone",
      mysql_time_zone_columns,
      sizeof(mysql_time_zone_columns) / sizeof(mysql_time_zone_columns[0])},
     mysql_time_zone_column_keys,
     mysql_time_zone_column_extras,
     mysql_time_zone_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"mysql",
     {MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_TIME_ZONE_LEAP_SECOND,
      "time_zone_leap_second",
      mysql_time_zone_leap_second_columns,
      sizeof(mysql_time_zone_leap_second_columns) / sizeof(mysql_time_zone_leap_second_columns[0])},
     mysql_time_zone_leap_second_column_keys,
     mysql_time_zone_leap_second_column_extras,
     mysql_time_zone_leap_second_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"mysql",
     {MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_TIME_ZONE_NAME,
      "time_zone_name",
      mysql_time_zone_name_columns,
      sizeof(mysql_time_zone_name_columns) / sizeof(mysql_time_zone_name_columns[0])},
     mysql_time_zone_name_column_keys,
     mysql_time_zone_name_column_extras,
     mysql_time_zone_name_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"mysql",
     {MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_TIME_ZONE_TRANSITION,
      "time_zone_transition",
      mysql_time_zone_transition_columns,
      sizeof(mysql_time_zone_transition_columns) / sizeof(mysql_time_zone_transition_columns[0])},
     mysql_time_zone_transition_column_keys,
     mysql_time_zone_transition_column_extras,
     mysql_time_zone_transition_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"mysql",
     {MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_TIME_ZONE_TRANSITION_TYPE,
      "time_zone_transition_type",
      mysql_time_zone_transition_type_columns,
      sizeof(mysql_time_zone_transition_type_columns) /
          sizeof(mysql_time_zone_transition_type_columns[0])},
     mysql_time_zone_transition_type_column_keys,
     mysql_time_zone_transition_type_column_extras,
     mysql_time_zone_transition_type_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"mysql",
     {MYLITE_EXECUTION_CATALOG_TABLE_INNODB_INDEXES,
      "innodb_index_stats",
      mysql_innodb_index_stats_columns,
      sizeof(mysql_innodb_index_stats_columns) / sizeof(mysql_innodb_index_stats_columns[0])},
     mysql_innodb_index_stats_column_keys,
     mysql_innodb_index_stats_column_extras,
     mysql_innodb_index_stats_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"mysql",
     {MYLITE_EXECUTION_CATALOG_TABLE_INNODB_TABLESTATS,
      "innodb_table_stats",
      mysql_innodb_table_stats_columns,
      sizeof(mysql_innodb_table_stats_columns) / sizeof(mysql_innodb_table_stats_columns[0])},
     mysql_innodb_table_stats_column_keys,
     mysql_innodb_table_stats_column_extras,
     mysql_innodb_table_stats_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_SYS_CONFIG,
      "sys_config",
      sys_sys_config_columns,
      sizeof(sys_sys_config_columns) / sizeof(sys_sys_config_columns[0])},
     sys_sys_config_column_keys,
     sys_sys_config_column_extras,
     sys_sys_config_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_VERSION,
      "version",
      sys_version_columns,
      sizeof(sys_version_columns) / sizeof(sys_version_columns[0])},
     sys_version_column_keys,
     sys_version_column_extras,
     sys_version_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_HOST_SUMMARY,
      "host_summary",
      sys_host_summary_columns,
      sizeof(sys_host_summary_columns) / sizeof(sys_host_summary_columns[0])},
     sys_host_summary_column_keys,
     sys_host_summary_column_extras,
     sys_host_summary_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_HOST_SUMMARY_BY_FILE_IO,
      "host_summary_by_file_io",
      sys_host_summary_by_file_io_columns,
      sizeof(sys_host_summary_by_file_io_columns) / sizeof(sys_host_summary_by_file_io_columns[0])},
     sys_host_summary_by_file_io_column_keys,
     sys_host_summary_by_file_io_column_extras,
     sys_host_summary_by_file_io_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_HOST_SUMMARY_BY_FILE_IO_TYPE,
      "host_summary_by_file_io_type",
      sys_host_summary_by_file_io_type_columns,
      sizeof(sys_host_summary_by_file_io_type_columns) /
          sizeof(sys_host_summary_by_file_io_type_columns[0])},
     sys_host_summary_by_file_io_type_column_keys,
     sys_host_summary_by_file_io_type_column_extras,
     sys_host_summary_by_file_io_type_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_HOST_SUMMARY_BY_STAGES,
      "host_summary_by_stages",
      sys_host_summary_by_stages_columns,
      sizeof(sys_host_summary_by_stages_columns) / sizeof(sys_host_summary_by_stages_columns[0])},
     sys_host_summary_by_stages_column_keys,
     sys_host_summary_by_stages_column_extras,
     sys_host_summary_by_stages_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY,
      "host_summary_by_statement_latency",
      sys_host_summary_by_statement_latency_columns,
      sizeof(sys_host_summary_by_statement_latency_columns) /
          sizeof(sys_host_summary_by_statement_latency_columns[0])},
     sys_host_summary_by_statement_latency_column_keys,
     sys_host_summary_by_statement_latency_column_extras,
     sys_host_summary_by_statement_latency_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_HOST_SUMMARY_BY_STATEMENT_TYPE,
      "host_summary_by_statement_type",
      sys_host_summary_by_statement_type_columns,
      sizeof(sys_host_summary_by_statement_type_columns) /
          sizeof(sys_host_summary_by_statement_type_columns[0])},
     sys_host_summary_by_statement_type_column_keys,
     sys_host_summary_by_statement_type_column_extras,
     sys_host_summary_by_statement_type_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_INNODB_BUFFER_STATS_BY_SCHEMA,
      "innodb_buffer_stats_by_schema",
      sys_innodb_buffer_stats_by_schema_columns,
      sizeof(sys_innodb_buffer_stats_by_schema_columns) /
          sizeof(sys_innodb_buffer_stats_by_schema_columns[0])},
     sys_innodb_buffer_stats_by_schema_column_keys,
     sys_innodb_buffer_stats_by_schema_column_extras,
     sys_innodb_buffer_stats_by_schema_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_INNODB_BUFFER_STATS_BY_TABLE,
      "innodb_buffer_stats_by_table",
      sys_innodb_buffer_stats_by_table_columns,
      sizeof(sys_innodb_buffer_stats_by_table_columns) /
          sizeof(sys_innodb_buffer_stats_by_table_columns[0])},
     sys_innodb_buffer_stats_by_table_column_keys,
     sys_innodb_buffer_stats_by_table_column_extras,
     sys_innodb_buffer_stats_by_table_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_INNODB_LOCK_WAITS,
      "innodb_lock_waits",
      sys_innodb_lock_waits_columns,
      sizeof(sys_innodb_lock_waits_columns) / sizeof(sys_innodb_lock_waits_columns[0])},
     sys_innodb_lock_waits_column_keys,
     sys_innodb_lock_waits_column_extras,
     sys_innodb_lock_waits_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_IO_BY_THREAD_BY_LATENCY,
      "io_by_thread_by_latency",
      sys_io_by_thread_by_latency_columns,
      sizeof(sys_io_by_thread_by_latency_columns) / sizeof(sys_io_by_thread_by_latency_columns[0])},
     sys_io_by_thread_by_latency_column_keys,
     sys_io_by_thread_by_latency_column_extras,
     sys_io_by_thread_by_latency_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_IO_GLOBAL_BY_FILE_BY_BYTES,
      "io_global_by_file_by_bytes",
      sys_io_global_by_file_by_bytes_columns,
      sizeof(sys_io_global_by_file_by_bytes_columns) /
          sizeof(sys_io_global_by_file_by_bytes_columns[0])},
     sys_io_global_by_file_by_bytes_column_keys,
     sys_io_global_by_file_by_bytes_column_extras,
     sys_io_global_by_file_by_bytes_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_IO_GLOBAL_BY_FILE_BY_LATENCY,
      "io_global_by_file_by_latency",
      sys_io_global_by_file_by_latency_columns,
      sizeof(sys_io_global_by_file_by_latency_columns) /
          sizeof(sys_io_global_by_file_by_latency_columns[0])},
     sys_io_global_by_file_by_latency_column_keys,
     sys_io_global_by_file_by_latency_column_extras,
     sys_io_global_by_file_by_latency_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_IO_GLOBAL_BY_WAIT_BY_BYTES,
      "io_global_by_wait_by_bytes",
      sys_io_global_by_wait_by_bytes_columns,
      sizeof(sys_io_global_by_wait_by_bytes_columns) /
          sizeof(sys_io_global_by_wait_by_bytes_columns[0])},
     sys_io_global_by_wait_by_bytes_column_keys,
     sys_io_global_by_wait_by_bytes_column_extras,
     sys_io_global_by_wait_by_bytes_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_IO_GLOBAL_BY_WAIT_BY_LATENCY,
      "io_global_by_wait_by_latency",
      sys_io_global_by_wait_by_latency_columns,
      sizeof(sys_io_global_by_wait_by_latency_columns) /
          sizeof(sys_io_global_by_wait_by_latency_columns[0])},
     sys_io_global_by_wait_by_latency_column_keys,
     sys_io_global_by_wait_by_latency_column_extras,
     sys_io_global_by_wait_by_latency_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_LATEST_FILE_IO,
      "latest_file_io",
      sys_latest_file_io_columns,
      sizeof(sys_latest_file_io_columns) / sizeof(sys_latest_file_io_columns[0])},
     sys_latest_file_io_column_keys,
     sys_latest_file_io_column_extras,
     sys_latest_file_io_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES,
      "memory_by_host_by_current_bytes",
      sys_memory_by_host_by_current_bytes_columns,
      sizeof(sys_memory_by_host_by_current_bytes_columns) /
          sizeof(sys_memory_by_host_by_current_bytes_columns[0])},
     sys_memory_by_host_by_current_bytes_column_keys,
     sys_memory_by_host_by_current_bytes_column_extras,
     sys_memory_by_host_by_current_bytes_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES,
      "memory_by_thread_by_current_bytes",
      sys_memory_by_thread_by_current_bytes_columns,
      sizeof(sys_memory_by_thread_by_current_bytes_columns) /
          sizeof(sys_memory_by_thread_by_current_bytes_columns[0])},
     sys_memory_by_thread_by_current_bytes_column_keys,
     sys_memory_by_thread_by_current_bytes_column_extras,
     sys_memory_by_thread_by_current_bytes_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_MEMORY_BY_USER_BY_CURRENT_BYTES,
      "memory_by_user_by_current_bytes",
      sys_memory_by_user_by_current_bytes_columns,
      sizeof(sys_memory_by_user_by_current_bytes_columns) /
          sizeof(sys_memory_by_user_by_current_bytes_columns[0])},
     sys_memory_by_user_by_current_bytes_column_keys,
     sys_memory_by_user_by_current_bytes_column_extras,
     sys_memory_by_user_by_current_bytes_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_PS_CHECK_LOST_INSTRUMENTATION,
      "ps_check_lost_instrumentation",
      sys_ps_check_lost_instrumentation_columns,
      sizeof(sys_ps_check_lost_instrumentation_columns) /
          sizeof(sys_ps_check_lost_instrumentation_columns[0])},
     sys_ps_check_lost_instrumentation_column_keys,
     sys_ps_check_lost_instrumentation_column_extras,
     sys_ps_check_lost_instrumentation_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_AUTO_INCREMENT_COLUMNS,
      "schema_auto_increment_columns",
      sys_schema_auto_increment_columns,
      sizeof(sys_schema_auto_increment_columns) / sizeof(sys_schema_auto_increment_columns[0])},
     sys_schema_auto_increment_columns_column_keys,
     sys_schema_auto_increment_columns_column_extras,
     sys_schema_auto_increment_columns_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_INDEX_STATISTICS,
      "schema_index_statistics",
      sys_schema_index_statistics_columns,
      sizeof(sys_schema_index_statistics_columns) / sizeof(sys_schema_index_statistics_columns[0])},
     sys_schema_index_statistics_column_keys,
     sys_schema_index_statistics_column_extras,
     sys_schema_index_statistics_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_OBJECT_OVERVIEW,
      "schema_object_overview",
      sys_schema_object_overview_columns,
      sizeof(sys_schema_object_overview_columns) / sizeof(sys_schema_object_overview_columns[0])},
     sys_schema_object_overview_column_keys,
     sys_schema_object_overview_column_extras,
     sys_schema_object_overview_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_REDUNDANT_INDEXES,
      "schema_redundant_indexes",
      sys_schema_redundant_indexes_columns,
      sizeof(sys_schema_redundant_indexes_columns) /
          sizeof(sys_schema_redundant_indexes_columns[0])},
     sys_schema_redundant_indexes_column_keys,
     sys_schema_redundant_indexes_column_extras,
     sys_schema_redundant_indexes_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_TABLE_LOCK_WAITS,
      "schema_table_lock_waits",
      sys_schema_table_lock_waits_columns,
      sizeof(sys_schema_table_lock_waits_columns) / sizeof(sys_schema_table_lock_waits_columns[0])},
     sys_schema_table_lock_waits_column_keys,
     sys_schema_table_lock_waits_column_extras,
     sys_schema_table_lock_waits_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_TABLE_STATISTICS,
      "schema_table_statistics",
      sys_schema_table_statistics_columns,
      sizeof(sys_schema_table_statistics_columns) / sizeof(sys_schema_table_statistics_columns[0])},
     sys_schema_table_statistics_column_keys,
     sys_schema_table_statistics_column_extras,
     sys_schema_table_statistics_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_TABLE_STATISTICS_WITH_BUFFER,
      "schema_table_statistics_with_buffer",
      sys_schema_table_statistics_with_buffer_columns,
      sizeof(sys_schema_table_statistics_with_buffer_columns) /
          sizeof(sys_schema_table_statistics_with_buffer_columns[0])},
     sys_schema_table_statistics_with_buffer_column_keys,
     sys_schema_table_statistics_with_buffer_column_extras,
     sys_schema_table_statistics_with_buffer_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS,
      "schema_tables_with_full_table_scans",
      sys_schema_tables_with_full_table_scans_columns,
      sizeof(sys_schema_tables_with_full_table_scans_columns) /
          sizeof(sys_schema_tables_with_full_table_scans_columns[0])},
     sys_schema_tables_with_full_table_scans_column_keys,
     sys_schema_tables_with_full_table_scans_column_extras,
     sys_schema_tables_with_full_table_scans_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_UNUSED_INDEXES,
      "schema_unused_indexes",
      sys_schema_unused_indexes_columns,
      sizeof(sys_schema_unused_indexes_columns) / sizeof(sys_schema_unused_indexes_columns[0])},
     sys_schema_unused_indexes_column_keys,
     sys_schema_unused_indexes_column_extras,
     sys_schema_unused_indexes_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_SCHEMA_FLATTENED_KEYS,
      "x$schema_flattened_keys",
      sys_x_schema_flattened_keys_columns,
      sizeof(sys_x_schema_flattened_keys_columns) / sizeof(sys_x_schema_flattened_keys_columns[0])},
     sys_x_schema_flattened_keys_column_keys,
     sys_x_schema_flattened_keys_column_extras,
     sys_x_schema_flattened_keys_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_HOST_SUMMARY,
      "x$host_summary",
      sys_x_host_summary_columns,
      sizeof(sys_x_host_summary_columns) / sizeof(sys_x_host_summary_columns[0])},
     sys_host_summary_column_keys,
     sys_host_summary_column_extras,
     sys_host_summary_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_HOST_SUMMARY_BY_FILE_IO,
      "x$host_summary_by_file_io",
      sys_x_host_summary_by_file_io_columns,
      sizeof(sys_x_host_summary_by_file_io_columns) /
          sizeof(sys_x_host_summary_by_file_io_columns[0])},
     sys_host_summary_by_file_io_column_keys,
     sys_host_summary_by_file_io_column_extras,
     sys_host_summary_by_file_io_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_HOST_SUMMARY_BY_FILE_IO_TYPE,
      "x$host_summary_by_file_io_type",
      sys_x_host_summary_by_file_io_type_columns,
      sizeof(sys_x_host_summary_by_file_io_type_columns) /
          sizeof(sys_x_host_summary_by_file_io_type_columns[0])},
     sys_host_summary_by_file_io_type_column_keys,
     sys_host_summary_by_file_io_type_column_extras,
     sys_host_summary_by_file_io_type_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_HOST_SUMMARY_BY_STAGES,
      "x$host_summary_by_stages",
      sys_x_host_summary_by_stages_columns,
      sizeof(sys_x_host_summary_by_stages_columns) /
          sizeof(sys_x_host_summary_by_stages_columns[0])},
     sys_host_summary_by_stages_column_keys,
     sys_host_summary_by_stages_column_extras,
     sys_host_summary_by_stages_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_HOST_SUMMARY_BY_STATEMENT_LATENCY,
      "x$host_summary_by_statement_latency",
      sys_x_host_summary_by_statement_latency_columns,
      sizeof(sys_x_host_summary_by_statement_latency_columns) /
          sizeof(sys_x_host_summary_by_statement_latency_columns[0])},
     sys_host_summary_by_statement_latency_column_keys,
     sys_host_summary_by_statement_latency_column_extras,
     sys_host_summary_by_statement_latency_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_HOST_SUMMARY_BY_STATEMENT_TYPE,
      "x$host_summary_by_statement_type",
      sys_x_host_summary_by_statement_type_columns,
      sizeof(sys_x_host_summary_by_statement_type_columns) /
          sizeof(sys_x_host_summary_by_statement_type_columns[0])},
     sys_host_summary_by_statement_type_column_keys,
     sys_host_summary_by_statement_type_column_extras,
     sys_host_summary_by_statement_type_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_INNODB_BUFFER_STATS_BY_SCHEMA,
      "x$innodb_buffer_stats_by_schema",
      sys_x_innodb_buffer_stats_by_schema_columns,
      sizeof(sys_x_innodb_buffer_stats_by_schema_columns) /
          sizeof(sys_x_innodb_buffer_stats_by_schema_columns[0])},
     sys_innodb_buffer_stats_by_schema_column_keys,
     sys_innodb_buffer_stats_by_schema_column_extras,
     sys_innodb_buffer_stats_by_schema_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_INNODB_BUFFER_STATS_BY_TABLE,
      "x$innodb_buffer_stats_by_table",
      sys_x_innodb_buffer_stats_by_table_columns,
      sizeof(sys_x_innodb_buffer_stats_by_table_columns) /
          sizeof(sys_x_innodb_buffer_stats_by_table_columns[0])},
     sys_innodb_buffer_stats_by_table_column_keys,
     sys_innodb_buffer_stats_by_table_column_extras,
     sys_innodb_buffer_stats_by_table_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_INNODB_LOCK_WAITS,
      "x$innodb_lock_waits",
      sys_x_innodb_lock_waits_columns,
      sizeof(sys_x_innodb_lock_waits_columns) / sizeof(sys_x_innodb_lock_waits_columns[0])},
     sys_innodb_lock_waits_column_keys,
     sys_innodb_lock_waits_column_extras,
     sys_innodb_lock_waits_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_IO_BY_THREAD_BY_LATENCY,
      "x$io_by_thread_by_latency",
      sys_x_io_by_thread_by_latency_columns,
      sizeof(sys_x_io_by_thread_by_latency_columns) /
          sizeof(sys_x_io_by_thread_by_latency_columns[0])},
     sys_io_by_thread_by_latency_column_keys,
     sys_io_by_thread_by_latency_column_extras,
     sys_io_by_thread_by_latency_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_IO_GLOBAL_BY_FILE_BY_BYTES,
      "x$io_global_by_file_by_bytes",
      sys_x_io_global_by_file_by_bytes_columns,
      sizeof(sys_x_io_global_by_file_by_bytes_columns) /
          sizeof(sys_x_io_global_by_file_by_bytes_columns[0])},
     sys_io_global_by_file_by_bytes_column_keys,
     sys_io_global_by_file_by_bytes_column_extras,
     sys_io_global_by_file_by_bytes_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_IO_GLOBAL_BY_FILE_BY_LATENCY,
      "x$io_global_by_file_by_latency",
      sys_x_io_global_by_file_by_latency_columns,
      sizeof(sys_x_io_global_by_file_by_latency_columns) /
          sizeof(sys_x_io_global_by_file_by_latency_columns[0])},
     sys_io_global_by_file_by_latency_column_keys,
     sys_io_global_by_file_by_latency_column_extras,
     sys_io_global_by_file_by_latency_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_IO_GLOBAL_BY_WAIT_BY_BYTES,
      "x$io_global_by_wait_by_bytes",
      sys_x_io_global_by_wait_by_bytes_columns,
      sizeof(sys_x_io_global_by_wait_by_bytes_columns) /
          sizeof(sys_x_io_global_by_wait_by_bytes_columns[0])},
     sys_io_global_by_wait_by_bytes_column_keys,
     sys_io_global_by_wait_by_bytes_column_extras,
     sys_io_global_by_wait_by_bytes_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_IO_GLOBAL_BY_WAIT_BY_LATENCY,
      "x$io_global_by_wait_by_latency",
      sys_x_io_global_by_wait_by_latency_columns,
      sizeof(sys_x_io_global_by_wait_by_latency_columns) /
          sizeof(sys_x_io_global_by_wait_by_latency_columns[0])},
     sys_io_global_by_wait_by_latency_column_keys,
     sys_io_global_by_wait_by_latency_column_extras,
     sys_io_global_by_wait_by_latency_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_LATEST_FILE_IO,
      "x$latest_file_io",
      sys_x_latest_file_io_columns,
      sizeof(sys_x_latest_file_io_columns) / sizeof(sys_x_latest_file_io_columns[0])},
     sys_latest_file_io_column_keys,
     sys_latest_file_io_column_extras,
     sys_latest_file_io_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_MEMORY_BY_HOST_BY_CURRENT_BYTES,
      "x$memory_by_host_by_current_bytes",
      sys_x_memory_by_host_by_current_bytes_columns,
      sizeof(sys_x_memory_by_host_by_current_bytes_columns) /
          sizeof(sys_x_memory_by_host_by_current_bytes_columns[0])},
     sys_memory_by_host_by_current_bytes_column_keys,
     sys_memory_by_host_by_current_bytes_column_extras,
     sys_memory_by_host_by_current_bytes_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_MEMORY_BY_THREAD_BY_CURRENT_BYTES,
      "x$memory_by_thread_by_current_bytes",
      sys_x_memory_by_thread_by_current_bytes_columns,
      sizeof(sys_x_memory_by_thread_by_current_bytes_columns) /
          sizeof(sys_x_memory_by_thread_by_current_bytes_columns[0])},
     sys_memory_by_thread_by_current_bytes_column_keys,
     sys_memory_by_thread_by_current_bytes_column_extras,
     sys_memory_by_thread_by_current_bytes_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_MEMORY_BY_USER_BY_CURRENT_BYTES,
      "x$memory_by_user_by_current_bytes",
      sys_x_memory_by_user_by_current_bytes_columns,
      sizeof(sys_x_memory_by_user_by_current_bytes_columns) /
          sizeof(sys_x_memory_by_user_by_current_bytes_columns[0])},
     sys_memory_by_user_by_current_bytes_column_keys,
     sys_memory_by_user_by_current_bytes_column_extras,
     sys_memory_by_user_by_current_bytes_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_PS_SCHEMA_TABLE_STATISTICS_IO,
      "x$ps_schema_table_statistics_io",
      sys_x_ps_schema_table_statistics_io_columns,
      sizeof(sys_x_ps_schema_table_statistics_io_columns) /
          sizeof(sys_x_ps_schema_table_statistics_io_columns[0])},
     sys_x_ps_schema_table_statistics_io_column_keys,
     sys_x_ps_schema_table_statistics_io_column_extras,
     sys_x_ps_schema_table_statistics_io_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_SCHEMA_INDEX_STATISTICS,
      "x$schema_index_statistics",
      sys_x_schema_index_statistics_columns,
      sizeof(sys_x_schema_index_statistics_columns) /
          sizeof(sys_x_schema_index_statistics_columns[0])},
     sys_schema_index_statistics_column_keys,
     sys_schema_index_statistics_column_extras,
     sys_schema_index_statistics_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_SCHEMA_TABLE_LOCK_WAITS,
      "x$schema_table_lock_waits",
      sys_schema_table_lock_waits_columns,
      sizeof(sys_schema_table_lock_waits_columns) / sizeof(sys_schema_table_lock_waits_columns[0])},
     sys_schema_table_lock_waits_column_keys,
     sys_schema_table_lock_waits_column_extras,
     sys_schema_table_lock_waits_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_SCHEMA_TABLE_STATISTICS,
      "x$schema_table_statistics",
      sys_x_schema_table_statistics_columns,
      sizeof(sys_x_schema_table_statistics_columns) /
          sizeof(sys_x_schema_table_statistics_columns[0])},
     sys_schema_table_statistics_column_keys,
     sys_schema_table_statistics_column_extras,
     sys_schema_table_statistics_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_SCHEMA_TABLE_STATISTICS_WITH_BUFFER,
      "x$schema_table_statistics_with_buffer",
      sys_x_schema_table_statistics_with_buffer_columns,
      sizeof(sys_x_schema_table_statistics_with_buffer_columns) /
          sizeof(sys_x_schema_table_statistics_with_buffer_columns[0])},
     sys_schema_table_statistics_with_buffer_column_keys,
     sys_schema_table_statistics_with_buffer_column_extras,
     sys_schema_table_statistics_with_buffer_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
    {"sys",
     {MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS,
      "x$schema_tables_with_full_table_scans",
      sys_x_schema_tables_with_full_table_scans_columns,
      sizeof(sys_x_schema_tables_with_full_table_scans_columns) /
          sizeof(sys_x_schema_tables_with_full_table_scans_columns[0])},
     sys_schema_tables_with_full_table_scans_column_keys,
     sys_schema_tables_with_full_table_scans_column_extras,
     sys_schema_tables_with_full_table_scans_column_privileges,
     NULL,
     NULL,
     0U,
     NULL,
     NULL,
     0U},
};

size_t mylite_execution_catalog_mysql_system_table_definition_count(void) {
    return sizeof(mysql_system_table_definitions) / sizeof(mysql_system_table_definitions[0]);
}

const struct mylite_execution_catalog_mysql_system_table *mylite_execution_catalog_mysql_system_table_definition_at(
    size_t index
) {
    if (index >= mylite_execution_catalog_mysql_system_table_definition_count()) {
        return NULL;
    }
    return &mysql_system_table_definitions[index];
}

const struct mylite_execution_catalog_mysql_system_table *mylite_execution_catalog_mysql_system_table_definition_by_name(
    const char *schema_name,
    const char *table_name
) {
    if (schema_name == NULL || table_name == NULL) {
        return NULL;
    }
    for (size_t index = 0U; index < mylite_execution_catalog_mysql_system_table_definition_count();
         ++index) {
        if (strcmp(schema_name, mysql_system_table_definitions[index].schema_name) == 0 &&
            strcmp(table_name, mysql_system_table_definitions[index].query_definition.name) == 0) {
            return &mysql_system_table_definitions[index];
        }
    }
    return NULL;
}

const char *mylite_execution_catalog_sys_sys_config_trigger_action_statement(void) {
    return sys_sys_config_trigger_action_statement;
}

const char *mylite_execution_catalog_sys_sys_config_trigger_sql_mode(void) {
    return sys_sys_config_trigger_sql_mode;
}

size_t mylite_execution_catalog_sys_sys_config_trigger_count(void) {
    return sizeof(sys_sys_config_triggers) / sizeof(sys_sys_config_triggers[0]);
}

const struct mylite_execution_catalog_sys_config_trigger *mylite_execution_catalog_sys_sys_config_trigger_at(
    size_t index
) {
    if (index >= mylite_execution_catalog_sys_sys_config_trigger_count()) {
        return NULL;
    }
    return &sys_sys_config_triggers[index];
}

size_t mylite_execution_catalog_builtin_sys_view_definition_count(void) {
    return sizeof(builtin_sys_view_definitions) / sizeof(builtin_sys_view_definitions[0]);
}

const struct mylite_execution_catalog_builtin_sys_view *mylite_execution_catalog_builtin_sys_view_definition_at(
    size_t index
) {
    if (index >= mylite_execution_catalog_builtin_sys_view_definition_count()) {
        return NULL;
    }
    return &builtin_sys_view_definitions[index];
}

const struct mylite_execution_catalog_builtin_sys_view *mylite_execution_catalog_builtin_sys_view_definition_by_name(
    const char *view_name
) {
    if (view_name == NULL) {
        return NULL;
    }
    for (size_t index = 0U; index < mylite_execution_catalog_builtin_sys_view_definition_count();
         ++index) {
        if (strcmp(view_name, builtin_sys_view_definitions[index].name) == 0) {
            return &builtin_sys_view_definitions[index];
        }
    }
    return NULL;
}
