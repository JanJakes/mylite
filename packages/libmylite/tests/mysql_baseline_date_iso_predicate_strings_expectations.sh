#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_date_iso_predicates_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_date_iso_predicate_strings_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_BIN" ]; then
        if [ -n "$MYSQL_SOCKET" ]; then
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                    --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
        else
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=TCP -h127.0.0.1 -uroot \
                    --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
        fi
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
                --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
    fi
}

expect_output() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    output=$(printf '%s\n' "$output" | tr '\t' '|')
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

trap cleanup EXIT HUP INT TERM

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE} DEFAULT CHARACTER SET utf8mb4 "\
"COLLATE utf8mb4_0900_ai_ci;" >/dev/null

run_mysql \
    "SET time_zone = '+02:00'; SET sql_mode = ''; "\
"CREATE TABLE d (id INT NOT NULL, value DATE NULL); "\
"INSERT INTO d VALUES "\
"(1, '2016-01-15'), "\
"(2, '2016-01-14'), "\
"(3, '2016-01-16'), "\
"(4, NULL);" \
    "$DATABASE" >/dev/null

expect_output \
    "DATE equality compares datetime midnight" \
    "date_eq_midnight|1
0
date_eq_non_midnight|NULL
0" \
    "SELECT 'date_eq_midnight', GROUP_CONCAT(id ORDER BY id) FROM d "\
"WHERE value = '2016-01-15T00:00:00'; "\
"SHOW COUNT(*) WARNINGS; "\
"SELECT 'date_eq_non_midnight', GROUP_CONCAT(id ORDER BY id) FROM d "\
"WHERE value = '2016-01-15T23:59:59'; "\
"SHOW COUNT(*) WARNINGS;" \
    "$DATABASE"

expect_output \
    "DATE range predicates compare stored date as midnight" \
    "date_lt|1,2
0
date_le|1,2
0
date_gt|3
0
date_between|1
0
date_in|3
0
date_not_in|1,2
0" \
    "SELECT 'date_lt', GROUP_CONCAT(id ORDER BY id) FROM d "\
"WHERE value < '2016-01-15T23:59:59'; "\
"SHOW COUNT(*) WARNINGS; "\
"SELECT 'date_le', GROUP_CONCAT(id ORDER BY id) FROM d "\
"WHERE value <= '2016-01-15T23:59:59'; "\
"SHOW COUNT(*) WARNINGS; "\
"SELECT 'date_gt', GROUP_CONCAT(id ORDER BY id) FROM d "\
"WHERE value > '2016-01-15T00:00:00'; "\
"SHOW COUNT(*) WARNINGS; "\
"SELECT 'date_between', GROUP_CONCAT(id ORDER BY id) FROM d "\
"WHERE value BETWEEN '2016-01-15T00:00:00' AND '2016-01-15T23:59:59'; "\
"SHOW COUNT(*) WARNINGS; "\
"SELECT 'date_in', GROUP_CONCAT(id ORDER BY id) FROM d "\
"WHERE value IN ('2016-01-15T23:59:59', '2016-01-16T00:00:00'); "\
"SHOW COUNT(*) WARNINGS; "\
"SELECT 'date_not_in', GROUP_CONCAT(id ORDER BY id) FROM d "\
"WHERE value NOT IN ('2016-01-15T23:59:59', '2016-01-16T00:00:00'); "\
"SHOW COUNT(*) WARNINGS;" \
    "$DATABASE"

expect_output \
    "DATE numeric offsets are validated but not shifted" \
    "date_offset_plus00|1
0
date_offset_plus14|1
0
date_offset_minus01|1
0
date_offset_non_midnight|NULL
0" \
    "SELECT 'date_offset_plus00', GROUP_CONCAT(id ORDER BY id) FROM d "\
"WHERE value = '2016-01-15T00:00:00+00:00'; "\
"SHOW COUNT(*) WARNINGS; "\
"SELECT 'date_offset_plus14', GROUP_CONCAT(id ORDER BY id) FROM d "\
"WHERE value = '2016-01-15T00:00:00+14:00'; "\
"SHOW COUNT(*) WARNINGS; "\
"SELECT 'date_offset_minus01', GROUP_CONCAT(id ORDER BY id) FROM d "\
"WHERE value = '2016-01-15T00:00:00-01:00'; "\
"SHOW COUNT(*) WARNINGS; "\
"SELECT 'date_offset_non_midnight', GROUP_CONCAT(id ORDER BY id) FROM d "\
"WHERE value = '2016-01-15T23:59:59+00:00'; "\
"SHOW COUNT(*) WARNINGS;" \
    "$DATABASE"

expect_output \
    "DATE trailing Z truncates with warning" \
    "date_null_safe_z|1
1
Warning|1292|Incorrect date value: '2016-01-15T00:00:00Z' for column 'value' at row 1
date_between_z|1
6" \
    "SELECT 'date_null_safe_z', GROUP_CONCAT(id ORDER BY id) FROM d "\
"WHERE value <=> '2016-01-15T00:00:00Z'; "\
"SHOW COUNT(*) WARNINGS; SHOW WARNINGS; "\
"SELECT 'date_between_z', GROUP_CONCAT(id ORDER BY id) FROM d "\
"WHERE value BETWEEN '2016-01-15T00:00:00Z' AND '2016-01-15T23:59:59z'; "\
"SHOW COUNT(*) WARNINGS;" \
    "$DATABASE"

expect_output \
    "DATE DML predicates reuse trailing Z conversion" \
    "1|1
2:2016-01-14,3:2016-01-16,4:N,10:2016-01-15" \
    "SET sql_mode = ''; "\
"UPDATE d SET id = 10 WHERE value = '2016-01-15T00:00:00Z'; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', IFNULL(value, 'N')) ORDER BY id) FROM d;" \
    "$DATABASE"

expect_error \
    "DATE one-digit offset hour is rejected" \
    1525 \
    "HY000" \
    "Incorrect DATE value: '2016-01-15T00:00:00+1:00'" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM d "\
"WHERE value = '2016-01-15T00:00:00+1:00';" \
    "$DATABASE"

expect_error \
    "DATE negative zero offset is rejected" \
    1525 \
    "HY000" \
    "Incorrect DATE value: '2016-01-15T00:00:00-00:00'" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM d "\
"WHERE value = '2016-01-15T00:00:00-00:00';" \
    "$DATABASE"

expect_error \
    "DATE out-of-range offset is rejected" \
    1525 \
    "HY000" \
    "Incorrect DATE value: '2016-01-15T00:00:00+14:01'" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM d "\
"WHERE value = '2016-01-15T00:00:00+14:01';" \
    "$DATABASE"

expect_output \
    "broader trailing garbage truncation is a deferred MySQL surface" \
    "date_z_offset|10
1" \
    "SELECT 'date_z_offset', GROUP_CONCAT(id ORDER BY id) FROM d "\
"WHERE value = '2016-01-15T00:00:00Z+00:00'; "\
"SHOW COUNT(*) WARNINGS;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_date_iso_predicate_strings_expectations: ok"
