#include <mylite/mylite.h>

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
    test_path_suffix_capacity = 16U,
    sql_capacity = 1024,
    qualified_left_join_column_count = 5,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_column_ambiguous = 1052,
    mysql_error_unknown_column = 1054,
    mysql_error_parse = 1064,
    mysql_error_not_unique_table_alias = 1066,
    mysql_error_incorrect_database_name = 1102,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_table_does_not_exist = 1146,
};

struct expected_query {
    const char *sql;
    const char *const *columns;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

struct expected_statement {
    int64_t affected_rows;
    size_t warning_count;
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

static int test_left_join_success_persistence_and_table_lifecycle(void);
static int test_left_join_derived_grouped_source(void);
static int test_left_join_diagnostics(void);
static int test_independent_file_backed_join_handles(void);
static int seed_app_schema(mylite_db *database);
static int seed_join_tables(
    mylite_db *database,
    const char *left_rows,
    int64_t left_count,
    const char *right_rows,
    int64_t right_count
);
static int expect_statement(
    mylite_db *database,
    const char *sql,
    struct expected_statement expected
);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
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
static int expect_size(size_t actual, size_t expected, const char *context);
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

    failures += test_left_join_success_persistence_and_table_lifecycle();
    failures += test_left_join_derived_grouped_source();
    failures += test_left_join_diagnostics();
    failures += test_independent_file_backed_join_handles();

    return failures == 0 ? 0 : 1;
}

static int test_left_join_success_persistence_and_table_lifecycle(void) {
    static const char *const star_columns[] = {"id", "k", "v", "name", "id", "k", "w", "name"};
    static const char *const star_rows[] = {
        "2",  "20", "200", "Beta", NULL, NULL, NULL,  NULL,       "3",  NULL, "300", "none",
        NULL, NULL, NULL,  NULL,   "4",  "40", "400", "onlyleft", NULL, NULL, NULL,  NULL,
    };
    static const char *const left_rows[] = {
        "2",
        "20",
        NULL,
        NULL,
        NULL,
        "3",
        NULL,
        NULL,
        NULL,
        NULL,
        "4",
        "40",
        NULL,
        NULL,
        NULL,
    };
    static const char *const anti_rows[] = {"2", NULL, "3", NULL, "4", NULL};
    static const char *const on_right_literal_rows[] = {"1", "7", "2", NULL, "3", NULL, "4", NULL};
    static const char *const right_order_asc_rows[] = {"4", NULL, "1", "7", "1", "8"};
    static const char *const right_order_desc_rows[] = {"1", "8", "1", "7", "4", NULL};
    static const char *const string_join_rows[] = {"1", "7", "2", "8", "3", "9", "4", NULL};
    static const char *const alias_order_rows[] = {"1", "8", "1", "7", "4", NULL};
    static const char *const chained_left_rows[] = {
        "1",
        "7",
        "7",
        "1",
        "7",
        "8",
        "1",
        "8",
        "7",
        "1",
        "8",
        "8",
    };
    static const char *const temp_shadow_rows[] = {"10", "7", "10", "8"};
    static const char *const row_count_rows[] = {"-1"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open join success file");
    failures += seed_app_schema(database);
    failures += seed_join_tables(
        database,
        "(1,10,100,'alpha'),(2,20,200,'Beta'),(3,NULL,300,'none'),"
        "(4,40,400,'onlyleft')",
        4,
        "(7,10,700,'ALPHA'),(8,10,800,'beta'),(9,NULL,900,'none'),"
        "(10,50,1000,'onlyright')",
        4
    );

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM lefts LEFT JOIN rights ON lefts.k = rights.k "
                   "WHERE lefts.id >= 2 ORDER BY lefts.id",
            .columns = star_columns,
            .values = star_rows,
            .column_count = sizeof(star_columns) / sizeof(star_columns[0]),
            .row_count = 3U,
            .context = "star left join preserves unmatched left rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT l.id, l.k, r.id, r.k, r.w FROM lefts AS l LEFT JOIN rights AS r "
                   "ON l.k = r.k WHERE l.id >= 2 ORDER BY l.id",
            .values = left_rows,
            .column_count = qualified_left_join_column_count,
            .row_count = 3U,
            .context = "qualified left join columns include right NULLs",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT l.id, r.id FROM lefts AS l LEFT JOIN rights AS r "
                   "ON l.k = r.k WHERE r.id IS NULL ORDER BY l.id",
            .values = anti_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "where filters after null extension",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT l.id, r.id FROM lefts AS l LEFT JOIN rights AS r "
                   "ON l.k = r.k AND r.name = 'ALPHA' ORDER BY l.id, r.id",
            .values = on_right_literal_rows,
            .column_count = 2U,
            .row_count = 4U,
            .context = "on condition filters right rows before null extension",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT l.id, r.id FROM lefts AS l LEFT OUTER JOIN rights AS r "
                   "ON l.k = r.k WHERE l.id = 1 OR l.id = 4 ORDER BY r.id",
            .values = right_order_asc_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "left outer join keyword and ascending NULL order",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT l.id, r.id FROM lefts AS l LEFT JOIN rights AS r "
                   "ON l.k = r.k WHERE l.id = 1 OR l.id = 4 ORDER BY r.id DESC",
            .values = right_order_desc_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "descending right order keeps NULL last",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT lefts.id, rights.id FROM lefts LEFT JOIN rights "
                   "ON lefts.name = rights.name ORDER BY lefts.id",
            .values = string_join_rows,
            .column_count = 2U,
            .row_count = 4U,
            .context = "left string join equality uses registered collation",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT l.id AS left_id, r.id AS right_id FROM lefts AS l LEFT JOIN rights AS r "
                   "ON l.k = r.k WHERE l.id = 1 OR l.id = 4 ORDER BY right_id DESC",
            .values = alias_order_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "order by selected alias",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT l.id, r.id, r2.id FROM lefts AS l "
                   "LEFT JOIN rights AS r ON l.k = r.k "
                   "LEFT JOIN rights AS r2 ON l.k = r2.k "
                   "WHERE l.id = 1 ORDER BY r.id, r2.id",
            .values = chained_left_rows,
            .column_count = 3U,
            .row_count = 4U,
            .context = "chained left joins preserve earlier matches",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT l.id FROM lefts AS l LEFT JOIN rights AS r ON l.k = r.k "
                   "ORDER BY r.id LIMIT 0",
            .column_count = 1U,
            .row_count = 0U,
            .context = "limit zero returns no rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT l.id, r.id FROM lefts AS l LEFT JOIN rights AS r "
                   "ON l.k = r.k WHERE l.id = 1 OR l.id = 4 ORDER BY r.id LIMIT 3",
            .values = right_order_asc_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "exact limit returns all limited left join rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT l.id, r.id FROM lefts AS l LEFT JOIN rights AS r "
                   "ON l.k = r.k WHERE l.id = 1 OR l.id = 4 ORDER BY r.id LIMIT 10",
            .values = right_order_asc_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "oversized limit returns matched joined set",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT()",
            .values = row_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "row count after joined select",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT app.lefts.id, app.rights.id FROM app.lefts LEFT JOIN app.rights "
                   "ON app.lefts.k = app.rights.k WHERE app.lefts.id = 1 OR app.lefts.id = 4 "
                   "ORDER BY app.rights.id",
            .values = right_order_asc_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "schema-qualified join sources and columns",
        }
    );

    failures += expect_statement(
        database,
        "CREATE TEMPORARY TABLE lefts (id INT NOT NULL, k INT NULL, v INT NULL, name VARCHAR(20))",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(
        database,
        "INSERT INTO lefts VALUES (10,10,1000,'alpha')",
        (struct expected_statement){1, 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT lefts.id, rights.id FROM lefts LEFT JOIN rights "
                   "ON lefts.k = rights.k "
                   "ORDER BY rights.id",
            .values = temp_shadow_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "temporary table shadows persistent join source",
        }
    );
    failures += expect_statement(
        database,
        "DROP TEMPORARY TABLE lefts",
        (struct expected_statement){0, 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT lefts.id, rights.id FROM lefts LEFT JOIN rights "
                   "ON lefts.k = rights.k WHERE lefts.id = 1 OR lefts.id = 4 "
                   "ORDER BY rights.id",
            .values = right_order_asc_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "persistent table visible after dropping temporary shadow",
        }
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "left joins preserve MyLite preamble"
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen join file");
    failures += expect_statement(database, "USE app", (struct expected_statement){0, 0U});
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT lefts.id, rights.id FROM lefts LEFT JOIN rights "
                   "ON lefts.k = rights.k WHERE lefts.id = 1 OR lefts.id = 4 "
                   "ORDER BY rights.id",
            .values = right_order_asc_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "joined rows persist after reopen",
        }
    );
    failures += expect_statement(
        database,
        "RENAME TABLE rights TO rights2",
        (struct expected_statement){0, 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT lefts.id, rights2.id FROM lefts LEFT JOIN rights2 "
                   "ON lefts.k = rights2.k WHERE lefts.id = 1 OR lefts.id = 4 "
                   "ORDER BY rights2.id",
            .values = right_order_asc_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "join after table rename",
        }
    );
    failures +=
        expect_statement(database, "DROP TABLE rights2", (struct expected_statement){0, 0U});
    failures += expect_error(
        database,
        "SELECT lefts.id FROM lefts LEFT JOIN rights2 ON lefts.k = rights2.k",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.rights2' doesn't exist",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_left_join_derived_grouped_source(void) {
    static const char *const post_count_columns[] = {"ID"};
    static const char *const post_count_rows[] = {"3", "1", "2"};
    static const char *const qualified_post_count_columns[] = {"ID", "post_count"};
    static const char *const qualified_post_count_rows[] = {"3", "1", "1", "2", "2", "3"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "derived") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open derived left join file");
    failures += seed_app_schema(database);
    failures += expect_statement(
        database,
        "CREATE TABLE users (ID BIGINT NOT NULL, user_login VARCHAR(30) NOT NULL)",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(
        database,
        "CREATE TABLE posts (ID BIGINT NOT NULL, post_author BIGINT NOT NULL, "
        "post_type VARCHAR(20) NOT NULL, post_status VARCHAR(20) NOT NULL)",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(
        database,
        "INSERT INTO users VALUES (1,'alice'),(2,'bob'),(3,'carol')",
        (struct expected_statement){3, 0U}
    );
    failures += expect_statement(
        database,
        "INSERT INTO posts VALUES "
        "(10,1,'post','publish'),(11,1,'post','publish'),"
        "(12,2,'post','publish'),(13,2,'post','publish'),(14,2,'post','publish'),"
        "(15,3,'post','publish'),(16,3,'page','publish'),(17,3,'post','draft')",
        (struct expected_statement){8, 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT users.ID FROM users LEFT OUTER JOIN "
                   "(SELECT post_author, COUNT(*) AS post_count FROM posts "
                   "WHERE ((post_type = 'post' AND (post_status = 'publish'))) "
                   "GROUP BY post_author) p ON (users.ID = p.post_author) "
                   "WHERE 1=1 ORDER BY post_count ASC",
            .columns = post_count_columns,
            .values = post_count_rows,
            .column_count = 1U,
            .row_count = 3U,
            .context = "left join derived grouped source orders by aggregate alias",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT users.ID, p.post_count FROM users LEFT JOIN "
                   "(SELECT post_author, COUNT(*) AS post_count FROM posts "
                   "WHERE post_type = 'post' AND post_status = 'publish' "
                   "GROUP BY post_author) AS p ON users.ID = p.post_author "
                   "ORDER BY p.post_count ASC",
            .columns = qualified_post_count_columns,
            .values = qualified_post_count_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "left join derived grouped source projects aggregate alias",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_left_join_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_db *missing_default_database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += seed_app_schema(database);
    failures += seed_join_tables(database, "(1,10,100,'alpha')", 1, "(7,10,700,'ALPHA')", 1);

    failures += expect_int(
        mylite_open_memory(&missing_default_database),
        MYLITE_OK,
        "open missing default memory"
    );
    failures += expect_error(
        missing_default_database,
        "SELECT l.id FROM lefts l LEFT JOIN rights r ON l.k = r.k",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += expect_error(
        missing_default_database,
        "SELECT * FROM lefts LEFT JOIN rights",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    mylite_close(missing_default_database);

    failures += expect_error(
        database,
        "SELECT id FROM lefts LEFT JOIN rights ON lefts.k = rights.k",
        (struct expected_sql_error){
            .code = mysql_error_column_ambiguous,
            .sqlstate = "23000",
            .message_part = "Column 'id' in field list is ambiguous",
        }
    );
    failures += expect_error(
        database,
        "SELECT lefts.id FROM lefts LEFT JOIN rights ON k = k",
        (struct expected_sql_error){
            .code = mysql_error_column_ambiguous,
            .sqlstate = "23000",
            .message_part = "Column 'k' in on clause is ambiguous",
        }
    );
    failures += expect_error(
        database,
        "SELECT lefts.v FROM lefts LEFT JOIN rights ON lefts.k = rights.k WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_column_ambiguous,
            .sqlstate = "23000",
            .message_part = "Column 'id' in where clause is ambiguous",
        }
    );
    failures += expect_error(
        database,
        "SELECT lefts.v FROM lefts LEFT JOIN rights ON lefts.k = rights.k ORDER BY id",
        (struct expected_sql_error){
            .code = mysql_error_column_ambiguous,
            .sqlstate = "23000",
            .message_part = "Column 'id' in order clause is ambiguous",
        }
    );
    failures += expect_error(
        database,
        "SELECT l.id FROM lefts AS l LEFT JOIN rights AS r ON lefts.k = r.k",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'lefts.k' in 'on clause'",
        }
    );
    failures += expect_error(
        database,
        "SELECT x.id FROM lefts AS x LEFT JOIN rights AS x ON x.k = x.k",
        (struct expected_sql_error){
            .code = mysql_error_not_unique_table_alias,
            .sqlstate = "42000",
            .message_part = "Not unique table/alias: 'x'",
        }
    );
    failures += expect_error(
        database,
        "SELECT missing.id FROM lefts LEFT JOIN rights ON lefts.k = rights.k",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing.id' in 'field list'",
        }
    );
    failures += expect_error(
        database,
        "SELECT lefts.id FROM lefts LEFT JOIN rights ON lefts.missing = rights.k",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'lefts.missing' in 'on clause'",
        }
    );
    failures += expect_error(
        database,
        "SELECT lefts.id FROM lefts LEFT JOIN rights ON lefts.k = rights.name",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "joined SELECT supports only same-family integer or string descriptor equality ON",
        }
    );
    failures += expect_error(
        database,
        "SELECT lefts.id FROM lefts LEFT JOIN rights ON lefts.k = rights.k WHERE missing = 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'where clause'",
        }
    );
    failures += expect_error(
        database,
        "SELECT lefts.id FROM lefts LEFT JOIN rights ON lefts.k = rights.k ORDER BY missing",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'order clause'",
        }
    );
    failures += expect_error(
        database,
        "SELECT lefts.id FROM lefts LEFT JOIN missing ON lefts.k = missing.k",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing' doesn't exist",
        }
    );
    failures += expect_error(
        database,
        "SELECT lefts.id FROM lefts LEFT JOIN missing_schema.rights "
        "ON lefts.k = missing_schema.rights.k",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );
    failures += expect_error(
        database,
        "SELECT lefts.id FROM _mylite_private.lefts LEFT JOIN rights "
        "ON _mylite_private.lefts.k = rights.k",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name '_mylite_private'",
        }
    );
    failures += expect_error(
        database,
        "SELECT lefts.id FROM _mylite_private LEFT JOIN rights "
        "ON _mylite_private.k = rights.k",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name '_mylite_private'",
        }
    );
    failures += expect_error(
        database,
        "SELECT lefts.id FROM lefts LEFT JOIN rights",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += expect_error(
        database,
        "SELECT * FROM missing LEFT JOIN also_missing",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += expect_error(
        database,
        "SELECT * FROM lefts AS x LEFT JOIN rights AS x",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += expect_error(
        database,
        "SELECT lefts.id FROM lefts LEFT JOIN rights USING (k)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += expect_error(
        database,
        "SELECT lefts.id FROM lefts NATURAL LEFT JOIN rights",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_independent_file_backed_join_handles(void) {
    static const char *const first_rows[] = {"1", "7"};
    static const char *const second_rows[] = {"2", "8"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "independent_a") != 0 ||
        make_test_path(second_path, sizeof(second_path), "independent_b") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first join file");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second join file");
    failures += seed_app_schema(first);
    failures += seed_app_schema(second);
    failures += seed_join_tables(first, "(1,10,100,'alpha')", 1, "(7,10,700,'ALPHA')", 1);
    failures += seed_join_tables(second, "(2,20,200,'beta')", 1, "(8,20,800,'BETA')", 1);

    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT lefts.id, rights.id FROM lefts LEFT JOIN rights ON lefts.k = rights.k "
                   "ORDER BY rights.id",
            .values = first_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "first file-backed handle join state",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT lefts.id, rights.id FROM lefts LEFT JOIN rights ON lefts.k = rights.k "
                   "ORDER BY rights.id",
            .values = second_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "second file-backed handle join state",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int seed_app_schema(mylite_db *database) {
    int failures = 0;

    failures +=
        expect_statement(database, "CREATE DATABASE app", (struct expected_statement){1, 0U});
    failures += expect_statement(database, "USE app", (struct expected_statement){0, 0U});
    return failures;
}

static int seed_join_tables(
    mylite_db *database,
    const char *left_rows,
    int64_t left_count,
    const char *right_rows,
    int64_t right_count
) {
    char sql[sql_capacity];
    int written = 0;
    int failures = 0;

    failures += expect_statement(
        database,
        "CREATE TABLE lefts (id INT NOT NULL, k INT NULL, v INT NULL, name VARCHAR(20))",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(
        database,
        "CREATE TABLE rights (id INT NOT NULL, k INT NULL, w INT NULL, name VARCHAR(20))",
        (struct expected_statement){0, 0U}
    );

    written = snprintf(sql, sizeof(sql), "INSERT INTO lefts VALUES %s", left_rows);
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "insert left rows SQL too long\n");
        return failures + 1;
    }
    failures += expect_statement(database, sql, (struct expected_statement){left_count, 0U});

    written = snprintf(sql, sizeof(sql), "INSERT INTO rights VALUES %s", right_rows);
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "insert right rows SQL too long\n");
        return failures + 1;
    }
    failures += expect_statement(database, sql, (struct expected_statement){right_count, 0U});

    return failures;
}

static int expect_statement(
    mylite_db *database,
    const char *sql,
    struct expected_statement expected
) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    failures += expect_int(rc, MYLITE_OK, sql);
    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: %d %s %s\n",
            sql,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
    }
    if (rc == MYLITE_OK) {
        failures += expect_size(mylite_result_column_count(result), 0U, sql);
        failures += expect_int64(mylite_result_affected_rows(result), expected.affected_rows, sql);
        failures += expect_size(mylite_result_warning_count(result), expected.warning_count, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    size_t value_count = query.column_count * query.row_count;
    int failures = 0;
    int rc = mylite_execute(database, query.sql, strlen(query.sql), &result);

    failures += expect_int(rc, MYLITE_OK, query.context);
    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: %d %s %s\n",
            query.sql,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return failures;
    }

    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, query.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, query.context);
    for (size_t column_index = 0U; query.columns != NULL && column_index < query.column_count;
         ++column_index) {
        failures += expect_text(
            mylite_result_column_name(result, column_index),
            query.columns[column_index],
            query.context
        );
    }
    for (size_t value_index = 0U; query.values != NULL && value_index < value_count;
         ++value_index) {
        failures += expect_result_value(
            result,
            value_index / query.column_count,
            value_index % query.column_count,
            query.values[value_index],
            query.context
        );
    }

    mylite_result_free(result);
    return failures;
}

static int expect_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, statement succeeded\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
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
        return expect_int(actual == NULL, 1, context);
    }
    return expect_text(actual, expected, context);
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "runtime_left_join_select_%s_%d.mylite",
        name,
        current_process_id()
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path is too long\n");
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
    remove(path);
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity + test_path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        remove(buffer);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t read_size = 0U;

    if (file == NULL) {
        fprintf(stderr, "%s: failed to open file\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "%s: failed to seek\n", path);
        fclose(file);
        return 1;
    }
    read_size = fread(buffer, 1U, size, file);
    fclose(file);
    if (read_size != size) {
        fprintf(stderr, "%s: expected to read %zu bytes, read %zu\n", path, size, read_size);
        return 1;
    }
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
    fprintf(
        stderr,
        "%s: expected %lld, got %lld\n",
        context,
        (long long)expected,
        (long long)actual
    );
    return 1;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
    return 1;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    fprintf(stderr, "%s: expected [%s], got [%s]\n", context, expected, actual);
    return 1;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual != NULL && needle != NULL && strstr(actual, needle) != NULL) {
        return 0;
    }
    fprintf(stderr, "%s: expected [%s] to contain [%s]\n", context, actual, needle);
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
