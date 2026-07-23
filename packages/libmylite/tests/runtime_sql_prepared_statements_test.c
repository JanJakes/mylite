#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

enum {
    mysql_error_parse = 1064,
    mysql_error_incorrect_arguments = 1210,
    mysql_error_unknown_prepared_statement_handler = 1243,
    mysql_error_prepared_command_not_supported = 1295,
    test_path_capacity = 256,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_query {
    const char *sql;
    const char *const *columns;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    int64_t affected_rows;
    size_t warning_count;
    const char *context;
};

static int test_prepared_statement_lifecycle(void);
static int test_prepared_statement_source_variables_and_dml(void);
static int test_prepared_statement_prepare_context(void);
static int test_prepared_statement_diagnostics(void);
static int test_prepared_statement_reopen_is_nonpersistent(void);
static int test_prepared_statement_independent_handles(void);
static int setup_app_table(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_statement_result(
    mylite_db *database,
    const char *sql,
    int64_t affected_rows,
    const char *context
);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);

int main(void) {
    int failures = 0;

    failures += test_prepared_statement_lifecycle();
    failures += test_prepared_statement_source_variables_and_dml();
    failures += test_prepared_statement_prepare_context();
    failures += test_prepared_statement_diagnostics();
    failures += test_prepared_statement_reopen_is_nonpersistent();
    failures += test_prepared_statement_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_prepared_statement_lifecycle(void) {
    char path[test_path_capacity];
    static const char *const select_columns[] = {"id", "v"};
    static const char *const select_values[] = {"2", "two"};
    static const char *const row_count_columns[] = {
        "ROW_COUNT()",
        "@@warning_count",
        "@@error_count",
    };
    static const char *const select_row_count_values[] = {"-1", "0", "0"};
    static const char *const deallocate_row_count_values[] = {"0", "0", "0"};
    mylite_db *database = NULL;
    int failures = mylite_test_make_path(path, sizeof(path), "prepared-lifecycle");

    if (failures != 0) {
        return failures;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open lifecycle");
    if (failures != 0) {
        remove_related_files(path);
        return failures;
    }
    failures += setup_app_table(database);
    failures += expect_statement_result(
        database,
        "PREPARE stmt FROM 'SELECT id, v FROM t WHERE id = ?'",
        0,
        "prepare select"
    );
    failures += execute_statement_ok(database, "SET @id = 2");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "EXECUTE STMT USING @id",
            .columns = select_columns,
            .values = select_values,
            .column_count = 2U,
            .row_count = 1U,
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "execute prepared select",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count, @@error_count",
            .columns = row_count_columns,
            .values = select_row_count_values,
            .column_count = 3U,
            .row_count = 1U,
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "execute select row count",
        }
    );
    failures += expect_statement_result(database, "DROP PREPARE stmt", 0, "drop prepare");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count, @@error_count",
            .columns = row_count_columns,
            .values = deallocate_row_count_values,
            .column_count = 3U,
            .row_count = 1U,
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "deallocate row count",
        }
    );
    failures += execute_error(
        database,
        "EXECUTE stmt",
        (struct expected_sql_error){
            .code = mysql_error_unknown_prepared_statement_handler,
            .sqlstate = "HY000",
            .message_part = "Unknown prepared statement handler",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_prepared_statement_source_variables_and_dml(void) {
    char path[test_path_capacity];
    static const unsigned char binary_parameter[] = {'a', 0U, 'b'};
    static const char *const updated_columns[] = {"id", "v"};
    static const char *const updated_values[] = {"2", "two-updated"};
    static const char *const all_columns[] = {"id", "v"};
    static const char *const all_values[] = {"1", "one", "2", "two-updated", "3", NULL};
    static const char *const null_string_columns[] = {"id"};
    static const char *const null_string_values[] = {"1", "3"};
    static const char *const escaped_columns[] = {"id"};
    static const char *const default_backslash_values[] = {"1"};
    static const char *const no_backslash_escape_values[] = {"2"};
    static const char *const decimal_columns[] = {"v"};
    static const char *const decimal_values[] = {"-1.50", "1.00"};
    mylite_db *database = NULL;
    mylite_result *binary_result = NULL;
    int failures = mylite_test_make_path(path, sizeof(path), "prepared-source-dml");

    if (failures != 0) {
        return failures;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open source dml");
    if (failures != 0) {
        remove_related_files(path);
        return failures;
    }
    failures += setup_app_table(database);
    failures += execute_statement_ok(database, "SET @sql = 'UPDATE t SET v = ? WHERE id = ?'");
    failures += expect_statement_result(database, "PREPARE upd FROM @sql", 0, "prepare update");
    failures += execute_statement_ok(database, "SET @v = 'two-updated', @id = 2");
    failures += expect_statement_result(database, "EXECUTE upd USING @v, @id", 1, "execute update");
    failures += expect_statement_result(database, "DEALLOCATE PREPARE upd", 0, "deallocate update");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t WHERE id = 2",
            .columns = updated_columns,
            .values = updated_values,
            .column_count = 2U,
            .row_count = 1U,
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "updated row readback",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .columns = all_columns,
            .values = all_values,
            .column_count = 2U,
            .row_count = 3U,
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "all rows after prepared update",
        }
    );

    failures += execute_statement_ok(
        database,
        "PREPARE nullable FROM 'SELECT id FROM t WHERE v <=> ? ORDER BY id'"
    );
    failures += execute_statement_ok(database, "SET @quoted = 'one', @missing_source = @never");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "EXECUTE nullable USING @quoted",
            .columns = null_string_columns,
            .values = null_string_values,
            .column_count = 1U,
            .row_count = 1U,
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "string parameter predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "EXECUTE nullable USING @missing_source",
            .columns = null_string_columns,
            .values = &null_string_values[1],
            .column_count = 1U,
            .row_count = 1U,
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "null parameter predicate",
        }
    );

    failures += execute_statement_ok(database, "CREATE TABLE decimal_params (v DECIMAL(6,2))");
    failures += execute_statement_ok(
        database,
        "PREPARE decimals FROM 'INSERT INTO decimal_params VALUES (?), (?)'"
    );
    failures += execute_statement_ok(database, "SET @d = 1.0, @nd = -1.50");
    failures +=
        expect_statement_result(database, "EXECUTE decimals USING @nd, @d", 2, "decimal insert");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT v FROM decimal_params",
            .columns = decimal_columns,
            .values = decimal_values,
            .column_count = 1U,
            .row_count = 2U,
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "fixed decimal parameter readback",
        }
    );
    failures +=
        expect_statement_result(database, "DEALLOCATE PREPARE decimals", 0, "deallocate decimal");

    failures += execute_statement_ok(database, "SELECT UNHEX('610062') INTO @binary_value");
    failures += execute_statement_ok(database, "PREPARE binary_value FROM 'SELECT ? AS payload'");
    failures += execute_ok(database, "EXECUTE binary_value USING @binary_value", &binary_result);
    if (binary_result != NULL) {
        failures += mylite_test_expect_size(
            mylite_result_value_size(binary_result, 0U, 0U),
            sizeof(binary_parameter),
            "embedded NUL parameter size"
        );
        if (mylite_result_value_bytes(binary_result, 0U, 0U) == NULL ||
            memcmp(
                mylite_result_value_bytes(binary_result, 0U, 0U),
                binary_parameter,
                sizeof(binary_parameter)
            ) != 0) {
            fprintf(stderr, "embedded NUL parameter: unexpected value bytes\n");
            ++failures;
        }
    }
    mylite_result_free(binary_result);
    binary_result = NULL;
    failures += expect_statement_result(
        database,
        "DEALLOCATE PREPARE binary_value",
        0,
        "deallocate binary value"
    );

    failures +=
        execute_statement_ok(database, "PREPARE escstmt FROM 'SELECT id FROM t WHERE v <=> ?'");
    failures += execute_statement_ok(database, "UPDATE t SET v = 'a\\\\b' WHERE id = 1");
    failures += execute_statement_ok(database, "SET @escaped = 'a\\\\b'");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "EXECUTE escstmt USING @escaped",
            .columns = escaped_columns,
            .values = default_backslash_values,
            .column_count = 1U,
            .row_count = 1U,
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "default backslash parameter escaping",
        }
    );
    failures += execute_statement_ok(database, "SET SESSION sql_mode = 'NO_BACKSLASH_ESCAPES'");
    failures += execute_statement_ok(database, "UPDATE t SET v = 'a\\\\b' WHERE id = 2");
    failures += execute_statement_ok(database, "SET @escaped = 'a\\\\b'");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "EXECUTE escstmt USING @escaped",
            .columns = escaped_columns,
            .values = no_backslash_escape_values,
            .column_count = 1U,
            .row_count = 1U,
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "no backslash escapes parameter escaping",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_prepared_statement_prepare_context(void) {
    char path[test_path_capacity];
    static const char *const default_mode_columns[] = {"mode_value", "current_mode"};
    static const char *const default_mode_values[] = {"literal", "ANSI_QUOTES"};
    static const char *const concat_mode_columns[] = {"mode_value", "current_mode"};
    static const char *const concat_mode_values[] = {"10", "ANSI_QUOTES"};
    static const char *const schema_columns[] = {"v", "current_database"};
    static const char *const prepared_schema_values[] = {"first", "prepare_a"};
    static const char *const current_schema_values[] = {"second", "prepare_b"};
    static const char *const no_schema_columns[] = {"current_database"};
    static const char *const no_schema_values[] = {NULL};
    static const char *const collation_columns[] = {
        "literal_collation",
        "current_collation",
    };
    static const char *const case_sensitive_values[] = {
        "utf8mb4_0900_as_cs",
        "utf8mb4_0900_ai_ci",
    };
    static const char *const case_insensitive_values[] = {
        "utf8mb4_0900_ai_ci",
        "utf8mb4_0900_as_cs",
    };
    mylite_db *database = NULL;
    int failures = mylite_test_make_path(path, sizeof(path), "prepared-context");

    if (failures != 0) {
        return failures;
    }
    remove_related_files(path);
    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open prepare context");
    if (failures != 0) {
        remove_related_files(path);
        return failures;
    }

    failures += execute_statement_ok(database, "SET SESSION sql_mode = ''");
    failures += execute_statement_ok(
        database,
        "PREPARE mode_default FROM "
        "'SELECT \"literal\" AS mode_value, @@SESSION.sql_mode AS current_mode'"
    );
    failures += execute_statement_ok(database, "SET SESSION sql_mode = 'ANSI_QUOTES'");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "EXECUTE mode_default",
            .columns = default_mode_columns,
            .values = default_mode_values,
            .column_count = 2U,
            .row_count = 1U,
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "prepare-time default sql mode",
        }
    );
    failures += execute_statement_ok(database, "SET SESSION sql_mode = 'PIPES_AS_CONCAT'");
    failures += execute_statement_ok(
        database,
        "SET @mode_source = "
        "'SELECT 1 || 0 AS mode_value, @@SESSION.sql_mode AS current_mode'"
    );
    failures += execute_statement_ok(database, "PREPARE mode_concat FROM @mode_source");
    failures += execute_statement_ok(database, "SET SESSION sql_mode = 'ANSI_QUOTES'");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "EXECUTE mode_concat",
            .columns = concat_mode_columns,
            .values = concat_mode_values,
            .column_count = 2U,
            .row_count = 1U,
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "prepare-time concat sql mode",
        }
    );

    failures += execute_statement_ok(database, "CREATE DATABASE prepare_a");
    failures += execute_statement_ok(database, "CREATE DATABASE prepare_b");
    failures += execute_statement_ok(database, "USE prepare_a");
    failures += execute_statement_ok(database, "CREATE TABLE schema_context (v VARCHAR(20))");
    failures += execute_statement_ok(database, "INSERT INTO schema_context VALUES ('first')");
    failures += execute_statement_ok(database, "USE prepare_b");
    failures += execute_statement_ok(database, "CREATE TABLE schema_context (v VARCHAR(20))");
    failures += execute_statement_ok(database, "INSERT INTO schema_context VALUES ('second')");
    failures += execute_statement_ok(database, "USE prepare_a");
    failures += execute_statement_ok(
        database,
        "PREPARE schema_stmt FROM "
        "'SELECT v, DATABASE() AS current_database FROM schema_context'"
    );
    failures += execute_statement_ok(database, "USE prepare_b");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "EXECUTE schema_stmt",
            .columns = schema_columns,
            .values = prepared_schema_values,
            .column_count = 2U,
            .row_count = 1U,
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "prepare-time default database",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT v, DATABASE() AS current_database FROM schema_context",
            .columns = schema_columns,
            .values = current_schema_values,
            .column_count = 2U,
            .row_count = 1U,
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "execute-time database restored",
        }
    );
    failures += execute_statement_ok(database, "CREATE DATABASE prepare_drop");
    failures += execute_statement_ok(
        database,
        "PREPARE drop_current_schema FROM 'DROP DATABASE prepare_drop'"
    );
    failures += execute_statement_ok(database, "USE prepare_drop");
    failures += execute_statement_ok(database, "EXECUTE drop_current_schema");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DATABASE() AS current_database",
            .columns = no_schema_columns,
            .values = no_schema_values,
            .column_count = 1U,
            .row_count = 1U,
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "dropped execute-time database remains cleared",
        }
    );

    failures += execute_statement_ok(database, "SET NAMES utf8mb4 COLLATE utf8mb4_0900_as_cs");
    failures += execute_statement_ok(
        database,
        "PREPARE collation_cs FROM "
        "'SELECT COLLATION(''x'') AS literal_collation, "
        "@@SESSION.collation_connection AS current_collation'"
    );
    failures +=
        execute_statement_ok(database, "SET SESSION collation_connection = 'utf8mb4_0900_ai_ci'");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "EXECUTE collation_cs",
            .columns = collation_columns,
            .values = case_sensitive_values,
            .column_count = 2U,
            .row_count = 1U,
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "prepare-time case-sensitive collation",
        }
    );
    failures += execute_statement_ok(
        database,
        "PREPARE collation_ci FROM "
        "'SELECT COLLATION(''x'') AS literal_collation, "
        "@@SESSION.collation_connection AS current_collation'"
    );
    failures +=
        execute_statement_ok(database, "SET SESSION collation_connection = 'utf8mb4_0900_as_cs'");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "EXECUTE collation_ci",
            .columns = collation_columns,
            .values = case_insensitive_values,
            .column_count = 2U,
            .row_count = 1U,
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "prepare-time case-insensitive collation",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_prepared_statement_diagnostics(void) {
    static const char *const one_columns[] = {"1"};
    static const char *const one_values[] = {"1"};
    mylite_db *database = NULL;
    int failures =
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open diagnostics");

    if (failures != 0) {
        return failures;
    }
    failures +=
        expect_statement_result(database, "PREPARE repl FROM 'SELECT 1'", 0, "prepare repl");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "EXECUTE repl",
            .columns = one_columns,
            .values = one_values,
            .column_count = 1U,
            .row_count = 1U,
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "execute replacement before failure",
        }
    );
    failures += execute_error(
        database,
        "PREPARE repl FROM 'SELECT FROM'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "EXECUTE repl",
        (struct expected_sql_error){
            .code = mysql_error_unknown_prepared_statement_handler,
            .sqlstate = "HY000",
            .message_part = "Unknown prepared statement handler",
        }
    );
    failures += execute_error(
        database,
        "PREPARE arg_count FROM 'SELECT ?'; EXECUTE arg_count",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near 'EXECUTE arg_count'",
        }
    );
    failures += expect_statement_result(
        database,
        "PREPARE arg_count FROM 'SELECT ?'",
        0,
        "prepare arg count"
    );
    failures += execute_error(
        database,
        "EXECUTE arg_count",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_arguments,
            .sqlstate = "HY000",
            .message_part = "Incorrect arguments to EXECUTE",
        }
    );
    failures += execute_error(
        database,
        "SET @a = 1, @b = 2; EXECUTE arg_count USING @a, @b",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near 'EXECUTE arg_count USING @a, @b'",
        }
    );
    failures += execute_statement_ok(database, "SET @a = 1, @b = 2");
    failures += execute_error(
        database,
        "EXECUTE arg_count USING @a, @b",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_arguments,
            .sqlstate = "HY000",
            .message_part = "Incorrect arguments to EXECUTE",
        }
    );
    failures += execute_error(
        database,
        "EXECUTE arg_count USING 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT ?",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "PREPARE nullsrc FROM @missing",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "PREPARE nested FROM 'PREPARE x FROM ''SELECT 1'''",
        (struct expected_sql_error){
            .code = mysql_error_prepared_command_not_supported,
            .sqlstate = "HY000",
            .message_part = "This command is not supported in the prepared statement protocol yet",
        }
    );
    failures += execute_error(
        database,
        "PREPARE multi FROM 'SELECT 1; SELECT 2'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_prepared_statement_reopen_is_nonpersistent(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = mylite_test_make_path(path, sizeof(path), "prepared-reopen");

    if (failures != 0) {
        return failures;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open prepared file");
    failures += setup_app_table(database);
    failures += expect_statement_result(database, "PREPARE p FROM 'SELECT 1'", 0, "prepare file");
    mylite_close(database);

    database = NULL;
    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen prepared file");
    failures += execute_error(
        database,
        "EXECUTE p",
        (struct expected_sql_error){
            .code = mysql_error_unknown_prepared_statement_handler,
            .sqlstate = "HY000",
            .message_part = "Unknown prepared statement handler",
        }
    );
    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_prepared_statement_independent_handles(void) {
    static const char *const first_columns[] = {"1"};
    static const char *const second_columns[] = {"2"};
    static const char *const first_values[] = {"1"};
    static const char *const second_values[] = {"2"};
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures =
        mylite_test_expect_int(mylite_open_memory(&first), MYLITE_OK, "open first handle");

    failures +=
        mylite_test_expect_int(mylite_open_memory(&second), MYLITE_OK, "open second handle");
    if (failures != 0) {
        mylite_close(first);
        mylite_close(second);
        return failures;
    }

    failures += expect_statement_result(first, "PREPARE p FROM 'SELECT 1'", 0, "first prepare");
    failures += expect_statement_result(second, "PREPARE p FROM 'SELECT 2'", 0, "second prepare");
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "EXECUTE p",
            .columns = first_columns,
            .values = first_values,
            .column_count = 1U,
            .row_count = 1U,
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "first prepared state",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "EXECUTE p",
            .columns = second_columns,
            .values = second_values,
            .column_count = 1U,
            .row_count = 1U,
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "second prepared state",
        }
    );

    mylite_close(first);
    mylite_close(second);
    return failures;
}

static int setup_app_table(mylite_db *database) {
    int failures = execute_statement_ok(database, "CREATE DATABASE app");

    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(database, "CREATE TABLE t (id INT, v VARCHAR(20))");
    failures += execute_statement_ok(database, "INSERT INTO t VALUES (1,'one'),(2,'two'),(3,NULL)");
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d/%s/%s\n",
            sql,
            rc,
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }
    if (out_result != NULL) {
        *out_result = result;
    } else {
        mylite_result_free(result);
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

    if (rc == MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected error %d/%s, got success\n",
            sql,
            expected.code,
            expected.sqlstate
        );
        mylite_result_free(result);
        return 1;
    }
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, "error code");
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, "sqlstate");
    failures += mylite_test_expect_contains(
        mylite_errmsg(database),
        expected.message_part,
        "error message"
    );
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    size_t value_index = 0U;
    int failures = execute_ok(database, expected.sql, &result);

    if (failures != 0) {
        mylite_result_free(result);
        return failures;
    }
    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        expected.column_count,
        expected.context
    );
    failures += mylite_test_expect_size(
        mylite_result_row_count(result),
        expected.row_count,
        expected.context
    );
    failures += mylite_test_expect_int64(
        mylite_result_affected_rows(result),
        expected.affected_rows,
        expected.context
    );
    failures += mylite_test_expect_size(
        mylite_result_warning_count(result),
        expected.warning_count,
        expected.context
    );
    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += mylite_test_expect_text(
            mylite_result_column_name(result, column),
            expected.columns[column],
            expected.context
        );
    }
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            failures += expect_result_value(
                result,
                row,
                column,
                expected.values[value_index],
                expected.context
            );
            ++value_index;
        }
    }

    mylite_result_free(result);
    return failures;
}

static int expect_statement_result(
    mylite_db *database,
    const char *sql,
    int64_t affected_rows,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, context);
        failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, context);
        failures +=
            mylite_test_expect_int64(mylite_result_affected_rows(result), affected_rows, context);
        failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, context);
    }
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
        if (actual != NULL) {
            fprintf(stderr, "%s: expected NULL at %zu/%zu, got %s\n", context, row, column, actual);
            return 1;
        }
        return 0;
    }

    return mylite_test_expect_text(actual, expected, context);
}

static void remove_related_files(const char *path) {
    if (path == NULL) {
        return;
    }

    (void)remove(path);
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-shm");
    remove_with_suffix(path, "-wal");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related_path[test_path_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }

    (void)remove(related_path);
}
