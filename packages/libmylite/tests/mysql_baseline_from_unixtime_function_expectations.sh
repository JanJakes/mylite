#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_from_unixtime_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_from_unixtime_function_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw "$@"
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

expect_upstream_accepts() {
    label=$1
    sql=$2
    shift 2

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status_code=$?
    set -e

    if [ "$status_code" -ne 0 ]; then
        fail "$label: expected MySQL to accept deferred behavior, got [$output]"
    fi
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE};" >/dev/null

core_expected=$(cat <<EXPECTED
1970-01-01 00:00:00	1970-01-01 00:00:01	2000-02-29 00:00:00	2015-11-13 16:08:01	NULL	1970-01-01 00:00:01	1970-01-01 00:00:00	0
-1	0
EXPECTED
)
expect_output \
    "core from_unixtime values" \
    "$core_expected" \
    "SET time_zone = '+00:00'; "\
"SELECT FROM_UNIXTIME(0), FROM_UNIXTIME(1), FROM_UNIXTIME(951782400), "\
"FROM_UNIXTIME(1447430881), "\
"FROM_UNIXTIME(NULL), FROM_UNIXTIME(TRUE), FROM_UNIXTIME(FALSE), @@warning_count; "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

labels_expected=$(cat <<EXPECTED
dt
1970-01-01 00:00:01
EXPECTED
)
expect_output_with_headers \
    "from_unixtime labels and whitespace" \
    "$labels_expected" \
    "SELECT FROM_UNIXTIME (1) AS dt FROM DUAL;" \
    "$DATABASE"

timezone_expected=$(cat <<EXPECTED
1970-01-01 02:30:00	1970-01-01 02:30:01	2015-11-13 18:38:01	0
1969-12-31 18:00:00	1969-12-31 18:00:01	2015-11-13 10:08:01	0
EXPECTED
)
expect_output \
    "from_unixtime time zone offsets" \
    "$timezone_expected" \
    "SET time_zone = '+02:30'; "\
"SELECT FROM_UNIXTIME(0), FROM_UNIXTIME(1), FROM_UNIXTIME(1447430881), @@warning_count; "\
"SET time_zone = '-06:00'; "\
"SELECT FROM_UNIXTIME(0), FROM_UNIXTIME(1), FROM_UNIXTIME(1447430881), @@warning_count;" \
    "$DATABASE"

range_expected=$(cat <<EXPECTED
NULL	1970-01-01 00:00:01	3001-01-18 23:59:59	NULL	0
EXPECTED
)
expect_output \
    "from_unixtime ranges" \
    "$range_expected" \
    "SET time_zone = '+00:00'; "\
"SELECT FROM_UNIXTIME(-1), FROM_UNIXTIME(+1), FROM_UNIXTIME(32536771199), "\
"FROM_UNIXTIME(32536771200), @@warning_count;" \
    "$DATABASE"

table_expected=$(cat <<EXPECTED
1	1970-01-01 00:00:00
2	1970-01-01 00:00:01
3	2000-02-29 00:00:00
4	2015-11-13 16:08:01
5	NULL
6	NULL
EXPECTED
)
expect_output \
    "descriptor-backed from_unixtime values" \
    "$table_expected" \
    "CREATE TABLE t(id INT, seconds BIGINT); "\
"INSERT INTO t VALUES (1,0),(2,1),(3,951782400),(4,1447430881),(5,32536771200),(6,NULL); "\
"SELECT id, FROM_UNIXTIME(seconds) FROM t ORDER BY id;" \
    "$DATABASE"

envelope_expected=$(cat <<EXPECTED
3	2000-02-29 00:00:00
2	1970-01-01 00:00:01
EXPECTED
)
expect_output \
    "row-scalar filtered ordered limited from_unixtime values" \
    "$envelope_expected" \
    "SELECT id, FROM_UNIXTIME(seconds) FROM t WHERE id BETWEEN 2 AND 3 ORDER BY id DESC LIMIT 2;" \
    "$DATABASE"

do_expected=$(cat <<EXPECTED
0	0
EXPECTED
)
expect_output \
    "from_unixtime do status" \
    "$do_expected" \
    "DO FROM_UNIXTIME(1), FROM_UNIXTIME(NULL); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_error \
    "FROM_UNIXTIME empty argument count" \
    1582 \
    "42000" \
    "Incorrect parameter count in the call to native function 'FROM_UNIXTIME'" \
    "SELECT FROM_UNIXTIME() AS x;" \
    "$DATABASE"

expect_error \
    "FROM_UNIXTIME extra argument count" \
    1582 \
    "42000" \
    "Incorrect parameter count in the call to native function 'FROM_UNIXTIME'" \
    "SELECT FROM_UNIXTIME(1, 2, 3) AS x;" \
    "$DATABASE"

expect_upstream_accepts \
    "formatted FROM_UNIXTIME deferred by MyLite" \
    "SELECT FROM_UNIXTIME(1, '%Y-%m-%d');" \
    "$DATABASE"

expect_upstream_accepts \
    "string FROM_UNIXTIME seconds deferred by MyLite" \
    "SELECT FROM_UNIXTIME('1');" \
    "$DATABASE"

expect_upstream_accepts \
    "decimal FROM_UNIXTIME seconds deferred by MyLite" \
    "SELECT FROM_UNIXTIME(1.5);" \
    "$DATABASE"
