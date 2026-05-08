#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_engine_surface_$$"

fail() {
    printf '%s\n' "mysql_baseline_innodb_engine_surface_expectations: $1" >&2
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

expect_show_create() {
    show_label=$1
    table=$2
    expected_create=$3

    output=$(run_mysql_with_headers "USE ${DATABASE}; SHOW CREATE TABLE ${table};")
    headers=$(printf '%s\n' "$output" | sed -n '1p')
    table_name=$(printf '%s\n' "$output" | sed -n '2p' | cut -f 1)
    create_text=$(printf '%s\n' "$output" | sed -n '2,$p' | cut -f 2-)

    expect_value "$show_label headers" "Table	Create Table" "$headers"
    expect_value "$show_label table" "$table" "$table_name"
    expect_value "$show_label create" "$expected_create" "$create_text"
}

expect_engines_result() {
    engines_label=$1
    sql_text=$2

    output=$(run_mysql_with_headers "$sql_text")
    headers=$(printf '%s\n' "$output" | sed -n '1p')
    innodb_row=$(printf '%s\n' "$output" | awk -F '\t' '$1 == "InnoDB" {print}')

    expect_value "$engines_label headers" "Engine	Support	Comment	Transactions	XA	Savepoints" "$headers"
    expect_value \
        "$engines_label innodb row" \
        "InnoDB	DEFAULT	Supports transactions, row-level locking, and foreign keys	YES	YES	YES" \
        "$innodb_row"
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup

expect_engines_result "show engines" "SHOW ENGINES;"
expect_engines_result "show storage engines" "SHOW STORAGE ENGINES;"

status=$(run_mysql "SHOW ENGINES; SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "show engines status" "0	-1" "$status"

run_mysql \
    "CREATE DATABASE ${DATABASE};
     USE ${DATABASE};
     CREATE TABLE no_engine(id INT);
     CREATE TABLE explicit_equal(id INT) ENGINE=InnoDB;
     CREATE TABLE explicit_space(id INT) ENGINE InnoDB;
     CREATE TABLE lower_name(id INT) ENGINE=innodb;
     CREATE TABLE string_name(id INT) ENGINE='InnoDB';
     CREATE TABLE double_string_name(id INT) ENGINE=\"InnoDB\";
     CREATE TABLE quoted_name(id INT) ENGINE=\`InnoDB\`;
     INSERT INTO explicit_equal VALUES (1);" >/dev/null

expected_no_engine="CREATE TABLE \`no_engine\` (
  \`id\` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci"

expected_explicit_equal="CREATE TABLE \`explicit_equal\` (
  \`id\` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci"

expected_explicit_space="CREATE TABLE \`explicit_space\` (
  \`id\` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci"

expected_lower_name="CREATE TABLE \`lower_name\` (
  \`id\` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci"

expected_string_name="CREATE TABLE \`string_name\` (
  \`id\` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci"

expected_double_string_name="CREATE TABLE \`double_string_name\` (
  \`id\` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci"

expected_quoted_name="CREATE TABLE \`quoted_name\` (
  \`id\` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci"

expect_show_create "no engine" "no_engine" "$expected_no_engine"
expect_show_create "explicit equal" "explicit_equal" "$expected_explicit_equal"
expect_show_create "explicit space" "explicit_space" "$expected_explicit_space"
expect_show_create "lower name" "lower_name" "$expected_lower_name"
expect_show_create "string name" "string_name" "$expected_string_name"
expect_show_create "double string name" "double_string_name" "$expected_double_string_name"
expect_show_create "quoted name" "quoted_name" "$expected_quoted_name"

rows=$(run_mysql "USE ${DATABASE}; SELECT * FROM explicit_equal;")
expect_value "explicit innodb row storage" "1" "$rows"

create_status=$(run_mysql "USE ${DATABASE}; CREATE TABLE status_table(id INT) ENGINE=InnoDB; SELECT @@warning_count, ROW_COUNT();")
expect_value "create explicit innodb status" "0	0" "$(printf '%s\n' "$create_status" | tail -n 1)"

expect_error \
    "unknown engine" \
    1286 \
    42000 \
    "Unknown storage engine 'NoSuchEngine'" \
    "CREATE DATABASE IF NOT EXISTS ${DATABASE}; USE ${DATABASE}; CREATE TABLE unknown_engine(id INT) ENGINE=NoSuchEngine;"

expect_error \
    "empty string engine" \
    1286 \
    42000 \
    "Unknown storage engine ''" \
    "CREATE DATABASE IF NOT EXISTS ${DATABASE}; USE ${DATABASE}; CREATE TABLE empty_engine(id INT) ENGINE='';"

expect_error \
    "show engines like syntax" \
    1064 \
    42000 \
    "near 'LIKE 'InnoDB''" \
    "SHOW ENGINES LIKE 'InnoDB';"

expect_error \
    "show engines where syntax" \
    1064 \
    42000 \
    "near 'WHERE Engine = 'InnoDB''" \
    "SHOW ENGINES WHERE Engine = 'InnoDB';"

expect_error \
    "engine default syntax" \
    1064 \
    42000 \
    "near 'DEFAULT'" \
    "CREATE DATABASE IF NOT EXISTS ${DATABASE}; USE ${DATABASE}; CREATE TABLE default_engine(id INT) ENGINE=DEFAULT;"
