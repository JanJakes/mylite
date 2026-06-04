#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_alter_modify_column_expectations_$$"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_alter_table_modify_column_expectations: $1" >&2
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
    run_mysql "DROP DATABASE IF EXISTS ${MISSING_DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

expect_error \
    "modify column without selected schema" \
    1046 \
    3D000 \
    "No database selected" \
    "ALTER TABLE numbers MODIFY n BIGINT;"

expect_error \
    "modify column qualified unknown schema" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "ALTER TABLE ${MISSING_DATABASE}.numbers MODIFY n BIGINT;"

expect_error \
    "modify column unknown table" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_numbers' doesn't exist" \
    "ALTER TABLE missing_numbers MODIFY n BIGINT;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE ${DATABASE}.qualified_numbers (id INT NOT NULL, n INT NULL); "\
"INSERT INTO ${DATABASE}.qualified_numbers VALUES (1, 2), (2, 3);" \
    >/dev/null
expect_output \
    "schema-qualified modify column without selected schema" \
    "2	0	1:2,2:3" \
    "ALTER TABLE ${DATABASE}.qualified_numbers MODIFY COLUMN n BIGINT NOT NULL; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', n) ORDER BY id) "\
"FROM ${DATABASE}.qualified_numbers;"

run_mysql \
    "CREATE TABLE numbers (id INT NOT NULL, n INT NOT NULL, u INT UNSIGNED NULL, "\
"b BIGINT NULL, bu BIGINT UNSIGNED NULL); "\
"INSERT INTO numbers VALUES (1, 2, 3, 4, 5), (2, 6, 7, 8, 9);" \
    "$DATABASE" >/dev/null

expect_output \
    "modify without column keyword replaces type and nullability" \
    "2	0" \
    "ALTER TABLE numbers MODIFY n BIGINT; SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"
show_columns_expected=$(cat <<'EXPECTED'
id	int	NO		NULL	
n	bigint	YES		NULL	
u	int unsigned	YES		NULL	
b	bigint	YES		NULL	
bu	bigint unsigned	YES		NULL	
EXPECTED
)
expect_output \
    "show columns after nullable modify" \
    "$show_columns_expected" \
    "SHOW COLUMNS FROM numbers;" \
    "$DATABASE"

show_create_expected=$(cat <<'EXPECTED'
numbers	CREATE TABLE `numbers` (
  `id` int NOT NULL,
  `n` bigint DEFAULT NULL,
  `u` int unsigned DEFAULT NULL,
  `b` bigint DEFAULT NULL,
  `bu` bigint unsigned DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "show create after nullable modify" \
    "$show_create_expected" \
    "SHOW CREATE TABLE numbers;" \
    "$DATABASE"

expect_output \
    "same definition modify no-op" \
    "0	0" \
    "ALTER TABLE numbers MODIFY n BIGINT NULL; SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

modify_families_expected=$(cat <<'EXPECTED'
2	0
2	0	1:2:3:4:5,2:6:7:8:9
EXPECTED
)
expect_output \
    "modify unsigned and signed families" \
    "$modify_families_expected" \
    "ALTER TABLE numbers MODIFY COLUMN u BIGINT UNSIGNED; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE numbers MODIFY COLUMN b INT; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', n, ':', u, ':', b, ':', bu) ORDER BY id) "\
"FROM numbers;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE case_columns (MiXeD INT NULL); "\
"INSERT INTO case_columns VALUES (1);" \
    "$DATABASE" >/dev/null
expect_output \
    "case-only spelling update row count" \
    "0	0" \
    "ALTER TABLE case_columns MODIFY mixed INT; SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"
expect_output \
    "case-only spelling visible" \
    "mixed	int	YES		NULL	" \
    "SHOW COLUMNS FROM case_columns;" \
    "$DATABASE"

run_mysql "CREATE TABLE nullable_ok (c INT NULL); INSERT INTO nullable_ok VALUES (1), (2);" \
    "$DATABASE" >/dev/null
nullable_ok_expected=$(cat <<'EXPECTED'
2	0
c	bigint	NO		NULL	
EXPECTED
)
expect_output \
    "nullable to not null without null rows" \
    "$nullable_ok_expected" \
    "ALTER TABLE nullable_ok MODIFY c BIGINT NOT NULL; "\
"SELECT ROW_COUNT(), @@warning_count; SHOW COLUMNS FROM nullable_ok;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE nullability_only (c INT NOT NULL); INSERT INTO nullability_only VALUES (1), (2);" \
    "$DATABASE" >/dev/null
nullability_only_expected=$(cat <<'EXPECTED'
0	0
c	int	YES		NULL	
0	0
c	int	NO		NULL	
EXPECTED
)
expect_output \
    "nullability-only modify row count" \
    "$nullability_only_expected" \
    "ALTER TABLE nullability_only MODIFY c INT; "\
"SELECT ROW_COUNT(), @@warning_count; SHOW COLUMNS FROM nullability_only; "\
"ALTER TABLE nullability_only MODIFY c INT NOT NULL; "\
"SELECT ROW_COUNT(), @@warning_count; SHOW COLUMNS FROM nullability_only;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE nullable_bad (c INT NULL); INSERT INTO nullable_bad VALUES (NULL), (1);" \
    "$DATABASE" >/dev/null
expect_error \
    "nullable to not null with null row" \
    1265 \
    01000 \
    "Data truncated for column 'c' at row 1" \
    "ALTER TABLE nullable_bad MODIFY c BIGINT NOT NULL;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE range_bad (c BIGINT NOT NULL); INSERT INTO range_bad VALUES (2147483648);" \
    "$DATABASE" >/dev/null
expect_error \
    "bigint to int out of range" \
    1264 \
    22003 \
    "Out of range value for column 'c' at row 1" \
    "ALTER TABLE range_bad MODIFY c INT NOT NULL;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE unsigned_bad (c INT NOT NULL); INSERT INTO unsigned_bad VALUES (-1), (0);" \
    "$DATABASE" >/dev/null
expect_error \
    "signed to unsigned out of range" \
    1264 \
    22003 \
    "Out of range value for column 'c' at row 1" \
    "ALTER TABLE unsigned_bad MODIFY c INT UNSIGNED NOT NULL;" \
    "$DATABASE"

expect_error \
    "unknown modified column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'numbers'" \
    "ALTER TABLE numbers MODIFY missing BIGINT;" \
    "$DATABASE"

expect_error \
    "qualified modified column syntax" \
    1064 \
    42000 \
    ".n BIGINT" \
    "ALTER TABLE numbers MODIFY numbers.n BIGINT;" \
    "$DATABASE"

expect_upstream_accepts \
    "positioning accepted upstream outside mylite slice" \
    "CREATE TABLE positioned (a INT, b INT); "\
"ALTER TABLE positioned MODIFY b BIGINT FIRST;" \
    "$DATABASE"

expect_upstream_accepts \
    "default accepted upstream outside mylite slice" \
    "CREATE TABLE defaulted (a INT); "\
"ALTER TABLE defaulted MODIFY a BIGINT DEFAULT 5;" \
    "$DATABASE"

expect_upstream_accepts \
    "non-integer type accepted upstream outside mylite slice" \
    "CREATE TABLE varchar_modify (a INT); INSERT INTO varchar_modify VALUES (123); "\
"ALTER TABLE varchar_modify MODIFY a VARCHAR(10);" \
    "$DATABASE"

expect_output \
    "multiple modify actions" \
    "0	0
a	bigint	YES
b	bigint	NO" \
    "CREATE TABLE multi_modify (a INT, b INT); "\
"ALTER TABLE multi_modify MODIFY a BIGINT, MODIFY b BIGINT NOT NULL; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'multi_modify' "\
"ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

cleanup
