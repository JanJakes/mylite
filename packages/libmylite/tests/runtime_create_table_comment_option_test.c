#include <mylite/mylite.h>

#include "runtime_test_support.h"

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
    repeated_comment_sql_prefix_capacity = 128,
    table_comment_max_characters = 2048,
    show_table_status_comment_column = 17,
    mysql_error_no_database = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_unknown_table = 1146,
    mysql_error_parse = 1064,
    mysql_error_incorrect_database_name = 1102,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_table_comment_too_long = 1628,
    mysql_error_algorithm_not_supported = 1845,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

static int test_persistent_comments_metadata_and_persistence(void);
static int test_alter_table_comment_lifecycle(void);
static int test_temporary_and_like_comment_cloning(void);
static int test_comment_sql_modes(void);
static int test_comment_diagnostics(void);
static int test_independent_file_comments(void);
static int create_app_schema(mylite_db *database);
static int expect_single_cell(
    mylite_db *database,
    const char *sql,
    size_t column_index,
    const char *expected,
    const char *context
);
static int expect_single_cell_contains(
    mylite_db *database,
    const char *sql,
    size_t column_index,
    const char *needle,
    const char *context
);
static int expect_single_cell_not_contains(
    mylite_db *database,
    const char *sql,
    size_t column_index,
    const char *needle,
    const char *context
);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_sql_with_length_error(
    mylite_db *database,
    const char *sql,
    size_t sql_length,
    struct expected_sql_error expected
);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static char *make_repeated_comment_create_table_sql(
    const char *table_name,
    const unsigned char *unit,
    size_t unit_size,
    size_t repeat_count,
    char **out_comment
);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_text_not_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_persistent_comments_metadata_and_persistence();
    failures += test_alter_table_comment_lifecycle();
    failures += test_temporary_and_like_comment_cloning();
    failures += test_comment_sql_modes();
    failures += test_comment_diagnostics();
    failures += test_independent_file_comments();

    return failures == 0 ? 0 : 1;
}

static int test_persistent_comments_metadata_and_persistence(void) {
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "persistent") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open persistent file");
    failures += create_app_schema(database);
    failures +=
        execute_statement_ok(database, "CREATE TABLE plain_comment (id INT) COMMENT='plain'");
    failures += execute_statement_ok(database, "CREATE TABLE empty_comment (id INT) COMMENT=''");
    failures += execute_statement_ok(
        database,
        "CREATE TABLE escaped_comment (id INT) COMMENT='a\\'b\\\\c'"
    );
    failures += execute_statement_ok(
        database,
        "CREATE TABLE dup_comment (id INT) COMMENT='first' COMMENT='second'"
    );
    failures += execute_statement_ok(
        database,
        "CREATE TABLE ndb_comment_table (id INT) "
        "COMMENT='NDB_TABLE=READ_BACKUP=1,PARTITION_BALANCE=FOR_RA_BY_LDM'"
    );

    failures += expect_single_cell_contains(
        database,
        "SHOW CREATE TABLE plain_comment",
        1U,
        "COMMENT='plain'",
        "plain SHOW CREATE comment"
    );
    failures += expect_single_cell_not_contains(
        database,
        "SHOW CREATE TABLE empty_comment",
        1U,
        "COMMENT=",
        "empty SHOW CREATE omits comment"
    );
    failures += expect_single_cell_contains(
        database,
        "SHOW CREATE TABLE escaped_comment",
        1U,
        "COMMENT='a''b\\\\c'",
        "escaped SHOW CREATE comment"
    );
    failures += expect_single_cell(
        database,
        "SHOW TABLE STATUS WHERE Name = 'plain_comment'",
        show_table_status_comment_column,
        "plain",
        "SHOW TABLE STATUS comment"
    );
    failures += expect_single_cell(
        database,
        "SELECT TABLE_COMMENT FROM INFORMATION_SCHEMA.TABLES "
        "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'dup_comment'",
        0U,
        "second",
        "information schema table comment"
    );
    failures += expect_single_cell_contains(
        database,
        "SHOW CREATE TABLE ndb_comment_table",
        1U,
        "COMMENT='NDB_TABLE=READ_BACKUP=1,PARTITION_BALANCE=FOR_RA_BY_LDM'",
        "NDB-shaped table comment SHOW CREATE"
    );
    failures += expect_single_cell(
        database,
        "SHOW TABLE STATUS WHERE Name = 'ndb_comment_table'",
        show_table_status_comment_column,
        "NDB_TABLE=READ_BACKUP=1,PARTITION_BALANCE=FOR_RA_BY_LDM",
        "NDB-shaped table comment SHOW TABLE STATUS"
    );
    failures += expect_single_cell(
        database,
        "SELECT TABLE_COMMENT FROM INFORMATION_SCHEMA.TABLES "
        "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'ndb_comment_table'",
        0U,
        "NDB_TABLE=READ_BACKUP=1,PARTITION_BALANCE=FOR_RA_BY_LDM",
        "NDB-shaped table comment information schema"
    );

    failures += execute_statement_ok(database, "RENAME TABLE plain_comment TO renamed_comment");
    failures += expect_single_cell_contains(
        database,
        "SHOW CREATE TABLE renamed_comment",
        1U,
        "COMMENT='plain'",
        "rename preserves comment"
    );
    failures += execute_statement_ok(database, "DROP TABLE renamed_comment");
    failures += expect_single_cell(
        database,
        "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_NAME = 'renamed_comment'",
        0U,
        "0",
        "drop removes comment metadata"
    );
    failures += mylite_test_expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble unchanged"
    );

    mylite_close(database);
    database = NULL;

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen persistent file");
    failures += execute_statement_ok(database, "USE app");
    failures += expect_single_cell_contains(
        database,
        "SHOW CREATE TABLE dup_comment",
        1U,
        "COMMENT='second'",
        "reopen preserves comment"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_alter_table_comment_lifecycle(void) {
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_result *dml_result = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "alter") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open alter file");
    failures += create_app_schema(database);
    failures += execute_statement_ok(database, "CREATE TABLE altered (id INT) COMMENT='old'");
    failures += execute_statement_ok(database, "ALTER TABLE altered COMMENT='new'");
    failures += expect_single_cell_contains(
        database,
        "SHOW CREATE TABLE altered",
        1U,
        "COMMENT='new'",
        "altered SHOW CREATE comment"
    );
    failures += expect_single_cell_not_contains(
        database,
        "SHOW CREATE TABLE altered",
        1U,
        "COMMENT='old'",
        "old ALTER comment replaced"
    );
    failures += expect_single_cell(
        database,
        "SHOW TABLE STATUS WHERE Name = 'altered'",
        show_table_status_comment_column,
        "new",
        "altered SHOW TABLE STATUS comment"
    );
    failures += expect_single_cell(
        database,
        "SELECT TABLE_COMMENT FROM INFORMATION_SCHEMA.TABLES "
        "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'altered'",
        0U,
        "new",
        "altered information schema comment"
    );

    failures += execute_statement_ok(
        database,
        "ALTER TABLE altered COMMENT='locked', ALGORITHM=INPLACE, LOCK=NONE"
    );
    failures += expect_single_cell_contains(
        database,
        "SHOW CREATE TABLE altered",
        1U,
        "COMMENT='locked'",
        "algorithm and lock tail accepted"
    );
    failures += execute_statement_ok(database, "ALTER TABLE altered COMMENT=''");
    failures += expect_single_cell_not_contains(
        database,
        "SHOW CREATE TABLE altered",
        1U,
        "COMMENT=",
        "empty ALTER comment clears SHOW CREATE suffix"
    );
    failures += expect_single_cell(
        database,
        "SELECT TABLE_COMMENT FROM INFORMATION_SCHEMA.TABLES "
        "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'altered'",
        0U,
        "",
        "empty ALTER comment visible in metadata"
    );

    failures += execute_statement_ok(database, "CREATE TABLE shadow (id INT) COMMENT='persistent'");
    failures += execute_statement_ok(
        database,
        "CREATE TEMPORARY TABLE shadow (id INT) COMMENT='temporary old'"
    );
    failures += execute_statement_ok(database, "ALTER TABLE shadow COMMENT='temporary new'");
    failures += expect_single_cell_contains(
        database,
        "SHOW CREATE TABLE shadow",
        1U,
        "CREATE TEMPORARY TABLE",
        "ALTER COMMENT targets shadowing temporary table"
    );
    failures += expect_single_cell_contains(
        database,
        "SHOW CREATE TABLE shadow",
        1U,
        "COMMENT='temporary new'",
        "temporary ALTER comment stored"
    );
    failures += execute_statement_ok(database, "DROP TEMPORARY TABLE shadow");
    failures += expect_single_cell_contains(
        database,
        "SHOW CREATE TABLE shadow",
        1U,
        "COMMENT='persistent'",
        "persistent comment unchanged after temporary shadow alter"
    );
    failures += execute_statement_ok(database, "CREATE TABLE tx_guard (id INT)");
    failures +=
        execute_statement_ok(database, "CREATE TEMPORARY TABLE tx_comment (id INT) COMMENT='old'");
    failures += execute_statement_ok(database, "START TRANSACTION");
    failures += execute_ok(database, "INSERT INTO tx_guard VALUES (1)", &dml_result);
    if (dml_result != NULL) {
        failures += mylite_test_expect_int64(
            mylite_result_affected_rows(dml_result),
            1,
            "insert before rejected temporary ALTER comment"
        );
        mylite_result_free(dml_result);
        dml_result = NULL;
    }
    failures += execute_error(
        database,
        "ALTER TABLE tx_comment COMMENT='new'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "Temporary table DDL inside an active transaction is not supported",
        }
    );
    failures += execute_statement_ok(database, "ROLLBACK");
    failures += expect_single_cell(
        database,
        "SELECT COUNT(*) FROM tx_guard",
        0U,
        "0",
        "rejected temporary ALTER comment does not commit active transaction"
    );
    failures += expect_single_cell_contains(
        database,
        "SHOW CREATE TABLE tx_comment",
        1U,
        "COMMENT='old'",
        "rejected temporary ALTER comment preserves descriptor"
    );
    failures += execute_statement_ok(
        database,
        "CREATE TEMPORARY TABLE tx_comment_prepared (id INT) COMMENT='old'"
    );
    failures += execute_statement_ok(
        database,
        "PREPARE tx_comment_stmt FROM 'ALTER TABLE tx_comment_prepared COMMENT=''new'''"
    );
    failures += execute_statement_ok(database, "START TRANSACTION");
    failures += execute_ok(database, "INSERT INTO tx_guard VALUES (2)", &dml_result);
    if (dml_result != NULL) {
        failures += mylite_test_expect_int64(
            mylite_result_affected_rows(dml_result),
            1,
            "insert before rejected prepared temporary ALTER comment"
        );
        mylite_result_free(dml_result);
        dml_result = NULL;
    }
    failures += execute_error(
        database,
        "EXECUTE tx_comment_stmt",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "Temporary table DDL inside an active transaction is not supported",
        }
    );
    failures += execute_statement_ok(database, "ROLLBACK");
    failures += expect_single_cell(
        database,
        "SELECT COUNT(*) FROM tx_guard",
        0U,
        "0",
        "rejected prepared temporary ALTER comment does not commit active transaction"
    );
    failures += expect_single_cell_contains(
        database,
        "SHOW CREATE TABLE tx_comment_prepared",
        1U,
        "COMMENT='old'",
        "rejected prepared temporary ALTER comment preserves descriptor"
    );
    failures += execute_statement_ok(database, "DEALLOCATE PREPARE tx_comment_stmt");

    failures += mylite_test_expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read preamble after alter"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble unchanged after alter"
    );

    mylite_close(database);
    database = NULL;

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen alter file");
    failures += execute_statement_ok(database, "ALTER TABLE app.altered COMMENT='qualified'");
    failures += expect_single_cell_contains(
        database,
        "SHOW CREATE TABLE app.altered",
        1U,
        "COMMENT='qualified'",
        "schema-qualified ALTER comment without selected schema"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_temporary_and_like_comment_cloning(void) {
    mylite_db *database = NULL;
    int failures = mylite_test_expect_int(
        mylite_test_open_temporary(&database),
        MYLITE_OK,
        "open temp memory"
    );

    failures += create_app_schema(database);
    failures += execute_statement_ok(
        database,
        "CREATE TABLE source_comment (id INT) COMMENT='source table'"
    );
    failures += execute_statement_ok(database, "CREATE TABLE like_comment LIKE source_comment");
    failures += expect_single_cell_contains(
        database,
        "SHOW CREATE TABLE like_comment",
        1U,
        "COMMENT='source table'",
        "CREATE TABLE LIKE copies comment"
    );

    failures += execute_statement_ok(
        database,
        "CREATE TEMPORARY TABLE temp_comment (id INT) COMMENT='temp table'"
    );
    failures += expect_single_cell_contains(
        database,
        "SHOW CREATE TABLE temp_comment",
        1U,
        "CREATE TEMPORARY TABLE",
        "temporary SHOW CREATE prefix"
    );
    failures += expect_single_cell_contains(
        database,
        "SHOW CREATE TABLE temp_comment",
        1U,
        "COMMENT='temp table'",
        "temporary table comment"
    );
    failures +=
        execute_statement_ok(database, "CREATE TEMPORARY TABLE temp_like LIKE source_comment");
    failures += expect_single_cell_contains(
        database,
        "SHOW CREATE TABLE temp_like",
        1U,
        "COMMENT='source table'",
        "temporary LIKE copies comment"
    );

    mylite_close(database);
    return failures;
}

static int test_comment_sql_modes(void) {
    mylite_db *database = NULL;
    int failures =
        mylite_test_expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open sql modes");

    failures += create_app_schema(database);
    failures += execute_statement_ok(database, "SET SESSION sql_mode = 'NO_BACKSLASH_ESCAPES'");
    failures += execute_statement_ok(
        database,
        "CREATE TABLE no_backslash_comment (id INT) COMMENT='a\\\\b'"
    );
    failures += expect_single_cell(
        database,
        "SELECT TABLE_COMMENT FROM INFORMATION_SCHEMA.TABLES "
        "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'no_backslash_comment'",
        0U,
        "a\\\\b",
        "NO_BACKSLASH_ESCAPES stores literal backslashes"
    );
    failures += expect_single_cell_contains(
        database,
        "SHOW CREATE TABLE no_backslash_comment",
        1U,
        "COMMENT='a\\\\\\\\b'",
        "NO_BACKSLASH_ESCAPES SHOW CREATE comment"
    );
    failures += execute_statement_ok(database, "SET SESSION sql_mode = 'ANSI_QUOTES'");
    failures += execute_error(
        database,
        "CREATE TABLE ansi_comment (id INT) COMMENT \"quoted\"",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_comment_diagnostics(void) {
    enum {
        prefix_length = sizeof("CREATE TABLE too_long (id INT) COMMENT='") - 1U,
        comment_length = 2049,
        suffix_length = sizeof("'") - 1U,
    };

    static const unsigned char e_acute_utf8[] = {0xc3U, 0xa9U};
    char *multibyte_comment = NULL;
    char *multibyte_sql = NULL;
    char *sql = NULL;
    mylite_db *database = NULL;
    int failures = mylite_test_expect_int(
        mylite_test_open_temporary(&database),
        MYLITE_OK,
        "open diagnostics memory"
    );

    failures += execute_error(
        database,
        "ALTER TABLE missing_default COMMENT='x'",
        (struct expected_sql_error){
            .code = mysql_error_no_database,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += create_app_schema(database);
    failures += execute_statement_ok(database, "CREATE TABLE comment_target (id INT)");
    failures += execute_error(
        database,
        "ALTER TABLE unknown_schema.comment_target COMMENT='x'",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'unknown_schema'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE missing_comment COMMENT='x'",
        (struct expected_sql_error){
            .code = mysql_error_unknown_table,
            .sqlstate = "42S02",
            .message_part = "doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_reserved.comment_target COMMENT='x'",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name '_mylite_reserved'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_reserved COMMENT='x'",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name '_mylite_reserved'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE numeric_comment (id INT) COMMENT=123",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE comment_target COMMENT=123",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE comment_target COMMENT=NULL",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE comment_target COMMENT=abc",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE comment_target COMMENT='a', COMMENT='b'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "multi-action ALTER TABLE does not support this action",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE comment_target COMMENT='instant', ALGORITHM=INSTANT",
        (struct expected_sql_error){
            .code = mysql_error_algorithm_not_supported,
            .sqlstate = "0A000",
            .message_part = "ALGORITHM=INSTANT is not supported for this operation",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE nul_comment (id INT) COMMENT='a\\0b'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "table comments do not support NUL bytes",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE comment_target COMMENT='a\\0b'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "table comments do not support NUL bytes",
        }
    );

    multibyte_sql = make_repeated_comment_create_table_sql(
        "multibyte_ok",
        e_acute_utf8,
        sizeof(e_acute_utf8),
        table_comment_max_characters,
        &multibyte_comment
    );
    if (multibyte_sql == NULL || multibyte_comment == NULL) {
        fprintf(stderr, "failed to allocate multibyte comment SQL\n");
        failures += 1;
    } else {
        failures += execute_statement_ok(database, multibyte_sql);
        failures += expect_single_cell(
            database,
            "SELECT TABLE_COMMENT FROM INFORMATION_SCHEMA.TABLES "
            "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'multibyte_ok'",
            0U,
            multibyte_comment,
            "2048 multibyte character comment accepted"
        );
    }
    free(multibyte_sql);
    free(multibyte_comment);

    failures += execute_statement_ok(
        database,
        "CREATE TABLE four_byte_comment (id INT) COMMENT='\360\237\231\202'"
    );
    failures += expect_single_cell(
        database,
        "SELECT TABLE_COMMENT FROM INFORMATION_SCHEMA.TABLES "
        "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'four_byte_comment'",
        0U,
        "?",
        "four-byte table comment replaced"
    );

    sql = malloc(prefix_length + comment_length + suffix_length + 1U);
    if (sql == NULL) {
        fprintf(stderr, "failed to allocate overlength comment SQL\n");
        mylite_close(database);
        return failures + 1;
    }
    memcpy(sql, "CREATE TABLE too_long (id INT) COMMENT='", prefix_length);
    memset(sql + prefix_length, 'a', comment_length);
    memcpy(sql + prefix_length + comment_length, "'", suffix_length);
    sql[prefix_length + comment_length + suffix_length] = '\0';
    failures += execute_sql_with_length_error(
        database,
        sql,
        prefix_length + comment_length + suffix_length,
        (struct expected_sql_error){
            .code = mysql_error_table_comment_too_long,
            .sqlstate = "HY000",
            .message_part = "Comment for table 'too_long' is too long (max = 2048)",
        }
    );

    free(sql);
    mylite_close(database);
    return failures;
}

static char *make_repeated_comment_create_table_sql(
    const char *table_name,
    const unsigned char *unit,
    size_t unit_size,
    size_t repeat_count,
    char **out_comment
) {
    char prefix[repeated_comment_sql_prefix_capacity];
    char *comment = NULL;
    char *sql = NULL;
    size_t comment_length = 0U;
    size_t offset = 0U;
    size_t prefix_length = 0U;
    size_t sql_length = 0U;
    int written = 0;

    if (out_comment != NULL) {
        *out_comment = NULL;
    }
    if (table_name == NULL || unit == NULL || unit_size == 0U) {
        return NULL;
    }
    if (repeat_count > SIZE_MAX / unit_size) {
        return NULL;
    }
    comment_length = unit_size * repeat_count;
    written = snprintf(prefix, sizeof(prefix), "CREATE TABLE %s (id INT) COMMENT='", table_name);
    if (written < 0 || (size_t)written >= sizeof(prefix)) {
        return NULL;
    }
    prefix_length = (size_t)written;
    if (prefix_length > SIZE_MAX - comment_length ||
        prefix_length + comment_length > SIZE_MAX - 2U) {
        return NULL;
    }
    sql_length = prefix_length + comment_length + 1U;

    if (out_comment != NULL) {
        comment = malloc(comment_length + 1U);
        if (comment == NULL) {
            return NULL;
        }
    }
    sql = malloc(sql_length + 1U);
    if (sql == NULL) {
        free(comment);
        return NULL;
    }

    memcpy(sql, prefix, prefix_length);
    for (size_t index = 0U; index < repeat_count; ++index) {
        memcpy(sql + prefix_length + (index * unit_size), unit, unit_size);
        if (comment != NULL) {
            memcpy(comment + (index * unit_size), unit, unit_size);
        }
    }
    offset = prefix_length + comment_length;
    sql[offset] = '\'';
    sql[offset + 1U] = '\0';
    if (comment != NULL) {
        comment[comment_length] = '\0';
        *out_comment = comment;
    }
    return sql;
}

static int test_independent_file_comments(void) {
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (mylite_test_make_path(first_path, sizeof(first_path), "first") != 0 ||
        mylite_test_make_path(second_path, sizeof(second_path), "second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures +=
        mylite_test_expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first file");
    failures +=
        mylite_test_expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second file");
    failures += create_app_schema(first);
    failures += create_app_schema(second);
    failures += execute_statement_ok(first, "CREATE TABLE independent (id INT) COMMENT='first'");
    failures += execute_statement_ok(second, "CREATE TABLE independent (id INT) COMMENT='second'");
    failures += execute_statement_ok(first, "ALTER TABLE independent COMMENT='first altered'");
    failures += execute_statement_ok(second, "ALTER TABLE independent COMMENT='second altered'");
    failures += expect_single_cell_contains(
        first,
        "SHOW CREATE TABLE independent",
        1U,
        "COMMENT='first altered'",
        "first handle altered comment"
    );
    failures += expect_single_cell_contains(
        second,
        "SHOW CREATE TABLE independent",
        1U,
        "COMMENT='second altered'",
        "second handle altered comment"
    );

    mylite_close(second);
    mylite_close(first);
    remove_related_files(second_path);
    remove_related_files(first_path);
    return failures;
}

static int create_app_schema(mylite_db *database) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, "CREATE DATABASE app", &result);

    mylite_result_free(result);
    failures += execute_statement_ok(database, "USE app");
    return failures;
}

static int expect_single_cell(
    mylite_db *database,
    const char *sql,
    size_t column_index,
    const char *expected,
    const char *context
) {
    mylite_result *result = NULL;
    const char *actual = NULL;
    int failures = 0;

    failures += execute_ok(database, sql, &result);
    if (result == NULL) {
        return failures + 1;
    }
    failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, context);
    if (mylite_result_column_count(result) <= column_index) {
        fprintf(stderr, "%s: expected column %zu in result\n", context, column_index);
        failures += 1;
    } else {
        actual = mylite_result_value_text(result, 0U, column_index);
        failures += mylite_test_expect_text(actual, expected, context);
    }
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, context);

    mylite_result_free(result);
    return failures;
}

static int expect_single_cell_contains(
    mylite_db *database,
    const char *sql,
    size_t column_index,
    const char *needle,
    const char *context
) {
    mylite_result *result = NULL;
    const char *actual = NULL;
    int failures = 0;

    failures += execute_ok(database, sql, &result);
    if (result == NULL) {
        return failures + 1;
    }
    failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, context);
    if (mylite_result_column_count(result) <= column_index) {
        fprintf(stderr, "%s: expected column %zu in result\n", context, column_index);
        failures += 1;
    } else {
        actual = mylite_result_value_text(result, 0U, column_index);
        failures += mylite_test_expect_contains(actual, needle, context);
    }
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, context);

    mylite_result_free(result);
    return failures;
}

static int expect_single_cell_not_contains(
    mylite_db *database,
    const char *sql,
    size_t column_index,
    const char *needle,
    const char *context
) {
    mylite_result *result = NULL;
    const char *actual = NULL;
    int failures = 0;

    failures += execute_ok(database, sql, &result);
    if (result == NULL) {
        return failures + 1;
    }
    failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, context);
    if (mylite_result_column_count(result) <= column_index) {
        fprintf(stderr, "%s: expected column %zu in result\n", context, column_index);
        failures += 1;
    } else {
        actual = mylite_result_value_text(result, 0U, column_index);
        failures += expect_text_not_contains(actual, needle, context);
    }
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, context);

    mylite_result_free(result);
    return failures;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (result != NULL) {
        failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, sql);
        failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, sql);
        failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, sql);
        failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, sql);
    }
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

static int execute_sql_with_length_error(
    mylite_db *database,
    const char *sql,
    size_t sql_length,
    struct expected_sql_error expected
) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, sql_length, &result);
    int failures = 0;

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
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, sql);
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    return execute_sql_with_length_error(database, sql, strlen(sql), expected);
}

static void remove_related_files(const char *path) {
    (void)remove(path);
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
    size_t bytes_read = 0U;

    if (file == NULL) {
        fprintf(stderr, "failed to open %s for reading\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "failed to seek %s\n", path);
        fclose(file);
        return 1;
    }
    bytes_read = fread(buffer, 1U, size, file);
    fclose(file);
    if (bytes_read != size) {
        fprintf(stderr, "failed to read %zu bytes from %s\n", size, path);
        return 1;
    }

    return 0;
}

static int expect_text_not_contains(const char *actual, const char *needle, const char *context) {
    if (actual != NULL && needle != NULL && strstr(actual, needle) == NULL) {
        return 0;
    }

    fprintf(
        stderr,
        "%s: expected \"%s\" not to contain \"%s\"\n",
        context,
        actual == NULL ? "(null)" : actual,
        needle == NULL ? "(null)" : needle
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

    fprintf(stderr, "%s: byte sequence mismatch\n", context);
    return 1;
}
