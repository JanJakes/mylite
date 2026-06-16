#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_strcmp_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_strcmp_function_expectations: $1" >&2
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

run_mysql_with_headers() {
    sql=$1
    shift
    if [ -n "$MYSQL_BIN" ]; then
        if [ -n "$MYSQL_SOCKET" ]; then
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                    --batch --raw --default-character-set=utf8mb4 "$@"
        else
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=TCP -h127.0.0.1 -uroot \
                    --batch --raw --default-character-set=utf8mb4 "$@"
        fi
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
                --batch --raw --default-character-set=utf8mb4 "$@"
    fi
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

expect_output_with_headers() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql_with_headers "$sql" "$@")
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

expect_upstream_accepts() {
    label=$1
    sql=$2
    shift 2

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status_code=$?
    set -e

    if [ "$status_code" -ne 0 ]; then
        fail "$label: expected MySQL to accept deferred behavior, got [$output]"
    fi
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

core_expected=$(cat <<EXPECTED
-1	1	0	NULL	NULL	0	0	-1	1	1	-1	0
-1	0
EXPECTED
)
expect_output \
    "core string values" \
    "$core_expected" \
    "USE ${DATABASE}; SET NAMES utf8mb4; DO 0; "\
"SELECT STRCMP('text','text2'), STRCMP('text2','text'), STRCMP('text','text'), "\
"STRCMP(NULL,'a'), STRCMP('a',NULL), STRCMP('abc','ABC'), "\
"STRCMP('', ''), STRCMP('', 'a'), STRCMP('a', ''), "\
"STRCMP('a ', 'a'), STRCMP('a', 'a '), @@warning_count; "\
"SELECT ROW_COUNT(), @@warning_count;"

converted_expected=$(cat <<EXPECTED
-1	1	0	0	1	-1	0
EXPECTED
)
expect_output \
    "converted argument values" \
    "$converted_expected" \
    "USE ${DATABASE}; SET NAMES utf8mb4; "\
"SELECT STRCMP(10, '2'), STRCMP(2, '10'), STRCMP(TRUE, '1'), STRCMP(FALSE, '0'), "\
"STRCMP('9', 10), STRCMP(-1, '-2'), @@warning_count;"

nested_expected=$(cat <<EXPECTED
0	0
EXPECTED
)
expect_output \
    "nested argument values" \
    "$nested_expected" \
    "USE ${DATABASE}; SET NAMES utf8mb4; "\
"SELECT STRCMP(CONCAT('abc'), 'ABC'), STRCMP(LOCATE('b','abc'), '2');"

table_expected=$(cat <<EXPECTED
1	0	-1	-1	0	0
2	0	0	0	-1	-1
3	NULL	NULL	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "table-backed row scalar values" \
    "$table_expected" \
    "USE ${DATABASE}; SET NAMES utf8mb4; "\
"CREATE TABLE s (id INT PRIMARY KEY, v VARCHAR(10), t TEXT, n INT, y YEAR, d DATE); "\
"INSERT INTO s VALUES (1, 'abc', 'alpha', 10, 2024, '2024-01-02'), "\
"(2, 'ABC', 'Beta', 2, 70, '1970-03-04'), (3, NULL, NULL, NULL, NULL, NULL); "\
"SELECT id, STRCMP(v, 'abc'), STRCMP(t, 'beta'), STRCMP(n, '2'), "\
"STRCMP(y, '2024'), STRCMP(d, '2024-01-02') FROM s ORDER BY id;"

typed_expected=$(cat <<EXPECTED
1	0	0	0	0
2	NULL	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "table-backed descriptor type values" \
    "$typed_expected" \
    "USE ${DATABASE}; SET NAMES utf8mb4; "\
"CREATE TABLE typed (id INT PRIMARY KEY, c CHAR(5), dec_col DECIMAL(6,2), "\
"dt DATETIME, ts TIMESTAMP NULL DEFAULT NULL, ti TIME); "\
"INSERT INTO typed VALUES (1, 'abc', -12.30, '2024-01-02 03:04:05', "\
"'2024-01-02 03:04:06', '03:04:05'), (2, NULL, NULL, NULL, NULL, NULL); "\
"SELECT id, STRCMP(c, 'abc'), STRCMP(dec_col, '-12.30'), "\
"STRCMP(dt, '2024-01-02 03:04:05'), STRCMP(ts, '2024-01-02 03:04:06') "\
"FROM typed ORDER BY id;"

time_expected=$(cat <<EXPECTED
1	-1	0
2	NULL	NULL
EXPECTED
)
expect_output \
    "table-backed time value evidence" \
    "$time_expected" \
    "USE ${DATABASE}; SET NAMES utf8mb4; "\
"SELECT id, STRCMP(ti, '03:04:05'), STRCMP(CAST(ti AS CHAR), '03:04:05') "\
"FROM typed ORDER BY id;"

labels_expected=$(cat <<EXPECTED
c	STRCMP('b','a')
1	1
EXPECTED
)
expect_output_with_headers \
    "labels and whitespace" \
    "$labels_expected" \
    "USE ${DATABASE}; SET NAMES utf8mb4; "\
"SELECT STRCMP ('b','a') AS c, STRCMP('b','a') FROM DUAL;"

expect_output \
    "do status" \
    "0	0" \
    "USE ${DATABASE}; SET NAMES utf8mb4; DO STRCMP('x','x'), STRCMP(NULL,'a'); "\
"SELECT ROW_COUNT(), @@warning_count;"

expect_error \
    "strcmp zero arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count in the call to native function 'STRCMP'" \
    "USE ${DATABASE}; SELECT STRCMP();"

expect_error \
    "strcmp one argument" \
    1582 \
    "42000" \
    "Incorrect parameter count in the call to native function 'STRCMP'" \
    "USE ${DATABASE}; SELECT STRCMP('x');"

expect_error \
    "strcmp three arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count in the call to native function 'STRCMP'" \
    "USE ${DATABASE}; SELECT STRCMP('x','y','z');"

expect_upstream_accepts \
    "binary string comparison is deferred" \
    "USE ${DATABASE}; SELECT STRCMP(CAST('abc' AS BINARY), 'ABC');"

expect_upstream_accepts \
    "non-ASCII collation comparison is deferred" \
    "USE ${DATABASE}; SET NAMES utf8mb4; SELECT STRCMP('é', 'e');"

expect_upstream_accepts \
    "approximate numeric conversion is deferred" \
    "USE ${DATABASE}; SELECT STRCMP(1.5, '1.50');"

printf '%s\n' "mysql_baseline_strcmp_function_expectations: ok"
