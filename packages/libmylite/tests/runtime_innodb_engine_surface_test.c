#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
#include "storage/mylite_file_format.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

enum {
    test_path_capacity = 1024,
    show_create_sql_capacity = 256,
    show_engines_column_count = 6,
    show_engine_status_column_count = 3,
    show_create_column_count = 2,
    show_warnings_column_count = 3,
    default_storage_engine_variable_column_count = 6,
    scoped_default_storage_engine_variable_column_count = 4,
    selected_default_storage_engine_variable_column_count = 2,
    diagnostics_default_storage_engine_variable_column_count = 4,
    independent_default_storage_engine_variable_column_count = 2,
    decimal_base = 10,
    mysql_error_parse = 1064,
    mysql_error_table_exists = 1050,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_system_variable = 1193,
    mysql_error_unknown_storage_engine = 1286,
    mysql_warning_using_storage_engine = 1266,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_single_row_result {
    const char *const *columns;
    const char *const *values;
    size_t column_count;
};

struct expected_show_create_single_int {
    const char *sql;
    const char *table_name;
    const char *context;
};

struct expected_show_create_exact {
    const char *sql;
    const char *table_name;
    const char *create_sql;
    const char *context;
};

struct expected_count_result {
    const char *sql;
    const char *expected;
    const char *context;
};

struct expected_warning_row {
    const char *level;
    const char *code;
    const char *message;
};

static const char *const show_engines_columns[show_engines_column_count] = {
    "Engine",
    "Support",
    "Comment",
    "Transactions",
    "XA",
    "Savepoints",
};

static const char *const show_engines_values[show_engines_column_count] = {
    "InnoDB",
    "DEFAULT",
    "Supports transactions, row-level locking, and foreign keys",
    "YES",
    "YES",
    "YES",
};

static const char *const show_engine_status_columns[show_engine_status_column_count] = {
    "Type",
    "Name",
    "Status",
};

static const char *const show_engine_status_values[show_engine_status_column_count] = {
    "InnoDB",
    "",
    "MyLite embedded InnoDB-compatible storage engine is active",
};

static const char *const show_create_columns[show_create_column_count] = {
    "Table",
    "Create Table",
};

static int test_innodb_create_forms_persistence_and_preamble(void);
static int test_default_storage_engine_system_variable_values_and_diagnostics(void);
static int test_innodb_engine_diagnostics(void);
static int test_storage_engine_substitution(void);
static int test_independent_innodb_engine_handles(void);
static int expect_show_engines_result(mylite_db *database, const char *sql, const char *context);
static int expect_show_engine_status_result(
    mylite_db *database,
    const char *sql,
    const char *context
);
static int expect_show_create_single_int(
    mylite_db *database,
    struct expected_show_create_single_int expected
);
static int expect_show_create_exact(
    mylite_db *database,
    struct expected_show_create_exact expected
);
static int expect_show_warnings(
    mylite_db *database,
    const struct expected_warning_row *rows,
    size_t row_count,
    const char *context
);
static int expect_single_row_result(
    mylite_db *database,
    const char *sql,
    struct expected_single_row_result expected,
    const char *context
);
static int expect_row_count(mylite_db *database, int64_t expected, const char *context);
static int expect_count_statement(mylite_db *database, struct expected_count_result expected);
static int expect_show_warning_count(
    mylite_db *database,
    const char *expected,
    const char *context
);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_statement_ok_with_warnings(
    mylite_db *database,
    const char *sql,
    size_t expected_warning_count
);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int execute_error_with_length(
    mylite_db *database,
    const char *sql,
    size_t sql_size,
    struct expected_sql_error expected,
    const char *context
);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_text_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_innodb_create_forms_persistence_and_preamble();
    failures += test_default_storage_engine_system_variable_values_and_diagnostics();
    failures += test_innodb_engine_diagnostics();
    failures += test_storage_engine_substitution();
    failures += test_independent_innodb_engine_handles();

    return failures == 0 ? 0 : 1;
}

static int test_innodb_create_forms_persistence_and_preamble(void) {
    static const char *const select_columns[] = {"id"};
    static const char *const select_values[] = {"1"};
    static const char *const status_diagnostic_columns[] = {
        "ROW_COUNT()",
        "@@warning_count",
        "@@error_count",
    };
    static const char *const status_diagnostic_values[] = {
        "-1",
        "0",
        "0",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "forms") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open forms database");
    failures += expect_show_engines_result(database, "SHOW ENGINES", "show engines");
    failures +=
        expect_show_engines_result(database, "SHOW STORAGE ENGINES", "show storage engines");
    failures += expect_show_engine_status_result(
        database,
        "SHOW ENGINE InnoDB STATUS",
        "show engine status"
    );
    failures += expect_single_row_result(
        database,
        "SELECT ROW_COUNT(), @@warning_count, @@error_count",
        (struct expected_single_row_result){
            .columns = status_diagnostic_columns,
            .values = status_diagnostic_values,
            .column_count =
                sizeof(status_diagnostic_columns) / sizeof(status_diagnostic_columns[0]),
        },
        "show engine status diagnostics"
    );
    failures += expect_show_engine_status_result(
        database,
        "SHOW ENGINE 'InnoDB' STATUS",
        "show engine string status"
    );
    failures += expect_row_count(database, -1, "row count after show engines");

    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_error(
        database,
        "CREATE TABLE no_schema (id INT) ENGINE=InnoDB",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(database, "CREATE TABLE no_engine (id INT)");
    failures +=
        execute_statement_ok(database, "CREATE TABLE explicit_equal (id INT) ENGINE=InnoDB");
    failures +=
        execute_statement_ok(database, "CREATE TABLE explicit_space (id INT) ENGINE InnoDB");
    failures += execute_statement_ok(database, "CREATE TABLE lower_name (id INT) ENGINE=innodb");
    failures += execute_statement_ok(database, "CREATE TABLE string_name (id INT) ENGINE='InnoDB'");
    failures += execute_statement_ok(
        database,
        "CREATE TABLE double_string_name (id INT) ENGINE=\"InnoDB\""
    );
    failures += execute_statement_ok(database, "CREATE TABLE quoted_name (id INT) ENGINE=`InnoDB`");
    failures +=
        execute_statement_ok(database, "CREATE TABLE app.qualified_engine (id INT) ENGINE=InnoDB");
    failures += expect_row_count(database, 0, "row count after create");

    failures += expect_show_create_single_int(
        database,
        (struct expected_show_create_single_int){
            .sql = "SHOW CREATE TABLE no_engine",
            .table_name = "no_engine",
            .context = "show create no engine",
        }
    );
    failures += expect_show_create_single_int(
        database,
        (struct expected_show_create_single_int){
            .sql = "SHOW CREATE TABLE explicit_equal",
            .table_name = "explicit_equal",
            .context = "show create explicit equal",
        }
    );
    failures += expect_show_create_single_int(
        database,
        (struct expected_show_create_single_int){
            .sql = "SHOW CREATE TABLE explicit_space",
            .table_name = "explicit_space",
            .context = "show create explicit space",
        }
    );
    failures += expect_show_create_single_int(
        database,
        (struct expected_show_create_single_int){
            .sql = "SHOW CREATE TABLE lower_name",
            .table_name = "lower_name",
            .context = "show create lower engine",
        }
    );
    failures += expect_show_create_single_int(
        database,
        (struct expected_show_create_single_int){
            .sql = "SHOW CREATE TABLE string_name",
            .table_name = "string_name",
            .context = "show create string engine",
        }
    );
    failures += expect_show_create_single_int(
        database,
        (struct expected_show_create_single_int){
            .sql = "SHOW CREATE TABLE double_string_name",
            .table_name = "double_string_name",
            .context = "show create double string engine",
        }
    );
    failures += expect_show_create_single_int(
        database,
        (struct expected_show_create_single_int){
            .sql = "SHOW CREATE TABLE quoted_name",
            .table_name = "quoted_name",
            .context = "show create quoted engine",
        }
    );
    failures += expect_show_create_single_int(
        database,
        (struct expected_show_create_single_int){
            .sql = "SHOW CREATE TABLE app.qualified_engine",
            .table_name = "qualified_engine",
            .context = "show create qualified engine",
        }
    );

    failures += execute_statement_ok(database, "INSERT INTO explicit_equal VALUES (1)");
    failures += expect_single_row_result(
        database,
        "SELECT id FROM explicit_equal",
        (struct expected_single_row_result){
            .columns = select_columns,
            .values = select_values,
            .column_count = sizeof(select_columns) / sizeof(select_columns[0]),
        },
        "row from explicit engine table"
    );

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after explicit engine create and status"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen forms database");
    failures += execute_statement_ok(database, "USE app");
    failures += expect_show_create_single_int(
        database,
        (struct expected_show_create_single_int){
            .sql = "SHOW CREATE TABLE explicit_equal",
            .table_name = "explicit_equal",
            .context = "reopened show create explicit engine",
        }
    );
    failures += expect_single_row_result(
        database,
        "SELECT id FROM explicit_equal",
        (struct expected_single_row_result){
            .columns = select_columns,
            .values = select_values,
            .column_count = sizeof(select_columns) / sizeof(select_columns[0]),
        },
        "reopened row from explicit engine table"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_default_storage_engine_system_variable_values_and_diagnostics(void) {
    static const char *const variable_columns[] = {
        "@@default_storage_engine",
        "@@global.default_storage_engine",
        "@@session.default_storage_engine",
        "@@local.default_storage_engine",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const variable_values[] = {
        "InnoDB",
        "InnoDB",
        "InnoDB",
        "InnoDB",
        "0",
        "-1",
    };
    static const char *const scoped_columns[] = {
        "@@DEFAULT_STORAGE_ENGINE",
        "@@Global.Default_Storage_Engine",
        "@@session.`default_storage_engine`",
        "@@`default_storage_engine`",
    };
    static const char *const scoped_values[] = {
        "InnoDB",
        "InnoDB",
        "InnoDB",
        "InnoDB",
    };
    static const char *const selected_columns[] = {
        "@@default_storage_engine",
        "DATABASE()",
    };
    static const char *const selected_values[] = {
        "InnoDB",
        "app",
    };
    static const char *const warning_columns[] = {
        "@@default_storage_engine",
        "@@warning_count",
        "@@error_count",
        "ROW_COUNT()",
    };
    static const char *const warning_values[] = {
        "InnoDB",
        "1",
        "0",
        "-1",
    };
    static const char *const error_values[] = {
        "InnoDB",
        "1",
        "1",
        "-1",
    };
    static const char *const mixed_columns[] = {
        "@@default_storage_engine",
        "@@character_set_server",
        "@@version_comment",
    };
    static const char *const mixed_values[] = {
        "InnoDB",
        "utf8mb4",
        "MyLite",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "default-variable") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures +=
        expect_int(mylite_open(path, &database), MYLITE_OK, "open default engine variable file");
    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;

    failures += expect_single_row_result(
        database,
        "SELECT @@default_storage_engine, @@global.default_storage_engine, "
        "@@session.default_storage_engine, @@local.default_storage_engine, @@warning_count, "
        "ROW_COUNT() FROM DUAL",
        (struct expected_single_row_result){
            .columns = variable_columns,
            .values = variable_values,
            .column_count = default_storage_engine_variable_column_count,
        },
        "default storage engine variable values"
    );
    failures += expect_single_row_result(
        database,
        "SELECT @@DEFAULT_STORAGE_ENGINE, @@Global.Default_Storage_Engine, "
        "@@session.`default_storage_engine`, @@`default_storage_engine`",
        (struct expected_single_row_result){
            .columns = scoped_columns,
            .values = scoped_values,
            .column_count = scoped_default_storage_engine_variable_column_count,
        },
        "scoped default storage engine labels"
    );
    failures += expect_single_row_result(
        database,
        "SELECT @@default_storage_engine, @@character_set_server, @@version_comment",
        (struct expected_single_row_result){
            .columns = mixed_columns,
            .values = mixed_values,
            .column_count = sizeof(mixed_columns) / sizeof(mixed_columns[0]),
        },
        "mixed default storage engine variable values"
    );

    failures += execute_statement_ok(database, "SHOW PROCESSLIST");
    failures += expect_single_row_result(
        database,
        "SELECT @@default_storage_engine, @@warning_count, @@error_count, ROW_COUNT()",
        (struct expected_single_row_result){
            .columns = warning_columns,
            .values = warning_values,
            .column_count = diagnostics_default_storage_engine_variable_column_count,
        },
        "default storage engine warning diagnostics"
    );
    failures += expect_count_statement(
        database,
        (struct expected_count_result){
            .sql = "SHOW COUNT(*) WARNINGS",
            .expected = "0",
            .context = "scalar clears warnings",
        }
    );

    failures += execute_error(
        database,
        "BAD SQL",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "BAD",
        }
    );
    failures += expect_single_row_result(
        database,
        "SELECT @@default_storage_engine, @@warning_count, @@error_count, ROW_COUNT()",
        (struct expected_single_row_result){
            .columns = warning_columns,
            .values = error_values,
            .column_count = diagnostics_default_storage_engine_variable_column_count,
        },
        "default storage engine error diagnostics"
    );
    failures += expect_count_statement(
        database,
        (struct expected_count_result){
            .sql = "SHOW COUNT(*) ERRORS",
            .expected = "0",
            .context = "scalar clears errors",
        }
    );
    failures += expect_count_statement(
        database,
        (struct expected_count_result){
            .sql = "SHOW COUNT(*) WARNINGS",
            .expected = "0",
            .context = "scalar clears error warnings",
        }
    );

    session = mylite_connection_session_state(database);
    failures += expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "catalog generation unchanged by default storage engine reads"
    );
    failures += expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "sqlite schema generation unchanged by default storage engine reads"
    );

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after default storage engine reads"
    );

    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += expect_single_row_result(
        database,
        "SELECT @@default_storage_engine, DATABASE()",
        (struct expected_single_row_result){
            .columns = selected_columns,
            .values = selected_values,
            .column_count = selected_default_storage_engine_variable_column_count,
        },
        "default storage engine with selected database"
    );

    mylite_close(database);
    database = NULL;

    failures +=
        expect_int(mylite_open(path, &database), MYLITE_OK, "reopen default engine variable file");
    failures += expect_single_row_result(
        database,
        "SELECT @@default_storage_engine",
        (struct expected_single_row_result){
            .columns = variable_columns,
            .values = variable_values,
            .column_count = 1U,
        },
        "reopened default storage engine variable"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_innodb_engine_diagnostics(void) {
    static const char raw_nul_string_engine_sql[] =
        "CREATE TABLE raw_nul_string_engine (id INT) ENGINE='InnoDB"
        "\0"
        "junk'";
    static const char raw_nul_identifier_engine_sql[] =
        "CREATE TABLE raw_nul_identifier_engine (id INT) ENGINE=`InnoDB"
        "\0"
        "junk`";
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics database");
    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");

    failures += execute_error(
        database,
        "CREATE TABLE myisam_table (id INT) ENGINE=MyISAM",
        (struct expected_sql_error){
            .code = mysql_error_unknown_storage_engine,
            .sqlstate = "42000",
            .message_part = "Unknown storage engine 'MyISAM'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE unknown_engine (id INT) ENGINE=NoSuchEngine",
        (struct expected_sql_error){
            .code = mysql_error_unknown_storage_engine,
            .sqlstate = "42000",
            .message_part = "Unknown storage engine 'NoSuchEngine'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE string_unknown (id INT) ENGINE='MyISAM'",
        (struct expected_sql_error){
            .code = mysql_error_unknown_storage_engine,
            .sqlstate = "42000",
            .message_part = "Unknown storage engine 'MyISAM'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE empty_engine (id INT) ENGINE=''",
        (struct expected_sql_error){
            .code = mysql_error_unknown_storage_engine,
            .sqlstate = "42000",
            .message_part = "Unknown storage engine ''",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE nul_engine (id INT) ENGINE='In\\0noDB'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "table engine names do not support NUL bytes",
        }
    );
    failures += execute_error_with_length(
        database,
        raw_nul_string_engine_sql,
        sizeof(raw_nul_string_engine_sql) - 1U,
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "table engine names do not support NUL bytes",
        },
        "raw NUL string engine"
    );
    failures += execute_error_with_length(
        database,
        raw_nul_identifier_engine_sql,
        sizeof(raw_nul_identifier_engine_sql) - 1U,
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "table engine names do not support NUL bytes",
        },
        "raw NUL identifier engine"
    );
    failures += execute_error(
        database,
        "SHOW ENGINES LIKE 'InnoDB'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW ENGINES WHERE Engine = 'InnoDB'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW FULL ENGINES",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += expect_show_engine_status_result(
        database,
        "SHOW ENGINE innodb STATUS",
        "diagnostics lower show engine status"
    );
    failures += execute_error(
        database,
        "SHOW ENGINE MyISAM STATUS",
        (struct expected_sql_error){
            .code = mysql_error_unknown_storage_engine,
            .sqlstate = "42000",
            .message_part = "Unknown storage engine 'MyISAM'",
        }
    );
    failures += execute_error(
        database,
        "SHOW ENGINE PERFORMANCE_SCHEMA STATUS",
        (struct expected_sql_error){
            .code = mysql_error_unknown_storage_engine,
            .sqlstate = "42000",
            .message_part = "Unknown storage engine 'PERFORMANCE_SCHEMA'",
        }
    );
    failures += execute_error(
        database,
        "SHOW ENGINE InnoDB MUTEX",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW ENGINE InnoDB LOGS",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW ENGINE InnoDB STATUS LIKE '%'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW FULL ENGINE InnoDB STATUS",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE default_engine (id INT) ENGINE=DEFAULT",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@no_such_default_storage_engine_variable",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable 'no_such_default_storage_engine_variable'",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@global.no_such_default_storage_engine_variable",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable 'no_such_default_storage_engine_variable'",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@`session`.default_storage_engine",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "quoted system variable scope",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@default_storage_engine + 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "signed 64-bit +, binary -, and * arithmetic",
        }
    );
    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_storage_engine_substitution(void) {
    static const char *const warning_count_columns[] = {"@@warning_count"};
    static const char *const warning_count_values[] = {"2"};
    static const char *const select_columns[] = {"id"};
    static const char *const select_values[] = {"7"};
    static const char temp_create_sql[] =
        "CREATE TEMPORARY TABLE `temp_unknown` (\n"
        "  `id` int DEFAULT NULL\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci";
    static const struct expected_warning_row unknown_engine_warnings[] = {
        {"Warning", "1286", "Unknown storage engine 'NoSuchEngine'"},
        {"Warning", "1266", "Using storage engine InnoDB for table 'unknown_loose'"},
    };
    static const struct expected_warning_row empty_engine_warnings[] = {
        {"Warning", "1286", "Unknown storage engine ''"},
        {"Warning", "1266", "Using storage engine InnoDB for table 'empty_loose'"},
    };
    static const struct expected_warning_row myisam_engine_warnings[] = {
        {"Warning", "1286", "Unknown storage engine 'MyISAM'"},
        {"Warning", "1266", "Using storage engine InnoDB for table 'myisam_loose'"},
    };
    static const struct expected_warning_row memory_engine_warnings[] = {
        {"Warning", "1286", "Unknown storage engine 'MEMORY'"},
        {"Warning", "1266", "Using storage engine InnoDB for table 'memory_loose'"},
    };
    static const struct expected_warning_row if_not_exists_warnings[] = {
        {"Warning", "1286", "Unknown storage engine 'NoSuchEngine'"},
        {"Warning", "1266", "Using storage engine InnoDB for table 'unknown_loose'"},
        {"Note", "1050", "Table 'unknown_loose' already exists"},
    };
    static const struct expected_warning_row temp_unknown_warnings[] = {
        {"Warning", "1286", "Unknown storage engine 'NoSuchEngine'"},
        {"Warning", "1266", "Using storage engine InnoDB for table 'temp_unknown'"},
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "substitution") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open substitution database");
    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");

    failures += execute_statement_ok(database, "SET SESSION sql_mode = ''");
    failures += execute_statement_ok_with_warnings(
        database,
        "CREATE TABLE unknown_loose (id INT) ENGINE=NoSuchEngine",
        2U
    );
    failures += expect_show_warnings(
        database,
        unknown_engine_warnings,
        sizeof(unknown_engine_warnings) / sizeof(unknown_engine_warnings[0]),
        "loose unknown engine warnings"
    );
    failures += expect_show_warning_count(database, "2", "loose unknown engine warning count");
    failures += expect_single_row_result(
        database,
        "SELECT @@warning_count",
        (struct expected_single_row_result){
            .columns = warning_count_columns,
            .values = warning_count_values,
            .column_count = sizeof(warning_count_columns) / sizeof(warning_count_columns[0]),
        },
        "loose unknown engine scalar warning count"
    );
    failures += expect_show_create_single_int(
        database,
        (struct expected_show_create_single_int){
            .sql = "SHOW CREATE TABLE unknown_loose",
            .table_name = "unknown_loose",
            .context = "loose unknown engine show create",
        }
    );
    failures += execute_statement_ok(database, "INSERT INTO unknown_loose VALUES (7)");
    failures += expect_single_row_result(
        database,
        "SELECT id FROM unknown_loose",
        (struct expected_single_row_result){
            .columns = select_columns,
            .values = select_values,
            .column_count = sizeof(select_columns) / sizeof(select_columns[0]),
        },
        "row from substituted unknown engine table"
    );

    failures += execute_statement_ok_with_warnings(
        database,
        "CREATE TABLE empty_loose (id INT) ENGINE=''",
        2U
    );
    failures += expect_show_warnings(
        database,
        empty_engine_warnings,
        sizeof(empty_engine_warnings) / sizeof(empty_engine_warnings[0]),
        "loose empty engine warnings"
    );

    failures += execute_statement_ok_with_warnings(
        database,
        "CREATE TABLE myisam_loose (id INT) ENGINE=MyISAM",
        2U
    );
    failures += expect_show_warnings(
        database,
        myisam_engine_warnings,
        sizeof(myisam_engine_warnings) / sizeof(myisam_engine_warnings[0]),
        "loose MyISAM engine warnings"
    );
    failures += expect_show_create_single_int(
        database,
        (struct expected_show_create_single_int){
            .sql = "SHOW CREATE TABLE myisam_loose",
            .table_name = "myisam_loose",
            .context = "loose MyISAM engine renders InnoDB",
        }
    );
    failures += execute_statement_ok_with_warnings(
        database,
        "CREATE TABLE memory_loose (id INT) ENGINE=MEMORY",
        2U
    );
    failures += expect_show_warnings(
        database,
        memory_engine_warnings,
        sizeof(memory_engine_warnings) / sizeof(memory_engine_warnings[0]),
        "loose MEMORY engine warnings"
    );
    failures += expect_show_create_single_int(
        database,
        (struct expected_show_create_single_int){
            .sql = "SHOW CREATE TABLE memory_loose",
            .table_name = "memory_loose",
            .context = "loose MEMORY engine renders InnoDB",
        }
    );

    failures += execute_statement_ok_with_warnings(
        database,
        "CREATE TABLE IF NOT EXISTS unknown_loose (id INT) ENGINE=NoSuchEngine",
        3U
    );
    failures += expect_show_warnings(
        database,
        if_not_exists_warnings,
        sizeof(if_not_exists_warnings) / sizeof(if_not_exists_warnings[0]),
        "if not exists substitution warning order"
    );

    failures += execute_statement_ok_with_warnings(
        database,
        "CREATE TEMPORARY TABLE temp_unknown (id INT) ENGINE=NoSuchEngine",
        2U
    );
    failures += expect_show_warnings(
        database,
        temp_unknown_warnings,
        sizeof(temp_unknown_warnings) / sizeof(temp_unknown_warnings[0]),
        "temporary substitution warnings"
    );
    failures += expect_show_create_exact(
        database,
        (struct expected_show_create_exact){
            .sql = "SHOW CREATE TABLE temp_unknown",
            .table_name = "temp_unknown",
            .create_sql = temp_create_sql,
            .context = "temporary substituted engine show create",
        }
    );

    failures += execute_statement_ok(database, "SET SESSION sql_mode = 'NO_ENGINE_SUBSTITUTION'");
    failures += execute_error(
        database,
        "CREATE TABLE strict_unknown (id INT) ENGINE=NoSuchEngine",
        (struct expected_sql_error){
            .code = mysql_error_unknown_storage_engine,
            .sqlstate = "42000",
            .message_part = "Unknown storage engine 'NoSuchEngine'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TEMPORARY TABLE strict_temp (id INT) ENGINE=NoSuchEngine",
        (struct expected_sql_error){
            .code = mysql_error_unknown_storage_engine,
            .sqlstate = "42000",
            .message_part = "Unknown storage engine 'NoSuchEngine'",
        }
    );

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after substituted engine creates"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen substitution database");
    failures += execute_statement_ok(database, "USE app");
    failures += expect_show_create_single_int(
        database,
        (struct expected_show_create_single_int){
            .sql = "SHOW CREATE TABLE unknown_loose",
            .table_name = "unknown_loose",
            .context = "reopened substituted engine show create",
        }
    );
    failures += expect_single_row_result(
        database,
        "SELECT id FROM unknown_loose",
        (struct expected_single_row_result){
            .columns = select_columns,
            .values = select_values,
            .column_count = sizeof(select_columns) / sizeof(select_columns[0]),
        },
        "reopened substituted engine row"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_independent_innodb_engine_handles(void) {
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "independent-a") != 0 ||
        make_test_path(second_path, sizeof(second_path), "independent-b") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first handle");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second handle");

    failures += execute_statement_ok(first, "CREATE DATABASE app");
    failures += execute_statement_ok(first, "USE app");
    failures += execute_statement_ok(first, "CREATE TABLE only_first (id INT) ENGINE=InnoDB");

    failures += execute_statement_ok(second, "CREATE DATABASE app");
    failures += execute_statement_ok(second, "USE app");
    failures += execute_statement_ok(second, "CREATE TABLE only_second (id INT) ENGINE=InnoDB");

    failures += expect_show_create_single_int(
        first,
        (struct expected_show_create_single_int){
            .sql = "SHOW CREATE TABLE only_first",
            .table_name = "only_first",
            .context = "first handle engine table",
        }
    );
    failures += expect_show_create_single_int(
        second,
        (struct expected_show_create_single_int){
            .sql = "SHOW CREATE TABLE only_second",
            .table_name = "only_second",
            .context = "second handle engine table",
        }
    );
    failures += expect_show_engines_result(first, "SHOW ENGINES", "first show engines");
    failures += expect_show_engines_result(second, "SHOW ENGINES", "second show engines");
    failures += expect_show_engine_status_result(
        first,
        "SHOW ENGINE InnoDB STATUS",
        "first show engine status"
    );
    failures += expect_show_engine_status_result(
        second,
        "SHOW ENGINE InnoDB STATUS",
        "second show engine status"
    );
    failures += expect_single_row_result(
        first,
        "SELECT @@default_storage_engine, @@global.default_storage_engine",
        (struct expected_single_row_result){
            .columns =
                (const char *const[]){
                    "@@default_storage_engine",
                    "@@global.default_storage_engine"
                },
            .values = (const char *const[]){"InnoDB", "InnoDB"},
            .column_count = independent_default_storage_engine_variable_column_count,
        },
        "first default storage engine variables"
    );
    failures += expect_single_row_result(
        second,
        "SELECT @@default_storage_engine, @@global.default_storage_engine",
        (struct expected_single_row_result){
            .columns =
                (const char *const[]){
                    "@@default_storage_engine",
                    "@@global.default_storage_engine"
                },
            .values = (const char *const[]){"InnoDB", "InnoDB"},
            .column_count = independent_default_storage_engine_variable_column_count,
        },
        "second default storage engine variables"
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int expect_show_engines_result(mylite_db *database, const char *sql, const char *context) {
    return expect_single_row_result(
        database,
        sql,
        (struct expected_single_row_result){
            .columns = show_engines_columns,
            .values = show_engines_values,
            .column_count = show_engines_column_count,
        },
        context
    );
}

static int expect_show_engine_status_result(
    mylite_db *database,
    const char *sql,
    const char *context
) {
    return expect_single_row_result(
        database,
        sql,
        (struct expected_single_row_result){
            .columns = show_engine_status_columns,
            .values = show_engine_status_values,
            .column_count = show_engine_status_column_count,
        },
        context
    );
}

static int expect_show_create_single_int(
    mylite_db *database,
    struct expected_show_create_single_int expected
) {
    char create_sql[show_create_sql_capacity];
    int written = snprintf(
        create_sql,
        sizeof(create_sql),
        "CREATE TABLE `%s` (\n"
        "  `id` int DEFAULT NULL\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
        expected.table_name
    );

    if (written < 0 || (size_t)written >= sizeof(create_sql)) {
        fprintf(stderr, "%s: failed to build expected SHOW CREATE TABLE text\n", expected.context);
        return 1;
    }

    return expect_show_create_exact(
        database,
        (struct expected_show_create_exact){
            .sql = expected.sql,
            .table_name = expected.table_name,
            .create_sql = create_sql,
            .context = expected.context,
        }
    );
}

static int expect_show_create_exact(
    mylite_db *database,
    struct expected_show_create_exact expected
) {
    const char *const values[show_create_column_count] = {expected.table_name, expected.create_sql};

    return expect_single_row_result(
        database,
        expected.sql,
        (struct expected_single_row_result){
            .columns = show_create_columns,
            .values = values,
            .column_count = show_create_column_count,
        },
        expected.context
    );
}

static int expect_show_warnings(
    mylite_db *database,
    const struct expected_warning_row *rows,
    size_t row_count,
    const char *context
) {
    static const char *const warning_columns[show_warnings_column_count] = {
        "Level",
        "Code",
        "Message",
    };
    mylite_result *result = NULL;
    int failures = execute_ok(database, "SHOW WARNINGS", &result);

    if (result == NULL) {
        return failures + 1;
    }

    failures +=
        expect_size(mylite_result_column_count(result), show_warnings_column_count, context);
    failures += expect_size(mylite_result_row_count(result), row_count, context);
    for (size_t column_index = 0U; column_index < show_warnings_column_count; ++column_index) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column_index),
            warning_columns[column_index],
            context
        );
    }
    for (size_t row_index = 0U; row_index < row_count; ++row_index) {
        const struct expected_warning_row *row = &rows[row_index];

        failures += expect_text_or_null(
            mylite_result_value_text(result, row_index, 0U),
            row->level,
            context
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row_index, 1U),
            row->code,
            context
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row_index, 2U),
            row->message,
            context
        );
    }

    mylite_result_free(result);
    return failures;
}

static int expect_single_row_result(
    mylite_db *database,
    const char *sql,
    struct expected_single_row_result expected,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += execute_ok(database, sql, &result);
    if (result == NULL) {
        return failures + 1;
    }

    failures += expect_size(mylite_result_column_count(result), expected.column_count, context);
    failures += expect_size(mylite_result_row_count(result), 1U, context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);
    for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column_index),
            expected.columns[column_index],
            context
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, 0U, column_index),
            expected.values[column_index],
            context
        );
    }

    mylite_result_free(result);
    return failures;
}

static int expect_row_count(mylite_db *database, int64_t expected, const char *context) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += execute_ok(database, "SELECT ROW_COUNT()", &result);
    if (result == NULL) {
        return failures + 1;
    }

    failures += expect_size(mylite_result_column_count(result), 1U, context);
    failures += expect_size(mylite_result_row_count(result), 1U, context);
    if (mylite_result_value_text(result, 0U, 0U) == NULL) {
        fprintf(stderr, "%s: expected row count value\n", context);
        failures += 1;
    } else {
        failures += expect_int64(
            strtoll(mylite_result_value_text(result, 0U, 0U), NULL, decimal_base),
            expected,
            context
        );
    }

    mylite_result_free(result);
    return failures;
}

static int expect_count_statement(mylite_db *database, struct expected_count_result expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    if (result == NULL) {
        return failures + 1;
    }

    failures += expect_size(mylite_result_row_count(result), 1U, expected.context);
    failures += expect_text_or_null(
        mylite_result_value_text(result, 0U, 0U),
        expected.expected,
        expected.context
    );
    failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);

    mylite_result_free(result);
    return failures;
}

static int expect_show_warning_count(
    mylite_db *database,
    const char *expected,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, "SHOW COUNT(*) WARNINGS", &result);

    if (result == NULL) {
        return failures + 1;
    }

    failures += expect_size(mylite_result_row_count(result), 1U, context);
    failures += expect_text_or_null(mylite_result_value_text(result, 0U, 0U), expected, context);

    mylite_result_free(result);
    return failures;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    mylite_result_free(result);
    return failures;
}

static int execute_statement_ok_with_warnings(
    mylite_db *database,
    const char *sql,
    size_t expected_warning_count
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (result == NULL) {
        return failures + 1;
    }

    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    failures += expect_size(mylite_result_row_count(result), 0U, sql);
    failures += expect_int64(mylite_result_affected_rows(result), 0, sql);
    failures += expect_size(mylite_result_warning_count(result), expected_warning_count, sql);

    mylite_result_free(result);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d/%s: %s\n",
            sql,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }
    if (out_result == NULL || *out_result == NULL) {
        fprintf(stderr, "%s: expected result object\n", sql);
        return 1;
    }

    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    return execute_error_with_length(database, sql, strlen(sql), expected, sql);
}

static int execute_error_with_length(
    mylite_db *database,
    const char *sql,
    size_t sql_size,
    struct expected_sql_error expected,
    const char *context
) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, sql_size, &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected error %d/%s, got success\n",
            context,
            expected.code,
            expected.sqlstate
        );
        mylite_result_free(result);
        return 1;
    }

    failures += expect_int(mylite_errcode(database), expected.code, context);
    failures += expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, context);
    failures += expect_text_contains(mylite_errmsg(database), expected.message_part, context);
    mylite_result_free(result);

    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_innodb_engine_%s_%d.mylite",
        name,
        current_process_id()
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "failed to build test path for %s\n", name);
        return 1;
    }

    return 0;
}

static int current_process_id(void) {
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related_path[test_path_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }

    (void)remove(related_path);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        fprintf(stderr, "%s: failed to open file\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "%s: failed to seek file\n", path);
        fclose(file);
        return 1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "%s: failed to read file\n", path);
        fclose(file);
        return 1;
    }

    fclose(file);
    return 0;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual == expected) {
        return 0;
    }

    fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
    return 1;
}

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }

    fprintf(stderr, "%s: expected %" PRId64 ", got %" PRId64 "\n", context, expected, actual);
    return 1;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }

    fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
    return 1;
}

static int expect_text_or_null(const char *actual, const char *expected, const char *context) {
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }

    fprintf(
        stderr,
        "%s: expected [%s], got [%s]\n",
        context,
        expected == NULL ? "NULL" : expected,
        actual == NULL ? "NULL" : actual
    );
    return 1;
}

static int expect_text_contains(const char *actual, const char *needle, const char *context) {
    if (actual != NULL && needle != NULL && strstr(actual, needle) != NULL) {
        return 0;
    }

    fprintf(
        stderr,
        "%s: expected [%s] to contain [%s]\n",
        context,
        actual == NULL ? "NULL" : actual,
        needle == NULL ? "NULL" : needle
    );
    return 1;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) == 0) {
        return 0;
    }

    fprintf(stderr, "%s: byte comparison failed\n", context);
    return 1;
}
