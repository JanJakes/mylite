#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_string_padding_functions_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_string_padding_functions_expectations: $1" >&2
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
??hi	hi???	h	h	MySQLMySQLMySQL	202020	3							NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	0	0
-1	0
EXPECTED
)
expect_output \
    "scalar padding values" \
    "$scalar_expected" \
    "DO 0; SELECT LPAD('hi',4,'??'), RPAD('hi',5,'?'), LPAD('hi',1,'??'), "\
"RPAD('hi',1,'?'), REPEAT('MySQL',3), HEX(SPACE(3)), CHAR_LENGTH(SPACE(3)), "\
"LPAD('hi',0,'?'), RPAD('hi',0,'?'), REPEAT('x',0), REPEAT('x',-1), "\
"HEX(SPACE(0)), HEX(SPACE(-1)), LPAD('hi',-1,'?'), RPAD('hi',-1,'?'), "\
"LPAD(NULL,4,'?'), LPAD('hi',NULL,'?'), LPAD('hi',4,NULL), RPAD(NULL,4,'?'), "\
"REPEAT(NULL,2), REPEAT('x',NULL), SPACE(NULL), "\
"LPAD('hi',4,'') IS NULL, RPAD('hi',4,'') IS NULL; "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "empty pad string and truncation" \
    "	0		0	hi	hi	h	h	he	he	abchi	hiabc" \
    "SELECT HEX(LPAD('hi',4,'')), CHAR_LENGTH(LPAD('hi',4,'')), "\
"HEX(RPAD('hi',4,'')), CHAR_LENGTH(RPAD('hi',4,'')), "\
"LPAD('hi',2,''), RPAD('hi',2,''), LPAD('hi',1,''), RPAD('hi',1,''), "\
"LPAD('hello',2,'?'), RPAD('hello',2,'?'), LPAD('hi',5,'abc'), RPAD('hi',5,'abc');" \
    "$DATABASE"

expect_output \
    "multibyte padding values" \
    "🙂🙂é	F09F9982F09F9982C3A9	3	10	é🙂🙂	C3A9F09F9982F09F9982	3	10	é🙂é🙂	C3A9F09F9982C3A9F09F9982	4	12" \
    "SELECT LPAD('é',3,'🙂'), HEX(LPAD('é',3,'🙂')), CHAR_LENGTH(LPAD('é',3,'🙂')), "\
"LENGTH(LPAD('é',3,'🙂')), RPAD('é',3,'🙂'), HEX(RPAD('é',3,'🙂')), "\
"CHAR_LENGTH(RPAD('é',3,'🙂')), LENGTH(RPAD('é',3,'🙂')), REPEAT('é🙂',2), "\
"HEX(REPEAT('é🙂',2)), CHAR_LENGTH(REPEAT('é🙂',2)), LENGTH(REPEAT('é🙂',2));" \
    "$DATABASE"

expect_output \
    "numeric and boolean conversion" \
    "00123	-7xx	111	00	20		x	" \
    "SELECT LPAD(123,5,'0'), RPAD(-7,4,'x'), REPEAT(TRUE,3), REPEAT(FALSE,2), "\
"HEX(SPACE(TRUE)), HEX(SPACE(FALSE)), LPAD('x', TRUE, '?'), RPAD('x', FALSE, '?');" \
    "$DATABASE"

expect_output \
    "from dual whitespace values" \
    "??hi	hi??	xx	2020" \
    "SELECT LPAD ('hi',4,'?'), RPAD ('hi',4,'?'), REPEAT ('x',2), HEX(SPACE (2)) FROM DUAL;" \
    "$DATABASE"

expect_output \
    "scalar integer function padding arguments" \
    "00hi	hix	abab	2020" \
    "SELECT LPAD('hi', ABS(-4), '0'), RPAD('hi', LENGTH('abc'), 'x'), "\
"REPEAT('ab', ABS(-2)), HEX(SPACE(LENGTH('xy')));" \
    "$DATABASE"

expect_output \
    "do row count" \
    "0	0" \
    "DO LPAD('abc', 5, '0'), RPAD(NULL, 5, '0'), REPEAT('x', 2), SPACE(2); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE t ("\
"id INT, v VARCHAR(20), c CHAR(5), txt TEXT, i INT, d DECIMAL(6,2), y YEAR, dt DATETIME"\
"); "\
"INSERT INTO t VALUES "\
"(1, 'abc', 'a  ', 'hi', 123, 12.30, 2024, '2024-01-02 13:29:17'), "\
"(2, 'é🙂', 'é', '', -7, -4.50, 70, NULL), "\
"(3, NULL, NULL, NULL, NULL, NULL, NULL, NULL);" \
    "$DATABASE" >/dev/null

table_expected=$(cat <<\EXPECTED
1	00abc	abc00	abcabc	2020	000a	a000	hihi	00123	12.30x	2024	323032342D30312D303220
2	000é🙂	é🙂000	é🙂é🙂	2020	000é	é000		000-7	-4.50x	1970	NULL
3	NULL	NULL	NULL	2020	NULL	NULL	NULL	NULL	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "table padding values" \
    "$table_expected" \
    "SELECT id, LPAD(v, 5, '0'), RPAD(v, 5, '0'), REPEAT(v, 2), HEX(SPACE(2)), "\
"LPAD(c, 4, '0'), RPAD(c, 4, '0'), REPEAT(txt, 2), LPAD(i, 5, '0'), "\
"RPAD(d, 6, 'x'), LPAD(y, 4, '0'), HEX(RPAD(dt, 11, ' ')) FROM t ORDER BY id;" \
    "$DATABASE"

expect_output \
    "table row envelope" \
    "3	NULL
2	0é🙂" \
    "SELECT id, LPAD(v, 3, '0') AS padded FROM t WHERE id >= 1 ORDER BY id DESC LIMIT 2;" \
    "$DATABASE"

expect_output \
    "table integer expression padding arguments" \
    "1	00abc	abc	abc	20
2	0000é🙂	é🙂xx	é🙂é🙂	2020
3	NULL	NULL	NULL	202020" \
    "SELECT id, LPAD(v, id + 4, '0'), RPAD(v, ABS(id + 2), 'x'), "\
"REPEAT(v, id), HEX(SPACE(id)) FROM t ORDER BY id;" \
    "$DATABASE"

expect_output \
    "mysql accepted deferred conversion and binary forms" \
    "??hi	hi??	xx	2020	00004142	41420000	41424142" \
    "SELECT LPAD('hi', 3.5, '?'), RPAD('hi', '4', '?'), REPEAT('x', 1.5), "\
"HEX(SPACE('2')), HEX(LPAD(CAST('AB' AS BINARY), 4, X'00')), "\
"HEX(RPAD(CAST('AB' AS BINARY), 4, X'00')), HEX(REPEAT(CAST('AB' AS BINARY), 2));" \
    "$DATABASE"

expect_error \
    "lpad rejects zero arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'LPAD'" \
    "SELECT LPAD();" \
    "$DATABASE"

expect_error \
    "rpad rejects two arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'RPAD'" \
    "SELECT RPAD('a', 1);" \
    "$DATABASE"

expect_error \
    "space rejects too many arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'SPACE'" \
    "SELECT SPACE(1, 2);" \
    "$DATABASE"

expect_error \
    "repeat rejects one argument as syntax" \
    1064 \
    42000 \
    "near ')' at line 1" \
    "SELECT REPEAT('a');" \
    "$DATABASE"

expect_error \
    "repeat rejects too many arguments as syntax" \
    1064 \
    42000 \
    "near ',2)' at line 1" \
    "SELECT REPEAT('a',1,2);" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_string_padding_functions_expectations: ok"
