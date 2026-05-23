#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_timestampadd_second_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_timestampadd_second_function_expectations: $1" >&2
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

run_mysql_type_info() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --default-character-set=utf8mb4 --column-type-info -vvv "$@"
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

expect_contains() {
    label=$1
    needle=$2
    haystack=$3

    case "$haystack" in
        *"$needle"*) ;;
        *) fail "$label: expected output to contain [$needle]" ;;
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
2008-01-02 13:29:18	2008-01-02 13:29:18	2008-01-02 13:29:16	2008-01-02 13:29:17	2008-01-02 00:00:01	NULL	NULL	0
-1	0
EXPECTED
)
expect_output \
    "core TIMESTAMPADD second values" \
    "$core_expected" \
    "DO 0; SELECT TIMESTAMPADD(SECOND, 1, '2008-01-02 13:29:17'), "\
"TIMESTAMPADD(SQL_TSI_SECOND, +1, \"2008-01-02 13:29:17\"), "\
"TIMESTAMPADD(SECOND, -1, '2008-01-02 13:29:17'), "\
"TIMESTAMPADD(SECOND, 0, '2008-01-02 13:29:17'), "\
"TIMESTAMPADD(SECOND, 1, '2008-01-02'), "\
"TIMESTAMPADD(SECOND, NULL, '2008-01-02 13:29:17'), "\
"TIMESTAMPADD(SECOND, 1, NULL), @@warning_count; "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

labels_expected=$(cat <<EXPECTED
TIMESTAMPADD(SECOND, 1, '2008-01-02 13:29:17')	shifted
2008-01-02 13:29:18	2008-01-02 13:29:16
EXPECTED
)
expect_output_with_headers \
    "TIMESTAMPADD labels" \
    "$labels_expected" \
    "SELECT TIMESTAMPADD(SECOND, 1, '2008-01-02 13:29:17'), "\
"TIMESTAMPADD(SECOND, -1, '2008-01-02 13:29:17') AS shifted FROM DUAL;" \
    "$DATABASE"

rollover_expected=$(cat <<EXPECTED
2024-02-29 00:00:00	2024-03-01 00:00:00
EXPECTED
)
expect_output \
    "TIMESTAMPADD leap rollover" \
    "$rollover_expected" \
    "SELECT TIMESTAMPADD(SECOND, 1, '2024-02-28 23:59:59'), "\
"TIMESTAMPADD(SECOND, 1, '2024-02-29 23:59:59');" \
    "$DATABASE"

do_expected=$(cat <<EXPECTED
0	0
EXPECTED
)
expect_output \
    "TIMESTAMPADD DO status" \
    "$do_expected" \
    "DO TIMESTAMPADD(SECOND, 1, '2008-01-02 13:29:17'), "\
"TIMESTAMPADD(SECOND, 1, NULL); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "TIMESTAMPADD whitespace before paren" \
    "2008-01-02 13:29:18" \
    "SET SESSION sql_mode = ''; "\
"SELECT TIMESTAMPADD (SECOND, 1, '2008-01-02 13:29:17');" \
    "$DATABASE"

expect_output \
    "TIMESTAMPADD identifier use" \
    "timestampadd" \
    "DROP TABLE IF EXISTS timestampadd; CREATE TABLE timestampadd (timestampadd INT); "\
"SHOW TABLES LIKE 'timestampadd'; DROP TABLE timestampadd;" \
    "$DATABASE"

expect_error \
    "quoted unit is a syntax error" \
    1064 \
    "42000" \
    "syntax" \
    "SELECT TIMESTAMPADD('SECOND', 1, '2008-01-02');" \
    "$DATABASE"

expect_error \
    "SQL_TSI_MICROSECOND is a syntax error" \
    1064 \
    "42000" \
    "syntax" \
    "SELECT TIMESTAMPADD(SQL_TSI_MICROSECOND, 1, '2008-01-02');" \
    "$DATABASE"

zero_expected=$(cat <<EXPECTED
NULL	NULL	0000-00-00 00:00:01	1
Warning	1292	Incorrect datetime value: '0000-00-00'
Warning	1292	Incorrect datetime value: '2001-11-00'
EXPECTED
)
expect_output \
    "zero and partial-zero warning behavior" \
    "$zero_expected" \
    "SELECT TIMESTAMPADD(SECOND, 1, '0000-00-00'), "\
"TIMESTAMPADD(SECOND, 1, '2001-11-00'), "\
"TIMESTAMPADD(SECOND, 1, '0000-01-02'), @@warning_count; SHOW WARNINGS;" \
    "$DATABASE"

run_mysql \
    "SET time_zone = '+00:00'; "\
"DROP TABLE IF EXISTS timestampadd_rows; "\
"CREATE TABLE timestampadd_rows ("\
"id INT, d DATE NULL, dt DATETIME NULL, ts TIMESTAMP NULL, v VARCHAR(32) NULL, tm TIME NULL"\
"); "\
"INSERT INTO timestampadd_rows VALUES "\
"(1,'2008-01-02','2008-01-02 13:29:17','2008-01-02 13:29:17',"\
"'2008-01-02 13:29:17','01:02:03'), "\
"(2,NULL,NULL,NULL,NULL,NULL), "\
"(3,NULL,'9999-12-31 23:59:59',NULL,'bad',NULL);" \
    "$DATABASE" >/dev/null

row_expected=$(cat <<EXPECTED
1	2008-01-02 00:00:01	2008-01-02 13:29:16	2008-01-02 13:29:19	2008-01-02 13:29:18
2	NULL	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "row TIMESTAMPADD second values" \
    "$row_expected" \
    "SELECT id, TIMESTAMPADD(SECOND,1,d), TIMESTAMPADD(SECOND,-1,dt), "\
"TIMESTAMPADD(SQL_TSI_SECOND,2,ts), TIMESTAMPADD(SECOND,1,v) "\
"FROM timestampadd_rows WHERE id < 3 ORDER BY id;" \
    "$DATABASE"

row_warning_expected=$(cat <<EXPECTED
3	NULL	NULL	1
Warning	1441	Datetime function: datetime field overflow
Warning	1292	Incorrect datetime value: 'bad'
EXPECTED
)
expect_output \
    "row TIMESTAMPADD warning behavior" \
    "$row_warning_expected" \
    "SELECT id, TIMESTAMPADD(SECOND,1,dt), TIMESTAMPADD(SECOND,1,v), @@warning_count "\
"FROM timestampadd_rows WHERE id = 3; SHOW WARNINGS;" \
    "$DATABASE"

row_metadata_output=$(run_mysql_type_info \
    "USE ${DATABASE}; SET NAMES utf8mb4; "\
"SELECT TIMESTAMPADD(SECOND,1,d) AS d_shift, "\
"TIMESTAMPADD(SECOND,1,dt) AS dt_shift, "\
"TIMESTAMPADD(SECOND,1,ts) AS ts_shift, "\
"TIMESTAMPADD(SECOND,1,v) AS v_shift "\
"FROM timestampadd_rows LIMIT 0;" \
    "$DATABASE")
expect_contains "row DATE metadata label" 'Field   1:  `d_shift`' "$row_metadata_output"
expect_contains "row DATE metadata type" 'Type:       DATETIME' "$row_metadata_output"
expect_contains "row DATETIME metadata label" 'Field   2:  `dt_shift`' "$row_metadata_output"
expect_contains "row TIMESTAMP metadata label" 'Field   3:  `ts_shift`' "$row_metadata_output"
expect_contains "row temporal metadata length" 'Length:     19' "$row_metadata_output"
expect_contains "row temporal metadata flags" 'Flags:      BINARY ' "$row_metadata_output"
expect_contains "row string metadata label" 'Field   4:  `v_shift`' "$row_metadata_output"
expect_contains "row string metadata type" 'Type:       STRING' "$row_metadata_output"
expect_contains \
    "row string metadata collation" \
    'Collation:  utf8mb4_0900_ai_ci (255)' \
    "$row_metadata_output"
expect_contains "row string metadata length" 'Length:     116' "$row_metadata_output"

expect_upstream_accepts \
    "MySQL accepts deferred minute unit" \
    "SELECT TIMESTAMPADD(MINUTE, 1, '2008-01-02');" \
    "$DATABASE"

expect_upstream_accepts \
    "MySQL accepts deferred expression interval" \
    "SELECT TIMESTAMPADD(SECOND, 1 + 1, '2008-01-02');" \
    "$DATABASE"

expect_upstream_accepts \
    "MySQL accepts deferred decimal interval" \
    "SELECT TIMESTAMPADD(SECOND, 1.9, '2008-01-02');" \
    "$DATABASE"

expect_upstream_accepts \
    "MySQL accepts deferred TIME column" \
    "SELECT TIMESTAMPADD(SECOND, 1, tm) FROM timestampadd_rows WHERE id = 1;" \
    "$DATABASE"
