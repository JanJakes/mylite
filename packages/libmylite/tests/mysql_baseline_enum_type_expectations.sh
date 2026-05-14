#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_enum_type_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_enum_type_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
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

expect_contains() {
    label=$1
    needle=$2
    haystack=$3

    case "$haystack" in
        *"$needle"*) ;;
        *) fail "$label: expected output to contain [$needle]" ;;
    esac
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
        fail "$label: expected MySQL to accept behavior, got [$output]"
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
    *STRICT_TRANS_TABLES*) ;;
    *) fail "expected strict default sql_mode" ;;
esac

cleanup
run_mysql \
    "CREATE DATABASE ${DATABASE} DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci;" \
    >/dev/null

show_columns_expected=$(cat <<\EXPECTED
id	int	NO	PRI	NULL	auto_increment
status	enum('draft','published')	NO		NULL	
nullable_status	enum('b','a')	YES		NULL	
spaced	enum('x','y')	YES		y	
numericish	enum('0','1','2')	YES		2	
EXPECTED
)
expect_output \
    "enum descriptors render normalized columns" \
    "$show_columns_expected" \
    "CREATE TABLE enum_values ("\
"id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, "\
"status ENUM('draft','published') NOT NULL, "\
"nullable_status ENUM('b','a') NULL, "\
"spaced ENUM('x ','y  ') DEFAULT 'y', "\
"numericish ENUM('0','1','2') DEFAULT '2'"\
"); "\
"SHOW COLUMNS FROM enum_values;" \
    "$DATABASE"

metadata_expected=$(cat <<\EXPECTED
nullable_status	enum	enum('b','a')	1	4	YES	NULL	utf8mb4	utf8mb4_0900_ai_ci
numericish	enum	enum('0','1','2')	1	4	YES	2	utf8mb4	utf8mb4_0900_ai_ci
spaced	enum	enum('x','y')	1	4	YES	y	utf8mb4	utf8mb4_0900_ai_ci
status	enum	enum('draft','published')	9	36	NO	NULL	utf8mb4	utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "enum information schema metadata" \
    "$metadata_expected" \
    "SELECT COLUMN_NAME, DATA_TYPE, COLUMN_TYPE, CHARACTER_MAXIMUM_LENGTH, "\
"CHARACTER_OCTET_LENGTH, IS_NULLABLE, COLUMN_DEFAULT, CHARACTER_SET_NAME, COLLATION_NAME "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'enum_values' "\
"AND COLUMN_NAME <> 'id' ORDER BY COLUMN_NAME;" \
    "$DATABASE"

dml_expected=$(cat <<\EXPECTED
1	draft	1	a	2	2	3
2	published	2	b	1	1	2
3	draft	1	NULL	NULL	2	3
4	draft	1	NULL	NULL	0	1
EXPECTED
)
expect_output \
    "enum insert conversion and omitted defaults" \
    "$dml_expected" \
    "INSERT INTO enum_values (status, nullable_status, numericish) VALUES "\
"('draft','a','2'), ('Published','B',2), (1,NULL,'3'), (DEFAULT, DEFAULT, '0'); "\
"SELECT id, status, status + 0, nullable_status, nullable_status + 0, "\
"numericish, numericish + 0 FROM enum_values ORDER BY id;" \
    "$DATABASE"

predicate_expected=$(cat <<\EXPECTED
case label	1,3,4
case ordinal	2
quoted numeric predicate	NULL
null safe	3,4
not equal	1
EXPECTED
)
expect_output \
    "enum predicate conversion" \
    "$predicate_expected" \
    "SELECT 'case label', GROUP_CONCAT(id ORDER BY id) FROM enum_values WHERE status = 'DRAFT'; "\
"SELECT 'case ordinal', GROUP_CONCAT(id ORDER BY id) FROM enum_values WHERE status = 2; "\
"SELECT 'quoted numeric predicate', GROUP_CONCAT(id ORDER BY id) FROM enum_values WHERE status = '2'; "\
"SELECT 'null safe', GROUP_CONCAT(id ORDER BY id) FROM enum_values "\
"WHERE nullable_status <=> NULL; "\
"SELECT 'not equal', GROUP_CONCAT(id ORDER BY id) FROM enum_values WHERE nullable_status <> 'b';" \
    "$DATABASE"

trailing_predicate_expected=$(cat <<\EXPECTED
trailing predicate	NULL
0	0
0	0
4
EXPECTED
)
expect_output \
    "enum predicates preserve trailing spaces" \
    "$trailing_predicate_expected" \
    "SELECT 'trailing predicate', GROUP_CONCAT(id ORDER BY id) FROM enum_values "\
"WHERE status = 'draft '; "\
"UPDATE enum_values SET nullable_status = 'a' WHERE status = 'draft '; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"DELETE FROM enum_values WHERE status = 'draft '; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT COUNT(*) FROM enum_values;" \
    "$DATABASE"

update_expected=$(cat <<\EXPECTED
3	0
1	published	2
2	published	2
3	published	2
4	published	2
EXPECTED
)
expect_output \
    "enum update conversion and affected rows" \
    "$update_expected" \
    "UPDATE enum_values SET status = '2' WHERE status = 1; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, status, status + 0 FROM enum_values ORDER BY id;" \
    "$DATABASE"

trailing_value_expected=$(cat <<\EXPECTED
1	0
1	y	2
2	x	1
3	x	1
EXPECTED
)
expect_output \
    "enum trims trailing spaces in defaults and row values" \
    "$trailing_value_expected" \
    "CREATE TABLE enum_trailing (id INT, v ENUM('x','y') DEFAULT 'y '); "\
"INSERT INTO enum_trailing VALUES (1, DEFAULT), (2, 'x '), (3, '2 '); "\
"UPDATE enum_trailing SET v = 'x ' WHERE id = 3; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, v, v + 0 FROM enum_trailing ORDER BY id;" \
    "$DATABASE"

empty_label_expected=$(cat <<\EXPECTED
	1
	1
a	2
EXPECTED
)
expect_output \
    "enum empty string label is distinct from null" \
    "$empty_label_expected" \
    "CREATE TABLE empty_label (v ENUM('', 'a')); "\
"INSERT INTO empty_label VALUES (''), (1), (2); "\
"SELECT v, v + 0 FROM empty_label ORDER BY v + 0, v;" \
    "$DATABASE"

metadata_output=$(run_mysql_type_info \
    "SELECT status, nullable_status FROM enum_values LIMIT 0;" \
    "$DATABASE")

expect_contains "enum field type is string" 'Type:       STRING' "$metadata_output"
expect_contains "enum metadata reports enum flag" 'Flags:      NOT_NULL ENUM NO_DEFAULT_VALUE ' \
    "$metadata_output"
expect_contains "nullable enum metadata reports enum flag" 'Flags:      ENUM ' "$metadata_output"
expect_contains "enum metadata length uses longest label" 'Length:     36' "$metadata_output"

order_expected=$(cat <<\EXPECTED
NULL
NULL
b
a
EXPECTED
)
expect_output \
    "mysql enum order uses ordinal with null first" \
    "$order_expected" \
    "SELECT nullable_status FROM enum_values ORDER BY nullable_status;" \
    "$DATABASE"

expect_error \
    "mysql rejects duplicate labels after trailing-space and collation normalization" \
    1291 \
    HY000 \
    "duplicated value 'a' in ENUM" \
    "CREATE TABLE duplicate_enum (v ENUM('a ', 'A'));" \
    "$DATABASE"

expect_error \
    "mysql rejects empty enum list" \
    1064 \
    42000 \
    "near '))'" \
    "CREATE TABLE empty_enum (v ENUM());" \
    "$DATABASE"

expect_error \
    "mysql rejects numeric enum default" \
    1067 \
    42000 \
    "Invalid default value for 'v'" \
    "CREATE TABLE numeric_default (v ENUM('a','b') DEFAULT 2);" \
    "$DATABASE"

expect_error \
    "mysql rejects invalid strict enum label" \
    1265 \
    01000 \
    "Data truncated for column 'status' at row 1" \
    "INSERT INTO enum_values (status) VALUES ('missing');" \
    "$DATABASE"

expect_error \
    "mysql rejects ordinal zero in strict enum assignment" \
    1265 \
    01000 \
    "Data truncated for column 'status' at row 1" \
    "INSERT INTO enum_values (status) VALUES (0);" \
    "$DATABASE"

expect_error \
    "mysql rejects out-of-range quoted ordinal in strict enum assignment" \
    1265 \
    01000 \
    "Data truncated for column 'status' at row 1" \
    "INSERT INTO enum_values (status) VALUES ('3');" \
    "$DATABASE"

expect_error \
    "mysql rejects null into not null enum" \
    1048 \
    23000 \
    "Column 'status' cannot be null" \
    "INSERT INTO enum_values (status) VALUES (NULL);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts enum secondary index deferred by mylite" \
    "CREATE TABLE enum_key (v ENUM('a','b'), KEY v_idx (v));" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts enum primary key deferred by mylite" \
    "CREATE TABLE enum_pk (v ENUM('a','b') PRIMARY KEY);" \
    "$DATABASE"

expect_error \
    "mysql rejects enum prefix key parts" \
    1089 \
    HY000 \
    "Incorrect prefix key" \
    "CREATE TABLE enum_prefix (v ENUM('abcdef','ghij'), KEY p (v(2)));" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_enum_type_expectations: ok"
