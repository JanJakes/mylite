#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_table_default_binary_charset_$$"

fail() {
    printf '%s\n' "mysql_baseline_table_default_binary_charset: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw "$@"
}

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
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
        *) fail "$label: expected error $code/$state containing [$message], got [$output]" ;;
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

expect_show_create() {
    show_label=$1
    show_table=$2
    expected_create=$3

    show_output=$(run_mysql_with_headers "USE ${DATABASE}; SHOW CREATE TABLE ${show_table};")
    show_headers=$(printf '%s\n' "$show_output" | sed -n '1p')
    show_table_name=$(printf '%s\n' "$show_output" | sed -n '2p' | cut -f 1)
    show_create_text=$(printf '%s\n' "$show_output" | sed -n '2,$p' | cut -f 2-)

    expect_value "$show_label headers" "Table	Create Table" "$show_headers"
    expect_value "$show_label table" "$show_table" "$show_table_name"
    expect_value "$show_label create" "$expected_create" "$show_create_text"
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
run_mysql "CREATE DATABASE ${DATABASE} DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci;" >/dev/null

run_mysql \
    "USE ${DATABASE};
     CREATE TABLE binary_default(
       id INT,
       v VARCHAR(10),
       c CHAR(3),
       txt TEXT,
       tiny TINYTEXT,
       med MEDIUMTEXT,
       lon LONGTEXT
     ) DEFAULT CHARSET=binary;
     CREATE TABLE collate_binary(v VARCHAR(5), txt TEXT) COLLATE=binary;
     CREATE TABLE both_binary(v VARCHAR(5)) DEFAULT CHARACTER SET binary COLLATE binary;
     CREATE TABLE binary_override(
       v VARCHAR(10) CHARACTER SET utf8mb4,
       c CHAR(2) COLLATE utf8mb4_bin,
       txt TEXT
     ) DEFAULT CHARSET=binary;
     CREATE TABLE enum_set_binary(e ENUM('a','b'), s SET('x','y')) DEFAULT CHARSET=binary;
     CREATE TABLE like_binary LIKE binary_default;
     INSERT INTO binary_default VALUES(1, 'ab', 'xy', 'hello', 'ti', 'med', 'long');" >/dev/null

expected_binary_default=$(cat <<\EXPECTED
CREATE TABLE `binary_default` (
  `id` int DEFAULT NULL,
  `v` varbinary(10) DEFAULT NULL,
  `c` binary(3) DEFAULT NULL,
  `txt` blob,
  `tiny` tinyblob,
  `med` mediumblob,
  `lon` longblob
) ENGINE=InnoDB DEFAULT CHARSET=binary
EXPECTED
)
expect_show_create "binary default" "binary_default" "$expected_binary_default"

expected_collate_binary=$(cat <<\EXPECTED
CREATE TABLE `collate_binary` (
  `v` varbinary(5) DEFAULT NULL,
  `txt` blob
) ENGINE=InnoDB DEFAULT CHARSET=binary
EXPECTED
)
expect_show_create "collate binary" "collate_binary" "$expected_collate_binary"

expected_both_binary=$(cat <<\EXPECTED
CREATE TABLE `both_binary` (
  `v` varbinary(5) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=binary
EXPECTED
)
expect_show_create "both binary" "both_binary" "$expected_both_binary"

expected_binary_override=$(cat <<\EXPECTED
CREATE TABLE `binary_override` (
  `v` varchar(10) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci DEFAULT NULL,
  `c` char(2) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin DEFAULT NULL,
  `txt` blob
) ENGINE=InnoDB DEFAULT CHARSET=binary
EXPECTED
)
expect_show_create "binary override" "binary_override" "$expected_binary_override"

expected_enum_set=$(cat <<\EXPECTED
CREATE TABLE `enum_set_binary` (
  `e` enum('a','b') DEFAULT NULL,
  `s` set('x','y') DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=binary
EXPECTED
)
expect_show_create "enum set binary" "enum_set_binary" "$expected_enum_set"

expected_temp=$(cat <<\EXPECTED
CREATE TEMPORARY TABLE `temp_binary` (
  `v` varbinary(3) DEFAULT NULL,
  `c` binary(2) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=binary
EXPECTED
)
temp_output=$(run_mysql_with_headers "USE ${DATABASE}; CREATE TEMPORARY TABLE temp_binary(v VARCHAR(3), c CHAR(2)) DEFAULT CHARSET=binary; SHOW CREATE TABLE temp_binary;")
temp_headers=$(printf '%s\n' "$temp_output" | sed -n '1p')
temp_table_name=$(printf '%s\n' "$temp_output" | sed -n '2p' | cut -f 1)
temp_create_text=$(printf '%s\n' "$temp_output" | sed -n '2,$p' | cut -f 2-)
expect_value "temporary binary headers" "Table	Create Table" "$temp_headers"
expect_value "temporary binary table" "temp_binary" "$temp_table_name"
expect_value "temporary binary create" "$expected_temp" "$temp_create_text"

expected_like=$(cat <<\EXPECTED
CREATE TABLE `like_binary` (
  `id` int DEFAULT NULL,
  `v` varbinary(10) DEFAULT NULL,
  `c` binary(3) DEFAULT NULL,
  `txt` blob,
  `tiny` tinyblob,
  `med` mediumblob,
  `lon` longblob
) ENGINE=InnoDB DEFAULT CHARSET=binary
EXPECTED
)
expect_show_create "like binary" "like_binary" "$expected_like"

information_schema_expected=$(cat <<\EXPECTED
id	int	int	NULL	NULL	NULL	NULL
v	varbinary	varbinary(10)	NULL	NULL	10	10
c	binary	binary(3)	NULL	NULL	3	3
txt	blob	blob	NULL	NULL	65535	65535
tiny	tinyblob	tinyblob	NULL	NULL	255	255
med	mediumblob	mediumblob	NULL	NULL	16777215	16777215
lon	longblob	longblob	NULL	NULL	4294967295	4294967295
EXPECTED
)
expect_value \
    "binary information_schema columns" \
    "$information_schema_expected" \
    "$(run_mysql "SELECT COLUMN_NAME, DATA_TYPE, COLUMN_TYPE, CHARACTER_SET_NAME, COLLATION_NAME, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='binary_default' ORDER BY ORDINAL_POSITION;")"

expect_value \
    "binary table collation" \
    "binary" \
    "$(run_mysql "SELECT TABLE_COLLATION FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='binary_default';")"

expect_value \
    "binary stored values" \
    "6162	2	787900	3	68656C6C6F	5	7469	6D6564	6C6F6E67" \
    "$(run_mysql "USE ${DATABASE}; SELECT HEX(v), LENGTH(v), HEX(c), LENGTH(c), HEX(txt), LENGTH(txt), HEX(tiny), HEX(med), HEX(lon) FROM binary_default;")"

override_information_schema=$(cat <<\EXPECTED
v	varchar	utf8mb4	utf8mb4_0900_ai_ci	varchar(10)
c	char	utf8mb4	utf8mb4_bin	char(2)
txt	blob	NULL	NULL	blob
EXPECTED
)
expect_value \
    "override information_schema columns" \
    "$override_information_schema" \
    "$(run_mysql "SELECT COLUMN_NAME, DATA_TYPE, CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='binary_override' ORDER BY ORDINAL_POSITION;")"

expect_error \
    "binary charset utf8 collation mismatch" \
    1253 \
    42000 \
    "COLLATION 'utf8mb4_bin' is not valid for CHARACTER SET 'binary'" \
    "CREATE DATABASE IF NOT EXISTS ${DATABASE}; USE ${DATABASE}; CREATE TABLE binary_utf8_mismatch(id INT) DEFAULT CHARSET=binary COLLATE=utf8mb4_bin;"

expect_error \
    "utf8 charset binary collation mismatch" \
    1253 \
    42000 \
    "COLLATION 'binary' is not valid for CHARACTER SET 'utf8mb4'" \
    "CREATE DATABASE IF NOT EXISTS ${DATABASE}; USE ${DATABASE}; CREATE TABLE utf8_binary_mismatch(id INT) DEFAULT CHARSET=utf8mb4 COLLATE=binary;"

expect_upstream_accepts \
    "mysql accepts deferred binary default value" \
    "USE ${DATABASE}; CREATE TABLE deferred_binary_default(v VARCHAR(10) DEFAULT 'ab') DEFAULT CHARSET=binary; SHOW CREATE TABLE deferred_binary_default;"

expect_upstream_accepts \
    "mysql accepts deferred binary indexed column" \
    "USE ${DATABASE}; CREATE TABLE deferred_binary_key(v VARCHAR(10), KEY v_idx(v)) DEFAULT CHARSET=binary; SHOW CREATE TABLE deferred_binary_key;"

expect_upstream_accepts \
    "mysql accepts deferred alter binary table default" \
    "USE ${DATABASE}; CREATE TABLE deferred_alter_binary(v VARCHAR(10)); ALTER TABLE deferred_alter_binary DEFAULT CHARSET=binary; SHOW CREATE TABLE deferred_alter_binary;"
