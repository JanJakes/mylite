#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_isnull_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_isnull_function_expectations: $1" >&2
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
run_mysql "CREATE TABLE isnull (isnull INT); INSERT INTO isnull VALUES (13);" "$DATABASE" >/dev/null

expect_output \
    "ISNULL nonreserved identifier" \
    "13" \
    "SELECT isnull FROM isnull;" \
    "$DATABASE"

expect_output \
    "ISNULL absent from keywords table" \
    "0" \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEYWORDS WHERE WORD = 'ISNULL';" \
    "$DATABASE"

expect_output_with_headers \
    "null and non-null values" \
    "ISNULL(NULL)	ISNULL(1)	ISNULL(0)	ISNULL(FALSE)	ISNULL(TRUE)	ISNULL(+0)	ISNULL(-1)	ISNULL(9223372036854775807)
1	0	0	0	0	0	0	0" \
    "SELECT ISNULL(NULL), ISNULL(1), ISNULL(0), ISNULL(FALSE), ISNULL(TRUE), ISNULL(+0), ISNULL(-1), ISNULL(9223372036854775807);" \
    "$DATABASE"

expect_output_with_headers \
    "aliases spacing parenthesized and nested" \
    "n	bare_alias	ISNULL (NULL)	(ISNULL(NULL))	ISNULL(IF(0,NULL,2))	ISNULL(IFNULL(NULL,4))	ISNULL(COALESCE(NULL,NULL))	ISNULL(NULLIF(1,1))	ISNULL(ISNULL(NULL))
1	0	1	1	0	0	1	1	0" \
    "SELECT ISNULL(NULL) AS n, ISNULL(1) bare_alias, ISNULL (NULL), (ISNULL(NULL)), ISNULL(IF(0,NULL,2)), ISNULL(IFNULL(NULL,4)), ISNULL(COALESCE(NULL,NULL)), ISNULL(NULLIF(1,1)), ISNULL(ISNULL(NULL));" \
    "$DATABASE"

expect_output_with_headers \
    "nested ISNULL in existing scalar functions" \
    "IF(ISNULL(NULL),9,8)	IFNULL(ISNULL(NULL),7)	COALESCE(NULLIF(1,1),ISNULL(NULL))	NULLIF(ISNULL(NULL),1)
9	1	1	NULL" \
    "SELECT IF(ISNULL(NULL),9,8), IFNULL(ISNULL(NULL),7), COALESCE(NULLIF(1,1),ISNULL(NULL)), NULLIF(ISNULL(NULL),1);" \
    "$DATABASE"

expect_output \
    "dual row count and warning count" \
    "1
0	-1" \
    "DO 0; SELECT ISNULL(NULL) FROM DUAL; SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_error \
    "empty ISNULL arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'ISNULL'" \
    "SELECT ISNULL();" \
    "$DATABASE"

expect_error \
    "two ISNULL arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'ISNULL'" \
    "SELECT ISNULL(1,2);" \
    "$DATABASE"

expect_error \
    "nested empty ISNULL arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'ISNULL'" \
    "SELECT ISNULL(ISNULL());" \
    "$DATABASE"

expect_error \
    "empty ISNULL arguments in IF condition" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'ISNULL'" \
    "SELECT IF(ISNULL(),1,2);" \
    "$DATABASE"

expect_error \
    "malformed ISNULL arguments" \
    1064 \
    42000 \
    "near ',1)'" \
    "SELECT ISNULL(,1);" \
    "$DATABASE"

accepted_but_deferred=$(run_mysql_with_headers \
    "SELECT ISNULL('x');
     SELECT ISNULL('');
     SELECT ISNULL(1+2);
     SELECT ISNULL(@unset);
     SELECT ISNULL((SELECT NULL));
     SELECT ISNULL(1.5);
     SELECT ISNULL(0x0a);
     SELECT ISNULL(b'1');
     SELECT id, ISNULL(id) FROM t ORDER BY id IS NULL, id;
     SELECT ISNULL(NULL) WHERE TRUE;
     SELECT ISNULL(NULL) LIMIT 1;
     SELECT ISNULL(NULL) ORDER BY 1;" \
    "$DATABASE"
)
expect_value \
    "mysql accepted forms deferred by this slice" \
    "ISNULL('x')
0
ISNULL('')
0
ISNULL(1+2)
0
ISNULL(@unset)
1
ISNULL((SELECT NULL))
1
ISNULL(1.5)
0
ISNULL(0x0a)
0
ISNULL(b'1')
0
id	ISNULL(id)
0	0
1	0
7	0
NULL	1
ISNULL(NULL)
1
ISNULL(NULL)
1
ISNULL(NULL)
1" \
    "$accepted_but_deferred"

printf '%s\n' "mysql_baseline_isnull_function_expectations: ok"
