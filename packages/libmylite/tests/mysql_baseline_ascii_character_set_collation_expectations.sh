#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_ascii_charset_collation_$$"

fail() {
    printf '%s\n' "mysql_baseline_ascii_character_set_collation_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --default-character-set=utf8mb4 "$@"
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

expect_show_create() {
    label=$1
    table_name=$2
    expected_create=$3

    show_output=$(run_mysql_with_headers "USE ${DATABASE}; SHOW CREATE TABLE ${table_name};")
    show_headers=$(printf '%s\n' "$show_output" | sed -n '1p')
    show_table_name=$(printf '%s\n' "$show_output" | sed -n '2p' | cut -f 1)
    show_create_text=$(printf '%s\n' "$show_output" | sed -n '2,$p' | cut -f 2-)

    expect_value "$label headers" "Table	Create Table" "$show_headers"
    expect_value "$label table" "$table_name" "$show_table_name"
    expect_value "$label create" "$expected_create" "$show_create_text"
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

expect_value \
    "show ascii charset" \
    "ascii	US ASCII	ascii_general_ci	1" \
    "$(run_mysql "SHOW CHARACTER SET LIKE 'ascii';")"

ascii_collations_expected=$(cat <<\EXPECTED
ascii_bin	ascii	65		Yes	1	PAD SPACE
ascii_general_ci	ascii	11	Yes	Yes	1	PAD SPACE
EXPECTED
)
expect_value \
    "show ascii collations" \
    "$ascii_collations_expected" \
    "$(run_mysql "SHOW COLLATION LIKE 'ascii%';")"

expect_value \
    "information schema character set" \
    "ascii	ascii_general_ci	US ASCII	1" \
    "$(run_mysql "SELECT CHARACTER_SET_NAME, DEFAULT_COLLATE_NAME, DESCRIPTION, MAXLEN FROM INFORMATION_SCHEMA.CHARACTER_SETS WHERE CHARACTER_SET_NAME='ascii';")"

expect_value \
    "information schema collations" \
    "$ascii_collations_expected" \
    "$(run_mysql "SELECT COLLATION_NAME, CHARACTER_SET_NAME, ID, IS_DEFAULT, IS_COMPILED, SORTLEN, PAD_ATTRIBUTE FROM INFORMATION_SCHEMA.COLLATIONS WHERE CHARACTER_SET_NAME='ascii' ORDER BY COLLATION_NAME;")"

run_mysql \
    "ALTER DATABASE ${DATABASE} DEFAULT CHARACTER SET ascii COLLATE ascii_bin;
     USE ${DATABASE};
     CREATE TABLE inherited_ascii (
       v VARCHAR(10),
       c CHAR(5),
       t TEXT
     );
     CREATE TABLE table_charset_only (v VARCHAR(10)) DEFAULT CHARSET=ascii;
     CREATE TABLE table_collate_only (v VARCHAR(10)) DEFAULT COLLATE=ascii_bin;
     CREATE TABLE explicit_attrs (
       id INT,
       v VARCHAR(10) CHARACTER SET ascii,
       c CHAR(5) CHARACTER SET ascii COLLATE ascii_bin,
       t TEXT COLLATE ascii_general_ci,
       vc VARCHAR(10) COLLATE ascii_bin
     ) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
     CREATE TABLE like_explicit LIKE explicit_attrs;
     CREATE TABLE uppercase_attrs (v VARCHAR(10) CHARACTER SET ASCII COLLATE ASCII_GENERAL_CI);" >/dev/null

expect_value \
    "schema defaults" \
    "ascii	ascii_bin" \
    "$(run_mysql "SELECT DEFAULT_CHARACTER_SET_NAME, DEFAULT_COLLATION_NAME FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='${DATABASE}';")"

expected_inherited=$(cat <<\EXPECTED
CREATE TABLE `inherited_ascii` (
  `v` varchar(10) COLLATE ascii_bin DEFAULT NULL,
  `c` char(5) COLLATE ascii_bin DEFAULT NULL,
  `t` text COLLATE ascii_bin
) ENGINE=InnoDB DEFAULT CHARSET=ascii COLLATE=ascii_bin
EXPECTED
)
expect_show_create "inherited ascii" "inherited_ascii" "$expected_inherited"

expected_table_charset_only=$(cat <<\EXPECTED
CREATE TABLE `table_charset_only` (
  `v` varchar(10) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=ascii
EXPECTED
)
expect_show_create "table charset only" "table_charset_only" "$expected_table_charset_only"

expected_table_collate_only=$(cat <<\EXPECTED
CREATE TABLE `table_collate_only` (
  `v` varchar(10) COLLATE ascii_bin DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=ascii COLLATE=ascii_bin
EXPECTED
)
expect_show_create "table collate only" "table_collate_only" "$expected_table_collate_only"

expected_explicit=$(cat <<\EXPECTED
CREATE TABLE `explicit_attrs` (
  `id` int DEFAULT NULL,
  `v` varchar(10) CHARACTER SET ascii COLLATE ascii_general_ci DEFAULT NULL,
  `c` char(5) CHARACTER SET ascii COLLATE ascii_bin DEFAULT NULL,
  `t` text CHARACTER SET ascii COLLATE ascii_general_ci,
  `vc` varchar(10) CHARACTER SET ascii COLLATE ascii_bin DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_show_create "explicit attrs" "explicit_attrs" "$expected_explicit"

expected_like=$(cat <<\EXPECTED
CREATE TABLE `like_explicit` (
  `id` int DEFAULT NULL,
  `v` varchar(10) CHARACTER SET ascii COLLATE ascii_general_ci DEFAULT NULL,
  `c` char(5) CHARACTER SET ascii COLLATE ascii_bin DEFAULT NULL,
  `t` text CHARACTER SET ascii COLLATE ascii_general_ci,
  `vc` varchar(10) CHARACTER SET ascii COLLATE ascii_bin DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_show_create "like explicit" "like_explicit" "$expected_like"

expected_uppercase=$(cat <<\EXPECTED
CREATE TABLE `uppercase_attrs` (
  `v` varchar(10) CHARACTER SET ascii COLLATE ascii_general_ci DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=ascii COLLATE=ascii_bin
EXPECTED
)
expect_show_create "uppercase attrs" "uppercase_attrs" "$expected_uppercase"

show_full_expected=$(cat <<\EXPECTED
id	int	NULL	YES		NULL		select,insert,update,references	
v	varchar(10)	ascii_general_ci	YES		NULL		select,insert,update,references	
c	char(5)	ascii_bin	YES		NULL		select,insert,update,references	
t	text	ascii_general_ci	YES		NULL		select,insert,update,references	
vc	varchar(10)	ascii_bin	YES		NULL		select,insert,update,references	
EXPECTED
)
expect_value \
    "show full explicit attrs" \
    "$show_full_expected" \
    "$(run_mysql "USE ${DATABASE}; SHOW FULL COLUMNS FROM explicit_attrs;")"

information_schema_columns_expected=$(cat <<\EXPECTED
id	int	NULL	NULL	NULL	NULL	int
v	varchar	ascii	ascii_general_ci	10	10	varchar(10)
c	char	ascii	ascii_bin	5	5	char(5)
t	text	ascii	ascii_general_ci	65535	65535	text
vc	varchar	ascii	ascii_bin	10	10	varchar(10)
EXPECTED
)
expect_value \
    "information schema explicit columns" \
    "$information_schema_columns_expected" \
    "$(run_mysql "SELECT COLUMN_NAME, DATA_TYPE, CHARACTER_SET_NAME, COLLATION_NAME, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, COLUMN_TYPE FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='explicit_attrs' ORDER BY ORDINAL_POSITION;")"

expect_error \
    "ascii charset utf8mb4 collation mismatch" \
    1253 \
    42000 \
    "COLLATION 'utf8mb4_bin' is not valid for CHARACTER SET 'ascii'" \
    "USE ${DATABASE}; CREATE TABLE bad1(v VARCHAR(10) CHARACTER SET ascii COLLATE utf8mb4_bin);"

expect_error \
    "utf8mb4 charset ascii collation mismatch" \
    1253 \
    42000 \
    "COLLATION 'ascii_bin' is not valid for CHARACTER SET 'utf8mb4'" \
    "USE ${DATABASE}; CREATE TABLE bad2(v VARCHAR(10) CHARACTER SET utf8mb4 COLLATE ascii_bin);"

expect_error \
    "unknown charset" \
    1115 \
    42000 \
    "Unknown character set: 'nosuch_charset'" \
    "USE ${DATABASE}; CREATE TABLE bad3(v VARCHAR(10) CHARACTER SET nosuch_charset);"

expect_error \
    "unknown collation" \
    1273 \
    HY000 \
    "Unknown collation: 'ascii_nosuch_ci'" \
    "USE ${DATABASE}; CREATE TABLE bad4(v VARCHAR(10) COLLATE ascii_nosuch_ci);"

shorthand_expected=$(cat <<\EXPECTED
CREATE TABLE `shorthand_ascii` (
  `v` varchar(10) CHARACTER SET latin1 COLLATE latin1_swedish_ci DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=ascii COLLATE=ascii_bin
EXPECTED
)
run_mysql "USE ${DATABASE}; CREATE TABLE shorthand_ascii (v VARCHAR(10) ASCII);" >/dev/null
expect_show_create "deferred ASCII shorthand maps to latin1" "shorthand_ascii" "$shorthand_expected"

printf '%s\n' "mysql_baseline_ascii_character_set_collation_expectations: ok"
