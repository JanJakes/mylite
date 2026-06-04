#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
#include "storage/mylite_file_format.h"

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
    fixed_variable_column_count = 19,
    diagnostics_column_count = 3,
    mysql_error_parse = 1064,
    mysql_error_unknown_system_variable = 1193,
    mysql_error_variable_cant_be_set = 1231,
    mysql_error_session_variable_only = 1238,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

static const char default_sql_mode[] =
    "ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,"
    "ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION";

static int test_set_fixed_system_variables_success_and_file_safety(void);
static int test_set_fixed_system_variables_diagnostics(void);
static int test_set_fixed_system_variables_independent_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_set_ok(mylite_db *database, const char *sql);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_uint64(uint64_t actual, uint64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_true(int condition, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_set_fixed_system_variables_success_and_file_safety();
    failures += test_set_fixed_system_variables_diagnostics();
    failures += test_set_fixed_system_variables_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_set_fixed_system_variables_success_and_file_safety(void) {
    static const char *const fixed_values[] = {
        "1", "1", "1", "1", "1", "1", "1", "1", "0", "0", "0", "0", "0", "0", "0", default_sql_mode,
        "0", "0", "0",
    };
    static const char *const reopened_values[] = {
        "1", "1", "1",  "1", "1", "1", "1", "1",
        "0", "0", "0",  "0", "0", "0", "0", default_sql_mode,
        "0", "0", "-1",
    };
    static const char *const warning_count_values[] = {"0"};
    static const char *const bare_keyword_values[] = {
        "InnoDB",
        "utf8mb4_0900_ai_ci",
        "FULL",
        "OWN_GTID",
        "STATE",
        "FORCED",
    };
    static const char *const user_variable_increment_value[] = {"2"};
    static const char *const restored_charset_value[] = {"utf8mb4"};
    static const char *const dump_restore_values[] = {"1", "1", default_sql_mode};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation_before_set = 0U;
    uint64_t sqlite_generation_before_set = 0U;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open fixed SET file");
    session = mylite_connection_session_state(database);
    if (session != NULL) {
        catalog_generation_before_set = session->catalog_generation;
        sqlite_generation_before_set = session->sqlite_schema_generation;
    }

    failures += expect_set_ok(database, "SET autocommit = 1");
    failures += expect_set_ok(database, "SET SESSION `autocommit` = ON");
    failures += expect_set_ok(database, "SET LOCAL autocommit = TRUE");
    failures += expect_set_ok(database, "SET @@autocommit = DEFAULT");
    failures += expect_set_ok(database, "SET @@session.sql_notes = 1");
    failures += expect_set_ok(database, "SET sql_quote_show_create = DEFAULT");
    failures += expect_set_ok(database, "SET sql_big_selects = 1");
    failures += expect_set_ok(database, "SET explicit_defaults_for_timestamp = DEFAULT");
    failures += expect_set_ok(database, "SET SESSION explicit_defaults_for_timestamp = ON");
    failures += expect_set_ok(database, "SET LOCAL explicit_defaults_for_timestamp = TRUE");
    failures += expect_set_ok(database, "SET sql_log_bin = ON");
    failures += expect_set_ok(database, "SET foreign_key_checks = TRUE");
    failures += expect_set_ok(database, "SET unique_checks = ON");
    failures += expect_set_ok(database, "SET sql_warnings = 0");
    failures += expect_set_ok(database, "SET sql_safe_updates = FALSE");
    failures += expect_set_ok(database, "SET sql_buffer_result = OFF");
    failures += expect_set_ok(database, "SET sql_auto_is_null = DEFAULT");
    failures += expect_set_ok(database, "SET sql_generate_invisible_primary_key = 0");
    failures += expect_set_ok(database, "SET sql_log_off = FALSE");
    failures += expect_set_ok(database, "SET sql_require_primary_key = OFF");
    failures += expect_set_ok(database, "SET sql_mode = DEFAULT");
    failures += expect_set_ok(
        database,
        "SET @@session.sql_mode = "
        "'ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,"
        "ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION'"
    );
    failures += expect_set_ok(database, "SET default_storage_engine = InnoDB");
    failures += expect_set_ok(database, "SET default_collation_for_utf8mb4 = utf8mb4_0900_ai_ci");
    failures += expect_set_ok(database, "SET resultset_metadata = FULL");
    failures += expect_set_ok(database, "SET session_track_gtids = OWN_GTID");
    failures += expect_set_ok(database, "SET session_track_transaction_info = STATE");
    failures += expect_set_ok(database, "SET use_secondary_engine = FORCED");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@default_storage_engine, @@default_collation_for_utf8mb4, "
                   "@@resultset_metadata, @@session_track_gtids, "
                   "@@session_track_transaction_info, @@use_secondary_engine",
            .values = bare_keyword_values,
            .column_count = 6U,
            .row_count = 1U,
            .context = "bare keyword SET values read back",
        }
    );
    failures += expect_set_ok(database, "SET @my_var = 1");
    failures += expect_set_ok(database, "SET @my_var = @my_var + 1");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @my_var",
            .values = user_variable_increment_value,
            .column_count = 1U,
            .row_count = 1U,
            .context = "SET user variable increment",
        }
    );
    failures += expect_set_ok(
        database,
        "/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;"
    );
    failures += expect_set_ok(database, "/*!50503 SET character_set_client = latin1 */;");
    failures += expect_set_ok(database, "/*!40101 SET character_set_client = @OLD_CHARACTER_SET_CLIENT */;");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@character_set_client",
            .values = restored_charset_value,
            .column_count = 1U,
            .row_count = 1U,
            .context = "executable comment SET restores charset",
        }
    );
    failures += expect_set_ok(
        database,
        "/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;"
    );
    failures += expect_set_ok(
        database,
        "/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;"
    );
    failures += expect_set_ok(
        database,
        "/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;"
    );
    failures += expect_set_ok(database, "/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;");
    failures += expect_set_ok(database, "/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;");
    failures += expect_set_ok(database, "/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@unique_checks, @@sql_notes, @@sql_mode",
            .values = dump_restore_values,
            .column_count = 3U,
            .row_count = 1U,
            .context = "executable comment SET restores dump toggles",
        }
    );
    failures += expect_set_ok(database, "SET sql_mode = DEFAULT");

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@autocommit, @@foreign_key_checks, @@unique_checks, "
                   "@@sql_quote_show_create, @@sql_notes, @@sql_big_selects, @@sql_log_bin, "
                   "@@explicit_defaults_for_timestamp, @@sql_safe_updates, @@sql_warnings, "
                   "@@sql_buffer_result, @@sql_auto_is_null, "
                   "@@sql_generate_invisible_primary_key, @@sql_log_off, "
                   "@@sql_require_primary_key, @@sql_mode, @@warning_count, @@error_count, "
                   "ROW_COUNT()",
            .values = fixed_values,
            .column_count = fixed_variable_column_count,
            .row_count = 1U,
            .context = "fixed SET keeps baseline values",
        }
    );

    session = mylite_connection_session_state(database);
    if (session != NULL) {
        failures += expect_uint64(
            session->catalog_generation,
            catalog_generation_before_set,
            "fixed SET leaves catalog generation"
        );
        failures += expect_uint64(
            session->sqlite_schema_generation,
            sqlite_generation_before_set,
            "fixed SET leaves SQLite schema generation"
        );
    }

    failures += execute_statement_ok(database, "SHOW PROCESSLIST");
    failures += expect_set_ok(database, "SET autocommit = 1");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COUNT(*) WARNINGS",
            .values = warning_count_values,
            .column_count = 1U,
            .row_count = 1U,
            .context = "fixed SET clears previous diagnostics",
        }
    );

    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "fixed SET preserves preamble"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen fixed SET file");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@autocommit, @@foreign_key_checks, @@unique_checks, "
                   "@@sql_quote_show_create, @@sql_notes, @@sql_big_selects, @@sql_log_bin, "
                   "@@explicit_defaults_for_timestamp, @@sql_safe_updates, @@sql_warnings, "
                   "@@sql_buffer_result, @@sql_auto_is_null, "
                   "@@sql_generate_invisible_primary_key, @@sql_log_off, "
                   "@@sql_require_primary_key, @@sql_mode, @@warning_count, @@error_count, "
                   "ROW_COUNT()",
            .values = reopened_values,
            .column_count = fixed_variable_column_count,
            .row_count = 1U,
            .context = "reopened fixed SET values",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_set_fixed_system_variables_diagnostics(void) {
    static const char *const diagnostic_values[] = {"1", "1", "-1"};
    static const char *const sql_warnings_on[] = {"1"};
    static const char *const explicit_defaults_off[] = {"0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += execute_error(
        database,
        "SET no_such_mylite_variable = 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable 'no_such_mylite_variable'",
        }
    );
    failures += execute_error(
        database,
        "SET version = '8.4.9'",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'version' is a read only variable",
        }
    );
    failures += execute_error(
        database,
        "SET SESSION version = '8.4.9'",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'version' is a read only variable",
        }
    );
    failures += execute_error(
        database,
        "SET lower_case_table_names = 0",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'lower_case_table_names' is a read only variable",
        }
    );
    failures += execute_error(
        database,
        "SET lower_case_file_system = 0",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'lower_case_file_system' is a read only variable",
        }
    );
    failures += execute_error(
        database,
        "SET SESSION lower_case_table_names = 0",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'lower_case_table_names' is a read only variable",
        }
    );
    failures += execute_error(
        database,
        "SET SESSION lower_case_file_system = 0",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'lower_case_file_system' is a read only variable",
        }
    );
    failures += execute_error(
        database,
        "SET LOCAL lower_case_table_names = 0",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'lower_case_table_names' is a read only variable",
        }
    );
    failures += execute_error(
        database,
        "SET LOCAL lower_case_file_system = 0",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'lower_case_file_system' is a read only variable",
        }
    );
    failures += execute_error(
        database,
        "SET GLOBAL lower_case_table_names = 0",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'lower_case_table_names' is a read only variable",
        }
    );
    failures += execute_error(
        database,
        "SET GLOBAL lower_case_file_system = 0",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'lower_case_file_system' is a read only variable",
        }
    );
    failures += execute_error(
        database,
        "SET @@lower_case_table_names = 0",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'lower_case_table_names' is a read only variable",
        }
    );
    failures += execute_error(
        database,
        "SET @@lower_case_file_system = 0",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'lower_case_file_system' is a read only variable",
        }
    );
    failures += execute_error(
        database,
        "SET @@SESSION.lower_case_table_names = 0",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'lower_case_table_names' is a read only variable",
        }
    );
    failures += execute_error(
        database,
        "SET @@SESSION.lower_case_file_system = 0",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'lower_case_file_system' is a read only variable",
        }
    );
    failures += execute_error(
        database,
        "SET @@LOCAL.lower_case_table_names = 0",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'lower_case_table_names' is a read only variable",
        }
    );
    failures += execute_error(
        database,
        "SET @@LOCAL.lower_case_file_system = 0",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'lower_case_file_system' is a read only variable",
        }
    );
    failures += execute_error(
        database,
        "SET @@GLOBAL.lower_case_table_names = 0",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'lower_case_table_names' is a read only variable",
        }
    );
    failures += execute_error(
        database,
        "SET @@GLOBAL.lower_case_file_system = 0",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'lower_case_file_system' is a read only variable",
        }
    );
    failures += execute_error(
        database,
        "SET GLOBAL autocommit = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SET GLOBAL system variable assignment is not supported",
        }
    );
    failures += execute_error(
        database,
        "SET @@global.autocommit = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SET GLOBAL system variable assignment is not supported",
        }
    );
    failures += execute_error(
        database,
        "SET @@`session`.autocommit = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "unsupported quoted system variable scope",
        }
    );
    failures += execute_error(
        database,
        "SET autocommit = 0",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SET supports only fixed no-op system variable assignments",
        }
    );
    failures += expect_set_ok(database, "SET sql_warnings = 1");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@sql_warnings",
            .values = sql_warnings_on,
            .column_count = 1U,
            .row_count = 1U,
            .context = "mutable sql_warnings readback",
        }
    );
    failures += expect_set_ok(database, "SET explicit_defaults_for_timestamp = OFF");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@explicit_defaults_for_timestamp",
            .values = explicit_defaults_off,
            .column_count = 1U,
            .row_count = 1U,
            .context = "mutable explicit_defaults_for_timestamp readback",
        }
    );
    failures += expect_set_ok(database, "SET explicit_defaults_for_timestamp = 0");
    failures += execute_error(
        database,
        "SET sql_mode = 'BOGUS'",
        (struct expected_sql_error){
            .code = mysql_error_variable_cant_be_set,
            .sqlstate = "42000",
            .message_part = "Variable 'sql_mode' can't be set to the value of 'BOGUS'",
        }
    );
    failures += execute_error(
        database,
        "SET autocommit = '1'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SET supports only fixed no-op system variable assignments",
        }
    );
    failures += execute_error(
        database,
        "SET autocommit = NULL",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SET supports only fixed no-op system variable assignments",
        }
    );
    failures += expect_set_ok(database, "SET autocommit = 1, sql_notes = 1");
    failures += execute_error(
        database,
        "SET autocommit := 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SET app.autocommit = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count, @@error_count, ROW_COUNT()",
            .values = diagnostic_values,
            .column_count = diagnostics_column_count,
            .row_count = 1U,
            .context = "failed fixed SET diagnostics",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_set_fixed_system_variables_independent_handles(void) {
    static const char *const values[] = {"0", "0", "0"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first handle");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second handle");
    failures += execute_error(
        first,
        "SET autocommit = 0",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SET supports only fixed no-op system variable assignments",
        }
    );
    failures += expect_set_ok(second, "SET autocommit = 1");
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT @@warning_count, @@error_count, ROW_COUNT()",
            .values = values,
            .column_count = diagnostics_column_count,
            .row_count = 1U,
            .context = "second fixed SET handle diagnostics",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);

    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "execute '%s': expected OK, got %d / %d %s %s\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }

    return 0;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    mylite_result_free(result);
    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "execute '%s': expected MYLITE_ERROR, got %d\n", sql, rc);
        failures += 1;
    }
    failures += expect_true(result == NULL, "failed execute leaves result null");
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "SQLSTATE");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
    mylite_result_free(result);

    return failures;
}

static int expect_set_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "fixed SET column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "fixed SET row count");
    failures += expect_int64(mylite_result_affected_rows(result), 0, "fixed SET affected rows");
    failures += expect_size(mylite_result_warning_count(result), 0U, "fixed SET warning count");
    mylite_result_free(result);

    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t row = 0U; row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            size_t value_index = (row * query.column_count) + column;

            failures +=
                expect_result_value(result, row, column, query.values[value_index], query.context);
        }
    }
    failures += expect_int64(mylite_result_affected_rows(result), 0, query.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, query.context);
    mylite_result_free(result);

    return failures;
}

static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
) {
    const char *actual = mylite_result_value_text(result, row, column);

    if (expected == NULL) {
        return expect_true(actual == NULL, context);
    }

    return expect_text(actual, expected, context);
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    const char *directory = getenv("TMPDIR");
    int written = 0;

    if (directory == NULL || directory[0] == '\0') {
        directory = getenv("TEMP");
    }
    if (directory == NULL || directory[0] == '\0') {
        directory = ".";
    }

    written = snprintf(
        path,
        path_size,
        "%s/mylite_set_fixed_system_variables_%d_%s.mylite",
        directory,
        current_process_id(),
        name
    );
    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path is too long for %s\n", name);
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
    remove_with_suffix(path, "-journal");
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
        fprintf(stderr, "failed to open %s for reading\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "failed to seek %s\n", path);
        (void)fclose(file);
        return 1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "failed to read %zu bytes from %s\n", size, path);
        (void)fclose(file);
        return 1;
    }
    if (fclose(file) != 0) {
        fprintf(stderr, "failed to close %s\n", path);
        return 1;
    }

    return 0;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual != expected) {
        fprintf(
            stderr,
            "%s: expected %lld, got %lld\n",
            context,
            (long long)expected,
            (long long)actual
        );
        return 1;
    }

    return 0;
}

static int expect_uint64(uint64_t actual, uint64_t expected, const char *context) {
    if (actual != expected) {
        fprintf(
            stderr,
            "%s: expected %llu, got %llu\n",
            context,
            (unsigned long long)expected,
            (unsigned long long)actual
        );
        return 1;
    }

    return 0;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_true(int condition, const char *context) {
    if (!condition) {
        fprintf(stderr, "%s: expected true\n", context);
        return 1;
    }

    return 0;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected text '%s', got '%s'\n",
            context,
            expected == NULL ? "(null)" : expected,
            actual == NULL ? "(null)" : actual
        );
        return 1;
    }

    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected '%s' to contain '%s'\n",
            context,
            actual == NULL ? "(null)" : actual,
            needle == NULL ? "(null)" : needle
        );
        return 1;
    }

    return 0;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: byte comparison failed\n", context);
        return 1;
    }

    return 0;
}
