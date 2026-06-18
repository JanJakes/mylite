#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_temporal_update_contexts_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_temporal_row_scalar_update_contexts_expectations: $1" >&2
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
run_mysql "CREATE DATABASE ${DATABASE} DEFAULT CHARACTER SET utf8mb4 "\
"COLLATE utf8mb4_0900_ai_ci;" >/dev/null
run_mysql "SET time_zone = '+00:00';" "$DATABASE" >/dev/null

setup_sql="CREATE TABLE t("\
"id INT, d DATE, tm TIME, dt DATETIME, s VARCHAR(64), epoch INT, daynr INT, "\
"y INT, doy INT, h INT, mi INT, sec INT, out_date VARCHAR(64), "\
"out_time VARCHAR(64), out_date_format VARCHAR(64), out_time_format VARCHAR(64), "\
"out_str_to_date VARCHAR(64), out_datediff INT, out_timediff VARCHAR(64), "\
"out_timestampdiff INT, out_timestamp VARCHAR(64), out_unix_timestamp INT, "\
"out_sec_to_time VARCHAR(64), out_from_unixtime VARCHAR(64), "\
"out_from_days VARCHAR(64), out_makedate VARCHAR(64), out_maketime VARCHAR(64), "\
"out_time_to_sec INT, out_to_days INT, out_to_seconds BIGINT, "\
"out_dayofmonth INT, out_dayofweek INT, out_dayofyear INT, out_dayname VARCHAR(64), "\
"out_last_day VARCHAR(64), out_hour INT, out_minute INT, out_second INT, "\
"out_microsecond INT, out_month INT, out_monthname VARCHAR(64), out_quarter INT, "\
"out_week INT, out_weekday INT, out_weekofyear INT, out_year INT, "\
"out_yearweek INT, out_extract INT, out_timestampadd VARCHAR(64)); "\
"INSERT INTO t(id,d,tm,dt,s,epoch,daynr,y,doy,h,mi,sec) VALUES"\
"(1,'2008-02-20','01:02:03','2008-12-31 23:59:59',"\
"'2009-01-02 03:04:05.456789',3661,730669,2024,60,12,34,56);"
run_mysql "$setup_sql" "$DATABASE" >/dev/null

expected=$(cat <<EXPECTED
date	1
time	1
date_format	1
time_format	1
str_to_date	1
datediff	1
timediff	1
timestampdiff	1
timestamp	1
unix_timestamp	1
sec_to_time	1
from_unixtime	1
from_days	1
makedate	1
maketime	1
time_to_sec	1
to_days	1
to_seconds	1
dayofmonth	1
dayofweek	1
dayofyear	1
dayname	1
last_day	1
hour	1
minute	1
second	1
microsecond	1
month	1
monthname	1
quarter	1
week	1
weekday	1
weekofyear	1
year	1
yearweek	1
extract	1
timestampadd	1
1	2008-12-31	01:02:03	2008-12	01:02	2009-01-02	315	02:02:02	315	2008-02-20 01:02:03	1230767999	01:01:01	1970-01-01 01:01:01	2000-07-03	2024-02-29	12:34:56	3723	733457	63397987199	20	4	51	Wednesday	2008-02-29	1	2	3	456789	2	February	1	7	2	8	2008	200807	200802	2009-01-01 01:01:00
-1	0
EXPECTED
)
expect_output \
    "temporal row-scalar UPDATE assignments" \
    "$expected" \
    "SET time_zone = '+00:00'; "\
"UPDATE t SET out_date = DATE(dt); SELECT 'date', ROW_COUNT(); "\
"UPDATE t SET out_time = TIME(tm); SELECT 'time', ROW_COUNT(); "\
"UPDATE t SET out_date_format = DATE_FORMAT(dt, '%Y-%m'); SELECT 'date_format', ROW_COUNT(); "\
"UPDATE t SET out_time_format = TIME_FORMAT(tm, '%H:%i'); SELECT 'time_format', ROW_COUNT(); "\
"UPDATE t SET out_str_to_date = STR_TO_DATE('2009-01-02', '%Y-%m-%d'); "\
"SELECT 'str_to_date', ROW_COUNT(); "\
"UPDATE t SET out_datediff = DATEDIFF(dt, d); SELECT 'datediff', ROW_COUNT(); "\
"UPDATE t SET out_timediff = TIMEDIFF('03:04:05', tm); SELECT 'timediff', ROW_COUNT(); "\
"UPDATE t SET out_timestampdiff = TIMESTAMPDIFF(DAY, d, dt); "\
"SELECT 'timestampdiff', ROW_COUNT(); "\
"UPDATE t SET out_timestamp = TIMESTAMP(d, tm); SELECT 'timestamp', ROW_COUNT(); "\
"UPDATE t SET out_unix_timestamp = UNIX_TIMESTAMP(dt); SELECT 'unix_timestamp', ROW_COUNT(); "\
"UPDATE t SET out_sec_to_time = SEC_TO_TIME(epoch); SELECT 'sec_to_time', ROW_COUNT(); "\
"UPDATE t SET out_from_unixtime = FROM_UNIXTIME(epoch); "\
"SELECT 'from_unixtime', ROW_COUNT(); "\
"UPDATE t SET out_from_days = FROM_DAYS(daynr); SELECT 'from_days', ROW_COUNT(); "\
"UPDATE t SET out_makedate = MAKEDATE(y, doy); SELECT 'makedate', ROW_COUNT(); "\
"UPDATE t SET out_maketime = MAKETIME(h, mi, sec); SELECT 'maketime', ROW_COUNT(); "\
"UPDATE t SET out_time_to_sec = TIME_TO_SEC(tm); SELECT 'time_to_sec', ROW_COUNT(); "\
"UPDATE t SET out_to_days = TO_DAYS(d); SELECT 'to_days', ROW_COUNT(); "\
"UPDATE t SET out_to_seconds = TO_SECONDS(dt); SELECT 'to_seconds', ROW_COUNT(); "\
"UPDATE t SET out_dayofmonth = DAYOFMONTH(d); SELECT 'dayofmonth', ROW_COUNT(); "\
"UPDATE t SET out_dayofweek = DAYOFWEEK(d); SELECT 'dayofweek', ROW_COUNT(); "\
"UPDATE t SET out_dayofyear = DAYOFYEAR(d); SELECT 'dayofyear', ROW_COUNT(); "\
"UPDATE t SET out_dayname = DAYNAME(d); SELECT 'dayname', ROW_COUNT(); "\
"UPDATE t SET out_last_day = LAST_DAY(d); SELECT 'last_day', ROW_COUNT(); "\
"UPDATE t SET out_hour = HOUR(tm); SELECT 'hour', ROW_COUNT(); "\
"UPDATE t SET out_minute = MINUTE(tm); SELECT 'minute', ROW_COUNT(); "\
"UPDATE t SET out_second = SECOND(tm); SELECT 'second', ROW_COUNT(); "\
"UPDATE t SET out_microsecond = MICROSECOND(s); SELECT 'microsecond', ROW_COUNT(); "\
"UPDATE t SET out_month = MONTH(d); SELECT 'month', ROW_COUNT(); "\
"UPDATE t SET out_monthname = MONTHNAME(d); SELECT 'monthname', ROW_COUNT(); "\
"UPDATE t SET out_quarter = QUARTER(d); SELECT 'quarter', ROW_COUNT(); "\
"UPDATE t SET out_week = WEEK(d); SELECT 'week', ROW_COUNT(); "\
"UPDATE t SET out_weekday = WEEKDAY(d); SELECT 'weekday', ROW_COUNT(); "\
"UPDATE t SET out_weekofyear = WEEKOFYEAR(d); SELECT 'weekofyear', ROW_COUNT(); "\
"UPDATE t SET out_year = YEAR(d); SELECT 'year', ROW_COUNT(); "\
"UPDATE t SET out_yearweek = YEARWEEK(d); SELECT 'yearweek', ROW_COUNT(); "\
"UPDATE t SET out_extract = EXTRACT(YEAR_MONTH FROM d); SELECT 'extract', ROW_COUNT(); "\
"UPDATE t SET out_timestampadd = TIMESTAMPADD(SECOND, 3661, dt); "\
"SELECT 'timestampadd', ROW_COUNT(); "\
"SELECT id,out_date,out_time,out_date_format,out_time_format,out_str_to_date,"\
"out_datediff,out_timediff,out_timestampdiff,out_timestamp,out_unix_timestamp,"\
"out_sec_to_time,out_from_unixtime,out_from_days,out_makedate,out_maketime,"\
"out_time_to_sec,out_to_days,out_to_seconds,out_dayofmonth,out_dayofweek,"\
"out_dayofyear,out_dayname,out_last_day,out_hour,out_minute,out_second,"\
"out_microsecond,out_month,out_monthname,out_quarter,out_week,out_weekday,"\
"out_weekofyear,out_year,out_yearweek,out_extract,out_timestampadd FROM t; "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_temporal_row_scalar_update_contexts_expectations: ok"
