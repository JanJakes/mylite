#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_show_create_$$"
OTHER_DATABASE="${DATABASE}_other"

fail() {
    printf '%s\n' "mysql_baseline_show_create_table_expectations: $1" >&2
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

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE}; DROP DATABASE IF EXISTS ${OTHER_DATABASE};" \
        >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

quote_setting=$(run_mysql 'SELECT @@sql_quote_show_create;')
expect_value "default sql_quote_show_create" "1" "$quote_setting"

cleanup

run_mysql \
    "CREATE DATABASE ${DATABASE};
     CREATE DATABASE ${OTHER_DATABASE};
     USE ${DATABASE};
     CREATE TABLE numbers(
       id INT NOT NULL,
       i INTEGER NULL,
       iu INT UNSIGNED NULL,
       iuu INTEGER UNSIGNED NULL,
       b BIGINT NULL,
       bu BIGINT UNSIGNED NULL,
       nn BIGINT UNSIGNED NOT NULL
     );
     CREATE TABLE null_forms(a INT, b INT NULL, c INT NOT NULL);
     CREATE TABLE \`a\`\`b\`(\`x\`\`y\` INT NULL);
     CREATE TABLE ${OTHER_DATABASE}.numbers(other_id BIGINT NULL);
     CREATE VIEW number_view AS SELECT id FROM numbers;" >/dev/null

expected_numbers="CREATE TABLE \`numbers\` (
  \`id\` int NOT NULL,
  \`i\` int DEFAULT NULL,
  \`iu\` int unsigned DEFAULT NULL,
  \`iuu\` int unsigned DEFAULT NULL,
  \`b\` bigint DEFAULT NULL,
  \`bu\` bigint unsigned DEFAULT NULL,
  \`nn\` bigint unsigned NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci"

expected_other="CREATE TABLE \`numbers\` (
  \`other_id\` bigint DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci"

expected_null_forms="CREATE TABLE \`null_forms\` (
  \`a\` int DEFAULT NULL,
  \`b\` int DEFAULT NULL,
  \`c\` int NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci"

expected_quoted="CREATE TABLE \`a\`\`b\` (
  \`x\`\`y\` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci"

check_show_create() {
    label=$1
    sql=$2
    expected_table=$3
    expected_create=$4

    output=$(run_mysql_with_headers "$sql")
    headers=$(printf '%s\n' "$output" | sed -n '1p')
    table_name=$(printf '%s\n' "$output" | sed -n '2p' | cut -f 1)
    create_text=$(printf '%s\n' "$output" | sed -n '2,$p' | cut -f 2-)

    expect_value "$label headers" "Table	Create Table" "$headers"
    expect_value "$label table" "$expected_table" "$table_name"
    expect_value "$label create" "$expected_create" "$create_text"
}

check_show_create \
    "show create table" \
    "USE ${DATABASE}; SHOW CREATE TABLE numbers;" \
    "numbers" \
    "$expected_numbers"
check_show_create \
    "schema-qualified show create table" \
    "SHOW CREATE TABLE ${DATABASE}.numbers;" \
    "numbers" \
    "$expected_numbers"
check_show_create \
    "other schema show create table" \
    "SHOW CREATE TABLE ${OTHER_DATABASE}.numbers;" \
    "numbers" \
    "$expected_other"
check_show_create \
    "null forms" \
    "USE ${DATABASE}; SHOW CREATE TABLE null_forms;" \
    "null_forms" \
    "$expected_null_forms"
check_show_create \
    "quoted identifiers" \
    "USE ${DATABASE}; SHOW CREATE TABLE \`a\`\`b\`;" \
    'a`b' \
    "$expected_quoted"

status=$(run_mysql "USE ${DATABASE}; SHOW CREATE TABLE numbers; SELECT @@warning_count, ROW_COUNT();" \
    | tail -n 1)
expect_value "show create status" "0	-1" "$status"

view_output=$(run_mysql_with_headers "USE ${DATABASE}; SHOW CREATE TABLE number_view;")
expect_value \
    "mysql view header through show create table" \
    "View	Create View	character_set_client	collation_connection" \
    "$(printf '%s\n' "$view_output" | sed -n '1p')"

expect_error \
    "missing default schema" \
    1046 \
    3D000 \
    "No database selected" \
    "SHOW CREATE TABLE numbers;"

expect_error \
    "unknown schema" \
    1049 \
    42000 \
    "Unknown database 'missing_schema'" \
    "SHOW CREATE TABLE missing_schema.numbers;"

expect_error \
    "unknown table unqualified" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_table' doesn't exist" \
    "USE ${DATABASE}; SHOW CREATE TABLE missing_table;"

expect_error \
    "unknown table qualified" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_table' doesn't exist" \
    "SHOW CREATE TABLE ${DATABASE}.missing_table;"

expect_error \
    "like modifier syntax" \
    1064 \
    42000 \
    "near 'LIKE 'numbers''" \
    "USE ${DATABASE}; SHOW CREATE TABLE numbers LIKE 'numbers';"

expect_error \
    "where modifier syntax" \
    1064 \
    42000 \
    "near 'WHERE Table = 'numbers''" \
    "USE ${DATABASE}; SHOW CREATE TABLE numbers WHERE Table = 'numbers';"

expect_error \
    "from schema syntax" \
    1064 \
    42000 \
    "near 'FROM ${DATABASE}'" \
    "USE ${DATABASE}; SHOW CREATE TABLE numbers FROM ${DATABASE};"

expect_error \
    "temporary modifier syntax" \
    1064 \
    42000 \
    "near 'TEMPORARY TABLE numbers'" \
    "USE ${DATABASE}; SHOW CREATE TEMPORARY TABLE numbers;"

expect_error \
    "full modifier syntax" \
    1064 \
    42000 \
    "near 'CREATE TABLE numbers'" \
    "USE ${DATABASE}; SHOW FULL CREATE TABLE numbers;"
