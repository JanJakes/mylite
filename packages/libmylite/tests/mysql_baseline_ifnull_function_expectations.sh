#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_ifnull_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_ifnull_function_expectations: $1" >&2
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; CREATE TABLE t(id INT); INSERT INTO t VALUES (1), (NULL), (0);" >/dev/null
run_mysql "CREATE TABLE ifnull (ifnull INT); INSERT INTO ifnull VALUES (7);" "$DATABASE" >/dev/null

expect_output \
    "IFNULL nonreserved identifier" \
    "7" \
    "SELECT ifnull FROM ifnull;" \
    "$DATABASE"

expect_output_with_headers \
    "non-null first values and labels" \
    "IFNULL(1,0)	IFNULL(0,2)	IFNULL(FALSE,2)	IFNULL(TRUE,2)	IFNULL(+0,2)	IFNULL(-1,2)
1	0	0	1	0	-1" \
    "SELECT IFNULL(1,0), IFNULL(0,2), IFNULL(FALSE,2), IFNULL(TRUE,2), IFNULL(+0,2), IFNULL(-1,2);" \
    "$DATABASE"

expect_output_with_headers \
    "null fallback and boolean branch values" \
    "IFNULL(NULL,10)	IFNULL(NULL,NULL)	IFNULL(NULL,TRUE)	IFNULL(NULL,FALSE)
10	NULL	1	0" \
    "SELECT IFNULL(NULL,10), IFNULL(NULL,NULL), IFNULL(NULL,TRUE), IFNULL(NULL,FALSE);" \
    "$DATABASE"

expect_output \
    "integer normalization" \
    "1	-2	-1	9223372036854775807" \
    "SELECT IFNULL(0001,0002), IFNULL(NULL,-0002), IFNULL(-0001,0002), IFNULL(9223372036854775807,0);" \
    "$DATABASE"

expect_output_with_headers \
    "aliases spacing parenthesized and nested" \
    "chosen	fallback	IFNULL (NULL,10)	(IFNULL(NULL,10))	IFNULL(IFNULL(NULL,1),2)	IFNULL(IF(0,NULL,4),5)
1	3	10	10	1	4" \
    "SELECT IFNULL(1,2) AS chosen, IFNULL(NULL,3) fallback, IFNULL (NULL,10), (IFNULL(NULL,10)), IFNULL(IFNULL(NULL,1),2), IFNULL(IF(0,NULL,4),5);" \
    "$DATABASE"

expect_output \
    "dual row count and warning count" \
    "10
0	-1" \
    "DO 0; SELECT IFNULL(NULL,10) FROM DUAL; SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_error \
    "empty IFNULL arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'IFNULL'" \
    "SELECT IFNULL();" \
    "$DATABASE"

expect_error \
    "one IFNULL argument" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'IFNULL'" \
    "SELECT IFNULL(1);" \
    "$DATABASE"

expect_error \
    "three IFNULL arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'IFNULL'" \
    "SELECT IFNULL(1,2,3);" \
    "$DATABASE"

expect_error \
    "nested empty IFNULL arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'IFNULL'" \
    "SELECT IFNULL(NULL, IFNULL());" \
    "$DATABASE"

expect_error \
    "empty IFNULL arguments in IF condition" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'IFNULL'" \
    "SELECT IF(IFNULL(),1,2);" \
    "$DATABASE"

accepted_but_deferred=$(run_mysql_with_headers \
    "SELECT IFNULL('x',2);
     SELECT IFNULL(1,2+3);
     SELECT IFNULL(@unset,2);
     SELECT IFNULL((SELECT NULL),2);
     SELECT IFNULL(id,2) FROM t ORDER BY id IS NULL, id;
     SELECT IFNULL(1,2) LIMIT 1;
     SELECT IFNULL(1,2) ORDER BY 1;" \
    "$DATABASE"
)
expect_value \
    "mysql accepted forms deferred by this slice" \
    "IFNULL('x',2)
x
IFNULL(1,2+3)
1
IFNULL(@unset,2)
2
IFNULL((SELECT NULL),2)
2
IFNULL(id,2)
0
1
2
IFNULL(1,2)
1
IFNULL(1,2)
1" \
    "$accepted_but_deferred"

printf '%s\n' "mysql_baseline_ifnull_function_expectations: ok"
