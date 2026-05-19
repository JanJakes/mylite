#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_time_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_time_function_expectations: $1" >&2
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
01:02:03	01:02:03	-13:29:17	-13:29:17	-272:59:59	272:59:59	01:02:03	NULL	0
-1	0
EXPECTED
)
expect_output \
    "core TIME values" \
    "$core_expected" \
    "DO 0; SELECT TIME('2003-12-31 01:02:03'), TIME('01:02:03'), "\
"TIME('-13:29:17'), TIME(\"-13:29:17\"), TIME('-272:59:59'), "\
"TIME('272:59:59'), TIME('0000-00-00 01:02:03'), TIME(NULL), "\
"@@warning_count; SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

labels_expected=$(cat <<EXPECTED
TIME ('2003-12-31 01:02:03')	direct	null_time
01:02:03	01:02:03	NULL
EXPECTED
)
expect_output_with_headers \
    "TIME labels and whitespace" \
    "$labels_expected" \
    "SELECT TIME ('2003-12-31 01:02:03'), TIME('01:02:03') AS direct, "\
"TIME(NULL) AS null_time FROM DUAL;" \
    "$DATABASE"

table_expected=$(cat <<EXPECTED
1	00:00:00	13:29:17	13:29:17	13:29:17
2	NULL	NULL	NULL	NULL
3	00:00:00	01:02:03	NULL	-13:29:17
0
EXPECTED
)
expect_output \
    "descriptor-backed TIME values" \
    "$table_expected" \
    "SET SESSION sql_mode = ''; "\
"CREATE TABLE t(id INT, d DATE NULL, dt DATETIME NULL, ts TIMESTAMP NULL, tm TIME NULL); "\
"INSERT INTO t VALUES "\
"(1,'2008-01-02','2008-01-02 13:29:17','2008-01-02 13:29:17','13:29:17'),"\
"(2,NULL,NULL,NULL,NULL),"\
"(3,'0000-00-00','0000-00-00 01:02:03',NULL,'-13:29:17'); "\
"SELECT id, TIME(d), TIME(dt), TIME(ts), TIME(tm) FROM t ORDER BY id; "\
"SELECT @@warning_count;" \
    "$DATABASE"

envelope_expected=$(cat <<EXPECTED
3	-13:29:17
1	13:29:17
EXPECTED
)
expect_output \
    "row-scalar filtered ordered limited TIME values" \
    "$envelope_expected" \
    "SELECT id, TIME(tm) FROM t WHERE id <> 2 ORDER BY id DESC LIMIT 2;" \
    "$DATABASE"

do_expected=$(cat <<EXPECTED
0	0
EXPECTED
)
expect_output \
    "TIME do status" \
    "$do_expected" \
    "DO TIME('2003-12-31 01:02:03'), TIME(NULL); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

invalid_expected=$(cat <<EXPECTED
NULL	1
Warning	1292	Truncated incorrect time value: 'not-a-date'
EXPECTED
)
expect_output \
    "invalid TIME warning" \
    "$invalid_expected" \
    "SELECT TIME('not-a-date'), @@warning_count; SHOW WARNINGS;" \
    "$DATABASE"

expect_error \
    "TIME empty argument syntax" \
    1064 \
    "42000" \
    "syntax" \
    "SELECT TIME() AS x;" \
    "$DATABASE"

expect_error \
    "TIME extra argument syntax" \
    1064 \
    "42000" \
    "syntax" \
    "SELECT TIME('01:02:03', '04:05:06') AS x;" \
    "$DATABASE"

expect_upstream_accepts \
    "numeric TIME input deferred by MyLite" \
    "SELECT TIME(20080102132917);" \
    "$DATABASE"

expect_upstream_accepts \
    "fractional TIME input deferred by MyLite" \
    "SELECT TIME('2008-01-02 13:29:17.999999');" \
    "$DATABASE"

expect_upstream_accepts \
    "date-only TIME string coercion deferred by MyLite" \
    "SELECT TIME('2008-01-02');" \
    "$DATABASE"

expect_upstream_accepts \
    "string descriptor TIME input deferred by MyLite" \
    "CREATE TABLE string_time_source(s VARCHAR(32)); "\
"INSERT INTO string_time_source VALUES ('2008-01-02 13:29:17'); "\
"SELECT TIME(s) FROM string_time_source;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_time_function_expectations: ok"
