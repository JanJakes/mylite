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
