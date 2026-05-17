#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_ARGS="--protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names"
DATABASE="mylite_binary_character_set_collation_expectations_$$"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_binary_character_set_collation_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS "$@"
}

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

expect_output() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    expect_value "$label" "$expected" "$output"
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
        *) fail "$label: expected error $code/$state containing [$message], got [$output]" ;;
    esac
}

normalize_tsv() {
    sed "s/${TAB}/|/g"
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT HUP INT TERM

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

expect_output \
    "show binary character set" \
    "binary${TAB}Binary pseudo charset${TAB}binary${TAB}1" \
    "SHOW CHARACTER SET LIKE 'binary';" \
    "$DATABASE"

expect_output \
    "show binary collation" \
    "binary${TAB}binary${TAB}63${TAB}Yes${TAB}Yes${TAB}1${TAB}NO PAD" \
    "SHOW COLLATION LIKE 'binary';" \
    "$DATABASE"

metadata_rows=$(
    run_mysql "
        SELECT CHARACTER_SET_NAME, DEFAULT_COLLATE_NAME, DESCRIPTION, MAXLEN
          FROM INFORMATION_SCHEMA.CHARACTER_SETS
         WHERE CHARACTER_SET_NAME = 'binary';
        SELECT COLLATION_NAME, CHARACTER_SET_NAME, ID, IS_DEFAULT, IS_COMPILED, SORTLEN, PAD_ATTRIBUTE
          FROM INFORMATION_SCHEMA.COLLATIONS
         WHERE COLLATION_NAME = 'binary';
        SELECT COLLATION_NAME, CHARACTER_SET_NAME
          FROM INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY
         WHERE COLLATION_NAME = 'binary';" "$DATABASE" | normalize_tsv
)
expect_value \
    "binary information_schema metadata" \
    "binary|binary|Binary pseudo charset|1
binary|binary|63|Yes|Yes|1|NO PAD
binary|binary" \
    "$metadata_rows"

run_mysql "
    CREATE TABLE explicit_binary (
        v VARCHAR(10) CHARACTER SET binary,
        c CHAR(3) COLLATE binary,
        t TINYTEXT CHARACTER SET binary,
        b TEXT COLLATE binary,
        m MEDIUMTEXT CHARACTER SET binary,
        l LONGTEXT COLLATE binary,
        z VARCHAR(0) CHARACTER SET binary,
        cz CHAR(0) COLLATE binary
    );" "$DATABASE" >/dev/null

show_create=$(run_mysql "SHOW CREATE TABLE explicit_binary;" "$DATABASE")
expected_show_create="explicit_binary${TAB}CREATE TABLE \`explicit_binary\` (
  \`v\` varbinary(10) DEFAULT NULL,
  \`c\` binary(3) DEFAULT NULL,
  \`t\` tinyblob,
  \`b\` blob,
  \`m\` mediumblob,
  \`l\` longblob,
  \`z\` varbinary(0) DEFAULT NULL,
  \`cz\` binary(0) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci"
expect_value "explicit binary show create" "$expected_show_create" "$show_create"

column_rows=$(
    run_mysql "
        SELECT COLUMN_NAME, DATA_TYPE, COLUMN_TYPE,
               IFNULL(CHARACTER_SET_NAME, 'NULL'), IFNULL(COLLATION_NAME, 'NULL'),
               CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH
          FROM INFORMATION_SCHEMA.COLUMNS
         WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'explicit_binary'
         ORDER BY ORDINAL_POSITION;" "$DATABASE" | normalize_tsv
)
expect_value \
    "explicit binary information_schema columns" \
    "v|varbinary|varbinary(10)|NULL|NULL|10|10
c|binary|binary(3)|NULL|NULL|3|3
t|tinyblob|tinyblob|NULL|NULL|255|255
b|blob|blob|NULL|NULL|65535|65535
m|mediumblob|mediumblob|NULL|NULL|16777215|16777215
l|longblob|longblob|NULL|NULL|4294967295|4294967295
z|varbinary|varbinary(0)|NULL|NULL|0|0
cz|binary|binary(0)|NULL|NULL|0|0" \
    "$column_rows"

run_mysql "
    CREATE TABLE alter_binary(id INT, v VARCHAR(10), c CHAR(3), txt TEXT);
    INSERT INTO alter_binary VALUES (1, 'ab', 'xy', 'hello'), (2, '', 'z', '');
    ALTER TABLE alter_binary ADD added VARCHAR(2) COLLATE binary;
    ALTER TABLE alter_binary MODIFY c CHAR(4) CHARACTER SET binary;
    ALTER TABLE alter_binary CHANGE v renamed VARCHAR(10) COLLATE binary;
    ALTER TABLE alter_binary MODIFY txt TEXT CHARACTER SET binary;" "$DATABASE" >/dev/null
alter_show_create=$(run_mysql "SHOW CREATE TABLE alter_binary;" "$DATABASE")
expected_alter_show_create="alter_binary${TAB}CREATE TABLE \`alter_binary\` (
  \`id\` int DEFAULT NULL,
  \`renamed\` varbinary(10) DEFAULT NULL,
  \`c\` binary(4) DEFAULT NULL,
  \`txt\` blob,
  \`added\` varbinary(2) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci"
expect_value "alter binary show create" "$expected_alter_show_create" "$alter_show_create"

alter_values=$(
    run_mysql "
        SELECT id, HEX(renamed), LENGTH(renamed), HEX(c), LENGTH(c),
               HEX(txt), LENGTH(txt), IFNULL(HEX(added), 'NULL')
          FROM alter_binary
         ORDER BY id;" "$DATABASE" | normalize_tsv
)
expect_value \
    "alter binary stored values" \
    "1|6162|2|78790000|4|68656C6C6F|5|NULL
2||0|7A000000|4||0|NULL" \
    "$alter_values"

alter_column_rows=$(
    run_mysql "
        SELECT COLUMN_NAME, DATA_TYPE, COLUMN_TYPE,
               IFNULL(CHARACTER_SET_NAME, 'NULL'), IFNULL(COLLATION_NAME, 'NULL'),
               CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH
          FROM INFORMATION_SCHEMA.COLUMNS
         WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'alter_binary'
         ORDER BY ORDINAL_POSITION;" "$DATABASE" | normalize_tsv
)
expect_value \
    "alter binary information_schema columns" \
    "id|int|int|NULL|NULL|NULL|NULL
renamed|varbinary|varbinary(10)|NULL|NULL|10|10
c|binary|binary(4)|NULL|NULL|4|4
txt|blob|blob|NULL|NULL|65535|65535
added|varbinary|varbinary(2)|NULL|NULL|2|2" \
    "$alter_column_rows"

run_mysql "
    CREATE TABLE enum_set_binary (
        e ENUM('a','b') CHARACTER SET binary,
        s SET('a','b') COLLATE binary
    );" "$DATABASE" >/dev/null
enum_set_rows=$(
    run_mysql "
        SHOW CREATE TABLE enum_set_binary;
        SELECT COLUMN_NAME, DATA_TYPE, COLUMN_TYPE,
               IFNULL(CHARACTER_SET_NAME, 'NULL'), IFNULL(COLLATION_NAME, 'NULL')
          FROM INFORMATION_SCHEMA.COLUMNS
         WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'enum_set_binary'
         ORDER BY ORDINAL_POSITION;" "$DATABASE" | normalize_tsv
)
case "$enum_set_rows" in
    *"enum('a','b') CHARACTER SET binary COLLATE binary"*\
*"e|enum|enum('a','b')|NULL|NULL"*\
*"s|set|set('a','b')|NULL|NULL"*) ;;
    *) fail "enum/set binary follow-up evidence mismatch: [$enum_set_rows]" ;;
esac

run_mysql "CREATE TABLE table_default_binary (v VARCHAR(10), c CHAR(2), t TEXT) CHARACTER SET binary;" \
    "$DATABASE" >/dev/null
table_default_show=$(run_mysql "SHOW CREATE TABLE table_default_binary;" "$DATABASE")
case "$table_default_show" in
    *"\`v\` varbinary(10) DEFAULT NULL"*\
*"\`c\` binary(2) DEFAULT NULL"*\
*"\`t\` blob"*\
*") ENGINE=InnoDB DEFAULT CHARSET=binary"*) ;;
    *) fail "table default binary follow-up evidence mismatch: [$table_default_show]" ;;
esac

expect_error \
    "utf8mb4 charset with binary collation" \
    1253 \
    42000 \
    "COLLATION 'binary' is not valid for CHARACTER SET 'utf8mb4'" \
    "CREATE TABLE bad_utf8_binary (v VARCHAR(10) CHARACTER SET utf8mb4 COLLATE binary);" \
    "$DATABASE"

expect_error \
    "binary charset with utf8mb4 collation" \
    1253 \
    42000 \
    "COLLATION 'utf8mb4_bin' is not valid for CHARACTER SET 'binary'" \
    "CREATE TABLE bad_binary_utf8 (v VARCHAR(10) CHARACTER SET binary COLLATE utf8mb4_bin);" \
    "$DATABASE"

cleanup
