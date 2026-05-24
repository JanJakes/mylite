#!/usr/bin/env bash
set -euo pipefail

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_baseline_insert_select_keyed_targets"

fail() {
    printf '%s\n' "mysql_baseline_insert_select_keyed_targets_expectations: $1" >&2
    exit 1
}

run_mysql() {
    local sql="$1"
    shift || true

    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --batch --raw --skip-column-names "$@"
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
                --batch --raw --skip-column-names "$@"
    fi
}

expect_output() {
    local label="$1"
    local expected="$2"
    local sql="$3"
    local actual

    actual="$(run_mysql "$sql" "$DATABASE")"
    if [ "$actual" != "$expected" ]; then
        printf 'case: %s\n' "$label" >&2
        printf 'sql: %s\n' "$sql" >&2
        printf 'expected:\n%s\n' "$expected" >&2
        printf 'actual:\n%s\n' "$actual" >&2
        fail "unexpected output"
    fi
}

expect_error() {
    local label="$1"
    local code="$2"
    local state="$3"
    local message="$4"
    local sql="$5"
    local output
    local status_code

    set +e
    output="$(run_mysql "$sql" "$DATABASE" 2>&1)"
    status_code=$?
    set -e

    if [ "$status_code" -eq 0 ]; then
        fail "$label: expected error $code/$state, command succeeded with [$output]"
    fi
    case "$output" in
        *"ERROR $code ($state)"*"$message"*) ;;
        *) fail "$label: expected error $code/$state containing [$message], got [$output]" ;;
    esac
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS \`$DATABASE\`;" >/dev/null || true
}
trap cleanup EXIT

version="$(run_mysql 'SELECT VERSION();')"
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup

run_mysql "
CREATE DATABASE \`$DATABASE\`;
USE \`$DATABASE\`;
SET sql_mode = 'STRICT_TRANS_TABLES';
CREATE TABLE src(
    id INT,
    v INT,
    u INT,
    parent_id INT,
    label VARCHAR(20)
) ENGINE=InnoDB;
INSERT INTO src VALUES
    (1, 10, 100, 1, 'a'),
    (2, 20, 200, 2, 'b'),
    (3, 30, 200, 99, 'c'),
    (4, 40, NULL, NULL, 'd');
" >/dev/null

expect_output \
    "primary-key target success" \
    "2	0	0
1:10,2:20" \
    "CREATE TABLE pk_dst(id INT PRIMARY KEY, v INT) ENGINE=InnoDB;
     INSERT INTO pk_dst SELECT id, v FROM src WHERE id <= 2 ORDER BY id;
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM pk_dst;"

run_mysql "CREATE TABLE dup_pk_dst(id INT PRIMARY KEY, v INT) ENGINE=InnoDB;
           INSERT INTO dup_pk_dst VALUES (2, 2000);" "$DATABASE" >/dev/null
expect_error \
    "primary-key duplicate rolls back statement" \
    1062 \
    23000 \
    "Duplicate entry '2' for key 'dup_pk_dst.PRIMARY'" \
    "SET sql_mode = 'STRICT_TRANS_TABLES';
     INSERT INTO dup_pk_dst SELECT id, v FROM src WHERE id <= 3 ORDER BY id;"
expect_output \
    "primary-key duplicate leaves target unchanged" \
    "2:2000" \
    "SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM dup_pk_dst;"

expect_output \
    "unique target permits duplicate nulls" \
    "3	0	0
1:100,2:200,4:NULL" \
    "CREATE TABLE unique_dst(id INT, u INT, UNIQUE KEY uk_u(u)) ENGINE=InnoDB;
     INSERT INTO unique_dst SELECT id, u FROM src WHERE id IN (1, 2, 4) ORDER BY id;
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT GROUP_CONCAT(CONCAT(id, ':', IFNULL(u, 'NULL')) ORDER BY id) FROM unique_dst;"

run_mysql "CREATE TABLE dup_unique_dst(id INT, u INT, UNIQUE KEY uk_u(u)) ENGINE=InnoDB;
           INSERT INTO dup_unique_dst VALUES (9, 200);" "$DATABASE" >/dev/null
expect_error \
    "unique duplicate rolls back statement" \
    1062 \
    23000 \
    "Duplicate entry '200' for key 'dup_unique_dst.uk_u'" \
    "SET sql_mode = 'STRICT_TRANS_TABLES';
     INSERT INTO dup_unique_dst SELECT id, u FROM src WHERE id <= 3 ORDER BY id;"
expect_output \
    "unique duplicate leaves target unchanged" \
    "9:200" \
    "SELECT GROUP_CONCAT(CONCAT(id, ':', IFNULL(u, 'NULL')) ORDER BY id) FROM dup_unique_dst;"

expect_output \
    "composite unique target success" \
    "3	0	0
1:100:10,2:200:20,3:200:30" \
    "CREATE TABLE composite_unique_dst(
         id INT, u INT, v INT, UNIQUE KEY uk_u_v(u, v)
     ) ENGINE=InnoDB;
     INSERT INTO composite_unique_dst SELECT id, u, v FROM src WHERE id <= 3 ORDER BY id;
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT GROUP_CONCAT(CONCAT(id, ':', u, ':', v) ORDER BY id)
         FROM composite_unique_dst;"

run_mysql "CREATE TABLE dup_composite_unique_dst(
               id INT, u INT, v INT, UNIQUE KEY uk_u_v(u, v)
           ) ENGINE=InnoDB;
           INSERT INTO dup_composite_unique_dst VALUES (9, 200, 20);" "$DATABASE" >/dev/null
expect_error \
    "composite unique duplicate rolls back statement" \
    1062 \
    23000 \
    "Duplicate entry '200-20' for key 'dup_composite_unique_dst.uk_u_v'" \
    "SET sql_mode = 'STRICT_TRANS_TABLES';
     INSERT INTO dup_composite_unique_dst
         SELECT id, u, v FROM src WHERE id <= 3 ORDER BY id;"
expect_output \
    "composite unique duplicate leaves target unchanged" \
    "9:200:20" \
    "SELECT GROUP_CONCAT(CONCAT(id, ':', u, ':', v) ORDER BY id)
         FROM dup_composite_unique_dst;"

expect_output \
    "prefix unique target success" \
    "3	0	0
1:a,2:b,3:c" \
    "CREATE TABLE prefix_unique_dst(
         id INT, label VARCHAR(20), UNIQUE KEY uk_label(label(1))
     ) ENGINE=InnoDB;
     INSERT INTO prefix_unique_dst SELECT id, label FROM src WHERE id <= 3 ORDER BY id;
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT GROUP_CONCAT(CONCAT(id, ':', label) ORDER BY id) FROM prefix_unique_dst;"

run_mysql "CREATE TABLE prefix_src(id INT, label VARCHAR(20)) ENGINE=InnoDB;
           INSERT INTO prefix_src VALUES (1, 'apricot'), (2, 'banana');
           CREATE TABLE dup_prefix_unique_dst(
               id INT, label VARCHAR(20), UNIQUE KEY uk_label(label(1))
           ) ENGINE=InnoDB;
           INSERT INTO dup_prefix_unique_dst VALUES (9, 'apple');" "$DATABASE" >/dev/null
expect_error \
    "prefix unique duplicate rolls back statement" \
    1062 \
    23000 \
    "Duplicate entry 'a' for key 'dup_prefix_unique_dst.uk_label'" \
    "SET sql_mode = 'STRICT_TRANS_TABLES';
     INSERT INTO dup_prefix_unique_dst SELECT id, label FROM prefix_src ORDER BY id;"
expect_output \
    "prefix unique duplicate leaves target unchanged" \
    "9:apple" \
    "SELECT GROUP_CONCAT(CONCAT(id, ':', label) ORDER BY id) FROM dup_prefix_unique_dst;"

expect_output \
    "insert ignore skips duplicate keys" \
    "2	1	0
1:10,2:2000,3:30" \
    "CREATE TABLE ignore_pk_dst(id INT PRIMARY KEY, v INT) ENGINE=InnoDB;
     INSERT INTO ignore_pk_dst VALUES (2, 2000);
     INSERT IGNORE INTO ignore_pk_dst SELECT id, v FROM src WHERE id <= 3 ORDER BY id;
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM ignore_pk_dst;"

expect_output \
    "foreign-key child target success" \
    "2	0	0
1:1,2:2" \
    "CREATE TABLE parent(id INT PRIMARY KEY) ENGINE=InnoDB;
     INSERT INTO parent VALUES (1), (2);
     CREATE TABLE child_ok(
         id INT PRIMARY KEY,
         parent_id INT,
         CONSTRAINT fk_child_parent_ok FOREIGN KEY(parent_id) REFERENCES parent(id)
     ) ENGINE=InnoDB;
     INSERT INTO child_ok SELECT id, parent_id FROM src WHERE id <= 2 ORDER BY id;
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT GROUP_CONCAT(CONCAT(id, ':', parent_id) ORDER BY id) FROM child_ok;"

run_mysql "CREATE TABLE child_bad(
               id INT PRIMARY KEY,
               parent_id INT,
               CONSTRAINT fk_child_parent_bad FOREIGN KEY(parent_id) REFERENCES parent(id)
           ) ENGINE=InnoDB;" "$DATABASE" >/dev/null
expect_error \
    "foreign-key missing parent rolls back statement" \
    1452 \
    23000 \
    "Cannot add or update a child row" \
    "SET sql_mode = 'STRICT_TRANS_TABLES';
     INSERT INTO child_bad SELECT id, parent_id FROM src WHERE id <= 3 ORDER BY id;"
expect_output \
    "foreign-key failure leaves target empty" \
    "0" \
    "SELECT COUNT(*) FROM child_bad;"

expect_output \
    "insert ignore skips missing parent rows" \
    "3	1	0
1:1,2:2,4:NULL" \
    "CREATE TABLE child_ignore(
         id INT PRIMARY KEY,
         parent_id INT,
         CONSTRAINT fk_child_parent_ignore FOREIGN KEY(parent_id) REFERENCES parent(id)
     ) ENGINE=InnoDB;
     INSERT IGNORE INTO child_ignore SELECT id, parent_id FROM src WHERE id <= 4 ORDER BY id;
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT GROUP_CONCAT(CONCAT(id, ':', IFNULL(parent_id, 'NULL')) ORDER BY id)
         FROM child_ignore;"

expect_output \
    "auto-increment insert ignore skips duplicate and missing parent rows" \
    "1	2	1
1:a,2:b
0	1	1
1:a
1	1	1
1:1
1	11	1
1:dup,11:ok
0	1	2
1:a,2:b" \
    "CREATE TABLE auto_ignore(
         id INT AUTO_INCREMENT PRIMARY KEY,
         label VARCHAR(20) UNIQUE
     ) ENGINE=InnoDB;
     INSERT INTO auto_ignore(label) VALUES ('a');
     INSERT IGNORE INTO auto_ignore(label)
         SELECT label FROM src WHERE id <= 2 ORDER BY id;
     SELECT ROW_COUNT(), LAST_INSERT_ID(), @@warning_count;
     SELECT GROUP_CONCAT(CONCAT(id, ':', label) ORDER BY id) FROM auto_ignore;
     CREATE TABLE auto_ignore_all_skip(
         id INT AUTO_INCREMENT PRIMARY KEY,
         label VARCHAR(20) UNIQUE
     ) ENGINE=InnoDB;
     INSERT INTO auto_ignore_all_skip(label) VALUES ('a');
     INSERT IGNORE INTO auto_ignore_all_skip(label) SELECT label FROM src WHERE id = 1;
     SELECT ROW_COUNT(), LAST_INSERT_ID(), @@warning_count;
     SELECT GROUP_CONCAT(CONCAT(id, ':', label) ORDER BY id) FROM auto_ignore_all_skip;
     CREATE TABLE auto_child_ignore(
         id INT AUTO_INCREMENT PRIMARY KEY,
         parent_id INT,
         CONSTRAINT fk_auto_child_ignore FOREIGN KEY(parent_id) REFERENCES parent(id)
     ) ENGINE=InnoDB;
     INSERT IGNORE INTO auto_child_ignore(parent_id)
         SELECT parent_id FROM src WHERE id IN (1, 3) ORDER BY id DESC;
     SELECT ROW_COUNT(), LAST_INSERT_ID(), @@warning_count;
     SELECT GROUP_CONCAT(CONCAT(id, ':', parent_id) ORDER BY id) FROM auto_child_ignore;
     CREATE TABLE auto_high_duplicate_src(id INT, label VARCHAR(20)) ENGINE=InnoDB;
     INSERT INTO auto_high_duplicate_src VALUES (10, 'dup'), (NULL, 'ok');
     CREATE TABLE auto_high_duplicate_ignore(
         id INT AUTO_INCREMENT PRIMARY KEY,
         label VARCHAR(20) UNIQUE
     ) ENGINE=InnoDB;
     INSERT INTO auto_high_duplicate_ignore VALUES (1, 'dup');
     INSERT IGNORE INTO auto_high_duplicate_ignore(id, label)
         SELECT id, label FROM auto_high_duplicate_src ORDER BY label;
     SELECT ROW_COUNT(), LAST_INSERT_ID(), @@warning_count;
     SELECT GROUP_CONCAT(CONCAT(id, ':', label) ORDER BY id) FROM auto_high_duplicate_ignore;
     CREATE TABLE same_auto_ignore(
         id INT AUTO_INCREMENT PRIMARY KEY,
         label VARCHAR(20) UNIQUE
     ) ENGINE=InnoDB;
     INSERT INTO same_auto_ignore(label) VALUES ('a'), ('b');
     INSERT IGNORE INTO same_auto_ignore(label)
         SELECT label FROM same_auto_ignore ORDER BY id;
     SELECT ROW_COUNT(), LAST_INSERT_ID(), @@warning_count;
     SELECT GROUP_CONCAT(CONCAT(id, ':', label) ORDER BY id) FROM same_auto_ignore;"

expect_output \
    "auto-increment generated and mixed rows" \
    "3	1	0
1:a,2:b,3:c
4	6	0
5:five,6:null,7:seven,8:zero
3	6	1
5:five,6:null,7:seven,8:zero" \
    "CREATE TABLE auto_dst(id INT AUTO_INCREMENT PRIMARY KEY, label VARCHAR(20)) ENGINE=InnoDB;
     INSERT INTO auto_dst(label) SELECT label FROM src WHERE id <= 3 ORDER BY id;
     SELECT ROW_COUNT(), LAST_INSERT_ID(), @@warning_count;
     SELECT GROUP_CONCAT(CONCAT(id, ':', label) ORDER BY id) FROM auto_dst;
     CREATE TABLE auto_src(id INT, label VARCHAR(20)) ENGINE=InnoDB;
     INSERT INTO auto_src VALUES (5, 'five'), (0, 'zero'), (NULL, 'null'), (7, 'seven');
     CREATE TABLE auto_mixed(id INT AUTO_INCREMENT PRIMARY KEY, label VARCHAR(20)) ENGINE=InnoDB;
     INSERT INTO auto_mixed(id, label) SELECT id, label FROM auto_src ORDER BY label;
     SELECT ROW_COUNT(), LAST_INSERT_ID(), @@warning_count;
     SELECT GROUP_CONCAT(CONCAT(id, ':', label) ORDER BY id) FROM auto_mixed;
     CREATE TABLE auto_explicit_ignore(
         id INT AUTO_INCREMENT PRIMARY KEY,
         label VARCHAR(20)
     ) ENGINE=InnoDB;
     INSERT INTO auto_explicit_ignore VALUES (5, 'five');
     INSERT IGNORE INTO auto_explicit_ignore(id, label) SELECT id, label FROM auto_src ORDER BY label;
     SELECT ROW_COUNT(), LAST_INSERT_ID(), @@warning_count;
     SELECT GROUP_CONCAT(CONCAT(id, ':', label) ORDER BY id) FROM auto_explicit_ignore;"

expect_output \
    "no auto value on zero treats zero as explicit" \
    "3	6	0
0:zero,5:five,6:null
3	6	0
0:zero,5:five,6:null" \
    "SET SESSION sql_mode = 'STRICT_TRANS_TABLES,NO_AUTO_VALUE_ON_ZERO';
     CREATE TABLE auto_zero(id INT AUTO_INCREMENT PRIMARY KEY, label VARCHAR(20)) ENGINE=InnoDB;
     INSERT INTO auto_zero(id, label)
         SELECT id, label FROM auto_src WHERE label IN ('zero', 'null', 'five') ORDER BY label;
     SELECT ROW_COUNT(), LAST_INSERT_ID(), @@warning_count;
     SELECT GROUP_CONCAT(CONCAT(id, ':', label) ORDER BY id) FROM auto_zero;
     CREATE TABLE auto_zero_ignore(id INT AUTO_INCREMENT PRIMARY KEY, label VARCHAR(20)) ENGINE=InnoDB;
     INSERT IGNORE INTO auto_zero_ignore(id, label)
         SELECT id, label FROM auto_src WHERE label IN ('zero', 'null', 'five') ORDER BY label;
     SELECT ROW_COUNT(), LAST_INSERT_ID(), @@warning_count;
     SELECT GROUP_CONCAT(CONCAT(id, ':', label) ORDER BY id) FROM auto_zero_ignore;
     SET SESSION sql_mode = 'STRICT_TRANS_TABLES';"

expect_error \
    "same-table primary-key duplicate rolls back" \
    1062 \
    23000 \
    "Duplicate entry '1' for key 'same_pk.PRIMARY'" \
    "CREATE TABLE same_pk(id INT PRIMARY KEY, v INT) ENGINE=InnoDB;
     INSERT INTO same_pk VALUES (1, 10), (2, 20);
     INSERT INTO same_pk SELECT id, v FROM same_pk ORDER BY id;"
expect_output \
    "same-table duplicate leaves rows unchanged" \
    "1:10,2:20" \
    "SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM same_pk;"
