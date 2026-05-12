#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_timestamp_type_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_timestamp_type_expectations: $1" >&2
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

case "$(run_mysql "SELECT @@sql_mode;")" in
    *STRICT_TRANS_TABLES*NO_ZERO_IN_DATE*NO_ZERO_DATE*) ;;
    *) fail "expected strict default sql_mode with zero-date checks" ;;
esac

case "$(run_mysql "SELECT @@explicit_defaults_for_timestamp;")" in
    1) ;;
    *) fail "expected explicit_defaults_for_timestamp enabled" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

run_mysql \
    "SET time_zone = '+00:00'; "\
"CREATE TABLE timestamps ("\
"id INT NOT NULL, ts TIMESTAMP NULL, nn TIMESTAMP NOT NULL DEFAULT '2024-05-06 07:08:09');" \
    "$DATABASE" >/dev/null

show_columns_expected=$(
    printf 'id\tint\tNO\t\tNULL\t\n'
    printf 'ts\ttimestamp\tYES\t\tNULL\t\n'
    printf 'nn\ttimestamp\tNO\t\t2024-05-06 07:08:09\t'
)
expect_output \
    "show columns renders timestamp descriptors" \
    "$show_columns_expected" \
    "SET time_zone = '+00:00'; SHOW COLUMNS FROM timestamps;" \
    "$DATABASE"
expect_output \
    "describe renders timestamp descriptors" \
    "$show_columns_expected" \
    "SET time_zone = '+00:00'; DESCRIBE timestamps;" \
    "$DATABASE"
expect_output \
    "explain table renders timestamp descriptors" \
    "$show_columns_expected" \
    "SET time_zone = '+00:00'; EXPLAIN timestamps;" \
    "$DATABASE"

show_create_expected=$(cat <<\EXPECTED
timestamps	CREATE TABLE `timestamps` (
  `id` int NOT NULL,
  `ts` timestamp NULL DEFAULT NULL,
  `nn` timestamp NOT NULL DEFAULT '2024-05-06 07:08:09'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "show create renders timestamp descriptors" \
    "$show_create_expected" \
    "SET time_zone = '+00:00'; SHOW CREATE TABLE timestamps;" \
    "$DATABASE"

information_schema_expected=$(cat <<\EXPECTED
ts	timestamp	timestamp	YES	NULL	0	NULL	NULL	NULL
nn	timestamp	timestamp	NO	2024-05-06 07:08:09	0	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "information schema renders timestamp descriptors" \
    "$information_schema_expected" \
    "SET time_zone = '+00:00'; "\
"SELECT COLUMN_NAME, DATA_TYPE, COLUMN_TYPE, IS_NULLABLE, COLUMN_DEFAULT, "\
"DATETIME_PRECISION, CHARACTER_MAXIMUM_LENGTH, NUMERIC_PRECISION, NUMERIC_SCALE "\
"FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = '${DATABASE}' "\
"AND TABLE_NAME = 'timestamps' AND COLUMN_NAME <> 'id' ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

expect_output \
    "canonical timestamp values and boundaries store without warnings" \
    "1	1970-01-01 00:00:01	2038-01-19 03:14:07
2	2024-02-29 03:04:05	2024-05-06 07:08:09
3	NULL	2024-05-06 07:08:09" \
    "SET time_zone = '+00:00'; INSERT INTO timestamps VALUES "\
"(1, '1970-01-01 00:00:01', '2038-01-19 03:14:07'), "\
"(2, '2024-02-29 03:04:05', '2024-05-06 07:08:09'), "\
"(3, NULL, DEFAULT); "\
"SHOW WARNINGS; SELECT id, ts, nn FROM timestamps ORDER BY id;" \
    "$DATABASE"

expect_error \
    "lower out-of-range timestamp fails" \
    1292 \
    "22007" \
    "Incorrect datetime value: '1970-01-01 00:00:00' for column 'ts' at row 1" \
    "SET time_zone = '+00:00'; "\
"INSERT INTO timestamps VALUES (4, '1970-01-01 00:00:00', '2024-01-01 00:00:00');" \
    "$DATABASE"

expect_error \
    "upper out-of-range timestamp fails" \
    1292 \
    "22007" \
    "Incorrect datetime value: '2038-01-19 03:14:08' for column 'ts' at row 1" \
    "SET time_zone = '+00:00'; "\
"INSERT INTO timestamps VALUES (5, '2038-01-19 03:14:08', '2024-01-01 00:00:00');" \
    "$DATABASE"

expect_error \
    "invalid canonical timestamp fails" \
    1292 \
    "22007" \
    "Incorrect datetime value: '2024-02-30 00:00:00' for column 'ts' at row 1" \
    "SET time_zone = '+00:00'; "\
"INSERT INTO timestamps VALUES (6, '2024-02-30 00:00:00', '2024-01-01 00:00:00');" \
    "$DATABASE"

expect_error \
    "zero timestamp fails in strict default sql mode" \
    1292 \
    "22007" \
    "Incorrect datetime value: '0000-00-00 00:00:00' for column 'ts' at row 1" \
    "SET time_zone = '+00:00'; "\
"INSERT INTO timestamps VALUES (7, '0000-00-00 00:00:00', '2024-01-01 00:00:00');" \
    "$DATABASE"

expect_error \
    "partial-zero timestamp fails in strict default sql mode" \
    1292 \
    "22007" \
    "Incorrect datetime value: '2024-00-01 00:00:00' for column 'ts' at row 1" \
    "SET time_zone = '+00:00'; "\
"INSERT INTO timestamps VALUES (8, '2024-00-01 00:00:00', '2024-01-01 00:00:00');" \
    "$DATABASE"

expect_error \
    "timestamp not null rejects null" \
    1048 \
    "23000" \
    "Column 'nn' cannot be null" \
    "SET time_zone = '+00:00'; "\
"INSERT INTO timestamps VALUES (8, '2024-01-01 00:00:00', NULL);" \
    "$DATABASE"

ignore_expected=$(cat <<\EXPECTED
Warning	1264	Out of range value for column 'ts' at row 1
Warning	1264	Out of range value for column 'ts' at row 2
Warning	1048	Column 'nn' cannot be null
Warning	1264	Out of range value for column 'ts' at row 4
Warning	1264	Out of range value for column 'ts' at row 5
9	0000-00-00 00:00:00	2024-05-06 07:08:09
10	0000-00-00 00:00:00	2024-05-06 07:08:09
11	2024-01-01 00:00:00	0000-00-00 00:00:00
12	0000-00-00 00:00:00	2024-05-06 07:08:09
13	0000-00-00 00:00:00	2024-05-06 07:08:09
EXPECTED
)
expect_output \
    "timestamp insert ignore adjusts invalid zero out-of-range and null values" \
    "$ignore_expected" \
    "SET time_zone = '+00:00'; INSERT IGNORE INTO timestamps VALUES "\
"(9, '2024-02-30 00:00:00', DEFAULT), "\
"(10, '0000-00-00 00:00:00', DEFAULT), "\
"(11, '2024-01-01 00:00:00', NULL), "\
"(12, '1969-12-31 23:59:59', DEFAULT), "\
"(13, '2038-01-19 03:14:08', DEFAULT); "\
"SHOW WARNINGS; SELECT id, ts, nn FROM timestamps WHERE id BETWEEN 9 AND 13 ORDER BY id;" \
    "$DATABASE"

expect_error \
    "timestamp zero predicate literal fails" \
    1525 \
    HY000 \
    "Incorrect TIMESTAMP value: '0000-00-00 00:00:00'" \
    "SET time_zone = '+00:00'; SELECT id FROM timestamps WHERE ts = '0000-00-00 00:00:00';" \
    "$DATABASE"

expect_error \
    "timestamp insert select rejects stored zero timestamp" \
    1292 \
    "22007" \
    "Incorrect datetime value: '0000-00-00 00:00:00' for column 'ts' at row 1" \
    "SET time_zone = '+00:00'; CREATE TABLE zero_insert_copy (id INT, ts TIMESTAMP); "\
"INSERT INTO zero_insert_copy SELECT id, ts FROM timestamps WHERE id = 10;" \
    "$DATABASE"
expect_output \
    "timestamp failed zero insert select leaves target empty" \
    "0" \
    "SET time_zone = '+00:00'; SELECT COUNT(*) FROM zero_insert_copy;" \
    "$DATABASE"

expect_error \
    "timestamp replace select rejects stored zero timestamp" \
    1292 \
    "22007" \
    "Incorrect datetime value: '0000-00-00 00:00:00' for column 'ts' at row 1" \
    "SET time_zone = '+00:00'; CREATE TABLE zero_replace_copy (id INT, ts TIMESTAMP); "\
"REPLACE INTO zero_replace_copy SELECT id, ts FROM timestamps WHERE id = 10;" \
    "$DATABASE"
expect_output \
    "timestamp failed zero replace select leaves target empty" \
    "0" \
    "SET time_zone = '+00:00'; SELECT COUNT(*) FROM zero_replace_copy;" \
    "$DATABASE"

expect_error \
    "timestamp create table select rejects stored zero timestamp" \
    1292 \
    "22007" \
    "Incorrect datetime value: '0000-00-00 00:00:00' for column 'ts' at row 1" \
    "SET time_zone = '+00:00'; CREATE TABLE zero_created AS "\
"SELECT id, ts FROM timestamps WHERE id = 10;" \
    "$DATABASE"
expect_output \
    "timestamp failed zero create table select does not create target" \
    "" \
    "SET time_zone = '+00:00'; SHOW TABLES LIKE 'zero_created';" \
    "$DATABASE"

run_mysql \
    "SET time_zone = '+00:00'; CREATE TABLE required_t (id INT, ts TIMESTAMP NOT NULL);" \
    "$DATABASE" >/dev/null
expect_error \
    "timestamp omitted not null no-default insert fails" \
    1364 \
    HY000 \
    "Field 'ts' doesn't have a default value" \
    "SET time_zone = '+00:00'; INSERT INTO required_t (id) VALUES (1);" \
    "$DATABASE"

expect_output \
    "timestamp insert ignore adjusts missing defaults" \
    "Warning	1364	Field 'ts' doesn't have a default value
1	0000-00-00 00:00:00
2	0000-00-00 00:00:00" \
    "SET time_zone = '+00:00'; "\
"INSERT IGNORE INTO required_t (id) VALUES (1); "\
"INSERT IGNORE INTO required_t VALUES (2, DEFAULT); "\
"SHOW WARNINGS; SELECT id, ts FROM required_t ORDER BY id;" \
    "$DATABASE"

update_expected=$(cat <<\EXPECTED
0	0
1	0
1	2024-05-06 07:08:10	2024-05-06 07:08:09
1	0
1	2024-05-06 07:08:09	2024-05-06 07:08:09
1	0
1	NULL	2024-05-06 07:08:09
EXPECTED
)
expect_output \
    "timestamp update uses canonical changed-row semantics" \
    "$update_expected" \
    "SET time_zone = '+00:00'; "\
"CREATE TABLE update_t (id INT, ts TIMESTAMP DEFAULT '2024-05-06 07:08:09', "\
"nn TIMESTAMP NOT NULL DEFAULT '2024-05-06 07:08:09'); "\
"INSERT INTO update_t VALUES (1, '2024-05-06 07:08:09', '2024-05-06 07:08:09'); "\
"UPDATE update_t SET ts = '2024-05-06 07:08:09' WHERE id = 1; SELECT ROW_COUNT(), @@warning_count; "\
"UPDATE update_t SET ts = '2024-05-06 07:08:10' WHERE id = 1; SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, ts, nn FROM update_t WHERE id = 1; "\
"UPDATE update_t SET ts = DEFAULT WHERE id = 1; SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, ts, nn FROM update_t WHERE id = 1; "\
"UPDATE update_t SET ts = NULL WHERE id = 1; SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, ts, nn FROM update_t WHERE id = 1;" \
    "$DATABASE"

expect_error \
    "timestamp update invalid value fails" \
    1292 \
    "22007" \
    "Incorrect datetime value: '2024-02-30 00:00:00' for column 'ts' at row 1" \
    "SET time_zone = '+00:00'; UPDATE update_t SET ts = '2024-02-30 00:00:00' WHERE id = 1;" \
    "$DATABASE"

expect_error \
    "timestamp update lower out-of-range value fails" \
    1292 \
    "22007" \
    "Incorrect datetime value: '1970-01-01 00:00:00' for column 'ts' at row 1" \
    "SET time_zone = '+00:00'; UPDATE update_t SET ts = '1970-01-01 00:00:00' WHERE id = 1;" \
    "$DATABASE"

expect_error \
    "timestamp update not null rejects null" \
    1048 \
    "23000" \
    "Column 'nn' cannot be null" \
    "SET time_zone = '+00:00'; UPDATE update_t SET nn = NULL WHERE id = 1;" \
    "$DATABASE"

run_mysql \
    "SET time_zone = '+00:00'; INSERT INTO timestamps VALUES "\
"(4, '2024-01-02 00:00:00', '2024-01-01 00:00:00'), "\
"(5, '2038-01-19 03:14:07', '2024-01-01 00:00:00');" \
    "$DATABASE" >/dev/null

predicates_expected=$(cat <<\EXPECTED
eq	1
nseq	1
neq	2,4,5,9,10,11,12,13
ge	2,5
between	1,2,4,11
inlist	1,5
isnull	3
isnotnull	1,2,4,5,9,10,11,12,13
notbetween	1,5,9,10,12,13
notin	2,4,5,9,10,11,12,13
asc	3,9,10,12,13,1,11,4,2,5
desc	5,2,4,11,1,3,9,10,12,13
EXPECTED
)
expect_output \
    "timestamp predicates and ordering" \
    "$predicates_expected" \
    "SET time_zone = '+00:00'; "\
"SELECT 'eq', GROUP_CONCAT(id ORDER BY id) FROM timestamps WHERE ts = '1970-01-01 00:00:01' "\
"UNION ALL SELECT 'nseq', GROUP_CONCAT(id ORDER BY id) FROM timestamps WHERE ts <=> '1970-01-01 00:00:01' "\
"UNION ALL SELECT 'neq', GROUP_CONCAT(id ORDER BY id) FROM timestamps WHERE ts <> '1970-01-01 00:00:01' "\
"UNION ALL SELECT 'ge', GROUP_CONCAT(id ORDER BY id) FROM timestamps WHERE ts >= '2024-02-29 03:04:05' "\
"UNION ALL SELECT 'between', GROUP_CONCAT(id ORDER BY id) FROM timestamps "\
"WHERE ts BETWEEN '1970-01-01 00:00:01' AND '2024-02-29 03:04:05' "\
"UNION ALL SELECT 'inlist', GROUP_CONCAT(id ORDER BY id) FROM timestamps "\
"WHERE ts IN ('1970-01-01 00:00:01', NULL, '2038-01-19 03:14:07') "\
"UNION ALL SELECT 'isnull', GROUP_CONCAT(id ORDER BY id) FROM timestamps WHERE ts IS NULL "\
"UNION ALL SELECT 'isnotnull', GROUP_CONCAT(id ORDER BY id) FROM timestamps WHERE ts IS NOT NULL "\
"UNION ALL SELECT 'notbetween', GROUP_CONCAT(id ORDER BY id) FROM timestamps "\
"WHERE ts NOT BETWEEN '2024-01-01 00:00:00' AND '2025-12-31 23:59:59' "\
"UNION ALL SELECT 'notin', GROUP_CONCAT(id ORDER BY id) FROM timestamps "\
"WHERE ts NOT IN ('1970-01-01 00:00:01') "\
"UNION ALL SELECT 'asc', GROUP_CONCAT(id ORDER BY ts ASC, id ASC) FROM timestamps "\
"UNION ALL SELECT 'desc', GROUP_CONCAT(id ORDER BY ts DESC, id ASC) FROM timestamps;" \
    "$DATABASE"

order_update_expected=$(cat <<\EXPECTED
2	0
1	1970-01-01 00:00:01	0
2	2024-02-29 03:04:05	0
3	NULL	1
4	2024-01-02 00:00:00	0
5	2038-01-19 03:14:07	0
9	0000-00-00 00:00:00	1
10	0000-00-00 00:00:00	0
11	2024-01-01 00:00:00	0
12	0000-00-00 00:00:00	0
13	0000-00-00 00:00:00	0
2	0
1	1970-01-01 00:00:01	0
2	2024-02-29 03:04:05	2
3	NULL	1
4	2024-01-02 00:00:00	0
5	2038-01-19 03:14:07	2
9	0000-00-00 00:00:00	1
10	0000-00-00 00:00:00	0
11	2024-01-01 00:00:00	0
12	0000-00-00 00:00:00	0
13	0000-00-00 00:00:00	0
EXPECTED
)
expect_output \
    "timestamp order by limit update observes MySQL null and zero ordering" \
    "$order_update_expected" \
    "SET time_zone = '+00:00'; "\
"ALTER TABLE timestamps ADD COLUMN flag INT DEFAULT 0; "\
"UPDATE timestamps SET flag = 1 ORDER BY ts LIMIT 2; SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, ts, flag FROM timestamps ORDER BY id; "\
"UPDATE timestamps SET flag = 2 ORDER BY ts DESC LIMIT 2; SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, ts, flag FROM timestamps ORDER BY id;" \
    "$DATABASE"

alter_expected=$(
    printf '%s\n' "1	NULL	2025-01-02 03:04:05"
    printf '%s\n' "2	NULL	2025-01-02 03:04:05"
    printf '%s\n' "3	2026-02-03 04:05:06	2025-01-02 03:04:05"
    printf 'ts\ttimestamp\tYES\t\t2026-02-03 04:05:06\t\n'
    cat <<\EXPECTED
alter_t	CREATE TABLE `alter_t` (
  `id` int DEFAULT NULL,
  `ts` timestamp NULL,
  `n` timestamp NOT NULL DEFAULT '2025-01-02 03:04:05',
  `bad` timestamp NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
1	0000-00-00 00:00:00
2	0000-00-00 00:00:00
3	0000-00-00 00:00:00
EXPECTED
)
expect_output \
    "timestamp alter add default drop and not-null zero backfill behavior" \
    "$alter_expected" \
    "SET time_zone = '+00:00'; "\
"CREATE TABLE alter_t (id INT); INSERT INTO alter_t VALUES (1), (2); "\
"ALTER TABLE alter_t ADD COLUMN ts TIMESTAMP; "\
"ALTER TABLE alter_t ADD COLUMN n TIMESTAMP NOT NULL DEFAULT '2025-01-02 03:04:05'; "\
"SELECT id, ts, n FROM alter_t ORDER BY id; "\
"ALTER TABLE alter_t ALTER COLUMN ts SET DEFAULT '2026-02-03 04:05:06'; "\
"INSERT INTO alter_t (id, n) VALUES (3, DEFAULT); "\
"SELECT id, ts, n FROM alter_t WHERE id = 3; "\
"SHOW COLUMNS FROM alter_t LIKE 'ts'; "\
"ALTER TABLE alter_t ALTER COLUMN ts DROP DEFAULT; "\
"ALTER TABLE alter_t ADD COLUMN bad TIMESTAMP NOT NULL; "\
"SHOW CREATE TABLE alter_t; "\
"SELECT id, bad FROM alter_t ORDER BY id;" \
    "$DATABASE"

expect_error \
    "timestamp dropped default rejects omitted insert" \
    1364 \
    HY000 \
    "Field 'ts' doesn't have a default value" \
    "SET time_zone = '+00:00'; INSERT INTO alter_t (id, n) VALUES (4, DEFAULT);" \
    "$DATABASE"

expect_output \
    "timestamp alter add not null without default succeeds on empty table" \
    "$(printf 'id\tint\tYES\t\tNULL\t\nbad\ttimestamp\tNO\t\tNULL\t')" \
    "SET time_zone = '+00:00'; "\
"CREATE TABLE empty_alter (id INT); ALTER TABLE empty_alter ADD COLUMN bad TIMESTAMP NOT NULL; "\
"SHOW COLUMNS FROM empty_alter;" \
    "$DATABASE"

expect_error \
    "timestamp invalid default fails" \
    1067 \
    "42000" \
    "Invalid default value for 'ts'" \
    "SET time_zone = '+00:00'; "\
"CREATE TABLE bad_default (ts TIMESTAMP DEFAULT '2024-02-30 00:00:00');" \
    "$DATABASE"

expect_error \
    "timestamp zero default fails" \
    1067 \
    "42000" \
    "Invalid default value for 'ts'" \
    "SET time_zone = '+00:00'; "\
"CREATE TABLE bad_zero_default (ts TIMESTAMP DEFAULT '0000-00-00 00:00:00');" \
    "$DATABASE"

expect_error \
    "timestamp out-of-range default fails" \
    1067 \
    "42000" \
    "Invalid default value for 'ts'" \
    "SET time_zone = '+00:00'; "\
"CREATE TABLE bad_range_default (ts TIMESTAMP DEFAULT '1970-01-01 00:00:00');" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts timestamp T separator deferred by MyLite" \
    "SET time_zone = '+00:00'; "\
"CREATE TABLE upstream_t (ts TIMESTAMP); INSERT INTO upstream_t VALUES ('2024-01-02T03:04:05');" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts timestamp fractional input deferred by MyLite" \
    "SET time_zone = '+00:00'; CREATE TABLE upstream_fractional (ts TIMESTAMP); "\
"INSERT INTO upstream_fractional VALUES ('2024-01-02 03:04:05.123456');" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts timestamp fractional precision deferred by MyLite" \
    "SET time_zone = '+00:00'; CREATE TABLE upstream_fsp (ts TIMESTAMP(3));" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts timestamp literal deferred by MyLite" \
    "SET time_zone = '+00:00'; CREATE TABLE upstream_literal (ts TIMESTAMP); "\
"INSERT INTO upstream_literal VALUES (TIMESTAMP '2024-01-02 03:04:05');" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts current timestamp default deferred by MyLite" \
    "SET time_zone = '+00:00'; CREATE TABLE upstream_current_default (ts TIMESTAMP DEFAULT CURRENT_TIMESTAMP);" \
    "$DATABASE"

expect_output \
    "timestamp session time zone conversion is deferred by MyLite" \
    "2024-01-02 03:04:05
2024-01-02 01:04:05" \
    "CREATE TABLE upstream_tz (ts TIMESTAMP); "\
"SET time_zone = '+02:00'; INSERT INTO upstream_tz VALUES ('2024-01-02 03:04:05'); "\
"SELECT ts FROM upstream_tz; SET time_zone = '+00:00'; SELECT ts FROM upstream_tz;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_timestamp_type_expectations: ok"
