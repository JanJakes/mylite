#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_unhex_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_unhex_function_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
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
run_mysql "CREATE DATABASE ${DATABASE} CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci; USE ${DATABASE}; SET NAMES utf8mb4; SET SESSION sql_mode = 'NO_ENGINE_SUBSTITUTION';" >/dev/null

scalar_expected=$(cat <<EXPECTED
4D7953514C	1267	0F	0ABC		41	0A	1	1	0
-1	0
EXPECTED
)
expect_output \
    "scalar unhex values" \
    "$scalar_expected" \
    "DO 0; SELECT HEX(UNHEX('4D7953514C')), HEX(UNHEX('1267')), HEX(UNHEX('F')), "\
"HEX(UNHEX('ABC')), HEX(UNHEX('')), HEX(UNHEX(X'3431')), HEX(UNHEX(X'41')), "\
"UNHEX(NULL) IS NULL, UNHEX((NULL)) IS NULL, @@warning_count; "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

numeric_expected=$(cat <<EXPECTED
00	01	10	15	0255	01	00	0
EXPECTED
)
expect_output \
    "numeric unhex values" \
    "$numeric_expected" \
    "DO 0; SELECT HEX(UNHEX(0)), HEX(UNHEX(1)), HEX(UNHEX(10)), HEX(UNHEX(15)), "\
"HEX(UNHEX(255)), HEX(UNHEX(TRUE)), HEX(UNHEX(FALSE)), @@warning_count;" \
    "$DATABASE"

invalid_expected=$(cat <<EXPECTED
1	1	1	1	1	0
Warning	1411	Incorrect string value: ''GG'' for function unhex
Warning	1411	Incorrect string value: ''41G'' for function unhex
Warning	1411	Incorrect string value: '' 41'' for function unhex
Warning	1411	Incorrect string value: ''41 '' for function unhex
Warning	1411	Incorrect string value: ''é'' for function unhex
5
EXPECTED
)
expect_output \
    "invalid unhex warnings" \
    "$invalid_expected" \
    "DO 0; SELECT UNHEX('GG') IS NULL, UNHEX('41G') IS NULL, UNHEX(' 41') IS NULL, "\
"UNHEX('41 ') IS NULL, UNHEX('é') IS NULL, @@warning_count; SHOW WARNINGS; "\
"SELECT @@warning_count;" \
    "$DATABASE"

negative_expected=$(cat <<EXPECTED
1	1	0
2
EXPECTED
)
expect_output \
    "negative and decimal warnings" \
    "$negative_expected" \
    "DO 0; SELECT UNHEX(-15) IS NULL, UNHEX(1.5) IS NULL, @@warning_count; "\
"SELECT @@warning_count;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE t ("\
"id INT, v VARCHAR(10), c CHAR(3), txt TEXT, b BINARY(3), vb VARBINARY(10), bi BIGINT"\
"); "\
"INSERT INTO t VALUES "\
"(1, '41', '41', '4100', '41', X'3431', 1267), "\
"(2, 'GG', 'GG', 'GG', 'GG', X'4747', -15), "\
"(3, NULL, NULL, NULL, NULL, NULL, NULL);" \
    "$DATABASE" >/dev/null

table_expected=$(cat <<\EXPECTED
1	41	41	4100	NULL	41	1267
2	NULL	NULL	NULL	NULL	NULL	NULL
3	NULL	NULL	NULL	NULL	NULL	NULL
7
EXPECTED
)
expect_output \
    "table unhex values" \
    "$table_expected" \
    "SELECT id, HEX(UNHEX(v)), HEX(UNHEX(c)), HEX(UNHEX(txt)), HEX(UNHEX(b)), "\
"HEX(UNHEX(vb)), HEX(UNHEX(bi)) FROM t ORDER BY id; SELECT @@warning_count;" \
    "$DATABASE"

expect_output \
    "table row envelope" \
    "3	NULL
2	NULL" \
    "SELECT id, HEX(UNHEX(v)) AS h FROM t WHERE id >= 1 ORDER BY id DESC LIMIT 2;" \
    "$DATABASE"

expect_error \
    "unhex rejects zero arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'UNHEX'" \
    "SELECT UNHEX();" \
    "$DATABASE"

expect_error \
    "unhex rejects too many arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'UNHEX'" \
    "SELECT UNHEX('a', 'b');" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_unhex_function_expectations: ok"
