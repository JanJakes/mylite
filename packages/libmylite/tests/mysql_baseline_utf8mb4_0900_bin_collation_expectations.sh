#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_ARGS="--protocol=TCP -h127.0.0.1 -uroot --batch --raw --default-character-set=utf8mb4"
DATABASE="mylite_utf8mb4_0900_bin_$$"

fail() {
    printf '%s\n' "mysql_baseline_utf8mb4_0900_bin_collation: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS --skip-column-names "$@"
}

run_mysql_with_headers() {
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
run_mysql "CREATE DATABASE ${DATABASE} DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci;" \
    >/dev/null

show_collation_expected=$(printf 'utf8mb4_0900_bin\tutf8mb4\t309\t\tYes\t1\tNO PAD')
expect_value \
    "show collation utf8mb4_0900_bin" \
    "$show_collation_expected" \
    "$(run_mysql "SHOW COLLATION LIKE 'utf8mb4_0900_bin';")"

information_schema_collation_expected=$(printf 'utf8mb4_0900_bin\tutf8mb4\t309\t\tYes\t1\tNO PAD')
expect_value \
    "information schema collations utf8mb4_0900_bin" \
    "$information_schema_collation_expected" \
    "$(run_mysql "SELECT COLLATION_NAME, CHARACTER_SET_NAME, ID, IS_DEFAULT, IS_COMPILED, SORTLEN, PAD_ATTRIBUTE FROM INFORMATION_SCHEMA.COLLATIONS WHERE COLLATION_NAME = 'utf8mb4_0900_bin';")"

expect_value \
    "information schema applicability utf8mb4_0900_bin" \
    "$(printf 'utf8mb4_0900_bin\tutf8mb4')" \
    "$(run_mysql "SELECT COLLATION_NAME, CHARACTER_SET_NAME FROM INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY WHERE COLLATION_NAME = 'utf8mb4_0900_bin';")"

expect_value \
    "set names utf8mb4_0900_bin" \
    "$(printf 'utf8mb4\tutf8mb4\tutf8mb4\tutf8mb4_0900_bin\t0')" \
    "$(run_mysql "SET NAMES utf8mb4 COLLATE utf8mb4_0900_bin; SELECT @@character_set_client, @@character_set_connection, @@character_set_results, @@collation_connection, @@warning_count;")"

run_mysql \
    "USE ${DATABASE};
     CREATE TABLE metadata_bin (
       id INT,
       inherited VARCHAR(10),
       explicit_char CHAR(2) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_bin,
       explicit_text TEXT COLLATE utf8mb4_0900_bin
     ) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_bin;
     CREATE TABLE like_bin LIKE metadata_bin;
     CREATE TABLE alter_bin (id INT, v VARCHAR(10)) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
     ALTER TABLE alter_bin DEFAULT COLLATE utf8mb4_0900_bin;
     ALTER TABLE alter_bin MODIFY COLUMN v VARCHAR(12) COLLATE utf8mb4_0900_bin;" >/dev/null

expected_metadata_create=$(cat <<\EXPECTED
CREATE TABLE `metadata_bin` (
  `id` int DEFAULT NULL,
  `inherited` varchar(10) COLLATE utf8mb4_0900_bin DEFAULT NULL,
  `explicit_char` char(2) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_bin DEFAULT NULL,
  `explicit_text` text CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_bin
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_bin
EXPECTED
)
expect_show_create "metadata bin" "metadata_bin" "$expected_metadata_create"
expected_like_create=$(printf '%s\n' "$expected_metadata_create" | sed 's/`metadata_bin`/`like_bin`/')
expect_show_create "like bin" "like_bin" "$expected_like_create"

expected_alter_create=$(cat <<\EXPECTED
CREATE TABLE `alter_bin` (
  `id` int DEFAULT NULL,
  `v` varchar(12) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_bin DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_bin
EXPECTED
)
expect_show_create "alter bin" "alter_bin" "$expected_alter_create"

show_full_expected=$(cat <<\EXPECTED
id	int	NULL	YES		NULL		select,insert,update,references	
inherited	varchar(10)	utf8mb4_0900_bin	YES		NULL		select,insert,update,references	
explicit_char	char(2)	utf8mb4_0900_bin	YES		NULL		select,insert,update,references	
explicit_text	text	utf8mb4_0900_bin	YES		NULL		select,insert,update,references	
EXPECTED
)
expect_value \
    "show full columns metadata bin" \
    "$show_full_expected" \
    "$(run_mysql "USE ${DATABASE}; SHOW FULL COLUMNS FROM metadata_bin;")"

information_schema_columns_expected=$(cat <<\EXPECTED
id	NULL	NULL	int
inherited	utf8mb4	utf8mb4_0900_bin	varchar(10)
explicit_char	utf8mb4	utf8mb4_0900_bin	char(2)
explicit_text	utf8mb4	utf8mb4_0900_bin	text
EXPECTED
)
expect_value \
    "information schema columns metadata bin" \
    "$information_schema_columns_expected" \
    "$(run_mysql "SELECT COLUMN_NAME, CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='metadata_bin' ORDER BY ORDINAL_POSITION;")"

expect_error \
    "latin1 utf8mb4_0900_bin mismatch" \
    1253 \
    42000 \
    "COLLATION 'utf8mb4_0900_bin' is not valid for CHARACTER SET 'latin1'" \
    "CREATE DATABASE IF NOT EXISTS ${DATABASE}; USE ${DATABASE}; CREATE TABLE mismatch (v VARCHAR(10) CHARACTER SET latin1 COLLATE utf8mb4_0900_bin);"

printf '%s\n' "mysql_baseline_utf8mb4_0900_bin_collation: ok"
