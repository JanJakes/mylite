#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"

fail() {
    printf '%s\n' "mysql_convert_tz_utc_boundaries_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --batch --raw --skip-column-names "$@"
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

    output=$(run_mysql "$sql")
    if [ "$output" != "$expected" ]; then
        fail "$label: expected [$expected], got [$output]"
    fi
}

runtime=$(run_mysql "SELECT VERSION(), @@version_compile_machine;")
if [ "$runtime" != "8.4.9	x86_64" ]; then
    fail "expected MySQL 8.4.9 x86-64 runtime, got [$runtime]"
fi

lower_expected=$(cat <<'EXPECTED'
1970-01-01 00:00:00	1970-01-01 00:00:00.999999	1970-01-01 01:00:01	1970-01-01 01:00:01.000001	1969-12-31 10:01:00.999999	1970-01-01 14:00:01.000000	1970-01-01 14:00:00.999999	1969-12-31 10:01:01.000000
0
EXPECTED
)
expect_output \
    "lower UTC boundary and extreme source offsets" \
    "$lower_expected" \
    "SELECT
       CONVERT_TZ('1970-01-01 00:00:00','+00:00','+01:00'),
       CONVERT_TZ('1970-01-01 00:00:00.999999','+00:00','+01:00'),
       CONVERT_TZ('1970-01-01 00:00:01','+00:00','+01:00'),
       CONVERT_TZ('1970-01-01 00:00:01.000001','+00:00','+01:00'),
       CONVERT_TZ('1969-12-31 10:01:00.999999','-13:59','+14:00'),
       CONVERT_TZ('1969-12-31 10:01:01.000000','-13:59','+14:00'),
       CONVERT_TZ('1970-01-01 14:00:00.999999','+14:00','-13:59'),
       CONVERT_TZ('1970-01-01 14:00:01.000000','+14:00','-13:59');
     SELECT @@warning_count;"

upper_expected=$(cat <<'EXPECTED'
3001-01-19 00:59:59.999999	3001-01-19 00:00:00.000000	3001-01-19 13:59:59.999999	3001-01-18 10:01:00.000000	3001-01-18 10:00:59.999999	3001-01-19 14:00:00.000000
0
EXPECTED
)
expect_output \
    "upper UTC boundary and extreme source offsets" \
    "$upper_expected" \
    "SELECT
       CONVERT_TZ('3001-01-18 23:59:59.999999','+00:00','+01:00'),
       CONVERT_TZ('3001-01-19 00:00:00.000000','+00:00','+01:00'),
       CONVERT_TZ('3001-01-18 10:00:59.999999','-13:59','+14:00'),
       CONVERT_TZ('3001-01-18 10:01:00.000000','-13:59','+14:00'),
       CONVERT_TZ('3001-01-19 13:59:59.999999','+14:00','-13:59'),
       CONVERT_TZ('3001-01-19 14:00:00.000000','+14:00','-13:59');
     SELECT @@warning_count;"

calendar_expected=$(cat <<'EXPECTED'
2004-01-01 14:30:00.1	2004-01-01 14:30:00.12	2004-01-01 14:30:00.123456	2000-02-28 22:30:00.123456	2000-03-01 01:30:00.654321	2004-02-29 22:30:00.000001
0001-01-01 00:00:00	0999-12-31 23:59:59	1000-01-01 00:00:00	9999-12-31 23:59:59
0
EXPECTED
)
expect_output \
    "fractional precision leap days and distant years" \
    "$calendar_expected" \
    "SELECT
       CONVERT_TZ('2004-01-01 12:00:00.1','+00:00','+02:30'),
       CONVERT_TZ('2004-01-01 12:00:00.12','+00:00','+02:30'),
       CONVERT_TZ('2004-01-01 12:00:00.123456','+00:00','+02:30'),
       CONVERT_TZ('2000-02-29 00:30:00.123456','+01:00','-01:00'),
       CONVERT_TZ('2000-02-29 23:30:00.654321','-01:00','+01:00'),
       CONVERT_TZ('2004-03-01 00:30:00.000001','+01:00','-01:00');
     SELECT
       CONVERT_TZ('0001-01-01 00:00:00','+00:00','+01:00'),
       CONVERT_TZ('0999-12-31 23:59:59','+00:00','+01:00'),
       CONVERT_TZ('1000-01-01 00:00:00','+00:00','+01:00'),
       CONVERT_TZ('9999-12-31 23:59:59','+00:00','+01:00');
     SELECT @@warning_count;"

printf '%s\n' "mysql_convert_tz_utc_boundaries_expectations: ok"
