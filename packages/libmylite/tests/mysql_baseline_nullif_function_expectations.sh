#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_nullif_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_nullif_function_expectations: $1" >&2
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; CREATE TABLE t(id INT); INSERT INTO t VALUES (1), (NULL), (0), (7);" >/dev/null
run_mysql "CREATE TABLE nullif (nullif INT); INSERT INTO nullif VALUES (11);" "$DATABASE" >/dev/null

expect_output \
    "NULLIF nonreserved identifier" \
    "11" \
    "SELECT nullif FROM nullif;" \
    "$DATABASE"

expect_output \
    "NULLIF absent from keywords table" \
    "0" \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEYWORDS WHERE WORD = 'NULLIF';" \
    "$DATABASE"

expect_output_with_headers \
    "integer boolean and signed values" \
    "NULLIF(1,1)	NULLIF(1,2)	NULLIF(0,0)	NULLIF(0,1)	NULLIF(FALSE,0)	NULLIF(TRUE,1)	NULLIF(TRUE,FALSE)	NULLIF(+0,-0)	NULLIF(-1,-1)	NULLIF(-1,1)
NULL	1	NULL	0	NULL	NULL	1	NULL	NULL	-1" \
    "SELECT NULLIF(1,1), NULLIF(1,2), NULLIF(0,0), NULLIF(0,1), NULLIF(FALSE,0), NULLIF(TRUE,1), NULLIF(TRUE,FALSE), NULLIF(+0,-0), NULLIF(-1,-1), NULLIF(-1,1);" \
    "$DATABASE"

expect_output_with_headers \
    "null operand behavior" \
    "NULLIF(NULL,NULL)	NULLIF(NULL,1)	NULLIF(NULL,TRUE)	NULLIF(1,NULL)	NULLIF(TRUE,NULL)	NULLIF(FALSE,NULL)
NULL	NULL	NULL	1	1	0" \
    "SELECT NULLIF(NULL,NULL), NULLIF(NULL,1), NULLIF(NULL,TRUE), NULLIF(1,NULL), NULLIF(TRUE,NULL), NULLIF(FALSE,NULL);" \
    "$DATABASE"

expect_output \
    "integer normalization" \
    "NULL	1	NULL	9223372036854775807" \
    "SELECT NULLIF(0001,1), NULLIF(0001,2), NULLIF(9223372036854775807,9223372036854775807), NULLIF(9223372036854775807,0);" \
    "$DATABASE"

expect_output_with_headers \
    "aliases spacing parenthesized and nested" \
    "n	bare_alias	NULLIF (1,1)	(NULLIF(1,2))	NULLIF(TRUE,FALSE)	NULLIF(IF(1,2,3),2)	NULLIF(IFNULL(NULL,4),4)	NULLIF(COALESCE(NULL,6),6)	NULLIF(NULLIF(1,1),1)
NULL	1	NULL	1	1	NULL	NULL	NULL	NULL" \
    "SELECT NULLIF(1,1) AS n, NULLIF(1,2) bare_alias, NULLIF (1,1), (NULLIF(1,2)), NULLIF(TRUE,FALSE), NULLIF(IF(1,2,3),2), NULLIF(IFNULL(NULL,4),4), NULLIF(COALESCE(NULL,6),6), NULLIF(NULLIF(1,1),1);" \
    "$DATABASE"

expect_output_with_headers \
    "nested NULLIF in existing scalar functions" \
    "IF(NULLIF(1,1),9,8)	IFNULL(NULLIF(1,1),7)	COALESCE(NULLIF(1,1),6)	NULLIF(IFNULL(NULLIF(1,1),5),5)
8	7	6	NULL" \
    "SELECT IF(NULLIF(1,1),9,8), IFNULL(NULLIF(1,1),7), COALESCE(NULLIF(1,1),6), NULLIF(IFNULL(NULLIF(1,1),5),5);" \
    "$DATABASE"

expect_output \
    "dual row count and warning count" \
    "NULL
0	-1" \
    "DO 0; SELECT NULLIF(NULL,10) FROM DUAL; SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_error \
    "empty NULLIF arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'NULLIF'" \
    "SELECT NULLIF();" \
    "$DATABASE"

expect_error \
    "one NULLIF argument" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'NULLIF'" \
    "SELECT NULLIF(1);" \
    "$DATABASE"

expect_error \
    "three NULLIF arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'NULLIF'" \
    "SELECT NULLIF(1,2,3);" \
    "$DATABASE"

expect_error \
    "nested empty NULLIF arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'NULLIF'" \
    "SELECT NULLIF(NULL, NULLIF());" \
    "$DATABASE"

expect_error \
    "empty NULLIF arguments in IF condition" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'NULLIF'" \
    "SELECT IF(NULLIF(),1,2);" \
    "$DATABASE"

expect_error \
    "malformed NULLIF arguments" \
    1064 \
    42000 \
    "near ',2)'" \
    "SELECT NULLIF(1,,2);" \
    "$DATABASE"

accepted_but_deferred=$(run_mysql_with_headers \
    "SELECT NULLIF('x','x');
     SELECT NULLIF('x','y');
     SELECT NULLIF(1,2+3);
     SELECT NULLIF(@unset,2);
     SELECT NULLIF((SELECT 1),1);
     SELECT NULLIF((SELECT NULL),2);
     SELECT NULLIF(1.5,1.5);
     SELECT NULLIF(0x0a,10);
     SELECT NULLIF(b'1',1);
     SELECT NULLIF(id,7) FROM t ORDER BY id IS NULL, id;
     SELECT NULLIF(1,2) LIMIT 1;
     SELECT NULLIF(1,2) ORDER BY 1;" \
    "$DATABASE"
)
expect_value \
    "mysql accepted forms deferred by this slice" \
    "NULLIF('x','x')
NULL
NULLIF('x','y')
x
NULLIF(1,2+3)
1
NULLIF(@unset,2)
NULL
NULLIF((SELECT 1),1)
NULL
NULLIF((SELECT NULL),2)
NULL
NULLIF(1.5,1.5)
NULL
NULLIF(0x0a,10)
NULL
NULLIF(b'1',1)
NULL
NULLIF(id,7)
0
1
NULL
NULL
NULLIF(1,2)
1
NULLIF(1,2)
1" \
    "$accepted_but_deferred"

printf '%s\n' "mysql_baseline_nullif_function_expectations: ok"
