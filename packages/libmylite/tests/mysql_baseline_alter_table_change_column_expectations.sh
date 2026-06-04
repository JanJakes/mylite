#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_alter_change_column_expectations_$$"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_alter_table_change_column_expectations: $1" >&2
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
    "change column without selected schema" \
    1046 \
    3D000 \
    "No database selected" \
    "ALTER TABLE numbers CHANGE n renamed BIGINT;"

expect_error \
    "change column qualified unknown schema" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "ALTER TABLE ${MISSING_DATABASE}.numbers CHANGE n renamed BIGINT;"

expect_error \
    "change column unknown table" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_numbers' doesn't exist" \
    "ALTER TABLE missing_numbers CHANGE n renamed BIGINT;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE ${DATABASE}.qualified_numbers (id INT NOT NULL, n INT NULL); "\
"INSERT INTO ${DATABASE}.qualified_numbers VALUES (1, 2), (2, 3);" \
    >/dev/null
expect_output \
    "schema-qualified change column without selected schema" \
    "2	0	1:2,2:3" \
    "ALTER TABLE ${DATABASE}.qualified_numbers CHANGE COLUMN n renamed BIGINT NOT NULL; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', renamed) ORDER BY id) "\
"FROM ${DATABASE}.qualified_numbers;"

run_mysql \
    "CREATE TABLE numbers (id INT NOT NULL, n INT NOT NULL, u INT UNSIGNED NULL, "\
"b BIGINT NULL, bu BIGINT UNSIGNED NULL); "\
"INSERT INTO numbers VALUES (1, 2, 3, 4, 5), (2, 6, 7, 8, 9);" \
    "$DATABASE" >/dev/null

change_sequence_expected=$(cat <<'EXPECTED'
0	0
2	0
0	0
0	0
id	int	NO		NULL	
final	bigint	YES		NULL	
u	int unsigned	YES		NULL	
b	bigint	YES		NULL	
bu	bigint unsigned	YES		NULL	
1:2,2:6
EXPECTED
)
expect_output \
    "rename type change no-op and readback" \
    "$change_sequence_expected" \
    "ALTER TABLE numbers CHANGE n renamed INT NOT NULL; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE numbers CHANGE renamed renamed BIGINT; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE numbers CHANGE COLUMN renamed final BIGINT NULL; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE numbers CHANGE final final BIGINT NULL; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW COLUMNS FROM numbers; "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', final) ORDER BY id) FROM numbers;" \
    "$DATABASE"

show_create_expected=$(cat <<'EXPECTED'
numbers	CREATE TABLE `numbers` (
  `id` int NOT NULL,
  `final` bigint DEFAULT NULL,
  `u` int unsigned DEFAULT NULL,
  `b` bigint DEFAULT NULL,
  `bu` bigint unsigned DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "show create after change" \
    "$show_create_expected" \
    "SHOW CREATE TABLE numbers;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE nullability_only (c INT NOT NULL); INSERT INTO nullability_only VALUES (1), (2);" \
    "$DATABASE" >/dev/null
nullability_only_expected=$(cat <<'EXPECTED'
0	0
c	int	YES		NULL	
0	0
renamed	int	NO		NULL	
EXPECTED
)
expect_output \
    "nullability-only and rename row count" \
    "$nullability_only_expected" \
    "ALTER TABLE nullability_only CHANGE c c INT; "\
"SELECT ROW_COUNT(), @@warning_count; SHOW COLUMNS FROM nullability_only; "\
"ALTER TABLE nullability_only CHANGE c renamed INT NOT NULL; "\
"SELECT ROW_COUNT(), @@warning_count; SHOW COLUMNS FROM nullability_only;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE case_columns (MiXeD INT NULL); "\
"INSERT INTO case_columns VALUES (1);" \
    "$DATABASE" >/dev/null
expect_output \
    "case-only spelling update row count" \
    "0	0" \
    "ALTER TABLE case_columns CHANGE MiXeD mixed INT; SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"
expect_output \
    "case-only spelling visible" \
    "mixed	int	YES		NULL	" \
    "SHOW COLUMNS FROM case_columns;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE integer_family (i INTEGER, iu INT UNSIGNED, "\
"integer_unsigned INTEGER UNSIGNED, bu BIGINT UNSIGNED); "\
"INSERT INTO integer_family VALUES (2, 3, 4, 5), (6, 7, 8, 9);" \
    "$DATABASE" >/dev/null
integer_family_expected=$(cat <<'EXPECTED'
0	0
0	0
0	0
0	0
plain_integer	int	YES		NULL	
plain_int_unsigned	int unsigned	YES		NULL	
plain_integer_unsigned	int unsigned	YES		NULL	
plain_bigint_unsigned	bigint unsigned	YES		NULL	
2:3:4:5,6:7:8:9
EXPECTED
)
expect_output \
    "integer family successful changes" \
    "$integer_family_expected" \
    "ALTER TABLE integer_family CHANGE i plain_integer INTEGER; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE integer_family CHANGE iu plain_int_unsigned INT UNSIGNED; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE integer_family CHANGE integer_unsigned plain_integer_unsigned INTEGER UNSIGNED; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE integer_family CHANGE bu plain_bigint_unsigned BIGINT UNSIGNED; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW COLUMNS FROM integer_family; "\
"SELECT GROUP_CONCAT(CONCAT(plain_integer, ':', plain_int_unsigned, ':', "\
"plain_integer_unsigned, ':', plain_bigint_unsigned) ORDER BY plain_integer) "\
"FROM integer_family;" \
    "$DATABASE"

change_after_alter_expected=$(cat <<'EXPECTED'
2	0
id	int	NO		NULL	
final	bigint	YES		NULL	
u	int unsigned	YES		NULL	
b	bigint	YES		NULL	
bu	bigint unsigned	YES		NULL	
renamed	bigint	YES		NULL	
2:6
EXPECTED
)
expect_output \
    "change after add rename drop and delete" \
    "$change_after_alter_expected" \
    "ALTER TABLE numbers ADD added INT; "\
"ALTER TABLE numbers RENAME COLUMN added TO renamed; "\
"ALTER TABLE numbers CHANGE renamed renamed BIGINT; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW COLUMNS FROM numbers; "\
"ALTER TABLE numbers DROP COLUMN renamed; "\
"DELETE FROM numbers WHERE final = 2; "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', final) ORDER BY id) FROM numbers;" \
    "$DATABASE"

expect_error \
    "change after dropped column" \
    1054 \
    42S22 \
    "Unknown column 'renamed' in 'numbers'" \
    "ALTER TABLE numbers CHANGE renamed again BIGINT;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE nullable_bad (c INT NULL); INSERT INTO nullable_bad VALUES (NULL), (1);" \
    "$DATABASE" >/dev/null
expect_error \
    "nullable to not null with null row" \
    1265 \
    01000 \
    "Data truncated for column 'changed' at row 1" \
    "ALTER TABLE nullable_bad CHANGE c changed BIGINT NOT NULL;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE range_bad (c BIGINT NOT NULL); INSERT INTO range_bad VALUES (2147483648);" \
    "$DATABASE" >/dev/null
expect_error \
    "bigint to int out of range" \
    1264 \
    22003 \
    "Out of range value for column 'changed' at row 1" \
    "ALTER TABLE range_bad CHANGE c changed INT NOT NULL;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE unsigned_bad (c INT NOT NULL); INSERT INTO unsigned_bad VALUES (-1), (0);" \
    "$DATABASE" >/dev/null
expect_error \
    "signed to unsigned out of range" \
    1264 \
    22003 \
    "Out of range value for column 'changed' at row 1" \
    "ALTER TABLE unsigned_bad CHANGE c changed INT UNSIGNED NOT NULL;" \
    "$DATABASE"

expect_error \
    "unknown old column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'numbers'" \
    "ALTER TABLE numbers CHANGE missing changed BIGINT;" \
    "$DATABASE"

expect_error \
    "duplicate replacement column" \
    1060 \
    42S21 \
    "Duplicate column name 'u'" \
    "ALTER TABLE numbers CHANGE final u BIGINT;" \
    "$DATABASE"

expect_error \
    "qualified old column syntax" \
    1064 \
    42000 \
    ".final changed BIGINT" \
    "ALTER TABLE numbers CHANGE numbers.final changed BIGINT;" \
    "$DATABASE"

expect_error \
    "qualified replacement column syntax" \
    1064 \
    42000 \
    ".changed BIGINT" \
    "ALTER TABLE numbers CHANGE final numbers.changed BIGINT;" \
    "$DATABASE"

expect_error \
    "missing replacement type syntax" \
    1064 \
    42000 \
    "near ''" \
    "ALTER TABLE numbers CHANGE final changed;" \
    "$DATABASE"

expect_upstream_accepts \
    "positioning accepted upstream outside mylite slice" \
    "CREATE TABLE positioned (a INT, b INT); "\
"ALTER TABLE positioned CHANGE b c BIGINT FIRST;" \
    "$DATABASE"

expect_upstream_accepts \
    "default accepted upstream outside mylite slice" \
    "CREATE TABLE defaulted (a INT); "\
"ALTER TABLE defaulted CHANGE a c BIGINT DEFAULT 5;" \
    "$DATABASE"

expect_upstream_accepts \
    "non-integer type accepted upstream outside mylite slice" \
    "CREATE TABLE varchar_change (a INT); INSERT INTO varchar_change VALUES (123); "\
"ALTER TABLE varchar_change CHANGE a c VARCHAR(10);" \
    "$DATABASE"

expect_output \
    "multiple change actions" \
    "0	0
c	bigint	YES
d	bigint	NO" \
    "CREATE TABLE multi_change (a INT, b INT); "\
"ALTER TABLE multi_change CHANGE a c BIGINT, CHANGE b d BIGINT NOT NULL; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'multi_change' "\
"ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

expect_upstream_accepts \
    "algorithm accepted upstream outside mylite slice" \
    "CREATE TABLE algorithm_change (a INT); "\
"ALTER TABLE algorithm_change CHANGE a c INT, ALGORITHM=INPLACE;" \
    "$DATABASE"

expect_upstream_accepts \
    "lock accepted upstream outside mylite slice" \
    "CREATE TABLE lock_change (a INT); "\
"ALTER TABLE lock_change CHANGE a c INT, LOCK=NONE;" \
    "$DATABASE"

cleanup
