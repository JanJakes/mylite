#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_update_unix_timestamp_arithmetic_$$"

fail() {
    printf '%s\n' "mysql_baseline_update_unix_timestamp_arithmetic_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "${MYSQL_BIN:-mysql}" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --default-character-set=utf8mb4 --batch --raw --skip-column-names "$@"
        return
    fi
    if [ -n "$MYSQL_BIN" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=TCP -h127.0.0.1 -uroot --default-character-set=utf8mb4 \
                --batch --raw --skip-column-names "$@"
        return
    fi
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --default-character-set=utf8mb4 \
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; SET time_zone = '+00:00';" >/dev/null

expect_output \
    "wordpress longtext update" \
    "$(cat <<EXPECTED
1	0	1704067110
EXPECTED
)" \
    "USE ${DATABASE}; SET timestamp = 1704067200; "\
"CREATE TABLE wp_options(option_name VARCHAR(191), option_value LONGTEXT, autoload VARCHAR(20)); "\
"INSERT INTO wp_options VALUES ('_site_transient_timeout_tag1', 'old', 'no'), ('other', 'old', 'yes'); "\
"UPDATE wp_options SET option_value = UNIX_TIMESTAMP() + -90 "\
"WHERE option_name = '_site_transient_timeout_tag1'; "\
"SELECT ROW_COUNT(), @@warning_count, option_value FROM wp_options "\
"WHERE option_name = '_site_transient_timeout_tag1';"

expect_output \
    "integer updates order limit" \
    "$(cat <<EXPECTED
1	0	1:1704067201:1704067199:NULL,2:1704067210:0:0,3:0:0:0
0	0
EXPECTED
)" \
    "USE ${DATABASE}; SET timestamp = 1704067200; "\
"CREATE TABLE events(id INT PRIMARY KEY, v BIGINT, u BIGINT UNSIGNED, n BIGINT NULL); "\
"INSERT INTO events VALUES (1, 0, 0, 0), (2, 0, 0, 0), (3, 0, 0, 0); "\
"UPDATE events SET v = UNIX_TIMESTAMP() + 1 WHERE id = 1; "\
"UPDATE events SET u = UNIX_TIMESTAMP() - 1 WHERE id = 1; "\
"UPDATE events SET n = UNIX_TIMESTAMP() + NULL WHERE id = 1; "\
"UPDATE events SET v = UNIX_TIMESTAMP() + 10 WHERE id IN (1, 2) ORDER BY id DESC LIMIT 1; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', v, ':', u, ':', COALESCE(n, 'NULL')) ORDER BY id) FROM events; "\
"UPDATE events SET v = UNIX_TIMESTAMP() + 20 ORDER BY id LIMIT 0; "\
"SELECT ROW_COUNT(), @@warning_count;"

expect_output \
    "string target updates" \
    "$(cat <<EXPECTED
1704067200	1704067110	1704067205	1704067207	1	1704067208
EXPECTED
)" \
    "USE ${DATABASE}; SET timestamp = 1704067200; "\
"CREATE TABLE text_values(c CHAR(12), v VARCHAR(12), txt TEXT, lt LONGTEXT, n VARCHAR(12), nn VARCHAR(12) NOT NULL); "\
"INSERT INTO text_values VALUES ('a','a','a','a','a','a'); "\
"UPDATE text_values SET c = UNIX_TIMESTAMP(); "\
"UPDATE text_values SET v = UNIX_TIMESTAMP() + -90; "\
"UPDATE text_values SET txt = UNIX_TIMESTAMP() - -5; "\
"UPDATE text_values SET lt = UNIX_TIMESTAMP() + 7; "\
"UPDATE text_values SET n = UNIX_TIMESTAMP() + NULL; "\
"UPDATE text_values SET nn = UNIX_TIMESTAMP() + 8; "\
"SELECT c, v, txt, lt, n IS NULL, nn FROM text_values;"

expect_error \
    "strict null into not null text" \
    1048 \
    "23000" \
    "Column 'v' cannot be null" \
    "USE ${DATABASE}; SET timestamp = 1704067200; "\
"CREATE TABLE required_text(v VARCHAR(12) NOT NULL); "\
"INSERT INTO required_text VALUES ('x'); "\
"UPDATE required_text SET v = UNIX_TIMESTAMP() + NULL;"

expect_output \
    "nonstrict null into not null text" \
    "1	1	[]" \
    "USE ${DATABASE}; SET timestamp = 1704067200; SET sql_mode = ''; "\
"UPDATE required_text SET v = UNIX_TIMESTAMP() + NULL; "\
"SELECT ROW_COUNT(), @@warning_count, CONCAT('[', v, ']') FROM required_text;"

expect_error \
    "short varchar strict length" \
    1406 \
    "22001" \
    "Data too long for column 'v' at row 1" \
    "USE ${DATABASE}; SET sql_mode = 'STRICT_TRANS_TABLES'; SET timestamp = 1704067200; "\
"CREATE TABLE short_text(v VARCHAR(4)); INSERT INTO short_text VALUES ('x'); "\
"UPDATE short_text SET v = UNIX_TIMESTAMP();"

expect_output \
    "no match skips assignment conversion" \
    "0	0	x" \
    "USE ${DATABASE}; SET timestamp = 1704067200; "\
"UPDATE short_text SET v = UNIX_TIMESTAMP() WHERE v = 'missing'; "\
"SELECT ROW_COUNT(), @@warning_count, v FROM short_text;"

expect_error \
    "small int target out of range" \
    1264 \
    "22003" \
    "Out of range value for column 'v' at row 1" \
    "USE ${DATABASE}; SET timestamp = 1704067200; "\
"CREATE TABLE small_target(v INT); INSERT INTO small_target VALUES (0); "\
"UPDATE small_target SET v = UNIX_TIMESTAMP() + 1000000000;"

expect_error \
    "bigint arithmetic overflow" \
    1690 \
    "22003" \
    "BIGINT value is out of range" \
    "USE ${DATABASE}; SET timestamp = 1704067200; "\
"CREATE TABLE overflow_target(v BIGINT); INSERT INTO overflow_target VALUES (1); "\
"UPDATE overflow_target SET v = UNIX_TIMESTAMP() + 9223372036854775807;"

expect_output \
    "no match skips arithmetic overflow" \
    "0	0	1" \
    "USE ${DATABASE}; SET timestamp = 1704067200; "\
"UPDATE overflow_target SET v = UNIX_TIMESTAMP() + 9223372036854775807 WHERE v = 999; "\
"SELECT ROW_COUNT(), @@warning_count, v FROM overflow_target;"

printf '%s\n' "mysql_baseline_update_unix_timestamp_arithmetic_expectations: ok"
