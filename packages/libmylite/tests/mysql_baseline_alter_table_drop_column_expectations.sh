#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_alter_drop_column_expectations_$$"
OTHER_DATABASE="${DATABASE}_other"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_alter_table_drop_column_expectations: $1" >&2
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
        fail "$label: expected MySQL to accept deferred syntax, got [$output]"
    fi
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
    run_mysql "DROP DATABASE IF EXISTS ${OTHER_DATABASE};" >/dev/null 2>&1 || true
    run_mysql "DROP DATABASE IF EXISTS ${MISSING_DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE}; CREATE DATABASE ${OTHER_DATABASE};" >/dev/null

expect_error \
    "drop column without selected schema" \
    1046 \
    3D000 \
    "No database selected" \
    "ALTER TABLE numbers DROP COLUMN n;"

expect_error \
    "drop column qualified unknown schema" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "ALTER TABLE ${MISSING_DATABASE}.numbers DROP COLUMN n;"

expect_error \
    "drop column unknown table" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_numbers' doesn't exist" \
    "ALTER TABLE missing_numbers DROP COLUMN n;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE ${DATABASE}.qualified_numbers (id INT NOT NULL, n INT NULL); "\
"INSERT INTO ${DATABASE}.qualified_numbers VALUES (1, 2);" \
    >/dev/null
expect_output \
    "schema-qualified drop column without selected schema" \
    "0	0	1" \
    "ALTER TABLE ${DATABASE}.qualified_numbers DROP COLUMN n; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(id ORDER BY id) "\
"FROM ${DATABASE}.qualified_numbers;"

run_mysql \
    "CREATE TABLE numbers (id INT NOT NULL, i INT NULL, nn BIGINT UNSIGNED NOT NULL); "\
"INSERT INTO numbers VALUES (1, NULL, 10), (2, 20, 30);" \
    "$DATABASE" >/dev/null
show_columns_expected=$(cat <<'EXPECTED'
id	int	NO		NULL	
nn	bigint unsigned	NO		NULL	
EXPECTED
)
show_create_expected=$(cat <<'EXPECTED'
numbers	CREATE TABLE `numbers` (
  `id` int NOT NULL,
  `nn` bigint unsigned NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "drop nullable middle column preserves rows" \
    "0	0	1:10,2:30" \
    "ALTER TABLE numbers DROP COLUMN i; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', nn) ORDER BY id) "\
"FROM numbers;" \
    "$DATABASE"
expect_output \
    "show columns after drop" \
    "$show_columns_expected" \
    "SHOW COLUMNS FROM numbers;" \
    "$DATABASE"
expect_output \
    "show create after drop" \
    "$show_create_expected" \
    "SHOW CREATE TABLE numbers;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE edge_columns (first_col INT NULL, middle_col INT NULL, last_col INT NULL); "\
"INSERT INTO edge_columns VALUES (1, 2, 3), (4, 5, 6);" \
    "$DATABASE" >/dev/null
expect_output \
    "drop without column keyword" \
    "0	0	2:3,5:6" \
    "ALTER TABLE edge_columns DROP first_col; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(middle_col, ':', last_col) ORDER BY middle_col) "\
"FROM edge_columns;" \
    "$DATABASE"
expect_output \
    "drop last non-only column" \
    "0	0	2,5" \
    "ALTER TABLE edge_columns DROP COLUMN last_col; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(middle_col ORDER BY middle_col) "\
"FROM edge_columns;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE dml_after_drop (id INT NOT NULL, dropped INT NULL, kept INT NULL); "\
"INSERT INTO dml_after_drop VALUES (1, 10, NULL), (2, 20, 5); "\
"ALTER TABLE dml_after_drop DROP COLUMN dropped;" \
    "$DATABASE" >/dev/null
expect_output \
    "insert update delete after drop" \
    "1	0	1:7,3:9" \
    "INSERT INTO dml_after_drop VALUES (3, 9); "\
"UPDATE dml_after_drop SET kept = 7 WHERE id = 1; "\
"DELETE FROM dml_after_drop WHERE id = 2; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', IFNULL(kept, 'N')) ORDER BY id) "\
"FROM dml_after_drop;" \
    "$DATABASE"

expect_error \
    "unknown dropped column" \
    1091 \
    42000 \
    "Can't DROP 'missing'; check that column/key exists" \
    "ALTER TABLE numbers DROP COLUMN missing;" \
    "$DATABASE"

expect_error \
    "qualified dropped column syntax" \
    1064 \
    42000 \
    ".nn" \
    "ALTER TABLE numbers DROP COLUMN numbers.nn;" \
    "$DATABASE"

expect_error \
    "drop only column" \
    1090 \
    42000 \
    "You can't delete all columns with ALTER TABLE; use DROP TABLE instead" \
    "CREATE TABLE one_col (id INT NOT NULL); ALTER TABLE one_col DROP COLUMN id;" \
    "$DATABASE"

expect_error \
    "drop if exists syntax" \
    1064 \
    42000 \
    "IF EXISTS" \
    "ALTER TABLE numbers DROP COLUMN IF EXISTS nn;" \
    "$DATABASE"

expect_error \
    "parenthesized drop syntax" \
    1064 \
    42000 \
    "(nn)" \
    "ALTER TABLE numbers DROP (nn);" \
    "$DATABASE"

expect_error \
    "positioning token syntax" \
    1064 \
    42000 \
    "FIRST" \
    "ALTER TABLE numbers DROP COLUMN nn FIRST;" \
    "$DATABASE"

expect_upstream_accepts \
    "multiple drop actions accepted upstream outside mylite slice" \
    "CREATE TABLE multi_drop (id INT, a INT, b INT, c INT); "\
"ALTER TABLE multi_drop DROP COLUMN a, DROP COLUMN b;" \
    "$DATABASE"

expect_upstream_accepts \
    "algorithm modifier accepted upstream outside mylite slice" \
    "CREATE TABLE algorithm_drop (id INT, a INT); "\
"ALTER TABLE algorithm_drop DROP COLUMN a, ALGORITHM=INSTANT;" \
    "$DATABASE"

expect_upstream_accepts \
    "lock modifier accepted upstream outside mylite slice" \
    "CREATE TABLE lock_drop (id INT, a INT); "\
"ALTER TABLE lock_drop DROP COLUMN a, LOCK=DEFAULT;" \
    "$DATABASE"

cleanup
