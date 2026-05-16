#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_trim_string_functions_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_trim_string_functions_expectations: $1" >&2
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
run_mysql "CREATE DATABASE ${DATABASE} CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci; USE ${DATABASE}; SET NAMES utf8mb4;" >/dev/null

scalar_expected=$(cat <<\EXPECTED
barbar	barbar	bar	NULL	NULL	NULL	123	-7	1	0
-1	0
EXPECTED
)
expect_output \
    "scalar trim values" \
    "$scalar_expected" \
    "SELECT LTRIM('  barbar'), RTRIM('barbar   '), TRIM('  bar   '), "\
"TRIM(NULL), LTRIM(NULL), RTRIM(NULL), LTRIM(123), RTRIM(-7), TRIM(TRUE), TRIM(FALSE); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

explicit_expected=$(cat <<\EXPECTED
626172787878	626172	62617278	626172	6261722020	2020626172	626172
EXPECTED
)
expect_output \
    "explicit trim values" \
    "$explicit_expected" \
    "SELECT HEX(TRIM(LEADING 'x' FROM 'xxxbarxxx')), "\
"HEX(TRIM(BOTH 'x' FROM 'xxxbarxxx')), "\
"HEX(TRIM(TRAILING 'xyz' FROM 'barxxyz')), HEX(TRIM('x' FROM 'xxxbarxxx')), "\
"HEX(TRIM(LEADING FROM '  bar  ')), HEX(TRIM(TRAILING FROM '  bar  ')), "\
"HEX(TRIM(BOTH FROM '  bar  '));" \
    "$DATABASE"

sequence_expected=$(cat <<\EXPECTED
0920616263200A	616263	abc	abcxyz	xyzxyzabc	xabc	a	a	abc	abc	2	2	NULL	NULL
EXPECTED
)
expect_output \
    "sequence and null trim values" \
    "$sequence_expected" \
    "SELECT HEX(TRIM('\t abc \n')), HEX(TRIM('\n' FROM '\n\nabc\n')), "\
"TRIM('xyz' FROM 'xyzxyzabcxyz'), TRIM(LEADING 'xyz' FROM 'xyzxyzabcxyz'), "\
"TRIM(TRAILING 'xyz' FROM 'xyzxyzabcxyz'), TRIM('xy' FROM 'xyxabcxy'), "\
"TRIM('aa' FROM 'aaaaa'), TRIM(' ' FROM '  a  '), TRIM('' FROM 'abc'), "\
"TRIM(LEADING '' FROM 'abc'), TRIM(1 FROM 1112111), TRIM(BOTH 1 FROM 1112111), "\
"TRIM(NULL FROM 'abc'), TRIM('x' FROM NULL);" \
    "$DATABASE"

expect_output \
    "multibyte remove strings" \
    "616263	616263" \
    "SELECT HEX(TRIM('é' FROM 'ééabcé')), HEX(TRIM('🙂' FROM '🙂abc🙂'));" \
    "$DATABASE"

expect_output \
    "charset and collation values" \
    "utf8mb4	utf8mb4_0900_ai_ci	utf8mb4	utf8mb4_0900_ai_ci" \
    "SELECT CHARSET(TRIM(' a ')), COLLATION(TRIM(' a ')), "\
"CHARSET(LTRIM(' a')), COLLATION(RTRIM('a '));" \
    "$DATABASE"

expect_output \
    "mysql accepted deferred binary trim values" \
    "binary	binary	61" \
    "SELECT CHARSET(TRIM(CAST(' a ' AS BINARY))), "\
"COLLATION(TRIM(CAST(' a ' AS BINARY))), HEX(TRIM(CAST(' a ' AS BINARY)));" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE t ("\
"id INT, v VARCHAR(20), c CHAR(5), txt TEXT, i INT, d DECIMAL(6,2), y YEAR, dt DATETIME"\
"); "\
"INSERT INTO t VALUES "\
"(1, '  AbC  ', 'A  ', '  HeLLo  ', 123, 12.30, 2024, '2024-01-02 13:29:17'), "\
"(2, 'xYz', 'b   ', '', -7, -4.50, 70, NULL), "\
"(3, NULL, NULL, NULL, NULL, NULL, NULL, NULL);" \
    "$DATABASE" >/dev/null

table_expected=$(cat <<\EXPECTED
1	AbC	4162432020	2020416243	A	48654C4C6F2020	202048654C4C6F	123	12.30	2024	2024-01-02 13:29:17
2	xYz	78597A	78597A	b			-7	-4.50	1970	NULL
3	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "table trim values" \
    "$table_expected" \
    "SELECT id, TRIM(v), HEX(LTRIM(v)), HEX(RTRIM(v)), TRIM(c), HEX(LTRIM(txt)), "\
"HEX(RTRIM(txt)), TRIM(i), TRIM(d), TRIM(y), TRIM(dt) FROM t ORDER BY id;" \
    "$DATABASE"

expect_output \
    "table row envelope" \
    "3	NULL
2	xYz" \
    "SELECT id, TRIM(v) AS trimmed_v FROM t WHERE id >= 1 ORDER BY id DESC LIMIT 2;" \
    "$DATABASE"

labels_expected=$(cat <<\EXPECTED
implicit_trim	explicit_trim	ltrim_label	RTRIM(v)
AbC	a	AbC  	  AbC
EXPECTED
)
expect_output_with_headers \
    "labels and aliases" \
    "$labels_expected" \
    "SELECT TRIM('  AbC  ') AS implicit_trim, TRIM(BOTH 'x' FROM 'xax') AS explicit_trim, "\
"LTRIM(v) AS ltrim_label, RTRIM(v) FROM t WHERE id = 1;" \
    "$DATABASE"

expect_output \
    "mysql accepted deferred predicate" \
    "1" \
    "SELECT COUNT(*) FROM t WHERE TRIM(v) = 'AbC';" \
    "$DATABASE"

expect_output \
    "do row count" \
    "0	0" \
    "DO LTRIM('  a'), RTRIM(NULL), TRIM('  a  '), TRIM('x' FROM 'xxaxx'); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_error \
    "ltrim rejects zero arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'LTRIM'" \
    "SELECT LTRIM();" \
    "$DATABASE"

expect_error \
    "rtrim rejects too many arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'RTRIM'" \
    "SELECT RTRIM('a', 'b');" \
    "$DATABASE"

expect_error \
    "trim rejects zero arguments as syntax" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT TRIM();" \
    "$DATABASE"

expect_error \
    "trim rejects comma arguments as syntax" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT TRIM('a', 'b');" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_trim_string_functions_expectations: ok"
