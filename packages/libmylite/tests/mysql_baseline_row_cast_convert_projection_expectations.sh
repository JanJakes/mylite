#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_row_cast_convert_projection_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_row_cast_convert_projection_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw \
            --binary-as-hex=1 --default-character-set=utf8mb4 \
            --skip-column-names "$@"
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
        *)
            fail "$label: expected error $code/$state containing [$message], got [$output]"
            ;;
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
run_mysql "CREATE DATABASE ${DATABASE} CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci; USE ${DATABASE}; SET NAMES utf8mb4;" >/dev/null
run_mysql "CREATE TABLE t (id INT PRIMARY KEY, s VARCHAR(20), txt TEXT, n INT, u BIGINT UNSIGNED, nullable VARCHAR(20) NULL); INSERT INTO t VALUES (1, 'ABC', '123abc', 123, 7, NULL), (2, '3.9', 'abc', -7, 9, 'x'), (3, NULL, NULL, NULL, NULL, NULL); CREATE TABLE complement_t (id INT PRIMARY KEY, s VARCHAR(32), u VARCHAR(32)); INSERT INTO complement_t VALUES (1, '9223372036854775808', '-1');" "$DATABASE" >/dev/null

binary_expected=$(cat <<\EXPECTED
1	414243	313233	NULL	414243	414243
2	332E39	2D37	78	332E39	332E39
3	NULL	NULL	NULL	NULL	NULL
binary	binary	2	binary	binary	2
EXPECTED
)
expect_output \
    "row binary values and metadata" \
    "$binary_expected" \
    "SELECT id, HEX(CAST(s AS BINARY)), HEX(CAST(n AS BINARY)), "\
"HEX(CAST(nullable AS BINARY)), HEX(CONVERT(s, BINARY)), "\
"HEX(CONVERT(s USING BINARY)) FROM t ORDER BY id; "\
"SELECT CHARSET(CAST(s AS BINARY)), COLLATION(CAST(s AS BINARY)), "\
"COERCIBILITY(CAST(s AS BINARY)), CHARSET(CONVERT(s USING BINARY)), "\
"COLLATION(CONVERT(s USING BINARY)), COERCIBILITY(CONVERT(s USING BINARY)) "\
"FROM t WHERE id = 1;" \
    "$DATABASE"

char_expected=$(cat <<\EXPECTED
1	123	ABC	7	NULL
2	-7	3.9	9	x
3	NULL	NULL	NULL	NULL
utf8mb4	utf8mb4_0900_ai_ci	2	utf8mb4	utf8mb4_0900_ai_ci	2
EXPECTED
)
expect_output \
    "row char values and metadata" \
    "$char_expected" \
    "SELECT id, CAST(n AS CHAR), CAST(s AS CHAR), CONVERT(u, CHAR), "\
"CAST(nullable AS CHAR) FROM t ORDER BY id; "\
"SELECT CHARSET(CAST(n AS CHAR)), COLLATION(CAST(n AS CHAR)), "\
"COERCIBILITY(CAST(n AS CHAR)), CHARSET(CONVERT(n, CHAR)), "\
"COLLATION(CONVERT(n, CHAR)), COERCIBILITY(CONVERT(n, CHAR)) FROM t WHERE id = 1;" \
    "$DATABASE"

signed_expected=$(cat <<\EXPECTED
1	0	123	123	7	NULL
2	3	0	-7	9	0
3	NULL	NULL	NULL	NULL	NULL
5
Warning	1292	Truncated incorrect INTEGER value: 'ABC'
Warning	1292	Truncated incorrect INTEGER value: '123abc'
Warning	1292	Truncated incorrect INTEGER value: '3.9'
Warning	1292	Truncated incorrect INTEGER value: 'abc'
Warning	1292	Truncated incorrect INTEGER value: 'x'
EXPECTED
)
expect_output \
    "row signed values and warnings" \
    "$signed_expected" \
"SELECT id, CAST(s AS SIGNED), CONVERT(txt, SIGNED), "\
"CAST(n AS SIGNED INTEGER), CONVERT(u, UNSIGNED INTEGER), "\
"CAST(nullable AS SIGNED) FROM t ORDER BY id; "\
"SHOW COUNT(*) WARNINGS; SHOW WARNINGS;" \
    "$DATABASE"

complement_expected=$(cat <<\EXPECTED
-9223372036854775808	18446744073709551615
2
Warning	1105	Cast to signed converted positive out-of-range integer to its negative complement
Warning	1105	Cast to unsigned converted negative integer to its positive complement
EXPECTED
)
expect_output \
    "row string complement warnings" \
    "$complement_expected" \
"SELECT CAST(s AS SIGNED), CAST(u AS UNSIGNED) FROM complement_t ORDER BY id; "\
"SHOW COUNT(*) WARNINGS; SHOW WARNINGS;" \
    "$DATABASE"

charset_expected=$(cat <<\EXPECTED
1	utf8mb4	utf8mb4_0900_ai_ci	utf8mb3	utf8mb3_general_ci	utf8mb3	utf8mb3_general_ci	latin1	latin1_swedish_ci	123	NULL
2	utf8mb4	utf8mb4_0900_ai_ci	utf8mb3	utf8mb3_general_ci	utf8mb3	utf8mb3_general_ci	latin1	latin1_swedish_ci	-7	NULL
3	utf8mb4	utf8mb4_0900_ai_ci	utf8mb3	utf8mb3_general_ci	utf8mb3	utf8mb3_general_ci	latin1	latin1_swedish_ci	NULL	NULL
4
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
Warning	1287	'utf8mb3' is deprecated and will be removed in a future release. Please use utf8mb4 instead
Warning	1287	'utf8mb3' is deprecated and will be removed in a future release. Please use utf8mb4 instead
EXPECTED
)
expect_output \
    "row charset values metadata and warnings" \
    "$charset_expected" \
    "SELECT id, CHARSET(CONVERT(s USING utf8mb4)), "\
"COLLATION(CONVERT(s USING utf8mb4)), CHARSET(CONVERT(s USING utf8)), "\
"COLLATION(CONVERT(s USING utf8)), CHARSET(CONVERT(s USING utf8mb3)), "\
"COLLATION(CONVERT(s USING utf8mb3)), CHARSET(CONVERT(s USING latin1)), "\
"COLLATION(CONVERT(s USING latin1)), CONVERT(n USING 'latin1'), "\
"CONVERT(NULL USING 'utf8mb4') FROM t ORDER BY id; "\
"SHOW COUNT(*) WARNINGS; SHOW WARNINGS;" \
    "$DATABASE"

numeric_column_expected=$(cat <<\EXPECTED
1	123	123
2	18446744073709551609	18446744073709551609
3	NULL	NULL
0
EXPECTED
)
expect_output \
    "numeric signed to unsigned has no complement warning" \
    "$numeric_column_expected" \
    "SELECT id, CAST(n AS UNSIGNED), CONVERT(n, UNSIGNED) FROM t ORDER BY id; "\
"SHOW COUNT(*) WARNINGS;" \
    "$DATABASE"

constant_expected=$(cat <<\EXPECTED
1	414243	ABC	4	1
2	414243	ABC	4	1
3	414243	ABC	4	1
EXPECTED
)
expect_output \
    "row-backed constants" \
    "$constant_expected" \
    "SELECT id, HEX(CONVERT('ABC' USING BINARY)), CONVERT('ABC', CHAR), "\
"CAST('4' AS SIGNED), CAST(TRUE AS UNSIGNED) FROM t ORDER BY id;" \
    "$DATABASE"

expect_error \
    "unknown charset" \
    1115 \
    42000 \
    "Unknown character set: 'nosuch_charset'" \
    "SELECT CONVERT(s USING nosuch_charset) FROM t;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_row_cast_convert_projection_expectations: ok"
