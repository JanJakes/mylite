#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_date_type_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_date_type_expectations: $1" >&2
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
    "CREATE TABLE dates ("\
"id INT NOT NULL, d DATE, nn DATE NOT NULL DEFAULT '2024-02-29');" \
    "$DATABASE" >/dev/null

show_columns_expected=$(cat <<\EXPECTED
id	int	NO		NULL	
d	date	YES		NULL	
nn	date	NO		2024-02-29	
EXPECTED
)
expect_output \
    "show columns renders date descriptors" \
    "$show_columns_expected" \
    "SHOW COLUMNS FROM dates;" \
    "$DATABASE"

show_create_expected=$(cat <<\EXPECTED
dates	CREATE TABLE `dates` (
  `id` int NOT NULL,
  `d` date DEFAULT NULL,
  `nn` date NOT NULL DEFAULT '2024-02-29'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "show create renders date descriptors" \
    "$show_create_expected" \
    "SHOW CREATE TABLE dates;" \
    "$DATABASE"

information_schema_expected=$(cat <<\EXPECTED
d	date	date	YES	NULL	NULL	NULL	NULL	NULL
nn	date	date	NO	2024-02-29	NULL	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "information schema renders date descriptors" \
    "$information_schema_expected" \
    "SELECT COLUMN_NAME, DATA_TYPE, COLUMN_TYPE, IS_NULLABLE, COLUMN_DEFAULT, "\
"DATETIME_PRECISION, CHARACTER_MAXIMUM_LENGTH, NUMERIC_PRECISION, NUMERIC_SCALE "\
"FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = '${DATABASE}' "\
"AND TABLE_NAME = 'dates' AND COLUMN_NAME <> 'id' ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

expect_output \
    "canonical date values and boundaries store without warnings" \
    "1	1000-01-01	9999-12-31
2	2024-02-29	2024-01-02
3	NULL	2024-02-29" \
    "INSERT INTO dates VALUES "\
"(1, '1000-01-01', '9999-12-31'), "\
"(2, '2024-02-29', '2024-01-02'), "\
"(3, NULL, DEFAULT); "\
"SHOW WARNINGS; SELECT id, d, nn FROM dates ORDER BY id;" \
    "$DATABASE"

expect_error \
    "invalid canonical date fails" \
    1292 \
    "22007" \
    "Incorrect date value: '2024-02-30' for column 'd' at row 1" \
    "INSERT INTO dates VALUES (4, '2024-02-30', '2024-01-01');" \
    "$DATABASE"

expect_error \
    "zero date fails in strict default sql mode" \
    1292 \
    "22007" \
    "Incorrect date value: '0000-00-00' for column 'd' at row 1" \
    "INSERT INTO dates VALUES (5, '0000-00-00', '2024-01-01');" \
    "$DATABASE"

expect_error \
    "date not null rejects null" \
    1048 \
    "23000" \
    "Column 'nn' cannot be null" \
    "INSERT INTO dates VALUES (6, '2024-01-01', NULL);" \
    "$DATABASE"

ignore_expected=$(cat <<\EXPECTED
Warning	1264	Out of range value for column 'd' at row 1
Warning	1048	Column 'nn' cannot be null
7	0000-00-00	0000-00-00
EXPECTED
)
expect_output \
    "date insert ignore adjusts invalid and null values" \
    "$ignore_expected" \
    "INSERT IGNORE INTO dates VALUES (7, '2024-02-30', NULL); "\
"SHOW WARNINGS; SELECT id, d, nn FROM dates WHERE id = 7;" \
    "$DATABASE"

run_mysql \
    "INSERT INTO dates VALUES "\
"(4, '2024-01-02', '2024-01-01'), "\
"(5, '9999-12-31', '2024-01-01');" \
    "$DATABASE" >/dev/null

run_mysql \
    "CREATE TABLE required_t (id INT, d DATE NOT NULL); INSERT IGNORE INTO required_t (id) "\
"VALUES (1); INSERT IGNORE INTO required_t VALUES (2, DEFAULT);" \
    "$DATABASE" >/dev/null
expect_output \
    "date insert ignore adjusts missing defaults" \
    "1	0000-00-00
2	0000-00-00" \
    "SELECT id, d FROM required_t ORDER BY id;" \
    "$DATABASE"

update_expected=$(cat <<\EXPECTED
0	0
1	0
1	2024-03-01	2024-02-29
1	0
1	NULL	2024-02-29
EXPECTED
)
expect_output \
    "date update uses canonical changed-row semantics" \
    "$update_expected" \
    "CREATE TABLE update_t (id INT, d DATE DEFAULT '2024-02-29', "\
"nn DATE NOT NULL DEFAULT '2024-02-29'); "\
"INSERT INTO update_t VALUES (1, '2024-02-29', '2024-02-29'); "\
"UPDATE update_t SET d = '2024-02-29' WHERE id = 1; SELECT ROW_COUNT(), @@warning_count; "\
"UPDATE update_t SET d = '2024-03-01' WHERE id = 1; SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, d, nn FROM update_t WHERE id = 1; "\
"UPDATE update_t SET d = DEFAULT WHERE id = 1; SELECT ROW_COUNT(), @@warning_count; "\
"UPDATE update_t SET d = NULL WHERE id = 1; SELECT id, d, nn FROM update_t WHERE id = 1;" \
    "$DATABASE"

expect_error \
    "date update invalid value fails" \
    1292 \
    "22007" \
    "Incorrect date value: '2024-02-30' for column 'd' at row 1" \
    "UPDATE update_t SET d = '2024-02-30' WHERE id = 1;" \
    "$DATABASE"

expect_error \
    "date update not null rejects null" \
    1048 \
    "23000" \
    "Column 'nn' cannot be null" \
    "UPDATE update_t SET nn = NULL WHERE id = 1;" \
    "$DATABASE"

predicates_expected=$(cat <<\EXPECTED
eq	1
nseq	1
neq	2,4,5,7
ge	2,5
between	1,2,4
inlist	1,5
isnull	3
asc	3,7,1,4,2,5
desc	5,2,4,1,3,7
EXPECTED
)
expect_output \
    "date predicates and ordering" \
    "$predicates_expected" \
    "SELECT 'eq', GROUP_CONCAT(id ORDER BY id) FROM dates WHERE d = '1000-01-01' "\
"UNION ALL SELECT 'nseq', GROUP_CONCAT(id ORDER BY id) FROM dates WHERE d <=> '1000-01-01' "\
"UNION ALL SELECT 'neq', GROUP_CONCAT(id ORDER BY id) FROM dates WHERE d <> '1000-01-01' "\
"UNION ALL SELECT 'ge', GROUP_CONCAT(id ORDER BY id) FROM dates WHERE d >= '2024-02-29' "\
"UNION ALL SELECT 'between', GROUP_CONCAT(id ORDER BY id) FROM dates "\
"WHERE d BETWEEN '1000-01-01' AND '2024-02-29' "\
"UNION ALL SELECT 'inlist', GROUP_CONCAT(id ORDER BY id) FROM dates "\
"WHERE d IN ('1000-01-01', NULL, '9999-12-31') "\
"UNION ALL SELECT 'isnull', GROUP_CONCAT(id ORDER BY id) FROM dates WHERE d IS NULL "\
"UNION ALL SELECT 'asc', GROUP_CONCAT(id ORDER BY d ASC, id ASC) FROM dates "\
"UNION ALL SELECT 'desc', GROUP_CONCAT(id ORDER BY d DESC, id ASC) FROM dates;" \
    "$DATABASE"

order_update_expected=$(cat <<\EXPECTED
2	0
1	1000-01-01	0
2	2024-02-29	0
3	NULL	1
4	2024-01-02	0
5	9999-12-31	0
7	0000-00-00	1
2	0
1	1000-01-01	0
2	2024-02-29	2
3	NULL	1
4	2024-01-02	0
5	9999-12-31	2
7	0000-00-00	1
EXPECTED
)
expect_output \
    "date order by limit update observes MySQL null ordering" \
    "$order_update_expected" \
    "ALTER TABLE dates ADD COLUMN flag INT DEFAULT 0; "\
"UPDATE dates SET flag = 1 ORDER BY d LIMIT 2; SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, d, flag FROM dates ORDER BY id; "\
"UPDATE dates SET flag = 2 ORDER BY d DESC LIMIT 2; SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, d, flag FROM dates ORDER BY id;" \
    "$DATABASE"

alter_expected=$(cat <<\EXPECTED
1	NULL	2025-01-01
2	NULL	2025-01-01
3	2026-02-03	2025-01-01
d	date	YES		2026-02-03	
EXPECTED
)
expect_output \
    "date alter add and default behavior" \
    "$alter_expected" \
    "CREATE TABLE alter_t (id INT); INSERT INTO alter_t VALUES (1), (2); "\
"ALTER TABLE alter_t ADD COLUMN d DATE; "\
"ALTER TABLE alter_t ADD COLUMN n DATE NOT NULL DEFAULT '2025-01-01'; "\
"SELECT id, d, n FROM alter_t ORDER BY id; "\
"ALTER TABLE alter_t ALTER COLUMN d SET DEFAULT '2026-02-03'; "\
"INSERT INTO alter_t (id, n) VALUES (3, DEFAULT); "\
"SELECT id, d, n FROM alter_t WHERE id = 3; "\
"SHOW COLUMNS FROM alter_t LIKE 'd';" \
    "$DATABASE"

expect_error \
    "date alter add not null without default fails on nonempty table" \
    1292 \
    "22007" \
    "Incorrect date value: '0000-00-00' for column 'bad' at row 1" \
    "ALTER TABLE alter_t ADD COLUMN bad DATE NOT NULL;" \
    "$DATABASE"

expect_output \
    "date alter add not null without default succeeds on empty table" \
    "id	int	YES		NULL	
bad	date	NO		NULL	" \
    "CREATE TABLE empty_alter (id INT); ALTER TABLE empty_alter ADD COLUMN bad DATE NOT NULL; "\
"SHOW COLUMNS FROM empty_alter;" \
    "$DATABASE"

expect_error \
    "date invalid default fails" \
    1067 \
    "42000" \
    "Invalid default value for 'd'" \
    "CREATE TABLE bad_default (d DATE DEFAULT '2024-02-30');" \
    "$DATABASE"

expect_error \
    "date zero default fails" \
    1067 \
    "42000" \
    "Invalid default value for 'd'" \
    "CREATE TABLE bad_zero_default (d DATE DEFAULT '0000-00-00');" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts relaxed date conversion deferred by MyLite" \
    "CREATE TABLE upstream_relaxed (d DATE); "\
"INSERT INTO upstream_relaxed VALUES ('2024/01/02'), ('240102'), (20240102);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts standard date literal deferred by MyLite" \
    "CREATE TABLE upstream_literal (d DATE); "\
"INSERT INTO upstream_literal VALUES (DATE '2024-01-02');" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_date_type_expectations: ok"
