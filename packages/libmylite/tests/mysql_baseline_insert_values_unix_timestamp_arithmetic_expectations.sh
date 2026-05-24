#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_insert_values_unix_timestamp_arithmetic_$$"

fail() {
    printf '%s\n' "mysql_baseline_insert_values_unix_timestamp_arithmetic_expectations: $1" >&2
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE};" >/dev/null

values_expected=$(cat <<EXPECTED
2	1	0
1	1704067200	1704067260	1704067140
2	1704067201	1704067201	NULL
EXPECTED
)
expect_output \
    "insert values unix timestamp arithmetic" \
    "$values_expected" \
    "SET time_zone = '+00:00'; SET timestamp = 1704067200; "\
"CREATE TABLE events(id INT AUTO_INCREMENT PRIMARY KEY, v BIGINT, u BIGINT UNSIGNED, n BIGINT NULL); "\
"INSERT INTO events(v, u, n) VALUES "\
"(UNIX_TIMESTAMP(), UNIX_TIMESTAMP() + 60, UNIX_TIMESTAMP() - 60), "\
"(UNIX_TIMESTAMP() + +1, UNIX_TIMESTAMP() - -1, UNIX_TIMESTAMP() + NULL); "\
"SELECT ROW_COUNT(), LAST_INSERT_ID(), @@warning_count; "\
"SELECT id, v, u, n FROM events ORDER BY id;" \
    "$DATABASE"

set_replace_duplicate_expected=$(cat <<EXPECTED
1	0
2	0
1	0
2	0
1	1704067230	1704067231	NULL
2	1704067220	1704067221	NULL
3	1704067210	1704067190	1704067200
4	1704067240	1704067160	1704067200
EXPECTED
)
expect_output \
    "insert set replace and duplicate unix timestamp arithmetic" \
    "$set_replace_duplicate_expected" \
    "SET time_zone = '+00:00'; SET timestamp = 1704067200; "\
"INSERT INTO events SET v = UNIX_TIMESTAMP() + 10, u = UNIX_TIMESTAMP() - 10, n = UNIX_TIMESTAMP(); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"REPLACE INTO events(id, v, u, n) VALUES "\
"(2, UNIX_TIMESTAMP() + 20, UNIX_TIMESTAMP() + 21, UNIX_TIMESTAMP() + NULL); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"REPLACE INTO events SET id = 4, v = UNIX_TIMESTAMP() + 40, "\
"u = UNIX_TIMESTAMP() - 40, n = UNIX_TIMESTAMP(); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"INSERT INTO events(id, v, u, n) VALUES (1, 0, 0, 0) "\
"ON DUPLICATE KEY UPDATE "\
"v = UNIX_TIMESTAMP() + 30, u = UNIX_TIMESTAMP() + 31, n = UNIX_TIMESTAMP() + NULL; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, v, u, n FROM events ORDER BY id;" \
    "$DATABASE"

auto_increment_expression_null_expected=$(cat <<EXPECTED
1	1	0
1	7
EXPECTED
)
expect_output \
    "auto increment expression null remains deferred in MyLite" \
    "$auto_increment_expression_null_expected" \
    "SET time_zone = '+00:00'; SET timestamp = 1704067200; "\
"CREATE TABLE generated_id(id INT AUTO_INCREMENT PRIMARY KEY, v INT); "\
"INSERT INTO generated_id(id, v) VALUES (UNIX_TIMESTAMP() + NULL, 7); "\
"SELECT ROW_COUNT(), LAST_INSERT_ID(), @@warning_count; "\
"SELECT id, v FROM generated_id;" \
    "$DATABASE"

null_ignore_expected=$(cat <<EXPECTED
Warning	1048	Column 'v' cannot be null
-1	1	0
EXPECTED
)
expect_error \
    "null arithmetic into not null" \
    1048 \
    23000 \
    "Column 'v' cannot be null" \
    "USE ${DATABASE}; CREATE TABLE required_value(v BIGINT NOT NULL); "\
"INSERT INTO required_value VALUES (UNIX_TIMESTAMP() + NULL);"
expect_output \
    "insert ignore null arithmetic adjustment" \
    "$null_ignore_expected" \
    "INSERT IGNORE INTO required_value VALUES (UNIX_TIMESTAMP() + NULL); "\
"SHOW WARNINGS; SELECT ROW_COUNT(), @@warning_count, v FROM required_value;" \
    "$DATABASE"

expect_error \
    "target integer out of range" \
    1264 \
    22003 \
    "Out of range value for column 'small_value' at row 1" \
    "USE ${DATABASE}; CREATE TABLE small_target(small_value INT); "\
"SET timestamp = 1704067200; "\
"INSERT INTO small_target VALUES (UNIX_TIMESTAMP() + 1000000000);"

expect_error \
    "arithmetic overflow" \
    1690 \
    22003 \
    "BIGINT value is out of range" \
    "USE ${DATABASE}; CREATE TABLE overflow_target(v BIGINT); "\
"SET timestamp = 1704067200; "\
"INSERT INTO overflow_target VALUES (UNIX_TIMESTAMP() + 9223372036854775807);"

printf '%s\n' "mysql_baseline_insert_values_unix_timestamp_arithmetic_expectations: ok"
