#!/usr/bin/env bash
set -euo pipefail

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_insert_select_odku_expectations_$$"
VALUES_WARNING="'VALUES function' is deprecated and will be removed in a future release. Please use an alias (INSERT INTO ... VALUES (...) AS alias) and replace VALUES(col) in the ON DUPLICATE KEY UPDATE clause with alias.col instead"

fail() {
    printf '%s\n' "mysql_baseline_insert_select_on_duplicate_key_update_expectations: $1" >&2
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
" >/dev/null

expect_output \
    "table source changed duplicate with values references" \
    "2	2	0
1	20	200" \
    "CREATE TABLE t(id INT PRIMARY KEY, v INT, n INT) ENGINE=InnoDB;
     CREATE TABLE s(id INT, v INT, n INT) ENGINE=InnoDB;
     INSERT INTO t VALUES (1, 10, 100);
     INSERT INTO s VALUES (1, 20, 200);
     INSERT INTO t SELECT id, v, n FROM s
         ON DUPLICATE KEY UPDATE v = VALUES(v), n = VALUES(n);
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT id, v, n FROM t;
     DROP TABLE t, s;"

expect_output \
    "table source cross-column values references" \
    "2	2	0
1	200	1" \
    "CREATE TABLE t(id INT PRIMARY KEY, v INT, n INT) ENGINE=InnoDB;
     CREATE TABLE s(id INT, v INT, n INT) ENGINE=InnoDB;
     INSERT INTO t VALUES (1, 10, 100);
     INSERT INTO s VALUES (1, 20, 200);
     INSERT INTO t SELECT id, v, n FROM s
         ON DUPLICATE KEY UPDATE n = VALUES(id), v = VALUES(n);
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT id, v, n FROM t;
     DROP TABLE t, s;"

expect_output \
    "table source row-scalar values reference" \
    "2	1	0
1	10	21" \
    "CREATE TABLE t(id INT PRIMARY KEY, v INT, n INT) ENGINE=InnoDB;
     CREATE TABLE s(id INT, v INT, n INT) ENGINE=InnoDB;
     INSERT INTO t VALUES (1, 10, 100);
     INSERT INTO s VALUES (1, 20, 200);
     INSERT INTO t SELECT id, v, n FROM s
         ON DUPLICATE KEY UPDATE n = GREATEST(VALUES(v) + 1, 0);
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT id, v, n FROM t;
     DROP TABLE t, s;"

expect_output \
    "values warning text" \
    "Warning	1287	${VALUES_WARNING}" \
    "CREATE TABLE t(id INT PRIMARY KEY, v INT) ENGINE=InnoDB;
     CREATE TABLE s(id INT, v INT) ENGINE=InnoDB;
     INSERT INTO t VALUES (1, 10);
     INSERT INTO s VALUES (1, 20);
     INSERT INTO t SELECT id, v FROM s ON DUPLICATE KEY UPDATE v = VALUES(v);
     SHOW WARNINGS;
     DROP TABLE t, s;"

expect_output \
    "no-op duplicate still warns" \
    "0	1	0
1	10" \
    "CREATE TABLE t(id INT PRIMARY KEY, v INT) ENGINE=InnoDB;
     CREATE TABLE s(id INT, v INT) ENGINE=InnoDB;
     INSERT INTO t VALUES (1, 10);
     INSERT INTO s VALUES (1, 10);
     INSERT INTO t SELECT id, v FROM s ON DUPLICATE KEY UPDATE v = VALUES(v);
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT id, v FROM t;
     DROP TABLE t, s;"

expect_output \
    "mixed source rows process in selected order" \
    "5	1	0
1	40
2	30" \
    "CREATE TABLE t(id INT PRIMARY KEY, v INT) ENGINE=InnoDB;
     CREATE TABLE s(id INT, v INT) ENGINE=InnoDB;
     INSERT INTO t VALUES (1, 10);
     INSERT INTO s VALUES (1, 20), (2, 30), (1, 40);
     INSERT INTO t SELECT id, v FROM s ORDER BY v
         ON DUPLICATE KEY UPDATE v = VALUES(v);
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT id, v FROM t ORDER BY id;
     DROP TABLE t, s;"

expect_output \
    "zero-row source still records values warning" \
    "0	1	0
0" \
    "CREATE TABLE t(id INT PRIMARY KEY, v INT) ENGINE=InnoDB;
     CREATE TABLE s(id INT, v INT) ENGINE=InnoDB;
     INSERT INTO t SELECT id, v FROM s WHERE id = 99
         ON DUPLICATE KEY UPDATE v = VALUES(v);
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT COUNT(*) FROM t;
     DROP TABLE t, s;"

expect_output \
    "no-key target inserts normally and values warning remains" \
    "1	1	0
1	10" \
    "CREATE TABLE t(id INT, v INT) ENGINE=InnoDB;
     CREATE TABLE s(id INT, v INT) ENGINE=InnoDB;
     INSERT INTO s VALUES (1, 10);
     INSERT INTO t SELECT id, v FROM s ON DUPLICATE KEY UPDATE v = VALUES(v);
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT id, v FROM t;
     DROP TABLE t, s;"

expect_output \
    "literal default and null duplicate assignments" \
    "2	0	0
1	7	N" \
    "CREATE TABLE t(id INT PRIMARY KEY, v INT NOT NULL DEFAULT 7, n INT NULL) ENGINE=InnoDB;
     CREATE TABLE s(id INT, v INT, n INT) ENGINE=InnoDB;
     INSERT INTO t VALUES (1, 10, 20);
     INSERT INTO s VALUES (1, 99, 88);
     INSERT INTO t SELECT id, v, n FROM s ON DUPLICATE KEY UPDATE v = DEFAULT, n = NULL;
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT id, v, IFNULL(n, 'N') FROM t;
     DROP TABLE t, s;"

expect_output \
    "row-scalar no-source duplicate update" \
    "2	1	0
1	20" \
    "CREATE TABLE t(id INT PRIMARY KEY, v INT) ENGINE=InnoDB;
     INSERT INTO t VALUES (1, 10);
     INSERT INTO t SELECT 1, 20 ON DUPLICATE KEY UPDATE v = VALUES(v);
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT id, v FROM t;
     DROP TABLE t;"

expect_output \
    "dual zero-row source warns but inserts nothing" \
    "0	1	0
0" \
    "CREATE TABLE t(id INT PRIMARY KEY, v INT) ENGINE=InnoDB;
     INSERT INTO t SELECT 1, 20 FROM DUAL WHERE EXISTS (SELECT 1 FROM t)
         ON DUPLICATE KEY UPDATE v = VALUES(v);
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT COUNT(*) FROM t;
     DROP TABLE t;"

expect_output \
    "union source duplicate update" \
    "3	1	0
1	20
2	30" \
    "CREATE TABLE t(id INT PRIMARY KEY, v INT) ENGINE=InnoDB;
     CREATE TABLE s(id INT, v INT) ENGINE=InnoDB;
     INSERT INTO t VALUES (1, 10);
     INSERT INTO s VALUES (1, 20), (2, 30);
     INSERT INTO t
         SELECT id, v FROM s WHERE id = 1
         UNION ALL
         SELECT id, v FROM s WHERE id = 2
         ON DUPLICATE KEY UPDATE v = VALUES(v);
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT id, v FROM t ORDER BY id;
     DROP TABLE t, s;"

expect_output \
    "same-table source materializes before duplicate updates" \
    "0	1	0
1	10
2	20" \
    "CREATE TABLE t(id INT PRIMARY KEY, v INT) ENGINE=InnoDB;
     INSERT INTO t VALUES (1, 10), (2, 20);
     INSERT INTO t SELECT id, v FROM t ORDER BY id
         ON DUPLICATE KEY UPDATE v = VALUES(v);
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT id, v FROM t ORDER BY id;
     DROP TABLE t;"

expect_error \
    "unknown assignment column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'field list'" \
    "CREATE TABLE t(id INT PRIMARY KEY, v INT) ENGINE=InnoDB;
     CREATE TABLE s(id INT, v INT) ENGINE=InnoDB;
     INSERT INTO t VALUES (1, 10);
     INSERT INTO s VALUES (1, 20);
     INSERT INTO t SELECT id, v FROM s ON DUPLICATE KEY UPDATE missing = VALUES(v);"

expect_error \
    "unknown values column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'field list'" \
    "CREATE TABLE t2(id INT PRIMARY KEY, v INT) ENGINE=InnoDB;
     CREATE TABLE s2(id INT, v INT) ENGINE=InnoDB;
     INSERT INTO t2 VALUES (1, 10);
     INSERT INTO s2 VALUES (1, 20);
     INSERT INTO t2 SELECT id, v FROM s2 ON DUPLICATE KEY UPDATE v = VALUES(missing);"

expect_error \
    "null into not null duplicate assignment" \
    1048 \
    23000 \
    "Column 'v' cannot be null" \
    "CREATE TABLE t3(id INT PRIMARY KEY, v INT NOT NULL) ENGINE=InnoDB;
     CREATE TABLE s3(id INT, v INT) ENGINE=InnoDB;
     INSERT INTO t3 VALUES (1, 10);
     INSERT INTO s3 VALUES (1, NULL);
     INSERT INTO t3 SELECT id, v FROM s3 ON DUPLICATE KEY UPDATE v = VALUES(v);"

expect_error \
    "range failure in duplicate assignment" \
    1264 \
    22003 \
    "Out of range value for column 'ti' at row 1" \
    "CREATE TABLE t4(id INT PRIMARY KEY, ti TINYINT NOT NULL, v INT) ENGINE=InnoDB;
     CREATE TABLE s4(id INT, ti INT, v INT) ENGINE=InnoDB;
     INSERT INTO t4 VALUES (1, 1, 10);
     INSERT INTO s4 VALUES (1, 128, 20);
     INSERT INTO t4 SELECT id, ti, v FROM s4
         ON DUPLICATE KEY UPDATE v = 20, ti = VALUES(ti);"

expect_output \
    "mysql accepts ignore with insert-select odku" \
    "2	1	0
1	20" \
    "CREATE TABLE t5(id INT PRIMARY KEY, v INT) ENGINE=InnoDB;
     CREATE TABLE s5(id INT, v INT) ENGINE=InnoDB;
     INSERT INTO t5 VALUES (1, 10);
     INSERT INTO s5 VALUES (1, 20);
     INSERT IGNORE INTO t5 SELECT id, v FROM s5 ON DUPLICATE KEY UPDATE v = VALUES(v);
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT id, v FROM t5;
     DROP TABLE t5, s5;"

expect_output \
    "mysql auto-increment target reservation behavior is deferred" \
    "3	1	2
1	10	200
2	20	300
3	30	400" \
    "CREATE TABLE t6(id INT AUTO_INCREMENT, email INT UNIQUE, v INT, KEY(id)) ENGINE=InnoDB;
     CREATE TABLE s6(email INT, v INT) ENGINE=InnoDB;
     INSERT INTO t6(email, v) VALUES (10, 100);
     INSERT INTO s6 VALUES (10, 200), (20, 300);
     INSERT INTO t6(email, v) SELECT email, v FROM s6 ORDER BY email
         ON DUPLICATE KEY UPDATE v = VALUES(v);
     SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID();
     INSERT INTO t6(email, v) VALUES (30, 400);
     SELECT id, email, v FROM t6 ORDER BY id;"

printf '%s\n' "mysql_baseline_insert_select_on_duplicate_key_update_expectations: ok"
