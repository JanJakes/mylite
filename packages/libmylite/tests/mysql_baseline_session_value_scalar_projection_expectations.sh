#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_session_value_scalar_projection_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_session_value_scalar_projection_expectations: $1" >&2
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

user_value=$(run_mysql 'SELECT USER();')
current_user_value=$(run_mysql 'SELECT CURRENT_USER();')

cleanup
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; CREATE TABLE t(id INT); INSERT INTO t VALUES (1), (NULL), (0);" >/dev/null

expect_output_with_headers \
    "mixed session and scalar value projection" \
    "VERSION()	1	IF(1,2,3)	ISNULL(NULL)	@@warning_count
${version}	1	2	1	0" \
    "DO 0; SELECT VERSION(), 1, IF(1,2,3), ISNULL(NULL), @@warning_count;" \
    "$DATABASE"

expect_output \
    "row count after mixed session and scalar value projection" \
    "${version}	1	2	1	0
0	-1" \
    "DO 0; SELECT VERSION(), 1, IF(1,2,3), ISNULL(NULL), @@warning_count; SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output_with_headers \
    "mixed dual aliases all and selected schema" \
    "selected	if_result	row_state	warnings
${DATABASE}	4	0	0" \
    "DO 0; SELECT ALL DATABASE() AS selected, IFNULL(NULL,4) if_result, ROW_COUNT() row_state, @@warning_count warnings FROM DUAL;" \
    "$DATABASE"

expect_output_with_headers \
    "mixed identity and nullable values" \
    "DATABASE()	SCHEMA()	USER()	CURRENT_USER	CURRENT_ROLE()	LAST_INSERT_ID()	1	NULL	IF(0,7,8)	@@warning_count
${DATABASE}	${DATABASE}	${user_value}	${current_user_value}	NONE	0	1	NULL	8	0" \
    "DO 0; SELECT DATABASE(), SCHEMA(), USER(), CURRENT_USER, CURRENT_ROLE(), LAST_INSERT_ID(), 1, NULL, IF(0,7,8), @@warning_count;" \
    "$DATABASE"

expect_output_with_headers \
    "parenthesized mixed labels" \
    "(VERSION())	1	(IF(1,2,3))	(@@warning_count)	(DATABASE())
${version}	1	2	0	${DATABASE}" \
    "DO 0; SELECT (VERSION()), (1), (IF(1,2,3)), (@@warning_count), (DATABASE());" \
    "$DATABASE"

connection_output=$(run_mysql "SELECT CONNECTION_ID(); SELECT CONNECTION_ID(), 1;" "$DATABASE")
connection_id=$(printf '%s\n' "$connection_output" | sed -n '1p')
connection_mixed=$(printf '%s\n' "$connection_output" | sed -n '2p')
case "$connection_id" in
    '' | *[!0123456789]*)
        fail "connection id should be a nonnegative integer, got [$connection_id]"
        ;;
esac
expect_value "connection id stays stable within mixed projection connection" "${connection_id}	1" "$connection_mixed"

expect_output_with_headers \
    "source-backed version predicate and order" \
    "VERSION()	id
${version}	0
${version}	1" \
    "DO 0;
     SELECT VERSION(), id FROM t
      WHERE id IS NOT NULL AND VERSION() = VERSION()
      ORDER BY VERSION(), id;" \
    "$DATABASE"

expect_output \
    "deprecated system variable warning sequencing" \
    "0	1	2	1	0	0
1	-1" \
    "DO 0; SELECT @@sql_slave_skip_counter, 1, IF(1,2,3), @@warning_count, @@error_count, ROW_COUNT(); SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "two deprecated system variable reads" \
    "0	0	1	2" \
    "DO 0; SELECT @@sql_slave_skip_counter, @@global.sql_slave_skip_counter, 1, @@warning_count;" \
    "$DATABASE"

accepted_but_deferred=$(run_mysql_with_headers \
    "DO 0;
     SELECT VERSION(), 1+2;
     SELECT VERSION(), 1 FROM t ORDER BY id IS NULL, id;
     SELECT DATABASE(), IF(1,2,3) WHERE TRUE;
     SELECT VERSION(), 1 LIMIT 1;
     SELECT VERSION(), 1 ORDER BY 1;
     SELECT IF(@@warning_count,1,0), 2;" \
    "$DATABASE"
)
expect_value \
    "mysql accepted forms deferred by this slice" \
    "VERSION()	1+2
${version}	3
VERSION()	1
${version}	1
${version}	1
${version}	1
DATABASE()	IF(1,2,3)
${DATABASE}	2
VERSION()	1
${version}	1
VERSION()	1
${version}	1
IF(@@warning_count,1,0)	2
0	2" \
    "$accepted_but_deferred"

expect_error \
    "wrong version arity in mixed projection" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'VERSION'" \
    "SELECT VERSION(1), 1;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_session_value_scalar_projection_expectations: ok"
