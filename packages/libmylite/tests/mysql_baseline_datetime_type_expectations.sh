#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_datetime_type_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_datetime_type_expectations: $1" >&2
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

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

run_mysql \
    "CREATE TABLE datetimes ("\
"id INT NOT NULL, d DATETIME, nn DATETIME NOT NULL DEFAULT '2024-05-06 07:08:09');" \
    "$DATABASE" >/dev/null

show_columns_expected=$(cat <<\EXPECTED
id	int	NO		NULL	
d	datetime	YES		NULL	
nn	datetime	NO		2024-05-06 07:08:09	
EXPECTED
)
expect_output \
    "show columns renders datetime descriptors" \
    "$show_columns_expected" \
    "SHOW COLUMNS FROM datetimes;" \
    "$DATABASE"

show_create_expected=$(cat <<\EXPECTED
datetimes	CREATE TABLE `datetimes` (
  `id` int NOT NULL,
  `d` datetime DEFAULT NULL,
  `nn` datetime NOT NULL DEFAULT '2024-05-06 07:08:09'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "show create renders datetime descriptors" \
    "$show_create_expected" \
    "SHOW CREATE TABLE datetimes;" \
    "$DATABASE"

information_schema_expected=$(cat <<\EXPECTED
d	datetime	datetime	YES	NULL	0	NULL	NULL	NULL
nn	datetime	datetime	NO	2024-05-06 07:08:09	0	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "information schema renders datetime descriptors" \
    "$information_schema_expected" \
    "SELECT COLUMN_NAME, DATA_TYPE, COLUMN_TYPE, IS_NULLABLE, COLUMN_DEFAULT, "\
"DATETIME_PRECISION, CHARACTER_MAXIMUM_LENGTH, NUMERIC_PRECISION, NUMERIC_SCALE "\
"FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = '${DATABASE}' "\
"AND TABLE_NAME = 'datetimes' AND COLUMN_NAME <> 'id' ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

expect_output \
    "canonical datetime values and boundaries store without warnings" \
    "1	1000-01-01 00:00:00	9999-12-31 23:59:59
2	2024-02-29 03:04:05	2024-05-06 07:08:09
3	NULL	2024-05-06 07:08:09" \
    "INSERT INTO datetimes VALUES "\
"(1, '1000-01-01 00:00:00', '9999-12-31 23:59:59'), "\
"(2, '2024-02-29 03:04:05', '2024-05-06 07:08:09'), "\
"(3, NULL, DEFAULT); "\
"SHOW WARNINGS; SELECT id, d, nn FROM datetimes ORDER BY id;" \
    "$DATABASE"

expect_error \
    "invalid canonical datetime fails" \
    1292 \
    "22007" \
    "Incorrect datetime value: '2024-02-30 00:00:00' for column 'd' at row 1" \
    "INSERT INTO datetimes VALUES (4, '2024-02-30 00:00:00', '2024-01-01 00:00:00');" \
    "$DATABASE"

expect_error \
    "zero datetime fails in strict default sql mode" \
    1292 \
    "22007" \
    "Incorrect datetime value: '0000-00-00 00:00:00' for column 'd' at row 1" \
    "INSERT INTO datetimes VALUES (5, '0000-00-00 00:00:00', '2024-01-01 00:00:00');" \
    "$DATABASE"

expect_error \
    "partial-zero datetime fails in strict default sql mode" \
    1292 \
    "22007" \
    "Incorrect datetime value: '2024-00-01 00:00:00' for column 'd' at row 1" \
    "INSERT INTO datetimes VALUES (6, '2024-00-01 00:00:00', '2024-01-01 00:00:00');" \
    "$DATABASE"

expect_error \
    "invalid time fails" \
    1292 \
    "22007" \
    "Incorrect datetime value: '2024-01-01 24:00:00' for column 'd' at row 1" \
    "INSERT INTO datetimes VALUES (7, '2024-01-01 24:00:00', '2024-01-01 00:00:00');" \
    "$DATABASE"

expect_error \
    "datetime not null rejects null" \
    1048 \
    "23000" \
    "Column 'nn' cannot be null" \
    "INSERT INTO datetimes VALUES (8, '2024-01-01 00:00:00', NULL);" \
    "$DATABASE"

ignore_expected=$(cat <<\EXPECTED
Warning	1264	Out of range value for column 'd' at row 1
Warning	1264	Out of range value for column 'd' at row 2
Warning	1048	Column 'nn' cannot be null
9	0000-00-00 00:00:00	2024-05-06 07:08:09
10	0000-00-00 00:00:00	2024-05-06 07:08:09
11	2024-01-01 00:00:00	0000-00-00 00:00:00
EXPECTED
)
expect_output \
    "datetime insert ignore adjusts invalid zero and null values" \
    "$ignore_expected" \
    "INSERT IGNORE INTO datetimes VALUES "\
"(9, '2024-02-30 00:00:00', DEFAULT), "\
"(10, '0000-00-00 00:00:00', DEFAULT), "\
"(11, '2024-01-01 00:00:00', NULL); "\
"SHOW WARNINGS; SELECT id, d, nn FROM datetimes WHERE id IN (9, 10, 11) ORDER BY id;" \
    "$DATABASE"

run_mysql \
    "INSERT INTO datetimes VALUES "\
"(4, '2024-01-02 00:00:00', '2024-01-01 00:00:00'), "\
"(5, '9999-12-31 23:59:59', '2024-01-01 00:00:00');" \
    "$DATABASE" >/dev/null

run_mysql \
    "CREATE TABLE required_t (id INT, d DATETIME NOT NULL); "\
"INSERT IGNORE INTO required_t (id) VALUES (1); "\
"INSERT IGNORE INTO required_t VALUES (2, DEFAULT);" \
    "$DATABASE" >/dev/null
expect_output \
    "datetime insert ignore adjusts missing defaults" \
    "1	0000-00-00 00:00:00
2	0000-00-00 00:00:00" \
    "SELECT id, d FROM required_t ORDER BY id;" \
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
    "datetime update uses canonical changed-row semantics" \
    "$update_expected" \
    "CREATE TABLE update_t (id INT, d DATETIME DEFAULT '2024-05-06 07:08:09', "\
"nn DATETIME NOT NULL DEFAULT '2024-05-06 07:08:09'); "\
"INSERT INTO update_t VALUES (1, '2024-05-06 07:08:09', '2024-05-06 07:08:09'); "\
"UPDATE update_t SET d = '2024-05-06 07:08:09' WHERE id = 1; SELECT ROW_COUNT(), @@warning_count; "\
"UPDATE update_t SET d = '2024-05-06 07:08:10' WHERE id = 1; SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, d, nn FROM update_t WHERE id = 1; "\
"UPDATE update_t SET d = DEFAULT WHERE id = 1; SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, d, nn FROM update_t WHERE id = 1; "\
"UPDATE update_t SET d = NULL WHERE id = 1; SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, d, nn FROM update_t WHERE id = 1;" \
    "$DATABASE"

expect_error \
    "datetime update invalid value fails" \
    1292 \
    "22007" \
    "Incorrect datetime value: '2024-02-30 00:00:00' for column 'd' at row 1" \
    "UPDATE update_t SET d = '2024-02-30 00:00:00' WHERE id = 1;" \
    "$DATABASE"

expect_error \
    "datetime update not null rejects null" \
    1048 \
    "23000" \
    "Column 'nn' cannot be null" \
    "UPDATE update_t SET nn = NULL WHERE id = 1;" \
    "$DATABASE"

predicates_expected=$(cat <<\EXPECTED
eq	1
nseq	1
neq	2,4,5,9,10,11
ge	2,5
between	1,2,4,11
inlist	1,5
isnull	3
asc	3,9,10,1,11,4,2,5
desc	5,2,4,11,1,3,9,10
EXPECTED
)
expect_output \
    "datetime predicates and ordering" \
    "$predicates_expected" \
    "SELECT 'eq', GROUP_CONCAT(id ORDER BY id) FROM datetimes WHERE d = '1000-01-01 00:00:00' "\
"UNION ALL SELECT 'nseq', GROUP_CONCAT(id ORDER BY id) FROM datetimes WHERE d <=> '1000-01-01 00:00:00' "\
"UNION ALL SELECT 'neq', GROUP_CONCAT(id ORDER BY id) FROM datetimes WHERE d <> '1000-01-01 00:00:00' "\
"UNION ALL SELECT 'ge', GROUP_CONCAT(id ORDER BY id) FROM datetimes WHERE d >= '2024-02-29 03:04:05' "\
"UNION ALL SELECT 'between', GROUP_CONCAT(id ORDER BY id) FROM datetimes "\
"WHERE d BETWEEN '1000-01-01 00:00:00' AND '2024-02-29 03:04:05' "\
"UNION ALL SELECT 'inlist', GROUP_CONCAT(id ORDER BY id) FROM datetimes "\
"WHERE d IN ('1000-01-01 00:00:00', NULL, '9999-12-31 23:59:59') "\
"UNION ALL SELECT 'isnull', GROUP_CONCAT(id ORDER BY id) FROM datetimes WHERE d IS NULL "\
"UNION ALL SELECT 'asc', GROUP_CONCAT(id ORDER BY d ASC, id ASC) FROM datetimes "\
"UNION ALL SELECT 'desc', GROUP_CONCAT(id ORDER BY d DESC, id ASC) FROM datetimes;" \
    "$DATABASE"

order_update_expected=$(cat <<\EXPECTED
2	0
1	1000-01-01 00:00:00	0
2	2024-02-29 03:04:05	0
3	NULL	1
4	2024-01-02 00:00:00	0
5	9999-12-31 23:59:59	0
9	0000-00-00 00:00:00	1
10	0000-00-00 00:00:00	0
11	2024-01-01 00:00:00	0
2	0
1	1000-01-01 00:00:00	0
2	2024-02-29 03:04:05	2
3	NULL	1
4	2024-01-02 00:00:00	0
5	9999-12-31 23:59:59	2
9	0000-00-00 00:00:00	1
10	0000-00-00 00:00:00	0
11	2024-01-01 00:00:00	0
EXPECTED
)
expect_output \
    "datetime order by limit update observes MySQL null ordering" \
    "$order_update_expected" \
    "ALTER TABLE datetimes ADD COLUMN flag INT DEFAULT 0; "\
"UPDATE datetimes SET flag = 1 ORDER BY d LIMIT 2; SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, d, flag FROM datetimes ORDER BY id; "\
"UPDATE datetimes SET flag = 2 ORDER BY d DESC LIMIT 2; SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, d, flag FROM datetimes ORDER BY id;" \
    "$DATABASE"

alter_expected=$(cat <<\EXPECTED
1	NULL	2025-01-02 03:04:05
2	NULL	2025-01-02 03:04:05
3	2026-02-03 04:05:06	2025-01-02 03:04:05
d	datetime	YES		2026-02-03 04:05:06	
EXPECTED
)
expect_output \
    "datetime alter add and default behavior" \
    "$alter_expected" \
    "CREATE TABLE alter_t (id INT); INSERT INTO alter_t VALUES (1), (2); "\
"ALTER TABLE alter_t ADD COLUMN d DATETIME; "\
"ALTER TABLE alter_t ADD COLUMN n DATETIME NOT NULL DEFAULT '2025-01-02 03:04:05'; "\
"SELECT id, d, n FROM alter_t ORDER BY id; "\
"ALTER TABLE alter_t ALTER COLUMN d SET DEFAULT '2026-02-03 04:05:06'; "\
"INSERT INTO alter_t (id, n) VALUES (3, DEFAULT); "\
"SELECT id, d, n FROM alter_t WHERE id = 3; "\
"SHOW COLUMNS FROM alter_t LIKE 'd';" \
    "$DATABASE"

expect_error \
    "datetime alter add not null without default fails on nonempty table" \
    1292 \
    "22007" \
    "Incorrect datetime value: '0000-00-00 00:00:00' for column 'bad' at row 1" \
    "ALTER TABLE alter_t ADD COLUMN bad DATETIME NOT NULL;" \
    "$DATABASE"

expect_output \
    "datetime alter add not null without default succeeds on empty table" \
    "id	int	YES		NULL	
bad	datetime	NO		NULL	" \
    "CREATE TABLE empty_alter (id INT); ALTER TABLE empty_alter ADD COLUMN bad DATETIME NOT NULL; "\
"SHOW COLUMNS FROM empty_alter;" \
    "$DATABASE"

expect_error \
    "datetime invalid default fails" \
    1067 \
    "42000" \
    "Invalid default value for 'd'" \
    "CREATE TABLE bad_default (d DATETIME DEFAULT '2024-02-30 00:00:00');" \
    "$DATABASE"

expect_error \
    "datetime zero default fails" \
    1067 \
    "42000" \
    "Invalid default value for 'd'" \
    "CREATE TABLE bad_zero_default (d DATETIME DEFAULT '0000-00-00 00:00:00');" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts datetime T separator deferred by MyLite" \
    "CREATE TABLE upstream_t (d DATETIME); INSERT INTO upstream_t VALUES ('2024-01-02T03:04:05');" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts datetime fractional input deferred by MyLite" \
    "CREATE TABLE upstream_fractional (d DATETIME); "\
"INSERT INTO upstream_fractional VALUES ('2024-01-02 03:04:05.123456');" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts datetime fractional precision deferred by MyLite" \
    "CREATE TABLE upstream_fsp (d DATETIME(3));" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts timestamp literal datetime value deferred by MyLite" \
    "CREATE TABLE upstream_literal (d DATETIME); "\
"INSERT INTO upstream_literal VALUES (TIMESTAMP '2024-01-02 03:04:05');" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts current timestamp datetime default deferred by MyLite" \
    "CREATE TABLE upstream_default (d DATETIME DEFAULT CURRENT_TIMESTAMP);" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_datetime_type_expectations: ok"
