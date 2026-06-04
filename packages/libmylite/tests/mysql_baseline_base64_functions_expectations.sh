#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_base64_functions_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_base64_functions_expectations: $1" >&2
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
YQ==	YWI=	YWJj	YWJjZA==	YWJjZGVm		1	MTIz	MQ==	MA==	LTE=	MQ==	LTE=	616263	616263		1	1	1	0	dGVzdA==	74657374
EXPECTED
)
expect_output \
    "scalar base64 values" \
    "$scalar_expected" \
    "DO 0; SELECT TO_BASE64('a'), TO_BASE64('ab'), TO_BASE64('abc'), TO_BASE64('abcd'), "\
"TO_BASE64('abcdef'), TO_BASE64(''), TO_BASE64(NULL) IS NULL, TO_BASE64(123), TO_BASE64(TRUE), "\
"TO_BASE64(FALSE), TO_BASE64(-1), TO_BASE64(+1), TO_BASE64(- 1), "\
"HEX(FROM_BASE64('Y W J j')), HEX(FROM_BASE64('Y\\tW\\rJ\\nj')), "\
"HEX(FROM_BASE64('')), "\
"FROM_BASE64('bad!') IS NULL, FROM_BASE64('Y') IS NULL, FROM_BASE64('Y===') IS NULL, "\
"@@warning_count, TO_BASE64(FROM_BASE64('dGVzdA==')), HEX(FROM_BASE64(TO_BASE64('test')));" \
    "$DATABASE"

exact_76_expected=$(cat <<EXPECTED
76	YWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFh
EXPECTED
)
expect_output \
    "exact line width base64 wrapping" \
    "$exact_76_expected" \
    "SELECT LENGTH(TO_BASE64(REPEAT('a', 57))), TO_BASE64(REPEAT('a', 57));" \
    "$DATABASE"

long_expected=$(cat <<EXPECTED
81	YWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFh
YQ==
EXPECTED
)
expect_output \
    "long base64 wrapping" \
    "$long_expected" \
    "SELECT LENGTH(TO_BASE64(REPEAT('a', 58))), TO_BASE64(REPEAT('a', 58));" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE t ("\
"id INT, v VARCHAR(20), vb VARBINARY(20), bi BIGINT, body TEXT, blobv BLOB"\
"); "\
"INSERT INTO t VALUES "\
"(1, 'abc', X'610062', 123, 'YWJj', X'59574A6A'), "\
"(2, NULL, NULL, NULL, NULL, NULL), "\
"(3, '', X'', 0, 'bad!', X'62616421');" \
    "$DATABASE" >/dev/null

table_expected=$(cat <<EXPECTED
1	YWJj	YQBi	MTIz	616263	616263
2	NULL	NULL	NULL	NULL	NULL
3			MA==	NULL	NULL
0
EXPECTED
)
expect_output \
    "table base64 values" \
    "$table_expected" \
    "SELECT id, TO_BASE64(v), TO_BASE64(vb), TO_BASE64(bi), HEX(FROM_BASE64(body)), "\
"HEX(FROM_BASE64(blobv)) FROM t ORDER BY id; SELECT @@warning_count;" \
    "$DATABASE"

expect_error \
    "to_base64 rejects zero arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'TO_BASE64'" \
    "SELECT TO_BASE64();" \
    "$DATABASE"

expect_error \
    "from_base64 rejects too many arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'FROM_BASE64'" \
    "SELECT FROM_BASE64('a', 'b');" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_base64_functions_expectations: ok"
