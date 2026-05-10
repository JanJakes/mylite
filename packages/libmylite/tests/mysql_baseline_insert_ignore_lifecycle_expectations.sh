#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_insert_ignore_$$"

fail() {
    printf '%s\n' "mysql_baseline_insert_ignore_lifecycle_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names "$@"
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

expect_output_silent_stderr() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@" 2>/dev/null)
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

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup

run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

expect_output \
    "ignore values status and adjusted rows" \
    "ints_status	2	11	0
0	-128	255	-32768	65535	-8388608	16777215	-2147483648	4294967295	-9223372036854775808	9223372036854775807	NULL	0
1	-128	255	-32768	65535	-8388608	16777215	-2147483648	4294967295	-9223372036854775808	9223372036854775807	NULL	10" \
    "CREATE TABLE ints("\
"id INT NOT NULL, ti TINYINT, tu TINYINT UNSIGNED, si SMALLINT, "\
"su SMALLINT UNSIGNED, mi MEDIUMINT, mu MEDIUMINT UNSIGNED, i INT, "\
"iu INT UNSIGNED, b BIGINT, bu BIGINT UNSIGNED, n INT NULL, nn INT NOT NULL) ENGINE=InnoDB; "\
"INSERT IGNORE INTO ints(id, ti, tu, si, su, mi, mu, i, iu, b, bu, n, nn) VALUES "\
"(1, -128, 255, -32768, 65535, -8388608, 16777215, -2147483648, 4294967295, "\
"-9223372036854775808, 9223372036854775807, NULL, 10), "\
"(NULL, -129, 256, -32769, 65536, -8388609, 16777216, -2147483649, "\
"4294967296, -9223372036854775809, 9223372036854775807, NULL, NULL); "\
"SELECT 'ints_status', ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id, ti, tu, si, su, mi, mu, i, iu, b, bu, IFNULL(n, 'NULL'), nn "\
"FROM ints ORDER BY id, nn;" \
    "$DATABASE"

expect_output \
    "ignore values warning rows" \
    "Warning	1048	Column 'id' cannot be null
Warning	1264	Out of range value for column 'ti' at row 1
Warning	1264	Out of range value for column 'tu' at row 1
Warning	1048	Column 'nn' cannot be null" \
    "TRUNCATE TABLE ints; "\
"INSERT IGNORE INTO ints(id, ti, tu, nn) VALUES (NULL, 128, -1, NULL); "\
"SHOW WARNINGS;" \
    "$DATABASE"

expect_output \
    "omitted no-default warning and adjusted rows" \
    "defaults_status	2	1	0
1	7	NULL	0
2	7	NULL	0" \
    "CREATE TABLE defaults_t(id INT NOT NULL, d INT DEFAULT 7, n INT NULL, nn INT NOT NULL) "\
"ENGINE=InnoDB; "\
"INSERT IGNORE INTO defaults_t(id) VALUES (1), (2); "\
"SELECT 'defaults_status', ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id, d, IFNULL(n, 'NULL'), nn FROM defaults_t ORDER BY id;" \
    "$DATABASE"

expect_output \
    "omitted no-default warning row" \
    "Warning	1364	Field 'nn' doesn't have a default value" \
    "TRUNCATE TABLE defaults_t; "\
"INSERT IGNORE INTO defaults_t(id) VALUES (3); "\
"SHOW WARNINGS;" \
    "$DATABASE"

expect_output \
    "nullable dropped default warning row" \
    "1
Warning	1364	Field 'n' doesn't have a default value
1	NULL
2	NULL" \
    "CREATE TABLE drop_default_t(id INT NOT NULL, n INT NULL) ENGINE=InnoDB; "\
"ALTER TABLE drop_default_t ALTER n DROP DEFAULT; "\
"INSERT IGNORE INTO drop_default_t(id) VALUES (1), (2); "\
"SHOW COUNT(*) WARNINGS; "\
"SHOW WARNINGS; "\
"SELECT id, IF(n IS NULL, 'NULL', n) FROM drop_default_t ORDER BY id;" \
    "$DATABASE"

expect_output \
    "mixed omitted default warning order" \
    "3
Warning	1264	Out of range value for column 'a' at row 1
Warning	1364	Field 'b' doesn't have a default value
Warning	1264	Out of range value for column 'a' at row 2" \
    "CREATE TABLE order_t(a TINYINT, b INT NOT NULL) ENGINE=InnoDB; "\
"INSERT IGNORE INTO order_t(a) VALUES (128), (129); "\
"SHOW COUNT(*) WARNINGS; "\
"SHOW WARNINGS;" \
    "$DATABASE"

expect_output \
    "priority ignore set forms" \
    "low_set_status	1	4	0
0	127	0	0
high_set_status	1	0	0
5	1	0	6" \
    "CREATE TABLE set_t(id INT NOT NULL, ti TINYINT, tu TINYINT UNSIGNED, nn INT NOT NULL) "\
"ENGINE=InnoDB; "\
"INSERT LOW_PRIORITY IGNORE INTO set_t SET id = NULL, ti = 128, tu = -1, nn = NULL; "\
"SELECT 'low_set_status', ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id, ti, tu, nn FROM set_t ORDER BY id; "\
"TRUNCATE TABLE set_t; "\
"INSERT HIGH_PRIORITY IGNORE INTO set_t SET id = 5, ti = TRUE, tu = FALSE, nn = 6; "\
"SELECT 'high_set_status', ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id, ti, tu, nn FROM set_t ORDER BY id;" \
    "$DATABASE"

expect_output \
    "delayed ignore warning order" \
    "Warning	3005	INSERT DELAYED is no longer supported. The statement was converted to INSERT.
Warning	1048	Column 'id' cannot be null
Warning	1048	Column 'nn' cannot be null
0	NULL	NULL	0" \
    "TRUNCATE TABLE set_t; "\
"INSERT DELAYED IGNORE INTO set_t SET id = NULL, nn = NULL; "\
"SHOW WARNINGS; "\
"SELECT id, IFNULL(ti, 'NULL'), IFNULL(tu, 'NULL'), nn FROM set_t ORDER BY id;" \
    "$DATABASE"

expect_error \
    "ignore before priority" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "INSERT IGNORE LOW_PRIORITY INTO set_t VALUES (1, 1, 1, 1);" \
    "$DATABASE"

expect_error \
    "mixed priority before ignore" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "INSERT LOW_PRIORITY HIGH_PRIORITY IGNORE INTO set_t VALUES (1, 1, 1, 1);" \
    "$DATABASE"

expect_error \
    "duplicate target remains error" \
    1110 \
    42000 \
    "Column 'id' specified twice" \
    "INSERT IGNORE INTO set_t(id, id, nn) VALUES (1, 2, 3);" \
    "$DATABASE"

expect_error \
    "column count mismatch remains error" \
    1136 \
    21S01 \
    "Column count doesn't match value count at row 1" \
    "INSERT IGNORE INTO set_t(id, nn) VALUES (1);" \
    "$DATABASE"

expect_output_silent_stderr \
    "shape error warning rows do not retain adjustment warnings" \
    "Error	1136	Column count doesn't match value count at row 2" \
    "CREATE TABLE shape_t(a INT, b INT NOT NULL) ENGINE=InnoDB; "\
"INSERT IGNORE INTO shape_t(a) VALUES (1), (1, 2); "\
"SHOW WARNINGS;" \
    --force \
    "$DATABASE"

expect_error \
    "unknown target remains error" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'field list'" \
    "INSERT IGNORE INTO set_t(missing, nn) VALUES (1, 2);" \
    "$DATABASE"

expect_error \
    "unknown table remains error" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_table' doesn't exist" \
    "INSERT IGNORE INTO missing_table VALUES (1);" \
    "$DATABASE"

run_mysql "CREATE TABLE src(id INT NOT NULL, nn INT NOT NULL); INSERT INTO src VALUES (9, 9);" \
    "$DATABASE" >/dev/null
expect_output \
    "mysql accepts insert ignore select outside this slice" \
    "1	0	1" \
    "INSERT LOW_PRIORITY IGNORE INTO set_t(id, nn) SELECT id, nn FROM src; "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM set_t WHERE id = 9;" \
    "$DATABASE"
