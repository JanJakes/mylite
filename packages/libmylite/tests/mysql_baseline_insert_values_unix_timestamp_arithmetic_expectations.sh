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

text_targets_expected=$(cat <<EXPECTED
1	0
_site_transient_timeout_tag1	1704067110	no
1704067200	1704067110	1704067205	1704067207	NULL	1704067208
1	1704067230	1704067231
2	1704067220	NULL
3	1704067240	NULL
EXPECTED
)
expect_output \
    "insert unix timestamp arithmetic text targets" \
    "$text_targets_expected" \
    "SET time_zone = '+00:00'; SET timestamp = 1704067200; "\
"CREATE TABLE wp_options(option_name VARCHAR(191), option_value LONGTEXT, autoload VARCHAR(20)); "\
"INSERT INTO wp_options(option_name, option_value, autoload) "\
"VALUES ('_site_transient_timeout_tag1', UNIX_TIMESTAMP() + -90, 'no'); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT option_name, option_value, autoload FROM wp_options; "\
"CREATE TABLE text_values(v VARCHAR(32), txt TEXT, lt LONGTEXT, c CHAR(20), "\
"n VARCHAR(20), nn VARCHAR(20) NOT NULL); "\
"INSERT INTO text_values(v, txt, lt, c, n, nn) VALUES "\
"(UNIX_TIMESTAMP(), UNIX_TIMESTAMP() + -90, UNIX_TIMESTAMP() - -5, "\
"UNIX_TIMESTAMP() + 7, UNIX_TIMESTAMP() + NULL, UNIX_TIMESTAMP() + 8); "\
"SELECT v, txt, lt, c, n, nn FROM text_values; "\
"CREATE TABLE text_events(id INT PRIMARY KEY, v LONGTEXT, s VARCHAR(32)); "\
"INSERT INTO text_events SET id = 1, v = UNIX_TIMESTAMP() + 10, "\
"s = UNIX_TIMESTAMP() + 11; "\
"INSERT INTO text_events VALUES (2, 'old', 'old'); "\
"REPLACE INTO text_events(id, v, s) VALUES "\
"(2, UNIX_TIMESTAMP() + 20, UNIX_TIMESTAMP() + NULL); "\
"REPLACE INTO text_events SET id = 3, v = UNIX_TIMESTAMP() + 40, "\
"s = UNIX_TIMESTAMP() + NULL; "\
"INSERT INTO text_events(id, v, s) VALUES (1, 'old', 'old') "\
"ON DUPLICATE KEY UPDATE v = UNIX_TIMESTAMP() + 30, s = UNIX_TIMESTAMP() + 31; "\
"SELECT id, v, s FROM text_events ORDER BY id;" \
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

text_null_ignore_expected=$(cat <<EXPECTED
Warning	1048	Column 'v' cannot be null
-1	1	[]
EXPECTED
)
expect_error \
    "null arithmetic into not null text" \
    1048 \
    23000 \
    "Column 'v' cannot be null" \
    "USE ${DATABASE}; CREATE TABLE required_text(v VARCHAR(10) NOT NULL); "\
"INSERT INTO required_text VALUES (UNIX_TIMESTAMP() + NULL);"
expect_output \
    "insert ignore null text arithmetic adjustment" \
    "$text_null_ignore_expected" \
    "INSERT IGNORE INTO required_text VALUES (UNIX_TIMESTAMP() + NULL); "\
"SHOW WARNINGS; SELECT ROW_COUNT(), @@warning_count, CONCAT('[', v, ']') FROM required_text;" \
    "$DATABASE"

expect_error \
    "target string too long" \
    1406 \
    22001 \
    "Data too long for column 'v' at row 1" \
    "USE ${DATABASE}; CREATE TABLE short_text(v VARCHAR(4)); "\
"SET timestamp = 1704067200; "\
"INSERT INTO short_text VALUES (UNIX_TIMESTAMP());"

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
