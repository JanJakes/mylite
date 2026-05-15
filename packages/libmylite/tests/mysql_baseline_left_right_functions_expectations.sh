#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_left_right_functions_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_left_right_functions_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --default-character-set=utf8mb4 "$@"
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
fooba	bar			ab	bc	abc	abc	NULL	NULL	NULL	NULL	é	🙂	é🙂	é🙂	2	1	4	1	12	345	mylite_	SUBSTITUTION	1	0
-1	0
EXPECTED
)
expect_output \
    "scalar left right values" \
    "$scalar_expected" \
    "DO 0; SELECT LEFT('foobarbar', 5), RIGHT('foobarbar', 3), "\
"LEFT('abc', 0), RIGHT('abc', 0), LEFT('abc', +2), RIGHT('abc', +2), "\
"LEFT('abc', 9), RIGHT('abc', 9), LEFT(NULL, 1), LEFT('abc', NULL), "\
"RIGHT(NULL, 1), RIGHT('abc', NULL), LEFT('é🙂', 1), RIGHT('é🙂', 1), "\
"LEFT('é🙂', 2), RIGHT('é🙂', 2), LENGTH(LEFT('é🙂', 1)), "\
"CHAR_LENGTH(LEFT('é🙂', 1)), LENGTH(RIGHT('é🙂', 1)), "\
"CHAR_LENGTH(RIGHT('é🙂', 1)), LEFT(12345, 2), RIGHT(-12345, 3), "\
"LEFT(DATABASE(), 7), RIGHT(@@sql_mode, 12), "\
"LEFT(TRUE, 1), RIGHT(FALSE, 1); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "zero and negative lengths" \
    "		" \
    "SELECT LEFT('abc', -1), RIGHT('abc', -1), LEFT('abc', 0);" \
    "$DATABASE"

expect_output \
    "from dual whitespace values" \
    "a	c" \
    "SELECT LEFT ('abc', 1), RIGHT ('abc', 1) FROM DUAL;" \
    "$DATABASE"

expect_output \
    "do row count" \
    "0	0" \
    "DO LEFT('abc', 1), RIGHT(NULL, 1), LEFT(TRUE, 1); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE t ("\
"id INT, v VARCHAR(20), c CHAR(5), txt TEXT, i INT, d DECIMAL(6,2), y YEAR, dt DATETIME"\
"); "\
"INSERT INTO t VALUES "\
"(1, 'abc', 'a  ', 'hello', 12345, 12.30, 2024, '2024-01-02 13:29:17'), "\
"(2, 'é🙂', 'é', '', -7, -4.50, 70, NULL), "\
"(3, NULL, NULL, NULL, NULL, NULL, NULL, NULL);" \
    "$DATABASE" >/dev/null

table_expected=$(cat <<\EXPECTED
1	ab	c	a	a	hel	lo	12	30	2024	2024-01-02
2	é🙂	🙂	é	é			-7	50	1970	NULL
3	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "table left right values" \
    "$table_expected" \
    "SELECT id, LEFT(v, 2), RIGHT(v, 1), LEFT(c, 2), RIGHT(c, 1), "\
"LEFT(txt, 3), RIGHT(txt, 2), LEFT(i, 2), RIGHT(d, 2), LEFT(y, 4), "\
"LEFT(dt, 10) FROM t ORDER BY id;" \
    "$DATABASE"

expect_output \
    "table row envelope" \
    "3	NULL
2	🙂" \
    "SELECT id, RIGHT(v, 1) AS rv FROM t WHERE id >= 1 ORDER BY id DESC LIMIT 2;" \
    "$DATABASE"

expect_output \
    "table null and nonpositive branches" \
    "1			NULL	NULL
3	NULL	NULL	NULL	NULL" \
    "SELECT id, LEFT(v, 0), RIGHT(v, -1), LEFT(v, NULL), RIGHT(v, NULL) "\
"FROM t WHERE id IN (1, 3) ORDER BY id;" \
    "$DATABASE"

labels_expected=$(cat <<\EXPECTED
LEFT(v, 2)	r
ab	c
EXPECTED
)
expect_output_with_headers \
    "labels and aliases" \
    "$labels_expected" \
    "SELECT LEFT(v, 2), RIGHT(v, 1) AS r FROM t WHERE id = 1;" \
    "$DATABASE"

expect_output \
    "mysql accepted deferred binary forms" \
    "4142	4243	4100	0042" \
    "SELECT HEX(LEFT(CAST('ABC' AS BINARY), 2)), "\
"HEX(RIGHT(CAST('ABC' AS BINARY), 2)), HEX(LEFT(X'410042', 2)), "\
"HEX(RIGHT(X'410042', 2));" \
    "$DATABASE"

expect_output \
    "mysql accepted deferred converted length and predicate" \
    "ab	ab	1" \
    "SELECT LEFT('abcdef', 1.5), LEFT('abcdef', '2'), "\
"(SELECT COUNT(*) FROM t WHERE LEFT(v, 1) = 'a');" \
    "$DATABASE"

expect_error \
    "left rejects zero arguments as syntax" \
    1064 \
    42000 \
    "near ')' at line 1" \
    "SELECT LEFT();" \
    "$DATABASE"

expect_error \
    "left rejects one argument as syntax" \
    1064 \
    42000 \
    "near ')' at line 1" \
    "SELECT LEFT('a');" \
    "$DATABASE"

expect_error \
    "right rejects too many arguments as syntax" \
    1064 \
    42000 \
    "near ', 2)' at line 1" \
    "SELECT RIGHT('a', 1, 2);" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_left_right_functions_expectations: ok"
