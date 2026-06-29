#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_statistical_aggregate_window_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_statistical_aggregate_window_functions_expectations: $1" >&2
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
run_mysql \
    "CREATE DATABASE ${DATABASE} CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci; "\
"USE ${DATABASE}; SET NAMES utf8mb4; SET SESSION sql_mode = 'NO_ENGINE_SUBSTITUTION'; "\
"CREATE TABLE stats(id INT, group_id INT, n INT, label VARCHAR(10)); "\
"INSERT INTO stats VALUES "\
"(1,10,10,'a'),"\
"(2,10,20,'b'),"\
"(3,10,NULL,'c'),"\
"(4,20,30,'d'),"\
"(5,20,40,'e'),"\
"(6,NULL,NULL,'f');" \
    >/dev/null

expect_output \
    "source-free statistical aggregate windows" \
    "0	NULL	0	NULL" \
    "SELECT STDDEV_POP(1) OVER () AS sp, STDDEV_SAMP(1) OVER () AS ss, "\
"VAR_POP(1) OVER () AS vp, VAR_SAMP(1) OVER () AS vs;" \
    "$DATABASE"

expect_output \
    "partitioned statistical aggregate windows" \
    "6	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
1	10	5	5	5	7.0710678118654755	25	50	25
2	10	5	5	5	7.0710678118654755	25	50	25
3	10	5	5	5	7.0710678118654755	25	50	25
4	20	5	5	5	7.0710678118654755	25	50	25
5	20	5	5	5	7.0710678118654755	25	50	25" \
    "SELECT id, group_id, "\
"STD(n) OVER (PARTITION BY group_id) AS std_alias, "\
"STDDEV(n) OVER (PARTITION BY group_id) AS stddev_alias, "\
"STDDEV_POP(n) OVER (PARTITION BY group_id) AS sp, "\
"STDDEV_SAMP(n) OVER (PARTITION BY group_id) AS ss, "\
"VAR_POP(n) OVER (PARTITION BY group_id) AS vp, "\
"VAR_SAMP(n) OVER (PARTITION BY group_id) AS vs, "\
"VARIANCE(n) OVER (PARTITION BY group_id) AS variance_alias "\
"FROM stats ORDER BY group_id, id;" \
    "$DATABASE"

expect_output \
    "ordered moving statistical aggregate windows" \
    "1	0	NULL	0	NULL
2	5	7.0710678118654755	25	50
3	0	NULL	0	NULL
4	0	NULL	0	NULL
5	5	7.0710678118654755	25	50
6	0	NULL	0	NULL" \
    "SELECT id, "\
"STDDEV_POP(n) OVER (ORDER BY id ROWS BETWEEN 1 PRECEDING AND CURRENT ROW) AS sp, "\
"STDDEV_SAMP(n) OVER (ORDER BY id ROWS BETWEEN 1 PRECEDING AND CURRENT ROW) AS ss, "\
"VAR_POP(n) OVER (ORDER BY id ROWS BETWEEN 1 PRECEDING AND CURRENT ROW) AS vp, "\
"VAR_SAMP(n) OVER (ORDER BY id ROWS BETWEEN 1 PRECEDING AND CURRENT ROW) AS vs "\
"FROM stats ORDER BY id;" \
    "$DATABASE"

expect_output \
    "empty frame statistical aggregate windows" \
    "1	NULL	NULL	NULL	NULL
2	0	NULL	0	NULL
3	0	NULL	0	NULL
4	NULL	NULL	NULL	NULL
5	0	NULL	0	NULL
6	0	NULL	0	NULL" \
    "SELECT id, "\
"STDDEV_POP(n) OVER (ORDER BY id ROWS BETWEEN 1 PRECEDING AND 1 PRECEDING) AS sp, "\
"STDDEV_SAMP(n) OVER (ORDER BY id ROWS BETWEEN 1 PRECEDING AND 1 PRECEDING) AS ss, "\
"VAR_POP(n) OVER (ORDER BY id ROWS BETWEEN 1 PRECEDING AND 1 PRECEDING) AS vp, "\
"VAR_SAMP(n) OVER (ORDER BY id ROWS BETWEEN 1 PRECEDING AND 1 PRECEDING) AS vs "\
"FROM stats ORDER BY id;" \
    "$DATABASE"

expect_output \
    "named statistical aggregate window" \
    "1	0
2	25
3	25
4	66.66666666666667
5	125
6	125" \
    "SELECT id, VAR_POP(n) OVER w AS vp FROM stats "\
"WINDOW w AS (ORDER BY id ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) "\
"ORDER BY id;" \
    "$DATABASE"

run_mysql \
    "CREATE VIEW v AS SELECT STDDEV_POP(n) OVER () AS sp, "\
"STDDEV_SAMP(n) OVER () AS ss, VAR_POP(n) OVER () AS vp, "\
"VAR_SAMP(n) OVER () AS vs FROM stats;" \
    "$DATABASE" >/dev/null
expect_output \
    "statistical aggregate window metadata" \
    "sp	double	YES	<NULL>
ss	double	YES	<NULL>
vp	double	YES	<NULL>
vs	double	YES	<NULL>" \
    "SELECT COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE, COALESCE(COLUMN_DEFAULT, '<NULL>') "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'v' ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

expect_error \
    "distinct statistical aggregate window syntax" \
    1064 \
    42000 \
    "near 'DISTINCT n) OVER () FROM stats'" \
    "SELECT STDDEV_POP(DISTINCT n) OVER () FROM stats;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_statistical_aggregate_window_functions_expectations: ok"
