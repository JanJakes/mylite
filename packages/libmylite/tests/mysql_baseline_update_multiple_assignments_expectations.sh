#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_update_multiple_assignments_$$"

fail() {
    printf '%s\n' "mysql_baseline_update_multiple_assignments_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
}

expect_output() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    if [ "$output" != "$expected" ]; then
        fail "$label: expected [$expected], got [$output]"
    fi
}

expect_error() {
    label=$1
    code=$2
    state=$3
    message=$4
    sql=$5
    shift 5

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status_code=$?
    set -e

    if [ "$status_code" -eq 0 ]; then
        fail "$label: expected error $code/$state, command succeeded with [$output]"
    fi

    case "$output" in
        *"ERROR $code ($state)"*"$message"*) ;;
        *)
            fail "$label: expected error $code/$state containing [$message], got [$output]"
            ;;
    esac
}

reset_rows() {
    run_mysql \
        "DROP TABLE IF EXISTS rows_t; "\
"CREATE TABLE rows_t ("\
"id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, "\
"a INT NULL, b INT NULL, nn INT NOT NULL DEFAULT 11, u INT UNSIGNED NULL, "\
"s VARCHAR(20) NULL DEFAULT 'd', "\
"ts TIMESTAMP NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP); "\
"INSERT INTO rows_t(a, b, nn, u, s, ts) VALUES "\
"(1, 2, 3, 4, 'one', '2025-01-01 00:00:00'), "\
"(1, 5, 3, 4, 'two', '2025-01-01 00:00:00'), "\
"(NULL, 5, 3, 4, NULL, '2025-01-01 00:00:00');" \
        "$DATABASE" >/dev/null
}

reset_key_rows() {
    run_mysql \
        "DROP TABLE IF EXISTS key_t; "\
"CREATE TABLE key_t ("\
"term_taxonomy_id BIGINT UNSIGNED NOT NULL PRIMARY KEY, "\
"term_id BIGINT UNSIGNED NOT NULL, "\
"taxonomy VARCHAR(32) NOT NULL, "\
"description TEXT NOT NULL, "\
"parent BIGINT UNSIGNED NOT NULL DEFAULT 0, "\
"UNIQUE KEY term_id_taxonomy (term_id, taxonomy), "\
"KEY parent (parent)); "\
"INSERT INTO key_t(term_taxonomy_id, term_id, taxonomy, description, parent) VALUES "\
"(70, 70, 'category', 'existing', 0), "\
"(71, 0, 'nav_menu', 'pending', 1);" \
        "$DATABASE" >/dev/null
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE}; SET GLOBAL sql_mode = 'STRICT_TRANS_TABLES';" >/dev/null

reset_rows
expect_output \
    "partial changed row count" \
    "1	0	1:1:2,2:1:2,3:N:5" \
    "SET sql_mode = 'STRICT_TRANS_TABLES';
     UPDATE rows_t SET a = 1, b = 2 WHERE id IN (1, 2);
     SELECT ROW_COUNT(), @@warning_count,
         GROUP_CONCAT(CONCAT(id, ':', IFNULL(a, 'N'), ':', IFNULL(b, 'N')) ORDER BY id)
     FROM rows_t;" \
    "$DATABASE"

expect_output \
    "all no-op row count" \
    "0	0	1:1:2,2:1:2,3:N:5" \
    "UPDATE rows_t SET a = 1, b = 2 WHERE id IN (1, 2);
     SELECT ROW_COUNT(), @@warning_count,
         GROUP_CONCAT(CONCAT(id, ':', IFNULL(a, 'N'), ':', IFNULL(b, 'N')) ORDER BY id)
     FROM rows_t;" \
    "$DATABASE"

reset_rows
expect_output \
    "nullable null assignment" \
    "1	0	1:1:2,2:1:5,3:N:9" \
    "UPDATE rows_t SET a = NULL, b = 9 WHERE id = 3;
     SELECT ROW_COUNT(), @@warning_count,
         GROUP_CONCAT(CONCAT(id, ':', IFNULL(a, 'N'), ':', IFNULL(b, 'N')) ORDER BY id)
     FROM rows_t;" \
    "$DATABASE"

reset_rows
expect_output \
    "default assignments" \
    "1	0	11	d" \
    "UPDATE rows_t SET nn = DEFAULT, s = DEFAULT WHERE id = 1;
     SELECT ROW_COUNT(), @@warning_count, nn, s FROM rows_t WHERE id = 1;" \
    "$DATABASE"

reset_rows
expect_output \
    "explicit current timestamp assignment" \
    "1	0	100	1" \
    "UPDATE rows_t SET a = 100, ts = CURRENT_TIMESTAMP WHERE id = 1;
     SELECT ROW_COUNT(), @@warning_count, a, ts <> '2025-01-01 00:00:00'
     FROM rows_t WHERE id = 1;" \
    "$DATABASE"

reset_rows
expect_output \
    "automatic current timestamp on changed assignment" \
    "1	0	1" \
    "UPDATE rows_t SET a = 10, b = 20 WHERE id = 1;
     SELECT ROW_COUNT(), @@warning_count, ts <> '2025-01-01 00:00:00'
     FROM rows_t WHERE id = 1;" \
    "$DATABASE"

reset_rows
expect_output \
    "automatic current timestamp skipped for no-op assignments" \
    "0	0	0" \
    "UPDATE rows_t SET a = 1, b = 2 WHERE id = 1;
     SELECT ROW_COUNT(), @@warning_count, ts <> '2025-01-01 00:00:00'
     FROM rows_t WHERE id = 1;" \
    "$DATABASE"

reset_rows
expect_output \
    "no-match skips nullability conversion" \
    "0	0	0" \
    "UPDATE rows_t SET nn = NULL, b = 99 WHERE id = 999;
     SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM rows_t WHERE b = 99;" \
    "$DATABASE"

reset_rows
expect_output \
    "no-match skips oversized value conversion" \
    "0	0	0" \
    "UPDATE rows_t SET a = 123456789012345678901234, b = 99 WHERE id = 999;
     SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM rows_t WHERE b = 99;" \
    "$DATABASE"

reset_rows
expect_output \
    "limit zero skips conversion" \
    "0	0	0" \
    "UPDATE rows_t SET nn = NULL, b = 99 LIMIT 0;
     SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM rows_t WHERE b = 99;" \
    "$DATABASE"

reset_rows
expect_output \
    "ordered limited update" \
    "1	0	1:1:2,2:1:5,3:10:20" \
    "UPDATE rows_t SET a = 10, b = 20 ORDER BY id DESC LIMIT 1;
     SELECT ROW_COUNT(), @@warning_count,
         GROUP_CONCAT(CONCAT(id, ':', IFNULL(a, 'N'), ':', IFNULL(b, 'N')) ORDER BY id)
     FROM rows_t;" \
    "$DATABASE"

reset_key_rows
expect_output \
    "composite unique key multiple assignment" \
    "1	0	71	71	nav_menu		0" \
    "UPDATE key_t SET term_id = 71, taxonomy = 'nav_menu', description = '', parent = 0
     WHERE term_taxonomy_id = 71;
     SELECT ROW_COUNT(), @@warning_count, term_taxonomy_id, term_id, taxonomy, description, parent
     FROM key_t WHERE term_taxonomy_id = 71;" \
    "$DATABASE"

expect_output \
    "primary key multiple assignment" \
    "1	0	72	72	post_tag	renamed	0" \
    "UPDATE key_t SET term_taxonomy_id = 72, term_id = 72, taxonomy = 'post_tag',
         description = 'renamed'
     WHERE term_taxonomy_id = 71;
     SELECT ROW_COUNT(), @@warning_count, term_taxonomy_id, term_id, taxonomy, description, parent
     FROM key_t WHERE term_taxonomy_id = 72;" \
    "$DATABASE"

reset_rows
expect_output \
    "auto increment primary key multiple assignment advances next value" \
    "1	0	5:1:2,6:7:8" \
    "UPDATE rows_t SET id = 5, a = 1 WHERE id = 1;
     SET @update_rows = ROW_COUNT(), @update_warnings = @@warning_count;
     INSERT INTO rows_t(a, b, nn) VALUES (7, 8, 9);
     SELECT @update_rows, @update_warnings,
         GROUP_CONCAT(CONCAT(id, ':', IFNULL(a, 'N'), ':', IFNULL(b, 'N')) ORDER BY id)
     FROM rows_t WHERE id IN (5, 6);" \
    "$DATABASE"

expect_output \
    "wordpress auto increment primary key multiple assignment" \
    "1	0	2015:http://example.org/?p=2015|2016:" \
    "DROP TABLE IF EXISTS wp_posts;
     CREATE TABLE wp_posts (
         ID BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
         guid VARCHAR(255) NOT NULL DEFAULT ''
     );
     INSERT INTO wp_posts(guid) VALUES ('http://example.org/?p=1');
     UPDATE wp_posts SET ID = 2015, guid = 'http://example.org/?p=2015' WHERE ID = 1;
     SET @update_rows = ROW_COUNT(), @update_warnings = @@warning_count;
     INSERT INTO wp_posts(guid) VALUES ('');
     SELECT @update_rows, @update_warnings,
         GROUP_CONCAT(CONCAT(ID, ':', guid) ORDER BY ID SEPARATOR '|')
     FROM wp_posts;" \
    "$DATABASE"

reset_key_rows
expect_error \
    "composite unique key duplicate" \
    1062 \
    23000 \
    "Duplicate entry '70-category'" \
    "UPDATE key_t SET term_id = 70, taxonomy = 'category', description = 'dup'
     WHERE term_taxonomy_id = 71;" \
    "$DATABASE"

expect_error \
    "primary key duplicate" \
    1062 \
    23000 \
    "Duplicate entry '70'" \
    "UPDATE key_t SET term_taxonomy_id = 70, description = 'dup'
     WHERE term_taxonomy_id = 71;" \
    "$DATABASE"

reset_rows
expect_output \
    "left to right expression behavior deferred" \
    "1	0	2	2" \
    "UPDATE rows_t SET a = a + 1, b = a WHERE id = 1;
     SELECT ROW_COUNT(), @@warning_count, a, b FROM rows_t WHERE id = 1;" \
    "$DATABASE"

reset_rows
expect_output \
    "duplicate assignment target accepted by mysql but deferred by mylite" \
    "1	0	8" \
    "UPDATE rows_t SET a = 7, a = 8 WHERE id = 1;
     SELECT ROW_COUNT(), @@warning_count, a FROM rows_t WHERE id = 1;" \
    "$DATABASE"

reset_rows
expect_error \
    "null into not null on matched row" \
    1048 \
    23000 \
    "Column 'nn' cannot be null" \
    "UPDATE rows_t SET nn = NULL, b = 99 WHERE id = 1;" \
    "$DATABASE"

expect_error \
    "out of range matched row" \
    1264 \
    22003 \
    "Out of range value for column 'a' at row 1" \
    "UPDATE rows_t SET a = 123456789012345678901234, b = 99 WHERE id = 1;" \
    "$DATABASE"
