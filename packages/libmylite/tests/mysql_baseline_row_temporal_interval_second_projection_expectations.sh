#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_row_temporal_interval_second_projection_$$"

fail() {
    printf '%s\n' "mysql_baseline_row_temporal_interval_second_projection_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | mysql --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot --batch --raw \
                --skip-column-names "$@"
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
                --batch --raw --skip-column-names "$@"
    fi
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
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

setup_sql="SET SESSION sql_mode=''; "\
"DROP TABLE IF EXISTS t; "\
"CREATE TABLE t ("\
"id INT, d DATE NULL, dt DATETIME NULL, ts TIMESTAMP NULL, "\
"v VARCHAR(32) NULL, txt TEXT NULL"\
"); "\
"INSERT INTO t VALUES "\
"(1, '2008-01-02', '2008-01-02 13:29:17', '2008-01-02 13:29:17', "\
"'2008-01-02', '2008-01-02 13:29:17'), "\
"(2, NULL, NULL, NULL, NULL, NULL), "\
"(3, '2024-02-29', '2024-02-28 23:59:59', '2024-02-28 23:59:59', "\
"'bad', '2016-07-00'), "\
"(4, '9999-12-31', '9999-12-31 23:59:59', NULL, "\
"'9999-12-31 23:59:59', '1000-01-01 00:00:00');"

projection_expected=$(cat <<EXPECTED
1	2008-01-02 00:00:01	2008-01-02 13:29:16	2008-01-02 13:29:16	2008-01-02 00:00:02	2008-01-02 13:29:19	NULL
2	NULL	NULL	NULL	NULL	NULL	NULL
3	2024-02-29 00:00:01	2024-02-28 23:59:58	2024-02-28 23:59:58	NULL	NULL	NULL
4	9999-12-31 00:00:01	9999-12-31 23:59:58	NULL	NULL	1000-01-01 00:00:02	NULL
EXPECTED
)
expect_output \
    "row-backed temporal interval projection" \
    "$projection_expected" \
    "${setup_sql} "\
"SELECT id, "\
"DATE_ADD(d, INTERVAL 1 SECOND), "\
"DATE_SUB(dt, INTERVAL 1 SECOND), "\
"ADDDATE(ts, INTERVAL -1 SECOND), "\
"SUBDATE(v, INTERVAL -2 SECOND), "\
"DATE_ADD(txt, INTERVAL 2 SECOND), "\
"DATE_ADD(dt, INTERVAL NULL SECOND) "\
"FROM t ORDER BY id;" \
    "$DATABASE"

warnings_expected=$(cat <<EXPECTED
1	2008-01-02 00:00:01	2008-01-02 13:29:16	2008-01-02 13:29:16	2008-01-02 00:00:02	2008-01-02 13:29:19	NULL
2	NULL	NULL	NULL	NULL	NULL	NULL
3	2024-02-29 00:00:01	2024-02-28 23:59:58	2024-02-28 23:59:58	NULL	NULL	NULL
4	9999-12-31 00:00:01	9999-12-31 23:59:58	NULL	NULL	1000-01-01 00:00:02	NULL
Warning	1292	Incorrect datetime value: 'bad'
Warning	1292	Incorrect datetime value: '2016-07-00'
Warning	1441	Datetime function: datetime field overflow
EXPECTED
)
expect_output \
    "row-backed temporal interval warnings" \
    "$warnings_expected" \
    "${setup_sql} "\
"SELECT id, DATE_ADD(d, INTERVAL 1 SECOND), DATE_SUB(dt, INTERVAL 1 SECOND), "\
"ADDDATE(ts, INTERVAL -1 SECOND), SUBDATE(v, INTERVAL -2 SECOND), "\
"DATE_ADD(txt, INTERVAL 2 SECOND), DATE_ADD(dt, INTERVAL NULL SECOND) "\
"FROM t ORDER BY id; SHOW WARNINGS;" \
    "$DATABASE"

limited_expected=$(cat <<EXPECTED
3	NULL
1	2008-01-02 13:29:18
EXPECTED
)
expect_output \
    "row-backed temporal interval where order limit" \
    "$limited_expected" \
    "${setup_sql} "\
"SELECT id, DATE_ADD(txt, INTERVAL 1 SECOND) AS shifted "\
"FROM t WHERE id IN (1, 3) ORDER BY txt DESC LIMIT 2;" \
    "$DATABASE"

qualified_expected="1	2008-01-02 13:29:18"
expect_output \
    "row-backed temporal interval qualified column" \
    "$qualified_expected" \
    "${setup_sql} SELECT id, DATE_ADD(t.dt, INTERVAL 1 SECOND) FROM t WHERE id = 1;" \
    "$DATABASE"

overflow_expected=$(cat <<EXPECTED
NULL
Warning	1441	Datetime function: datetime field overflow
EXPECTED
)
expect_output \
    "row-backed temporal interval overflow warning" \
    "$overflow_expected" \
    "SET SESSION sql_mode=''; DROP TABLE IF EXISTS overflows; "\
"CREATE TABLE overflows (dt DATETIME); "\
"INSERT INTO overflows VALUES ('9999-12-31 23:59:59'); "\
"SELECT DATE_ADD(dt, INTERVAL 1 SECOND) FROM overflows; SHOW WARNINGS;" \
    "$DATABASE"

expect_upstream_accepts \
    "MySQL accepts interval column expressions deferred by MyLite" \
    "${setup_sql} SELECT id, DATE_ADD(dt, INTERVAL id SECOND) FROM t ORDER BY id;" \
    "$DATABASE"

expect_upstream_accepts \
    "MySQL accepts interval arithmetic expressions deferred by MyLite" \
    "${setup_sql} SELECT id, DATE_ADD(dt, INTERVAL 1+1 SECOND) FROM t ORDER BY id;" \
    "$DATABASE"

expect_upstream_accepts \
    "MySQL accepts string interval values deferred by MyLite" \
    "${setup_sql} SELECT id, DATE_ADD(v, INTERVAL '1' SECOND) FROM t ORDER BY id;" \
    "$DATABASE"

cleanup
