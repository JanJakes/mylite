#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_alter_rename_column_expectations_$$"
OTHER_DATABASE="${DATABASE}_other"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_alter_table_rename_column_expectations: $1" >&2
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
    "rename column without selected schema" \
    1046 \
    3D000 \
    "No database selected" \
    "ALTER TABLE numbers RENAME COLUMN old_col TO new_col;"

expect_error \
    "rename column qualified unknown schema" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "ALTER TABLE ${MISSING_DATABASE}.numbers RENAME COLUMN old_col TO new_col;"

expect_error \
    "rename column unknown table" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_numbers' doesn't exist" \
    "ALTER TABLE missing_numbers RENAME COLUMN old_col TO new_col;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE ${DATABASE}.qualified_numbers (id INT NOT NULL, n INT NULL); "\
"INSERT INTO ${DATABASE}.qualified_numbers VALUES (1, 2);" \
    >/dev/null
expect_output \
    "schema-qualified rename column without selected schema" \
    "0	0	1:2" \
    "ALTER TABLE ${DATABASE}.qualified_numbers RENAME COLUMN n TO renamed; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', renamed) ORDER BY id) "\
"FROM ${DATABASE}.qualified_numbers;"

run_mysql \
    "CREATE TABLE numbers (id INT NOT NULL, i INT NULL, nn BIGINT UNSIGNED NOT NULL); "\
"INSERT INTO numbers VALUES (1, NULL, 10), (2, 20, 30);" \
    "$DATABASE" >/dev/null
show_columns_expected=$(cat <<'EXPECTED'
id	int	NO		NULL	
n	int	YES		NULL	
nn	bigint unsigned	NO		NULL	
EXPECTED
)
show_create_expected=$(cat <<'EXPECTED'
numbers	CREATE TABLE `numbers` (
  `id` int NOT NULL,
  `n` int DEFAULT NULL,
  `nn` bigint unsigned NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "rename nullable middle column preserves rows" \
    "0	0	1:N:10,2:20:30" \
    "ALTER TABLE numbers RENAME COLUMN i TO n; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', IFNULL(n, 'N'), ':', nn) ORDER BY id) "\
"FROM numbers;" \
    "$DATABASE"
expect_output \
    "show columns after rename" \
    "$show_columns_expected" \
    "SHOW COLUMNS FROM numbers;" \
    "$DATABASE"
expect_output \
    "show create after rename" \
    "$show_create_expected" \
    "SHOW CREATE TABLE numbers;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE edge_columns (first_col INT NULL, middle_col INT NULL, last_col INT NULL); "\
"INSERT INTO edge_columns VALUES (1, 2, 3), (4, 5, 6);" \
    "$DATABASE" >/dev/null
expect_output \
    "rename first column preserves order" \
    "renamed_first,middle_col,last_col	1:2:3,4:5:6" \
    "ALTER TABLE edge_columns RENAME COLUMN first_col TO renamed_first; "\
"SELECT GROUP_CONCAT(COLUMN_NAME ORDER BY ORDINAL_POSITION), "\
"(SELECT GROUP_CONCAT(CONCAT(renamed_first, ':', middle_col, ':', last_col) ORDER BY renamed_first) FROM edge_columns) "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'edge_columns';" \
    "$DATABASE"
expect_output \
    "rename last column preserves order" \
    "renamed_first,middle_col,renamed_last	1:2:3,4:5:6" \
    "ALTER TABLE edge_columns RENAME COLUMN last_col TO renamed_last; "\
"SELECT GROUP_CONCAT(COLUMN_NAME ORDER BY ORDINAL_POSITION), "\
"(SELECT GROUP_CONCAT(CONCAT(renamed_first, ':', middle_col, ':', renamed_last) ORDER BY renamed_first) FROM edge_columns) "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'edge_columns';" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE case_columns (c INT NULL, d INT NULL); "\
"INSERT INTO case_columns VALUES (1, 2);" \
    "$DATABASE" >/dev/null
expect_output \
    "same-name rename succeeds as no-op" \
    "0	0	c,d" \
    "ALTER TABLE case_columns RENAME COLUMN c TO c; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(COLUMN_NAME ORDER BY ORDINAL_POSITION) "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'case_columns';" \
    "$DATABASE"
expect_output \
    "case-only rename updates visible name" \
    "0	0	C,d" \
    "ALTER TABLE case_columns RENAME COLUMN c TO C; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(COLUMN_NAME ORDER BY ORDINAL_POSITION) "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'case_columns';" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE dml_after_rename (id INT NOT NULL, old_col INT NULL, kept INT NULL); "\
"INSERT INTO dml_after_rename VALUES (1, 10, NULL), (2, 20, 5); "\
"ALTER TABLE dml_after_rename RENAME COLUMN old_col TO renamed;" \
    "$DATABASE" >/dev/null
expect_output \
    "insert update delete after rename" \
    "1	0	1:10:7,3:30:9" \
    "INSERT INTO dml_after_rename VALUES (3, 30, 9); "\
"UPDATE dml_after_rename SET kept = 7 WHERE renamed = 10; "\
"DELETE FROM dml_after_rename WHERE renamed = 20; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', renamed, ':', IFNULL(kept, 'N')) ORDER BY id) "\
"FROM dml_after_rename;" \
    "$DATABASE"

expect_error \
    "unknown renamed column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'numbers'" \
    "ALTER TABLE numbers RENAME COLUMN missing TO x;" \
    "$DATABASE"

expect_error \
    "duplicate new column" \
    1060 \
    42S21 \
    "Duplicate column name 'id'" \
    "ALTER TABLE numbers RENAME COLUMN n TO id;" \
    "$DATABASE"

run_mysql "CREATE TABLE case_collision (a INT, B INT);" "$DATABASE" >/dev/null
expect_error \
    "case-insensitive duplicate new column uses existing spelling" \
    1060 \
    42S21 \
    "Duplicate column name 'B'" \
    "ALTER TABLE case_collision RENAME COLUMN a TO b;" \
    "$DATABASE"

expect_error \
    "old qualified column syntax" \
    1064 \
    42000 \
    ".n TO x" \
    "ALTER TABLE numbers RENAME COLUMN numbers.n TO x;" \
    "$DATABASE"

expect_error \
    "new qualified column syntax" \
    1064 \
    42000 \
    ".x" \
    "ALTER TABLE numbers RENAME COLUMN n TO numbers.x;" \
    "$DATABASE"

expect_error \
    "rename column missing column keyword" \
    1064 \
    42000 \
    "TO x" \
    "ALTER TABLE numbers RENAME n TO x;" \
    "$DATABASE"

expect_error \
    "rename column missing to keyword" \
    1064 \
    42000 \
    "x" \
    "ALTER TABLE numbers RENAME COLUMN n x;" \
    "$DATABASE"

expect_error \
    "rename column positioning token syntax" \
    1064 \
    42000 \
    "FIRST" \
    "ALTER TABLE numbers RENAME COLUMN n TO x FIRST;" \
    "$DATABASE"

expect_upstream_accepts \
    "multiple rename actions accepted upstream outside mylite slice" \
    "CREATE TABLE multi_rename (a INT, b INT, c INT); "\
"ALTER TABLE multi_rename RENAME COLUMN a TO b, RENAME COLUMN b TO a;" \
    "$DATABASE"

expect_upstream_accepts \
    "algorithm modifier accepted upstream outside mylite slice" \
    "CREATE TABLE algorithm_rename (a INT, b INT); "\
"ALTER TABLE algorithm_rename RENAME COLUMN a TO c, ALGORITHM=INSTANT;" \
    "$DATABASE"

expect_upstream_accepts \
    "lock modifier accepted upstream outside mylite slice" \
    "CREATE TABLE lock_rename (a INT, b INT); "\
"ALTER TABLE lock_rename RENAME COLUMN a TO c, LOCK=DEFAULT;" \
    "$DATABASE"

cleanup
