#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_result_column_metadata_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_result_column_metadata_expectations: $1" >&2
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

expect_contains() {
    label=$1
    needle=$2
    haystack=$3

    case "$haystack" in
        *"$needle"*) ;;
        *) fail "$label: expected output to contain [$needle]" ;;
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
run_mysql "CREATE DATABASE ${DATABASE} DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci;" \
    >/dev/null

run_mysql \
    "USE ${DATABASE}; SET NAMES utf8mb4; "\
"CREATE TABLE meta ("\
"id INT NOT NULL AUTO_INCREMENT, "\
"ti TINYINT, tiu TINYINT UNSIGNED, si SMALLINT, siu SMALLINT UNSIGNED, "\
"mi MEDIUMINT, miu MEDIUMINT UNSIGNED, i INT, iu INT UNSIGNED, "\
"bi BIGINT, biu BIGINT UNSIGNED, d DECIMAL(6,2), du DECIMAL(6,2) UNSIGNED, "\
"f FLOAT, x DOUBLE, y YEAR, dt DATE, tm TIME, ts TIMESTAMP NULL, dttm DATETIME, "\
"c CHAR(5) NOT NULL, v VARCHAR(20), txt TEXT, b BINARY(3), vb VARBINARY(4), "\
"blob_col BLOB, bitcol BIT(5), "\
"PRIMARY KEY (id), UNIQUE KEY uk_i (i), KEY k_v (v(3))"\
") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;" \
    "$DATABASE" >/dev/null

metadata_output=$(run_mysql_type_info \
    "USE ${DATABASE}; SET NAMES utf8mb4; "\
"SELECT id AS ident, ti, tiu, si, siu, mi, miu, i, iu, bi, biu, d, du, f, x, y, "\
"dt, tm, ts, dttm, c, v AS label_v, txt, b, vb, blob_col, bitcol "\
"FROM meta AS m LIMIT 0;" \
    "$DATABASE")

expect_contains "alias field label" 'Field   1:  `ident`' "$metadata_output"
expect_contains "alias table name" 'Table:      `m`' "$metadata_output"
expect_contains "alias origin table" 'Org_table:  `meta`' "$metadata_output"
expect_contains "int type" 'Type:       LONG' "$metadata_output"
expect_contains "int length" 'Length:     11' "$metadata_output"
expect_contains "primary flags" 'Flags:      NOT_NULL PRI_KEY AUTO_INCREMENT NUM PART_KEY ' \
    "$metadata_output"
expect_contains "tinyint signed length" 'Field   2:  `ti`' "$metadata_output"
expect_contains "tinyint signed type" 'Type:       TINY' "$metadata_output"
expect_contains "tinyint signed length value" 'Length:     4' "$metadata_output"
expect_contains "tinyint unsigned length" 'Length:     3' "$metadata_output"
expect_contains "tinyint unsigned flags" 'Flags:      UNSIGNED NUM ' "$metadata_output"
expect_contains "smallint type" 'Type:       SHORT' "$metadata_output"
expect_contains "mediumint type" 'Type:       INT24' "$metadata_output"
expect_contains "bigint type" 'Type:       LONGLONG' "$metadata_output"
expect_contains "decimal type" 'Type:       NEWDECIMAL' "$metadata_output"
expect_contains "decimal length" 'Length:     8' "$metadata_output"
expect_contains "decimal decimals" 'Decimals:   2' "$metadata_output"
expect_contains "unsigned decimal flags" 'Flags:      UNSIGNED NUM ' "$metadata_output"
expect_contains "float type" 'Type:       FLOAT' "$metadata_output"
expect_contains "float length" 'Length:     12' "$metadata_output"
expect_contains "double type" 'Type:       DOUBLE' "$metadata_output"
expect_contains "double length" 'Length:     22' "$metadata_output"
expect_contains "year type" 'Type:       YEAR' "$metadata_output"
expect_contains "year flags" 'Flags:      UNSIGNED ZEROFILL NUM ' "$metadata_output"
expect_contains "date type" 'Type:       DATE' "$metadata_output"
expect_contains "time type" 'Type:       TIME' "$metadata_output"
expect_contains "timestamp type" 'Type:       TIMESTAMP' "$metadata_output"
expect_contains "datetime type" 'Type:       DATETIME' "$metadata_output"
expect_contains "char type" 'Type:       STRING' "$metadata_output"
expect_contains "char length" 'Length:     20' "$metadata_output"
expect_contains "varchar alias label" 'Field  22:  `label_v`' "$metadata_output"
expect_contains "varchar type" 'Type:       VAR_STRING' "$metadata_output"
expect_contains "varchar length" 'Length:     80' "$metadata_output"
expect_contains "text type" 'Type:       BLOB' "$metadata_output"
expect_contains "text collation" 'Collation:  utf8mb4_0900_ai_ci (255)' "$metadata_output"
expect_contains "text flags" 'Flags:      BLOB ' "$metadata_output"
expect_contains "binary collation" 'Collation:  binary (63)' "$metadata_output"
expect_contains "binary flags" 'Flags:      BINARY ' "$metadata_output"
expect_contains "blob flags" 'Flags:      BLOB BINARY ' "$metadata_output"
expect_contains "bit type" 'Type:       BIT' "$metadata_output"
expect_contains "bit length" 'Length:     5' "$metadata_output"
expect_contains "bit flags" 'Flags:      UNSIGNED ' "$metadata_output"
expect_contains "warning count" '0 rows in set' "$metadata_output"

binary_collation_output=$(run_mysql_type_info \
    "USE ${DATABASE}; SET NAMES utf8mb4; "\
"CREATE TABLE bin_default (v VARCHAR(10)) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin; "\
"SELECT v FROM bin_default LIMIT 0;" \
    "$DATABASE")

expect_contains "utf8mb4_bin result collation stays result charset" \
    'Collation:  utf8mb4_0900_ai_ci (255)' "$binary_collation_output"
expect_contains "utf8mb4_bin sets binary flag" 'Flags:      BINARY ' "$binary_collation_output"

connection_collation_output=$(run_mysql_type_info \
    "USE ${DATABASE}; SET NAMES utf8mb4 COLLATE utf8mb4_bin; "\
"SELECT v FROM meta LIMIT 0;" \
    "$DATABASE")

expect_contains "connection collation selects utf8mb4_bin metadata" \
    'Collation:  utf8mb4_bin (46)' "$connection_collation_output"
expect_contains "connection collation does not imply binary flag" \
    'Flags:      MULTIPLE_KEY PART_KEY ' "$connection_collation_output"

information_schema_output=$(run_mysql_type_info \
    "SET NAMES utf8mb4; "\
"SELECT TABLE_NAME AS n, TABLE_ROWS, TABLE_TYPE "\
"FROM INFORMATION_SCHEMA.TABLES AS catalog_tables LIMIT 0;")

expect_contains "information schema alias" 'Field   1:  `n`' "$information_schema_output"
expect_contains "information schema alias table" 'Table:      `catalog_tables`' \
    "$information_schema_output"
expect_contains "information schema origin table" 'Org_table:  `TABLES`' \
    "$information_schema_output"
expect_contains "information schema name type" 'Type:       VAR_STRING' \
    "$information_schema_output"
expect_contains "information schema name length" 'Length:     256' \
    "$information_schema_output"
expect_contains "information schema row type" 'Type:       LONGLONG' \
    "$information_schema_output"
expect_contains "information schema row flags" 'Flags:      UNSIGNED BINARY NUM ' \
    "$information_schema_output"
expect_contains "information schema enum type" 'Type:       STRING' \
    "$information_schema_output"
expect_contains "information schema enum flags" \
    'Flags:      NOT_NULL MULTIPLE_KEY BINARY ENUM NO_DEFAULT_VALUE PART_KEY ' \
    "$information_schema_output"

information_schema_count_output=$(run_mysql_type_info \
    "SELECT COUNT(*) AS c FROM INFORMATION_SCHEMA.TABLES LIMIT 0;")

expect_contains "information schema count type" 'Type:       LONGLONG' \
    "$information_schema_count_output"
expect_contains "information schema count length" 'Length:     21' \
    "$information_schema_count_output"
expect_contains "information schema count flags" 'Flags:      NOT_NULL BINARY NUM ' \
    "$information_schema_count_output"

information_schema_join_output=$(run_mysql_type_info \
    "USE ${DATABASE}; SET NAMES utf8mb4; "\
"SELECT cols.DATA_TYPE, stats.INDEX_NAME, stats.COLUMN_NAME "\
"FROM INFORMATION_SCHEMA.COLUMNS AS cols "\
"JOIN INFORMATION_SCHEMA.STATISTICS AS stats "\
"ON cols.TABLE_SCHEMA = stats.TABLE_SCHEMA "\
"AND cols.TABLE_NAME = stats.TABLE_NAME "\
"AND cols.COLUMN_NAME = stats.COLUMN_NAME "\
"WHERE cols.TABLE_SCHEMA = '${DATABASE}' AND cols.TABLE_NAME = 'meta' "\
"ORDER BY INDEX_NAME ASC LIMIT 0;" \
    "$DATABASE")

expect_contains "information schema join data type field" 'Field   1:  `DATA_TYPE`' \
    "$information_schema_join_output"
expect_contains "information schema join data type" 'Type:       LONG_BLOB' \
    "$information_schema_join_output"
expect_contains "information schema join data length" 'Length:     201326580' \
    "$information_schema_join_output"
expect_contains "information schema join index field" 'Field   2:  `INDEX_NAME`' \
    "$information_schema_join_output"
expect_contains "information schema join column field" 'Field   3:  `COLUMN_NAME`' \
    "$information_schema_join_output"
expect_contains "information schema join name type" 'Type:       VAR_STRING' \
    "$information_schema_join_output"
expect_contains "information schema join name length" 'Length:     256' \
    "$information_schema_join_output"

information_schema_union_output=$(run_mysql_type_info \
    "USE ${DATABASE}; SET NAMES utf8mb4; "\
"WITH cols AS (SELECT COLUMN_NAME AS column_name FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'meta'), "\
"indexes AS (SELECT DISTINCT INDEX_NAME AS index_name FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'meta') "\
"SELECT CONCAT(column_name, ' (column)') AS name FROM cols UNION ALL "\
"SELECT CONCAT(index_name, ' (index)') AS name FROM indexes ORDER BY name;" \
    "$DATABASE")

expect_contains "information schema union name field" 'Field   1:  `name`' \
    "$information_schema_union_output"
expect_contains "information schema union name type" 'Type:       VAR_STRING' \
    "$information_schema_union_output"
expect_contains "information schema union name length" 'Length:     292' \
    "$information_schema_union_output"
expect_contains "information schema union name decimals" 'Decimals:   0' \
    "$information_schema_union_output"

information_schema_group_output=$(run_mysql_type_info \
    "USE ${DATABASE}; SET NAMES utf8mb4; "\
"SET SESSION sql_mode = REPLACE(@@SESSION.sql_mode, 'ONLY_FULL_GROUP_BY', ''); "\
"SELECT TABLE_NAME AS 'table', TABLE_ROWS AS 'rows', "\
"SUM(DATA_LENGTH + INDEX_LENGTH) AS 'bytes' FROM INFORMATION_SCHEMA.TABLES "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'meta' "\
"GROUP BY TABLE_NAME ORDER BY TABLE_NAME;" \
    "$DATABASE")

expect_contains "information schema grouped table field" 'Field   1:  `table`' \
    "$information_schema_group_output"
expect_contains "information schema grouped table type" 'Type:       VAR_STRING' \
    "$information_schema_group_output"
expect_contains "information schema grouped rows field" 'Field   2:  `rows`' \
    "$information_schema_group_output"
expect_contains "information schema grouped rows type" 'Type:       LONGLONG' \
    "$information_schema_group_output"
expect_contains "information schema grouped bytes field" 'Field   3:  `bytes`' \
    "$information_schema_group_output"
expect_contains "information schema grouped bytes type" 'Type:       NEWDECIMAL' \
    "$information_schema_group_output"
expect_contains "information schema grouped bytes length" 'Length:     45' \
    "$information_schema_group_output"
expect_contains "information schema grouped bytes flags" 'Flags:      BINARY NUM ' \
    "$information_schema_group_output"

performance_schema_output=$(run_mysql_type_info \
    "SELECT THREAD_ID FROM performance_schema.threads LIMIT 0;")

expect_contains "performance schema thread type" 'Type:       LONGLONG' \
    "$performance_schema_output"
expect_contains "performance schema thread length" 'Length:     20' \
    "$performance_schema_output"
expect_contains "performance schema thread flags" \
    'Flags:      NOT_NULL PRI_KEY UNSIGNED NO_DEFAULT_VALUE NUM PART_KEY ' \
    "$performance_schema_output"

sys_output=$(run_mysql_type_info "SELECT thd_id FROM sys.processlist LIMIT 0;")

expect_contains "sys thread type" 'Type:       LONGLONG' "$sys_output"
expect_contains "sys thread length" 'Length:     20' "$sys_output"
expect_contains "sys thread flags" 'Flags:      NOT_NULL UNSIGNED NO_DEFAULT_VALUE NUM ' \
    "$sys_output"

show_columns_output=$(run_mysql_type_info \
    "USE ${DATABASE}; SHOW COLUMNS FROM meta; SHOW INDEX FROM meta;" \
    "$DATABASE")

expect_contains "show columns type field" 'Field   2:  `Type`' "$show_columns_output"
expect_contains "show columns type" 'Type:       BLOB' "$show_columns_output"
expect_contains "show columns type length" 'Length:     67108860' "$show_columns_output"
expect_contains "show columns type flags" \
    'Flags:      NOT_NULL BLOB BINARY NO_DEFAULT_VALUE ' "$show_columns_output"
expect_contains "show index non-unique field" 'Field   2:  `Non_unique`' "$show_columns_output"
expect_contains "show index non-unique type" 'Type:       LONG' "$show_columns_output"
expect_contains "show index non-unique length" 'Length:     2' "$show_columns_output"
expect_contains "show index non-unique flags" 'Flags:      NOT_NULL NUM ' "$show_columns_output"

show_status_output=$(run_mysql_type_info \
    "USE ${DATABASE}; SHOW FULL TABLES; SHOW TABLE STATUS LIKE 'meta';" \
    "$DATABASE")

expect_contains "show full tables type field" 'Field   2:  `Table_type`' "$show_status_output"
expect_contains "show full tables type" 'Type:       STRING' "$show_status_output"
expect_contains "show full tables length" 'Length:     44' "$show_status_output"
expect_contains "show table status rows field" 'Field   5:  `Rows`' "$show_status_output"
expect_contains "show table status rows type" 'Type:       LONGLONG' "$show_status_output"
expect_contains "show table status rows flags" 'Flags:      UNSIGNED NUM ' "$show_status_output"

show_diagnostics_output=$(run_mysql_type_info \
    "SHOW PROCESSLIST; SHOW WARNINGS; SHOW COUNT(*) WARNINGS;")

expect_contains "show processlist id field" 'Field   1:  `Id`' "$show_diagnostics_output"
expect_contains "show processlist id type" 'Type:       LONGLONG' "$show_diagnostics_output"
expect_contains "show processlist id length" 'Length:     22' "$show_diagnostics_output"
expect_contains "show diagnostics code field" 'Field   2:  `Code`' "$show_diagnostics_output"
expect_contains "show diagnostics code type" 'Type:       LONG' "$show_diagnostics_output"
expect_contains "show diagnostics code length" 'Length:     5' "$show_diagnostics_output"
expect_contains "show count warnings field" 'Field   1:  `@@session.warning_count`' \
    "$show_diagnostics_output"
expect_contains "show count warnings flags" 'Flags:      UNSIGNED BINARY NUM ' \
    "$show_diagnostics_output"

show_create_output=$(run_mysql_type_info \
    "USE ${DATABASE}; SHOW VARIABLES LIKE 'autocommit'; SHOW CREATE TABLE meta;" \
    "$DATABASE")

expect_contains "show variables field" 'Field   1:  `Variable_name`' "$show_create_output"
expect_contains "show variables length" 'Length:     256' "$show_create_output"
expect_contains "show create field" 'Field   2:  `Create Table`' "$show_create_output"
expect_contains "show create length" 'Length:     4096' "$show_create_output"
expect_contains "show create decimals" 'Decimals:   31' "$show_create_output"

json_value_output=$(run_mysql_type_info \
    "USE ${DATABASE}; SET NAMES utf8mb4; "\
"SELECT JSON_VALUE('{\"a\":1}', '$.a') AS json_value;" \
    "$DATABASE")

expect_contains "json_value type" 'Type:       VAR_STRING' "$json_value_output"
expect_contains "json_value collation" 'Collation:  utf8mb4_0900_ai_ci (255)' \
    "$json_value_output"
expect_contains "json_value length" 'Length:     2048' "$json_value_output"
expect_contains "json_value flags" 'Flags:      BINARY ' "$json_value_output"

printf '%s\n' "mysql_baseline_result_column_metadata_expectations: ok"
