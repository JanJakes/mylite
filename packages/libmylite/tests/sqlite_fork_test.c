#include "fork/mylite_sqlite_fork.h"

#include "sqlite3.h"

#include <stdio.h>
#include <string.h>

struct expected_text_row {
    const char *sql;
    const char *expected;
    const char *context;
};

static int test_registered_functions(void);

static int test_mysql_collations(void);

static int test_wordpress_like_crud(void);

static int open_configured_database(sqlite3 **out_database);

static int exec_sql(sqlite3 *database, const char *sql, const char *context);

static int expect_text(sqlite3 *database, struct expected_text_row expectation);

static int expect_int64(
    sqlite3 *database,
    const char *sql,
    sqlite3_int64 expected,
    const char *context
);

static int prepare_single_column(
    sqlite3 *database,
    const char *sql,
    sqlite3_stmt **out_statement,
    const char *context
);

static int finish_single_row(sqlite3_stmt *statement, const char *context);

static int expect_sqlite_ok(int rc, sqlite3 *database, const char *context);

int main(void) {
    int failures = 0;

    failures += test_registered_functions();
    failures += test_mysql_collations();
    failures += test_wordpress_like_crud();

    return failures == 0 ? 0 : 1;
}

static int test_registered_functions(void) {
    sqlite3 *database = NULL;
    int failures = 0;

    failures += open_configured_database(&database);
    if (failures != 0) {
        return failures;
    }

    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT CONCAT('wp_', 'posts')",
            .expected = "wp_posts",
            .context = "CONCAT joins non-null arguments",
        }
    );
    failures +=
        expect_int64(database, "SELECT LENGTH(CAST(X'C5BE' AS TEXT))", 2, "LENGTH counts bytes");
    failures += expect_int64(
        database,
        "SELECT CHAR_LENGTH(CAST(X'C5BE' AS TEXT))",
        1,
        "CHAR_LENGTH counts UTF-8 characters"
    );
    failures += expect_int64(
        database,
        "SELECT CONCAT('a', NULL) IS NULL",
        1,
        "CONCAT returns NULL when an argument is NULL"
    );

    sqlite3_close(database);
    return failures;
}

static int test_mysql_collations(void) {
    sqlite3 *database = NULL;
    int failures = 0;

    failures += open_configured_database(&database);
    if (failures != 0) {
        return failures;
    }

    failures += exec_sql(
        database,
        "CREATE TABLE collated_names("
        "name TEXT COLLATE utf8mb4_unicode_ci,"
        "binary_name TEXT COLLATE utf8mb4_bin"
        ")",
        "create collation fixture"
    );
    failures += exec_sql(
        database,
        "INSERT INTO collated_names VALUES ('Hello', 'Hello')",
        "insert collation fixture"
    );
    failures += expect_int64(
        database,
        "SELECT COUNT(*) FROM collated_names WHERE name = 'hello'",
        1,
        "case-insensitive MySQL collation is visible to SQLite predicates"
    );
    failures += expect_int64(
        database,
        "SELECT COUNT(*) FROM collated_names WHERE binary_name = 'hello'",
        0,
        "binary MySQL collation remains byte-sensitive"
    );

    sqlite3_close(database);
    return failures;
}

static int test_wordpress_like_crud(void) {
    sqlite3 *database = NULL;
    int failures = 0;

    failures += open_configured_database(&database);
    if (failures != 0) {
        return failures;
    }

    failures += exec_sql(
        database,
        "CREATE TABLE wp_posts_like ("
        "ID INTEGER PRIMARY KEY AUTOINCREMENT,"
        "post_author INTEGER NOT NULL DEFAULT 0 CHECK(post_author >= 0),"
        "post_date TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "post_title TEXT NOT NULL COLLATE utf8mb4_unicode_ci,"
        "post_name TEXT NOT NULL DEFAULT '' COLLATE utf8mb4_unicode_ci "
        "CHECK(CHAR_LENGTH(post_name) <= 200),"
        "post_status TEXT NOT NULL DEFAULT 'publish' COLLATE utf8mb4_unicode_ci "
        "CHECK(CHAR_LENGTH(post_status) <= 20),"
        "comment_count INTEGER NOT NULL DEFAULT 0"
        ")",
        "create wp_posts_like physical table"
    );
    failures += exec_sql(
        database,
        "CREATE INDEX post_name ON wp_posts_like(post_name)",
        "create post_name index"
    );
    failures += exec_sql(
        database,
        "CREATE INDEX post_status_date ON wp_posts_like(post_status, post_date)",
        "create post_status_date index"
    );
    failures += exec_sql(
        database,
        "CREATE TABLE wp_postmeta_like ("
        "meta_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "post_id INTEGER NOT NULL DEFAULT 0 CHECK(post_id >= 0),"
        "meta_key TEXT DEFAULT NULL COLLATE utf8mb4_unicode_ci "
        "CHECK(meta_key IS NULL OR CHAR_LENGTH(meta_key) <= 255),"
        "meta_value TEXT COLLATE utf8mb4_unicode_ci"
        ")",
        "create wp_postmeta_like physical table"
    );
    failures += exec_sql(
        database,
        "CREATE INDEX post_id ON wp_postmeta_like(post_id)",
        "create post_id index"
    );
    failures += exec_sql(
        database,
        "CREATE INDEX meta_key ON wp_postmeta_like(meta_key)",
        "create meta_key index"
    );

    failures += exec_sql(
        database,
        "INSERT INTO wp_posts_like "
        "(post_author, post_date, post_title, post_name, post_status, comment_count) VALUES "
        "(1, '2026-05-06 09:15:00', 'Hello MyLite', 'hello-mylite', 'publish', 2),"
        "(2, '2026-05-06 10:00:00', 'Draft Notes', 'draft-notes', 'draft', 0),"
        "(1, '2026-05-07 08:30:00', 'SQLite Fork Plan', 'sqlite-fork-plan', "
        "'publish', 1)",
        "insert WordPress-like posts"
    );
    failures += exec_sql(
        database,
        "INSERT INTO wp_postmeta_like (post_id, meta_key, meta_value) VALUES "
        "(1, '_edit_lock', '1714994100:1'),"
        "(1, '_thumbnail_id', '99'),"
        "(3, '_wp_page_template', 'default')",
        "insert WordPress-like metadata"
    );
    failures += exec_sql(
        database,
        "UPDATE wp_posts_like "
        "SET post_status = 'publish', comment_count = comment_count + 1 "
        "WHERE post_name = 'draft-notes'",
        "publish draft post"
    );
    failures += exec_sql(
        database,
        "DELETE FROM wp_postmeta_like WHERE meta_key = '_edit_lock'",
        "delete edit lock metadata"
    );

    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT COUNT(*) || ':' || MIN(ID) || ':' || "
                   "MAX(ID) || ':' || SUM(comment_count) "
                   "FROM wp_posts_like",
            .expected = "3:1:3:4",
            .context = "post summary matches MySQL fixture",
        }
    );
    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT group_concat(ID || ':' || post_name || "
                   "':' || post_status || ':' || "
                   "comment_count, '|') FROM ("
                   "SELECT ID, post_name, post_status, "
                   "comment_count FROM wp_posts_like "
                   "WHERE post_status = 'publish' ORDER BY ID"
                   ")",
            .expected = "1:hello-mylite:publish:2|2:draft-notes:"
                        "publish:1|3:sqlite-fork-plan:publish:1",
            .context = "published rows match MySQL fixture",
        }
    );
    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT COUNT(*) || ':' || group_concat(post_id "
                   "|| ':' || meta_key || '=' || "
                   "meta_value, '|') FROM ("
                   "SELECT post_id, meta_key, meta_value FROM "
                   "wp_postmeta_like ORDER BY meta_id"
                   ")",
            .expected = "2:1:_thumbnail_id=99|3:_wp_page_template=default",
            .context = "metadata rows match MySQL fixture before "
                       "truncate",
        }
    );

    failures += expect_sqlite_ok(
        mylite_sqlite_fork_truncate_table(database, "wp_postmeta_like"),
        database,
        "truncate wp_postmeta_like"
    );
    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT COUNT(*) || ':' || "
                   "COALESCE(MAX(meta_id), 0) FROM "
                   "wp_postmeta_like",
            .expected = "0:0",
            .context = "truncate empties metadata table",
        }
    );
    failures += exec_sql(
        database,
        "INSERT INTO wp_postmeta_like (post_id, meta_key, meta_value) "
        "VALUES (2, '_restored', 'yes')",
        "insert metadata after truncate"
    );
    failures += expect_text(
        database,
        (struct expected_text_row){
            .sql = "SELECT meta_id || ':' || post_id || ':' || "
                   "meta_key || ':' || meta_value "
                   "FROM wp_postmeta_like",
            .expected = "1:2:_restored:yes",
            .context = "truncate resets auto-increment sequence",
        }
    );
    failures += exec_sql(database, "DROP TABLE wp_postmeta_like", "drop metadata table");
    failures += expect_int64(
        database,
        "SELECT COUNT(*) FROM sqlite_schema WHERE type = 'table' "
        "AND name IN ('wp_posts_like', 'wp_postmeta_like')",
        1,
        "drop table leaves only posts table"
    );

    sqlite3_close(database);
    return failures;
}

static int open_configured_database(sqlite3 **out_database) {
    sqlite3 *database = NULL;
    int rc = SQLITE_OK;

    *out_database = NULL;
    rc = sqlite3_open_v2(
        ":memory:",
        &database,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_MEMORY,
        NULL
    );
    if (rc != SQLITE_OK) {
        fprintf(stderr, "sqlite open failed: %s\n", sqlite3_errmsg(database));
        sqlite3_close(database);
        return 1;
    }
    rc = mylite_sqlite_fork_configure(database);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "fork configure failed: %s\n", sqlite3_errmsg(database));
        sqlite3_close(database);
        return 1;
    }

    *out_database = database;
    return 0;
}

static int exec_sql(sqlite3 *database, const char *sql, const char *context) {
    return expect_sqlite_ok(sqlite3_exec(database, sql, NULL, NULL, NULL), database, context);
}

static int expect_text(sqlite3 *database, struct expected_text_row expectation) {
    sqlite3_stmt *statement = NULL;
    const unsigned char *actual = NULL;
    int failures =
        prepare_single_column(database, expectation.sql, &statement, expectation.context);

    if (failures != 0) {
        return failures;
    }

    actual = sqlite3_column_text(statement, 0);
    if ((actual == NULL && expectation.expected != NULL) ||
        (actual != NULL && expectation.expected == NULL) ||
        (actual != NULL && strcmp((const char *)actual, expectation.expected) != 0)) {
        fprintf(
            stderr,
            "%s: expected \"%s\", got \"%s\"\n",
            expectation.context,
            expectation.expected == NULL ? "(null)" : expectation.expected,
            actual == NULL ? "(null)" : (const char *)actual
        );
        ++failures;
    }

    failures += finish_single_row(statement, expectation.context);
    return failures;
}

static int expect_int64(
    sqlite3 *database,
    const char *sql,
    sqlite3_int64 expected,
    const char *context
) {
    sqlite3_stmt *statement = NULL;
    sqlite3_int64 actual = 0;
    int failures = prepare_single_column(database, sql, &statement, context);

    if (failures != 0) {
        return failures;
    }

    actual = sqlite3_column_int64(statement, 0);
    if (actual != expected) {
        fprintf(
            stderr,
            "%s: expected %lld, got %lld\n",
            context,
            (long long)expected,
            (long long)actual
        );
        ++failures;
    }

    failures += finish_single_row(statement, context);
    return failures;
}

static int prepare_single_column(
    sqlite3 *database,
    const char *sql,
    sqlite3_stmt **out_statement,
    const char *context
) {
    sqlite3_stmt *statement = NULL;
    int rc = sqlite3_prepare_v3(database, sql, -1, SQLITE_PREPARE_PERSISTENT, &statement, NULL);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "%s: prepare failed: %s\n", context, sqlite3_errmsg(database));
        return 1;
    }
    rc = sqlite3_step(statement);
    if (rc != SQLITE_ROW) {
        fprintf(stderr, "%s: expected one row, got rc=%d\n", context, rc);
        sqlite3_finalize(statement);
        return 1;
    }
    if (sqlite3_column_count(statement) != 1) {
        fprintf(
            stderr,
            "%s: expected one column, got %d\n",
            context,
            sqlite3_column_count(statement)
        );
        sqlite3_finalize(statement);
        return 1;
    }

    *out_statement = statement;
    return 0;
}

static int finish_single_row(sqlite3_stmt *statement, const char *context) {
    int rc = sqlite3_step(statement);
    sqlite3 *database = sqlite3_db_handle(statement);

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "%s: expected end of results, got rc=%d\n", context, rc);
        sqlite3_finalize(statement);
        return 1;
    }
    return expect_sqlite_ok(sqlite3_finalize(statement), database, context);
}

static int expect_sqlite_ok(int rc, sqlite3 *database, const char *context) {
    if (rc == SQLITE_OK) {
        return 0;
    }
    fprintf(stderr, "%s: sqlite rc=%d: %s\n", context, rc, sqlite3_errmsg(database));
    return 1;
}
