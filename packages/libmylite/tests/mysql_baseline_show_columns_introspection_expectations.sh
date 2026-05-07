#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_show_columns_expectations_$$"
OTHER_DATABASE="${DATABASE}_other"

fail() {
    printf '%s\n' "mysql_baseline_show_columns_introspection_expectations: $1" >&2
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
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE}; DROP DATABASE IF EXISTS ${OTHER_DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup

run_mysql \
    "CREATE DATABASE ${DATABASE};
     CREATE DATABASE ${OTHER_DATABASE};
     USE ${DATABASE};
     CREATE TABLE numbers(
       id INT NOT NULL,
       i INTEGER NULL,
       iu INT UNSIGNED NULL,
       b BIGINT NULL,
       bu BIGINT UNSIGNED NULL,
       nn BIGINT UNSIGNED NOT NULL
     );" >/dev/null

expected_columns="Field	Type	Null	Key	Default	Extra"
expected_rows="id	int	NO		NULL	
i	int	YES		NULL	
iu	int unsigned	YES		NULL	
b	bigint	YES		NULL	
bu	bigint unsigned	YES		NULL	
nn	bigint unsigned	NO		NULL	"
expected_other_rows="other_id	bigint	YES		NULL	"

check_show_output() {
    label=$1
    sql=$2

    output=$(run_mysql_with_headers "$sql")
    headers=$(printf '%s\n' "$output" | sed -n '1p')
    rows=$(printf '%s\n' "$output" | sed '1d')

    expect_value "$label headers" "$expected_columns" "$headers"
    expect_value "$label rows" "$expected_rows" "$rows"
}

check_show_output "show columns from table" "USE ${DATABASE}; SHOW COLUMNS FROM numbers;"
status=$(run_mysql "USE ${DATABASE}; SHOW COLUMNS FROM numbers; SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "show columns status" "0	-1" "$status"

check_show_output "show columns in table" "USE ${DATABASE}; SHOW COLUMNS IN numbers;"
check_show_output "show fields from table" "USE ${DATABASE}; SHOW FIELDS FROM numbers;"
check_show_output "show fields in table" "USE ${DATABASE}; SHOW FIELDS IN numbers;"
check_show_output "schema-qualified show columns" "SHOW COLUMNS FROM ${DATABASE}.numbers;"
check_show_output "show columns from table from schema" "SHOW COLUMNS FROM numbers FROM ${DATABASE};"
check_show_output "show columns from table in schema" "SHOW COLUMNS FROM numbers IN ${DATABASE};"
check_show_output "show columns in table from schema" "SHOW COLUMNS IN numbers FROM ${DATABASE};"
check_show_output "show columns in table in schema" "SHOW COLUMNS IN numbers IN ${DATABASE};"
check_show_output "show fields from table from schema" "SHOW FIELDS FROM numbers FROM ${DATABASE};"
check_show_output "show fields from table in schema" "SHOW FIELDS FROM numbers IN ${DATABASE};"
check_show_output "show fields in table from schema" "SHOW FIELDS IN numbers FROM ${DATABASE};"
check_show_output "show fields in table in schema" "SHOW FIELDS IN numbers IN ${DATABASE};"
check_show_output "describe table" "USE ${DATABASE}; DESCRIBE numbers;"
check_show_output "desc table" "USE ${DATABASE}; DESC numbers;"
check_show_output "schema-qualified describe" "DESCRIBE ${DATABASE}.numbers;"
check_show_output "schema-qualified desc" "DESC ${DATABASE}.numbers;"

run_mysql "CREATE TABLE ${OTHER_DATABASE}.numbers(other_id BIGINT NULL);" >/dev/null

check_other_output() {
    label=$1
    sql=$2

    output=$(run_mysql_with_headers "$sql")
    headers=$(printf '%s\n' "$output" | sed -n '1p')
    rows=$(printf '%s\n' "$output" | sed '1d')

    expect_value "$label headers" "$expected_columns" "$headers"
    expect_value "$label rows" "$expected_other_rows" "$rows"
}

check_other_output \
    "trailing schema wins from/from" \
    "SHOW COLUMNS FROM ${DATABASE}.numbers FROM ${OTHER_DATABASE};"
check_other_output \
    "trailing schema wins from/in" \
    "SHOW COLUMNS FROM ${DATABASE}.numbers IN ${OTHER_DATABASE};"
check_other_output \
    "trailing schema wins in/from" \
    "SHOW COLUMNS IN ${DATABASE}.numbers FROM ${OTHER_DATABASE};"
check_other_output \
    "trailing schema wins in/in" \
    "SHOW COLUMNS IN ${DATABASE}.numbers IN ${OTHER_DATABASE};"
check_other_output \
    "trailing schema wins fields from/from" \
    "SHOW FIELDS FROM ${DATABASE}.numbers FROM ${OTHER_DATABASE};"
check_other_output \
    "trailing schema wins fields from/in" \
    "SHOW FIELDS FROM ${DATABASE}.numbers IN ${OTHER_DATABASE};"
check_other_output \
    "trailing schema wins fields in/from" \
    "SHOW FIELDS IN ${DATABASE}.numbers FROM ${OTHER_DATABASE};"
check_other_output \
    "trailing schema wins fields in/in" \
    "SHOW FIELDS IN ${DATABASE}.numbers IN ${OTHER_DATABASE};"

accepted_but_deferred=$(run_mysql_with_headers \
    "USE ${DATABASE};
     SHOW FULL COLUMNS FROM numbers;
     SHOW EXTENDED COLUMNS FROM numbers;
     SHOW COLUMNS FROM numbers LIKE 'i%';
     SHOW COLUMNS FROM numbers WHERE Field = 'id';
     DESCRIBE numbers id;
     DESCRIBE SELECT 1;"
)
expect_value \
    "accepted but deferred marker" \
    "Field	Type	Collation	Null	Key	Default	Extra	Privileges	Comment" \
    "$(printf '%s\n' "$accepted_but_deferred" | sed -n '1p')"
expect_value \
    "extended includes hidden row id" \
    "DB_ROW_ID" \
    "$(printf '%s\n' "$accepted_but_deferred" | awk -F '\t' '$1 == "DB_ROW_ID" { print $1; exit }')"
expect_value \
    "like filters prefix rows" \
    "id" \
    "$(printf '%s\n' "$accepted_but_deferred" | awk -F '\t' '$1 == "id" && $2 == "int" { print $1; exit }')"
expect_value \
    "describe select explain output" \
    "id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra" \
    "$(printf '%s\n' "$accepted_but_deferred" | tail -n 2 | sed -n '1p')"

expect_error \
    "missing default schema show columns" \
    1046 \
    3D000 \
    "No database selected" \
    "SHOW COLUMNS FROM numbers;"

expect_error \
    "missing default schema describe" \
    1046 \
    3D000 \
    "No database selected" \
    "DESCRIBE numbers;"

expect_error \
    "unknown schema qualified show columns" \
    1049 \
    42000 \
    "Unknown database 'missing_schema'" \
    "SHOW COLUMNS FROM missing_schema.numbers;"

expect_error \
    "unknown schema explicit show columns" \
    1049 \
    42000 \
    "Unknown database 'missing_schema'" \
    "SHOW COLUMNS FROM numbers FROM missing_schema;"

expect_error \
    "unknown table show columns" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_table' doesn't exist" \
    "USE ${DATABASE}; SHOW COLUMNS FROM missing_table;"
