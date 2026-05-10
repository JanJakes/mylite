#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_coalesce_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_coalesce_function_expectations: $1" >&2
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
run_mysql "CREATE TABLE coalesce (coalesce INT); INSERT INTO coalesce VALUES (7);" "$DATABASE" >/dev/null

expect_output \
    "COALESCE nonreserved identifier" \
    "7" \
    "SELECT coalesce FROM coalesce;" \
    "$DATABASE"

expect_output_with_headers \
    "non-null first values and labels" \
    "COALESCE(1)	COALESCE(1,2)	COALESCE(0,2)	COALESCE(FALSE,2)	COALESCE(TRUE,2)	COALESCE(+0,2)	COALESCE(-1,2)
1	1	0	0	1	0	-1" \
    "SELECT COALESCE(1), COALESCE(1,2), COALESCE(0,2), COALESCE(FALSE,2), COALESCE(TRUE,2), COALESCE(+0,2), COALESCE(-1,2);" \
    "$DATABASE"

expect_output_with_headers \
    "null fallback and all-null values" \
    "COALESCE(NULL)	COALESCE(NULL,10)	COALESCE(NULL,NULL)	COALESCE(NULL,NULL,NULL)	COALESCE(NULL,TRUE)	COALESCE(NULL,FALSE)
NULL	10	NULL	NULL	1	0" \
    "SELECT COALESCE(NULL), COALESCE(NULL,10), COALESCE(NULL,NULL), COALESCE(NULL,NULL,NULL), COALESCE(NULL,TRUE), COALESCE(NULL,FALSE);" \
    "$DATABASE"

expect_output \
    "integer normalization" \
    "1	-2	-1	9223372036854775807" \
    "SELECT COALESCE(0001,0002), COALESCE(NULL,-0002), COALESCE(-0001,0002), COALESCE(9223372036854775807,0);" \
    "$DATABASE"

expect_output_with_headers \
    "aliases spacing parenthesized and nested" \
    "chosen	fallback	COALESCE (NULL,10)	(COALESCE(NULL,10))	COALESCE(NULL, COALESCE(NULL,1), 2)	COALESCE(IF(0,NULL,4),5)	COALESCE(NULL, IFNULL(COALESCE(NULL,NULL),9))	COALESCE(NULL, IF(COALESCE(NULL,0),1,2))
1	3	10	10	1	4	9	2" \
    "SELECT COALESCE(1,2) AS chosen, COALESCE(NULL,3) fallback, COALESCE (NULL,10), (COALESCE(NULL,10)), COALESCE(NULL, COALESCE(NULL,1), 2), COALESCE(IF(0,NULL,4),5), COALESCE(NULL, IFNULL(COALESCE(NULL,NULL),9)), COALESCE(NULL, IF(COALESCE(NULL,0),1,2));" \
    "$DATABASE"

expect_output_with_headers \
    "nested COALESCE in IFNULL and IF" \
    "IFNULL(COALESCE(NULL,NULL),9)	IF(COALESCE(NULL,0),1,2)
9	2" \
    "SELECT IFNULL(COALESCE(NULL,NULL),9), IF(COALESCE(NULL,0),1,2);" \
    "$DATABASE"

expect_output \
    "dual row count and warning count" \
    "10
0	-1" \
    "DO 0; SELECT COALESCE(NULL,10) FROM DUAL; SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_error \
    "empty COALESCE arguments" \
    1064 \
    42000 \
    "near ')'" \
    "SELECT COALESCE();" \
    "$DATABASE"

accepted_but_deferred=$(run_mysql_with_headers \
    "SELECT COALESCE('x',2);
     SELECT COALESCE(1,2+3);
     SELECT COALESCE(@unset,2);
     SELECT COALESCE((SELECT NULL),2);
     SELECT COALESCE(id,2) FROM t ORDER BY id IS NULL, id;
     SELECT COALESCE(1,2) LIMIT 1;
     SELECT COALESCE(1,2) ORDER BY 1;" \
    "$DATABASE"
)
expect_value \
    "mysql accepted forms deferred by this slice" \
    "COALESCE('x',2)
x
COALESCE(1,2+3)
1
COALESCE(@unset,2)
2
COALESCE((SELECT NULL),2)
2
COALESCE(id,2)
0
1
2
COALESCE(1,2)
1
COALESCE(1,2)
1" \
    "$accepted_but_deferred"

printf '%s\n' "mysql_baseline_coalesce_function_expectations: ok"
