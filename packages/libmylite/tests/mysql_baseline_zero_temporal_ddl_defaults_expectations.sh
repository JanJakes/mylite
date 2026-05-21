#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_zero_temporal_ddl_defaults_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_zero_temporal_ddl_defaults_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --socket="$MYSQL_SOCKET" -uroot \
                --batch --raw --skip-column-names "$@"
        return
    fi

    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql \
            --protocol=TCP \
            -h127.0.0.1 \
            -uroot \
            --batch \
            --raw \
            --skip-column-names \
            "$@"
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
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

expect_output \
    "empty sql_mode admits zero temporal ADD COLUMN defaults" \
    "0	0
0	0
0	0
id	int	YES		NULL	
d	date	YES		0000-00-00	
dt	datetime	YES		0000-00-00 00:00:00	
ts	timestamp	YES		0000-00-00 00:00:00	
1	0	1	0000-00-00	0000-00-00 00:00:00	0000-00-00 00:00:00" \
    "USE ${DATABASE}; SET time_zone = '+00:00'; SET sql_mode = ''; "\
"CREATE TABLE add_empty (id INT); "\
"ALTER TABLE add_empty ADD COLUMN d DATE DEFAULT '0000-00-00'; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE add_empty ADD COLUMN dt DATETIME DEFAULT '0000-00-00 00:00:00'; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE add_empty ADD COLUMN ts TIMESTAMP NULL DEFAULT '0000-00-00 00:00:00'; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW COLUMNS FROM add_empty; "\
"INSERT INTO add_empty (id) VALUES (1); "\
"SELECT ROW_COUNT(), @@warning_count, id, d, dt, ts FROM add_empty;"

expect_output \
    "nonstrict NO_ZERO_DATE counts existing zero defaults during ADD COLUMN" \
    "0	1
0	2
0	3
id	int	YES		NULL	
d	date	YES		0000-00-00	
dt	datetime	YES		0000-00-00 00:00:00	
ts	timestamp	YES		0000-00-00 00:00:00	" \
    "USE ${DATABASE}; SET time_zone = '+00:00'; SET sql_mode = 'NO_ZERO_DATE'; "\
"CREATE TABLE add_no_zero_date (id INT); "\
"ALTER TABLE add_no_zero_date ADD COLUMN d DATE DEFAULT '0000-00-00'; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE add_no_zero_date ADD COLUMN dt DATETIME DEFAULT '0000-00-00 00:00:00'; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE add_no_zero_date ADD COLUMN ts TIMESTAMP NULL DEFAULT '0000-00-00 00:00:00'; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW COLUMNS FROM add_no_zero_date;"

expect_output \
    "nonstrict NO_ZERO_IN_DATE adjusts partial ADD COLUMN defaults only for date parts" \
    "0	1
0	1
0	0
0	0
id	int	YES		NULL	
d	date	YES		0000-00-00	
dt	datetime	YES		0000-00-00 00:00:00	
t	time	YES		00:00:00	
y	year	YES		0000	" \
    "USE ${DATABASE}; SET time_zone = '+00:00'; SET sql_mode = 'NO_ZERO_IN_DATE'; "\
"CREATE TABLE add_no_zero_in_date (id INT); "\
"ALTER TABLE add_no_zero_in_date ADD COLUMN d DATE DEFAULT '2024-00-01'; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE add_no_zero_in_date ADD COLUMN dt DATETIME DEFAULT '2024-01-00 00:00:00'; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE add_no_zero_in_date ADD COLUMN t TIME DEFAULT '00:00:00'; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE add_no_zero_in_date ADD COLUMN y YEAR DEFAULT '0000'; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW COLUMNS FROM add_no_zero_in_date;"

expect_output \
    "empty sql_mode admits zero temporal ALTER COLUMN SET DEFAULT" \
    "0	0
0	0
0	0
id	int	YES		NULL	
d	date	YES		0000-00-00	
dt	datetime	YES		0000-00-00 00:00:00	
ts	timestamp	YES		0000-00-00 00:00:00	
1	0	1	0000-00-00	0000-00-00 00:00:00	0000-00-00 00:00:00" \
    "USE ${DATABASE}; SET time_zone = '+00:00'; SET sql_mode = ''; "\
"CREATE TABLE set_empty (id INT, d DATE, dt DATETIME, ts TIMESTAMP NULL); "\
"ALTER TABLE set_empty ALTER COLUMN d SET DEFAULT '0000-00-00'; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE set_empty ALTER COLUMN dt SET DEFAULT '0000-00-00 00:00:00'; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE set_empty ALTER COLUMN ts SET DEFAULT '0000-00-00 00:00:00'; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW COLUMNS FROM set_empty; "\
"INSERT INTO set_empty (id) VALUES (1); "\
"SELECT ROW_COUNT(), @@warning_count, id, d, dt, ts FROM set_empty;"

expect_output \
    "nonstrict NO_ZERO_DATE counts existing zero defaults during SET DEFAULT" \
    "0	1
0	2
0	3
id	int	YES		NULL	
d	date	YES		0000-00-00	
dt	datetime	YES		0000-00-00 00:00:00	
ts	timestamp	YES		0000-00-00 00:00:00	" \
    "USE ${DATABASE}; SET time_zone = '+00:00'; SET sql_mode = 'NO_ZERO_DATE'; "\
"CREATE TABLE set_no_zero_date (id INT, d DATE, dt DATETIME, ts TIMESTAMP NULL); "\
"ALTER TABLE set_no_zero_date ALTER COLUMN d SET DEFAULT '0000-00-00'; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE set_no_zero_date ALTER COLUMN dt SET DEFAULT '0000-00-00 00:00:00'; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE set_no_zero_date ALTER COLUMN ts SET DEFAULT '0000-00-00 00:00:00'; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW COLUMNS FROM set_no_zero_date;"

expect_output \
    "nonstrict NO_ZERO_IN_DATE adjusts partial SET DEFAULT values only for date parts" \
    "0	1
0	1
0	0
0	0
id	int	YES		NULL	
d	date	YES		0000-00-00	
dt	datetime	YES		0000-00-00 00:00:00	
t	time	YES		00:00:00	
y	year	YES		0000	" \
    "USE ${DATABASE}; SET time_zone = '+00:00'; SET sql_mode = 'NO_ZERO_IN_DATE'; "\
"CREATE TABLE set_no_zero_in_date (id INT, d DATE, dt DATETIME, t TIME, y YEAR); "\
"ALTER TABLE set_no_zero_in_date ALTER COLUMN d SET DEFAULT '2024-00-01'; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE set_no_zero_in_date ALTER COLUMN dt SET DEFAULT '2024-01-00 00:00:00'; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE set_no_zero_in_date ALTER COLUMN t SET DEFAULT '00:00:00'; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE set_no_zero_in_date ALTER COLUMN y SET DEFAULT '0000'; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW COLUMNS FROM set_no_zero_in_date;"

expect_output \
    "combined nonstrict zero modes adjust partial defaults and count stored full-zero defaults" \
    "0	1
0	2
id	int	YES		NULL	
d	date	YES		0000-00-00	
dt	datetime	YES		0000-00-00 00:00:00	
0	1
0	2
id	int	YES		NULL	
d	date	YES		0000-00-00	
dt	datetime	YES		0000-00-00 00:00:00	" \
    "USE ${DATABASE}; SET time_zone = '+00:00'; "\
"SET sql_mode = 'NO_ZERO_DATE,NO_ZERO_IN_DATE'; "\
"CREATE TABLE add_combined (id INT); "\
"ALTER TABLE add_combined ADD COLUMN d DATE DEFAULT '2024-00-01'; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE add_combined ADD COLUMN dt DATETIME DEFAULT '2024-01-00 00:00:00'; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW COLUMNS FROM add_combined; "\
"CREATE TABLE set_combined (id INT, d DATE, dt DATETIME); "\
"ALTER TABLE set_combined ALTER COLUMN d SET DEFAULT '2024-00-01'; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE set_combined ALTER COLUMN dt SET DEFAULT '2024-01-00 00:00:00'; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW COLUMNS FROM set_combined;"

expect_output \
    "MODIFY and CHANGE support zero temporal defaults in current temporal replacement subset" \
    "0	0
0	0
0	0
0	0
id	int	YES		NULL	
dt2	datetime	YES		2024-01-00 00:00:00	
ts2	timestamp	YES		0000-00-00 00:00:00	
0	1
0	1
id	int	YES		NULL	
dt2	datetime	YES		2024-01-00 00:00:00	
ts3	timestamp	YES		0000-00-00 00:00:00	" \
    "USE ${DATABASE}; SET time_zone = '+00:00'; SET sql_mode = ''; "\
"CREATE TABLE modify_change (id INT, dt DATETIME, ts TIMESTAMP NULL); "\
"ALTER TABLE modify_change MODIFY COLUMN dt DATETIME DEFAULT '2024-01-00 00:00:00'; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE modify_change MODIFY COLUMN ts TIMESTAMP NULL DEFAULT '0000-00-00 00:00:00'; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE modify_change CHANGE COLUMN dt dt2 DATETIME DEFAULT '2024-01-00 00:00:00'; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE modify_change CHANGE COLUMN ts ts2 TIMESTAMP NULL DEFAULT '0000-00-00 00:00:00'; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW COLUMNS FROM modify_change; "\
"SET sql_mode = 'NO_ZERO_DATE'; "\
"ALTER TABLE modify_change MODIFY COLUMN ts2 TIMESTAMP NULL DEFAULT '0000-00-00 00:00:00'; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE modify_change CHANGE COLUMN ts2 ts3 TIMESTAMP NULL DEFAULT '0000-00-00 00:00:00'; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW COLUMNS FROM modify_change;"

expect_error \
    "strict NO_ZERO_DATE rejects ADD COLUMN zero DATE default" \
    1067 \
    "42000" \
    "Invalid default value for 'd'" \
    "USE ${DATABASE}; SET time_zone = '+00:00'; "\
"SET sql_mode = 'STRICT_TRANS_TABLES,NO_ZERO_DATE'; "\
"CREATE TABLE bad_add_date (id INT); "\
"ALTER TABLE bad_add_date ADD COLUMN d DATE DEFAULT '0000-00-00';"

expect_error \
    "strict NO_ZERO_DATE rejects SET DEFAULT zero TIMESTAMP default" \
    1067 \
    "42000" \
    "Invalid default value for 'ts'" \
    "USE ${DATABASE}; SET time_zone = '+00:00'; "\
"SET sql_mode = 'STRICT_TRANS_TABLES,NO_ZERO_DATE'; "\
"CREATE TABLE bad_set_ts (id INT, ts TIMESTAMP NULL); "\
"ALTER TABLE bad_set_ts ALTER COLUMN ts SET DEFAULT '0000-00-00 00:00:00';"

expect_error \
    "strict NO_ZERO_IN_DATE rejects ADD COLUMN partial DATETIME default" \
    1067 \
    "42000" \
    "Invalid default value for 'dt'" \
    "USE ${DATABASE}; SET time_zone = '+00:00'; "\
"SET sql_mode = 'STRICT_TRANS_TABLES,NO_ZERO_IN_DATE'; "\
"CREATE TABLE bad_add_dt (id INT); "\
"ALTER TABLE bad_add_dt ADD COLUMN dt DATETIME DEFAULT '2024-01-00 00:00:00';"

expect_error \
    "strict NO_ZERO_IN_DATE rejects CHANGE COLUMN partial DATETIME default" \
    1067 \
    "42000" \
    "Invalid default value for 'dt2'" \
    "USE ${DATABASE}; SET time_zone = '+00:00'; "\
"SET sql_mode = 'STRICT_TRANS_TABLES,NO_ZERO_IN_DATE'; "\
"CREATE TABLE bad_change_dt (id INT, dt DATETIME); "\
"ALTER TABLE bad_change_dt CHANGE COLUMN dt dt2 DATETIME DEFAULT '2024-01-00 00:00:00';"

printf '%s\n' "mysql_baseline_zero_temporal_ddl_defaults_expectations: ok"
