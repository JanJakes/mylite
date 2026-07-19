#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_information_schema_core_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_core_expectations: $1" >&2
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
    "CREATE TABLE t ("\
"id INT AUTO_INCREMENT PRIMARY KEY, "\
"v VARCHAR(3), "\
"n INT NOT NULL DEFAULT 7, "\
"u TINYINT UNSIGNED, "\
"hidden INT"\
"); "\
"ALTER TABLE t ALTER COLUMN hidden SET INVISIBLE; "\
"CREATE TABLE other (x BIGINT UNSIGNED);" \
    "$DATABASE" >/dev/null

schemata_expected=$(printf '%b' "def\t${DATABASE}\tutf8mb4\tutf8mb4_0900_ai_ci\tNULL\tNO")
expect_output \
    "schemata user schema row" \
    "$schemata_expected" \
    "SELECT CATALOG_NAME, SCHEMA_NAME, DEFAULT_CHARACTER_SET_NAME, "\
"DEFAULT_COLLATION_NAME, SQL_PATH, DEFAULT_ENCRYPTION "\
"FROM INFORMATION_SCHEMA.SCHEMATA "\
"WHERE SCHEMA_NAME = '${DATABASE}';"

expect_output \
    "schemata limit offset" \
    "mysql" \
    "SELECT SCHEMA_NAME FROM INFORMATION_SCHEMA.SCHEMATA "\
"WHERE SCHEMA_NAME IN ('information_schema', 'mysql', 'performance_schema') "\
"ORDER BY SCHEMA_NAME LIMIT 1 OFFSET 1;"

expect_output \
    "schemata count limit offset" \
    "" \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.SCHEMATA LIMIT 1 OFFSET 1;"

tables_expected=$(printf '%b' \
"def\t${DATABASE}\tother\tBASE TABLE\tInnoDB\t10\tDynamic\t0\t0\t16384\t0\t0\t0\tNULL\t0\t1\t1\tutf8mb4_0900_ai_ci\tNULL\n"\
"def\t${DATABASE}\tt\tBASE TABLE\tInnoDB\t10\tDynamic\t0\t0\t16384\t0\t0\t0\t1\t0\t1\t1\tutf8mb4_0900_ai_ci\tNULL")
expect_output \
    "tables base table rows" \
    "$tables_expected" \
    "SELECT TABLE_CATALOG, TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, "\
"ROW_FORMAT, TABLE_ROWS, AVG_ROW_LENGTH, DATA_LENGTH, MAX_DATA_LENGTH, "\
"INDEX_LENGTH, DATA_FREE, AUTO_INCREMENT, CREATE_TIME IS NULL, UPDATE_TIME IS NULL, "\
"CHECK_TIME IS NULL, TABLE_COLLATION, CHECKSUM "\
"FROM INFORMATION_SCHEMA.TABLES "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME <> 'missing' "\
"ORDER BY TABLE_NAME;"

columns_expected=$(printf '%b' \
"def\t${DATABASE}\tt\tid\t1\tNULL\tNO\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tNULL\tint\tPRI\tauto_increment\tselect,insert,update,references\t\t\tNULL\n"\
"def\t${DATABASE}\tt\tv\t2\tNULL\tYES\tvarchar\t3\t12\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(3)\t\t\tselect,insert,update,references\t\t\tNULL\n"\
"def\t${DATABASE}\tt\tn\t3\t7\tNO\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tNULL\tint\t\t\tselect,insert,update,references\t\t\tNULL\n"\
"def\t${DATABASE}\tt\tu\t4\tNULL\tYES\ttinyint\tNULL\tNULL\t3\t0\tNULL\tNULL\tNULL\ttinyint unsigned\t\t\tselect,insert,update,references\t\t\tNULL\n"\
"def\t${DATABASE}\tt\thidden\t5\tNULL\tYES\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tNULL\tint\t\tINVISIBLE\tselect,insert,update,references\t\t\tNULL")
expect_output \
    "columns descriptor rows" \
    "$columns_expected" \
    "SELECT TABLE_CATALOG, TABLE_SCHEMA, TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, "\
"COLUMN_DEFAULT, IS_NULLABLE, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, "\
"CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION, NUMERIC_SCALE, DATETIME_PRECISION, "\
"CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE, COLUMN_KEY, EXTRA, PRIVILEGES, "\
"COLUMN_COMMENT, GENERATION_EXPRESSION, SRS_ID "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 't' "\
"ORDER BY ORDINAL_POSITION;"

system_tables_expected=$(cat <<\EXPECTED
information_schema	COLUMNS	SYSTEM VIEW	NULL	10	NULL	0
information_schema	SCHEMATA	SYSTEM VIEW	NULL	10	NULL	0
information_schema	TABLES	SYSTEM VIEW	NULL	10	NULL	0
EXPECTED
)
expect_output \
    "information schema system view table rows" \
    "$system_tables_expected" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS "\
"FROM INFORMATION_SCHEMA.TABLES "\
"WHERE TABLE_SCHEMA = 'information_schema' "\
"AND (TABLE_NAME = 'COLUMNS' OR TABLE_NAME = 'SCHEMATA' OR TABLE_NAME = 'TABLES') "\
"ORDER BY TABLE_NAME;"

system_columns_expected=$(cat <<\EXPECTED
SCHEMATA	CATALOG_NAME	1	varchar	varchar(64)
SCHEMATA	SCHEMA_NAME	2	varchar	varchar(64)
SCHEMATA	DEFAULT_CHARACTER_SET_NAME	3	varchar	varchar(64)
SCHEMATA	DEFAULT_COLLATION_NAME	4	varchar	varchar(64)
SCHEMATA	SQL_PATH	5	varbinary	varbinary(0)
SCHEMATA	DEFAULT_ENCRYPTION	6	enum	enum('NO','YES')
EXPECTED
)
expect_output \
    "information schema system view column rows" \
    "$system_columns_expected" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, DATA_TYPE, COLUMN_TYPE "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'SCHEMATA' "\
"ORDER BY ORDINAL_POSITION;"

alias_limit_expected=$(printf '%b' "id\nv\nn")
expect_output \
    "alias qualified columns ordered limited" \
    "$alias_limit_expected" \
    "SELECT c.COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS AS c "\
"WHERE c.TABLE_SCHEMA = '${DATABASE}' AND c.TABLE_NAME = 't' "\
"ORDER BY c.ORDINAL_POSITION LIMIT 3;"

id_expected="id"
expect_output \
    "metadata string predicate collation" \
    "$id_expected" \
    "SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 't' AND COLUMN_NAME = 'ID';"

expect_output \
    "numeric metadata string coercion" \
    "$id_expected" \
    "SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 't' AND ORDINAL_POSITION = '01';"

count_expected="1"
expect_output \
    "count star over filtered tables" \
    "$count_expected" \
    "USE ${DATABASE}; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES "\
"WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 't';"

table_grouped_size_expected=$(printf '%b' "other\t0\t16384\nt\t0\t16384")
expect_output \
    "tables grouped size metadata projection" \
    "$table_grouped_size_expected" \
    "SET SESSION sql_mode = REPLACE(@@SESSION.sql_mode, 'ONLY_FULL_GROUP_BY', ''); "\
"SELECT TABLE_NAME AS 'table', TABLE_ROWS AS 'rows', "\
"SUM(DATA_LENGTH + INDEX_LENGTH) AS 'bytes' "\
"FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = '${DATABASE}' "\
"AND TABLE_NAME IN ('t', 'other') GROUP BY TABLE_NAME ORDER BY TABLE_NAME;"

expect_output \
    "WordPress tables grouped size metadata projection without ordering" \
    "$table_grouped_size_expected" \
    "SET SESSION sql_mode = REPLACE(@@SESSION.sql_mode, 'ONLY_FULL_GROUP_BY', ''); "\
"SELECT TABLE_NAME AS 'table', TABLE_ROWS AS 'rows', "\
"SUM(data_length + index_length) AS 'bytes' "\
"FROM information_schema.TABLES WHERE TABLE_SCHEMA = '${DATABASE}' "\
"AND TABLE_NAME IN ('t', 'other') GROUP BY TABLE_NAME;"

expect_error \
    "unknown information schema table" \
    1109 \
    42S02 \
    "Unknown table 'NOPE' in information_schema" \
    "SELECT * FROM INFORMATION_SCHEMA.NOPE;"

expect_error \
    "unknown projection column" \
    1054 \
    42S22 \
    "Unknown column 'nope' in 'field list'" \
    "SELECT nope FROM INFORMATION_SCHEMA.TABLES;"

expect_error \
    "aliased source rejects base qualifier" \
    1054 \
    42S22 \
    "Unknown column 'TABLES.TABLE_NAME' in 'field list'" \
    "SELECT TABLES.TABLE_NAME FROM INFORMATION_SCHEMA.TABLES AS t LIMIT 1;"

expect_error \
    "unknown where column" \
    1054 \
    42S22 \
    "Unknown column 'nope' in 'where clause'" \
    "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES WHERE nope = 'x';"
