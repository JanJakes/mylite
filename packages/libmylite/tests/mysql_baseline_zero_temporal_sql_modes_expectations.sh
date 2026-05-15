#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_zero_temporal_sql_modes_expectations_$$"
EMPTY_SHOW_FIELD=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_zero_temporal_sql_modes_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw --skip-column-names "$@"
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
        *)
            fail "$label: expected error $code/$state containing [$message], got [$output]"
            ;;
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

run_mysql \
    "SET time_zone = '+00:00'; "\
"CREATE TABLE temporal_modes (id INT, d DATE, dt DATETIME, ts TIMESTAMP NULL);" \
    "$DATABASE" >/dev/null

expect_output \
    "empty sql_mode admits full zero temporals without warnings" \
    "1	0
1	0000-00-00	0000-00-00 00:00:00	0000-00-00 00:00:00" \
    "SET time_zone = '+00:00'; SET sql_mode = ''; "\
"TRUNCATE temporal_modes; "\
"INSERT INTO temporal_modes VALUES "\
"(1, '0000-00-00', '0000-00-00 00:00:00', '0000-00-00 00:00:00'); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, d, dt, ts FROM temporal_modes;" \
    "$DATABASE"

expect_output \
    "strict without zero modes still admits full zero temporals" \
    "1	0
2	0000-00-00	0000-00-00 00:00:00	0000-00-00 00:00:00" \
    "SET time_zone = '+00:00'; SET sql_mode = 'STRICT_TRANS_TABLES'; "\
"TRUNCATE temporal_modes; "\
"INSERT INTO temporal_modes VALUES "\
"(2, '0000-00-00', '0000-00-00 00:00:00', '0000-00-00 00:00:00'); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, d, dt, ts FROM temporal_modes;" \
    "$DATABASE"

expect_output \
    "nonstrict NO_ZERO_DATE stores full zeros with warnings" \
    "1	3
3	0000-00-00	0000-00-00 00:00:00	0000-00-00 00:00:00" \
    "SET time_zone = '+00:00'; SET sql_mode = 'NO_ZERO_DATE'; "\
"TRUNCATE temporal_modes; "\
"INSERT INTO temporal_modes VALUES "\
"(3, '0000-00-00', '0000-00-00 00:00:00', '0000-00-00 00:00:00'); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, d, dt, ts FROM temporal_modes;" \
    "$DATABASE"

expect_error \
    "strict NO_ZERO_DATE rejects full zero date" \
    1292 \
    "22007" \
    "Incorrect date value: '0000-00-00' for column 'd' at row 1" \
    "SET time_zone = '+00:00'; SET sql_mode = 'STRICT_TRANS_TABLES,NO_ZERO_DATE'; "\
"TRUNCATE temporal_modes; "\
"INSERT INTO temporal_modes VALUES "\
"(4, '0000-00-00', '0000-00-00 00:00:00', '0000-00-00 00:00:00');" \
    "$DATABASE"

expect_error \
    "STRICT_ALL_TABLES NO_ZERO_DATE rejects full zero date" \
    1292 \
    "22007" \
    "Incorrect date value: '0000-00-00' for column 'd' at row 1" \
    "SET time_zone = '+00:00'; SET sql_mode = 'STRICT_ALL_TABLES,NO_ZERO_DATE'; "\
"TRUNCATE temporal_modes; "\
"INSERT INTO temporal_modes VALUES "\
"(11, '0000-00-00', '0000-00-00 00:00:00', '0000-00-00 00:00:00');" \
    "$DATABASE"

expect_output \
    "empty sql_mode admits partial-zero DATE and DATETIME" \
    "1	0
5	2024-00-01	2024-01-00 00:00:00	0000-00-00 00:00:00" \
    "SET time_zone = '+00:00'; SET sql_mode = ''; "\
"TRUNCATE temporal_modes; "\
"INSERT INTO temporal_modes VALUES "\
"(5, '2024-00-01', '2024-01-00 00:00:00', '0000-00-00 00:00:00'); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, d, dt, ts FROM temporal_modes;" \
    "$DATABASE"

expect_output \
    "nonstrict NO_ZERO_IN_DATE adjusts partial-zero DATE and DATETIME" \
    "1	2
6	0000-00-00	0000-00-00 00:00:00	0000-00-00 00:00:00" \
    "SET time_zone = '+00:00'; SET sql_mode = 'NO_ZERO_IN_DATE'; "\
"TRUNCATE temporal_modes; "\
"INSERT INTO temporal_modes VALUES "\
"(6, '2024-00-01', '2024-01-00 00:00:00', '0000-00-00 00:00:00'); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, d, dt, ts FROM temporal_modes;" \
    "$DATABASE"

expect_error \
    "strict NO_ZERO_IN_DATE rejects partial-zero date" \
    1292 \
    "22007" \
    "Incorrect date value: '2024-00-01' for column 'd' at row 1" \
    "SET time_zone = '+00:00'; SET sql_mode = 'STRICT_TRANS_TABLES,NO_ZERO_IN_DATE'; "\
"TRUNCATE temporal_modes; "\
"INSERT INTO temporal_modes VALUES "\
"(7, '2024-00-01', '2024-01-00 00:00:00', '0000-00-00 00:00:00');" \
    "$DATABASE"

expect_output \
    "nonstrict invalid canonical temporals adjust to zero with warnings" \
    "1	3
8	0000-00-00	0000-00-00 00:00:00	0000-00-00 00:00:00" \
    "SET time_zone = '+00:00'; SET sql_mode = ''; "\
"TRUNCATE temporal_modes; "\
"INSERT INTO temporal_modes VALUES "\
"(8, '2024-02-31', '2024-02-31 00:00:00', '2024-02-31 00:00:00'); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, d, dt, ts FROM temporal_modes;" \
    "$DATABASE"

run_mysql \
    "SET time_zone = '+00:00'; "\
"CREATE TABLE invalid_dates (id INT, d DATE, dt DATETIME);" \
    "$DATABASE" >/dev/null

expect_output \
    "ALLOW_INVALID_DATES stores invalid DATE and DATETIME calendar days" \
    "1	0
9	2024-02-31	2024-02-31 00:00:00" \
    "SET sql_mode = 'STRICT_TRANS_TABLES,ALLOW_INVALID_DATES'; "\
"TRUNCATE invalid_dates; "\
"INSERT INTO invalid_dates VALUES (9, '2024-02-31', '2024-02-31 00:00:00'); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, d, dt FROM invalid_dates;" \
    "$DATABASE"

expect_error \
    "ALLOW_INVALID_DATES does not make invalid TIMESTAMP valid under strict mode" \
    1292 \
    "22007" \
    "Incorrect datetime value: '2024-02-31 00:00:00' for column 'ts' at row 1" \
    "SET time_zone = '+00:00'; SET sql_mode = 'STRICT_TRANS_TABLES,ALLOW_INVALID_DATES'; "\
"TRUNCATE temporal_modes; "\
"INSERT INTO temporal_modes VALUES "\
"(10, '2024-02-29', '2024-02-29 00:00:00', '2024-02-31 00:00:00');" \
    "$DATABASE"

expect_output \
    "empty sql_mode admits zero temporal defaults" \
    "d	date	YES		0000-00-00	
dt	datetime	YES		0000-00-00 00:00:00	
ts	timestamp	YES		0000-00-00 00:00:00	" \
    "SET time_zone = '+00:00'; SET sql_mode = ''; "\
"DROP TABLE IF EXISTS zero_defaults; "\
"CREATE TABLE zero_defaults ("\
"d DATE DEFAULT '0000-00-00', "\
"dt DATETIME DEFAULT '0000-00-00 00:00:00', "\
"ts TIMESTAMP NULL DEFAULT '0000-00-00 00:00:00'); "\
"SHOW COLUMNS FROM zero_defaults;" \
    "$DATABASE"

expect_output \
    "empty sql_mode admits WP-style NOT NULL zero temporal defaults" \
    "0	0
d	date	NO		0000-00-00${EMPTY_SHOW_FIELD}
dt	datetime	NO		0000-00-00 00:00:00${EMPTY_SHOW_FIELD}
ts	timestamp	NO		0000-00-00 00:00:00${EMPTY_SHOW_FIELD}
wp_defaults	CREATE TABLE \`wp_defaults\` (
  \`d\` date NOT NULL DEFAULT '0000-00-00',
  \`dt\` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  \`ts\` timestamp NOT NULL DEFAULT '0000-00-00 00:00:00'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
1	0
0000-00-00	0000-00-00 00:00:00	0000-00-00 00:00:00" \
    "SET time_zone = '+00:00'; SET sql_mode = ''; "\
"DROP TABLE IF EXISTS wp_defaults; "\
"CREATE TABLE wp_defaults ("\
"d DATE NOT NULL DEFAULT '0000-00-00', "\
"dt DATETIME NOT NULL DEFAULT '0000-00-00 00:00:00', "\
"ts TIMESTAMP NOT NULL DEFAULT '0000-00-00 00:00:00'); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW COLUMNS FROM wp_defaults; "\
"SHOW CREATE TABLE wp_defaults; "\
"INSERT INTO wp_defaults () VALUES (); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT d, dt, ts FROM wp_defaults;" \
    "$DATABASE"

expect_output \
    "nonstrict NO_ZERO_IN_DATE adjusts partial-zero defaults" \
    "d	date	YES		0000-00-00	
dt	datetime	YES		0000-00-00 00:00:00	" \
    "SET sql_mode = 'NO_ZERO_IN_DATE'; "\
"DROP TABLE IF EXISTS partial_defaults; "\
"CREATE TABLE partial_defaults ("\
"d DATE DEFAULT '2024-00-01', "\
"dt DATETIME DEFAULT '2024-01-00 00:00:00'); "\
"SHOW COLUMNS FROM partial_defaults;" \
    "$DATABASE"

expect_error \
    "strict NO_ZERO_DATE rejects zero temporal default" \
    1067 \
    "42000" \
    "Invalid default value for 'd'" \
    "SET time_zone = '+00:00'; SET sql_mode = 'STRICT_TRANS_TABLES,NO_ZERO_DATE'; "\
"DROP TABLE IF EXISTS bad_zero_default; "\
"CREATE TABLE bad_zero_default (d DATE DEFAULT '0000-00-00');" \
    "$DATABASE"

expect_output \
    "empty sql_mode admits partial-zero predicates" \
    "1	1	1" \
    "SET time_zone = '+00:00'; SET sql_mode = ''; "\
"TRUNCATE temporal_modes; "\
"INSERT INTO temporal_modes VALUES "\
"(11, '2024-00-01', '2024-01-00 00:00:00', '0000-00-00 00:00:00'); "\
"SELECT d = '2024-00-01', dt = '2024-01-00 00:00:00', "\
"ts = '0000-00-00 00:00:00' FROM temporal_modes;" \
    "$DATABASE"

expect_error \
    "NO_ZERO_IN_DATE rejects partial-zero date predicate" \
    1525 \
    "HY000" \
    "Incorrect DATE value: '2024-00-01'" \
    "SET time_zone = '+00:00'; SET sql_mode = 'NO_ZERO_IN_DATE'; "\
"SELECT COUNT(*) FROM temporal_modes WHERE d = '2024-00-01';" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_zero_temporal_sql_modes_expectations: ok"
