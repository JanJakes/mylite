#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_if_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_if_function_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --binary-as-hex=1 --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --binary-as-hex=1 "$@"
}

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

expect_output() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    expect_value "$label" "$expected" "$output"
}

expect_output_with_headers() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql_with_headers "$sql" "$@")
    expect_value "$label" "$expected" "$output"
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

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; CREATE TABLE t(id INT); INSERT INTO t VALUES (1), (0);" >/dev/null

expect_output_with_headers \
    "truthiness values and labels" \
    "IF(1,2,3)	IF(0,2,3)	IF(NULL,2,3)	IF(-1,2,3)	IF(+0,2,3)	IF(TRUE,2,3)	IF(FALSE,2,3)
2	3	3	2	3	2	3" \
    "SELECT IF(1,2,3), IF(0,2,3), IF(NULL,2,3), IF(-1,2,3), IF(+0,2,3), IF(TRUE,2,3), IF(FALSE,2,3);" \
    "$DATABASE"

expect_output_with_headers \
    "null and boolean branch values" \
    "IF(1,NULL,3)	IF(0,2,NULL)	IF(NULL,NULL,3)	IF(1,TRUE,FALSE)	IF(0,TRUE,FALSE)
NULL	NULL	3	1	0" \
    "SELECT IF(1,NULL,3), IF(0,2,NULL), IF(NULL,NULL,3), IF(1,TRUE,FALSE), IF(0,TRUE,FALSE);" \
    "$DATABASE"

expect_output \
    "integer normalization" \
    "2	3	-2" \
    "SELECT IF(0001,0002,0003), IF(0000,0002,0003), IF(-0001,-0002,-0003);" \
    "$DATABASE"

expect_output_with_headers \
    "aliases spacing parenthesized and nested" \
    "chosen	fallback	IF (1,2,3)	(IF(1,2,3))	IF(IF(1,1,0),2,3)
2	3	2	2	2" \
    "SELECT IF(1,2,3) AS chosen, IF(0,NULL,3) fallback, IF (1,2,3), (IF(1,2,3)), IF(IF(1,1,0),2,3);" \
    "$DATABASE"

expect_output \
    "dual row count and warning count" \
    "2
0	-1" \
    "DO 0; SELECT IF(1,2,3) FROM DUAL; SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_error \
    "empty IF arguments" \
    1064 \
    42000 \
    "near ')' at line 1" \
    "SELECT IF();" \
    "$DATABASE"

expect_error \
    "one IF argument" \
    1064 \
    42000 \
    "near ')' at line 1" \
    "SELECT IF(1);" \
    "$DATABASE"

expect_error \
    "two IF arguments" \
    1064 \
    42000 \
    "near ')' at line 1" \
    "SELECT IF(1,2);" \
    "$DATABASE"

expect_error \
    "four IF arguments" \
    1064 \
    42000 \
    "near ',4)' at line 1" \
    "SELECT IF(1,2,3,4);" \
    "$DATABASE"

accepted_but_deferred=$(run_mysql_with_headers \
    "SELECT IF('x',2,3);
     SELECT IF(1,2+3,4);
     SELECT IF(id,2,3) FROM t ORDER BY id DESC;
     SELECT IF(1,2,3) LIMIT 1;
     SELECT IF(1,2,3) ORDER BY 1;" \
    "$DATABASE"
)
expect_value \
    "mysql accepted forms deferred by this slice" \
    "IF('x',2,3)
3
IF(1,2+3,4)
5
IF(id,2,3)
2
3
IF(1,2,3)
2
IF(1,2,3)
2" \
    "$accepted_but_deferred"

printf '%s\n' "mysql_baseline_if_function_expectations: ok"
