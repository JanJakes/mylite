#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_insert_value_row_syntax_$$"

fail() {
    printf '%s\n' "mysql_baseline_insert_value_row_syntax_expectations: $1" >&2
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
        *) fail "$label: expected error $code/$state containing [$message], got [$output]" ;;
    esac
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

values_syntax_expected=$(cat <<'EXPECTED'
value_empty	1	0	1	10:5
value_multi	2	0	1:2,3:4
row_empty	1	0	1	10:5
row_multi	2	0	1:2,3:4
EXPECTED
)
expect_output \
    "insert value and row syntax" \
    "$values_syntax_expected" \
    "CREATE TABLE t(id INT NOT NULL DEFAULT 10, v INT DEFAULT 5) ENGINE=InnoDB; "\
"INSERT INTO t VALUE (); "\
"SELECT 'value_empty', ROW_COUNT(), @@warning_count, COUNT(*), "\
"GROUP_CONCAT(CONCAT(id, ':', IFNULL(v, 'NULL')) ORDER BY id, v) FROM t; "\
"TRUNCATE t; "\
"INSERT INTO t VALUE (1,2), (3,4); "\
"SELECT 'value_multi', ROW_COUNT(), @@warning_count, "\
"GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM t; "\
"TRUNCATE t; "\
"INSERT INTO t VALUES ROW(); "\
"SELECT 'row_empty', ROW_COUNT(), @@warning_count, COUNT(*), "\
"GROUP_CONCAT(CONCAT(id, ':', IFNULL(v, 'NULL')) ORDER BY id, v) FROM t; "\
"TRUNCATE t; "\
"INSERT INTO t VALUES ROW(1,2), ROW(3,4); "\
"SELECT 'row_multi', ROW_COUNT(), @@warning_count, "\
"GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM t;" \
    "$DATABASE"

ignore_expected=$(cat <<'EXPECTED'
1	2	0	[]
EXPECTED
)
expect_output \
    "insert ignore row constructor empty values" \
    "$ignore_expected" \
    "CREATE TABLE ignore_t(id INT NOT NULL, s VARCHAR(5) NOT NULL) ENGINE=InnoDB; "\
"INSERT IGNORE INTO ignore_t VALUES ROW(); "\
"SELECT ROW_COUNT(), @@warning_count, id, CONCAT('[', s, ']') FROM ignore_t;" \
    "$DATABASE"

duplicate_expected=$(cat <<'EXPECTED'
seed	1	0	10
value_update	2	0	20
row_default	2	0	10
row_values	0	1	10
EXPECTED
)
expect_output \
    "duplicate update value and row syntax" \
    "$duplicate_expected" \
    "CREATE TABLE duplicate_t(id INT NOT NULL DEFAULT 1 PRIMARY KEY, "\
"v INT NOT NULL DEFAULT 10) ENGINE=InnoDB; "\
"INSERT INTO duplicate_t VALUE (); "\
"SELECT 'seed', ROW_COUNT(), @@warning_count, v FROM duplicate_t; "\
"INSERT INTO duplicate_t VALUE () ON DUPLICATE KEY UPDATE v = 20; "\
"SELECT 'value_update', ROW_COUNT(), @@warning_count, v FROM duplicate_t; "\
"INSERT INTO duplicate_t VALUES ROW() ON DUPLICATE KEY UPDATE v = DEFAULT; "\
"SELECT 'row_default', ROW_COUNT(), @@warning_count, v FROM duplicate_t; "\
"INSERT INTO duplicate_t VALUES ROW() ON DUPLICATE KEY UPDATE v = VALUES(v); "\
"SELECT 'row_values', ROW_COUNT(), @@warning_count, v FROM duplicate_t;" \
    "$DATABASE"

replace_expected=$(cat <<'EXPECTED'
replace_value	1	0	1	10:5
replace_row	2	0	2	10:5,10:5
EXPECTED
)
expect_output \
    "replace value and row syntax" \
    "$replace_expected" \
    "CREATE TABLE replace_t(id INT NOT NULL DEFAULT 10, v INT DEFAULT 5) ENGINE=InnoDB; "\
"REPLACE INTO replace_t VALUE (); "\
"SELECT 'replace_value', ROW_COUNT(), @@warning_count, COUNT(*), "\
"GROUP_CONCAT(CONCAT(id, ':', IFNULL(v, 'NULL')) ORDER BY id, v) FROM replace_t; "\
"TRUNCATE replace_t; "\
"REPLACE INTO replace_t VALUES ROW(), ROW(); "\
"SELECT 'replace_row', ROW_COUNT(), @@warning_count, COUNT(*), "\
"GROUP_CONCAT(CONCAT(id, ':', IFNULL(v, 'NULL')) ORDER BY id, v) FROM replace_t;" \
    "$DATABASE"

expect_error \
    "insert row constructor shape mismatch" \
    1136 \
    21S01 \
    "Column count doesn't match value count at row 2" \
    "INSERT INTO t(id) VALUES ROW(1), ROW(2,3);" \
    "$DATABASE"

expect_error \
    "insert value row unsupported syntax" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "INSERT INTO t VALUE ROW();" \
    "$DATABASE"

expect_error \
    "insert mixed row constructor syntax" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "INSERT INTO t VALUES ROW(1,2), (3,4);" \
    "$DATABASE"

expect_error \
    "replace value row unsupported syntax" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "REPLACE INTO replace_t VALUE ROW();" \
    "$DATABASE"

expect_error \
    "replace mixed row constructor syntax" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "REPLACE INTO replace_t VALUES ROW(1,2), (3,4);" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_insert_value_row_syntax_expectations: ok"
