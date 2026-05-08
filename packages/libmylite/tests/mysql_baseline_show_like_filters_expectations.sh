#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_show_like_$$"
OTHER_DATABASE="${DATABASE}_other"
DATABASE_PATTERN="mylite\\_show\\_like\\_$$"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_show_like_filters_expectations: $1" >&2
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

normalize_tsv() {
    sed "s/${TAB}/|/g"
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

cleanup

run_mysql \
    "CREATE DATABASE ${DATABASE};
     CREATE DATABASE ${OTHER_DATABASE};
     USE ${DATABASE};
     CREATE TABLE alpha(
       id INT NOT NULL,
       id2 INT NULL,
       i_1 INT NULL,
       name_id BIGINT NULL,
       literal_percent INT NULL,
       MixedCase INT NULL,
       \`a%b\` INT NULL,
       \`a_b\` INT NULL,
       \`a\\b\` INT NULL
     );
     CREATE TABLE beta(id INT NOT NULL);
     CREATE TABLE \`a%b\`(id INT NOT NULL);
     CREATE TABLE \`a_b\`(id INT NOT NULL);
     CREATE TABLE \`a\\b\`(id INT NOT NULL);
     CREATE TABLE ${OTHER_DATABASE}.other_alpha(id INT NOT NULL);" >/dev/null

check_single_column_output() {
    label=$1
    sql=$2
    expected_header=$3
    expected_rows=$4

    output=$(run_mysql_with_headers "$sql")
    headers=$(printf '%s\n' "$output" | sed -n '1p')
    rows=$(printf '%s\n' "$output" | sed '1d')

    expect_value "$label headers" "$expected_header" "$headers"
    expect_value "$label rows" "$expected_rows" "$rows"
}

check_columns_output() {
    label=$1
    sql=$2
    expected_rows=$3

    output=$(run_mysql_with_headers "$sql")
    headers=$(printf '%s\n' "$output" | sed -n '1p')
    rows=$(printf '%s\n' "$output" | sed '1d')

    expect_value "$label headers" "Field	Type	Null	Key	Default	Extra" "$headers"
    expect_value "$label rows" "$expected_rows" "$(printf '%s\n' "$rows" | normalize_tsv)"
}

check_single_column_output \
    "show databases like" \
    "SHOW DATABASES LIKE '${DATABASE_PATTERN}';" \
    "Database (${DATABASE_PATTERN})" \
    "${DATABASE}"
check_single_column_output \
    "show schemas like" \
    "SHOW SCHEMAS LIKE '${DATABASE_PATTERN}\\_other';" \
    "Database (${DATABASE_PATTERN}\\_other)" \
    "${OTHER_DATABASE}"

check_single_column_output \
    "show tables like percent" \
    "USE ${DATABASE}; SHOW TABLES LIKE 'a%';" \
    "Tables_in_${DATABASE} (a%)" \
    "a%b
a\\b
a_b
alpha"
check_single_column_output \
    "show tables underscore wildcard" \
    "USE ${DATABASE}; SHOW TABLES LIKE 'a_b';" \
    "Tables_in_${DATABASE} (a_b)" \
    "a%b
a\\b
a_b"
check_single_column_output \
    "show tables escaped underscore" \
    "USE ${DATABASE}; SHOW TABLES LIKE 'a\\_b';" \
    "Tables_in_${DATABASE} (a\\_b)" \
    "a_b"
check_single_column_output \
    "show tables escaped percent" \
    "USE ${DATABASE}; SHOW TABLES LIKE 'a\\%b';" \
    "Tables_in_${DATABASE} (a\\%b)" \
    "a%b"
check_single_column_output \
    "show tables escaped backslash" \
    "USE ${DATABASE}; SHOW TABLES LIKE 'a\\\\\\\\b';" \
    "Tables_in_${DATABASE} (a\\\\b)" \
    "a\\b"
check_single_column_output \
    "show tables from schema like" \
    "SHOW TABLES FROM ${DATABASE} LIKE 'b%';" \
    "Tables_in_${DATABASE} (b%)" \
    "beta"
check_single_column_output \
    "show tables in schema like" \
    "SHOW TABLES IN ${OTHER_DATABASE} LIKE 'other\\_%';" \
    "Tables_in_${OTHER_DATABASE} (other\\_%)" \
    "other_alpha"

check_columns_output \
    "show columns percent" \
    "USE ${DATABASE}; SHOW COLUMNS FROM alpha LIKE 'i%';" \
    "id|int|NO||NULL|
id2|int|YES||NULL|
i_1|int|YES||NULL|"
check_columns_output \
    "show columns underscore wildcard" \
    "USE ${DATABASE}; SHOW COLUMNS FROM alpha LIKE 'i_';" \
    "id|int|NO||NULL|"
check_columns_output \
    "show columns escaped underscore" \
    "USE ${DATABASE}; SHOW FIELDS FROM alpha LIKE 'i\\_1';" \
    "i_1|int|YES||NULL|"
check_columns_output \
    "show columns escaped percent" \
    "USE ${DATABASE}; SHOW COLUMNS FROM alpha LIKE 'a\\%b';" \
    "a%b|int|YES||NULL|"
check_columns_output \
    "show columns escaped underscore literal" \
    "USE ${DATABASE}; SHOW COLUMNS FROM alpha LIKE 'a\\_b';" \
    "a_b|int|YES||NULL|"
check_columns_output \
    "show columns escaped backslash" \
    "USE ${DATABASE}; SHOW COLUMNS FROM alpha LIKE 'a\\\\\\\\b';" \
    "a\\b|int|YES||NULL|"
check_columns_output \
    "show columns case insensitive" \
    "USE ${DATABASE}; SHOW COLUMNS FROM alpha LIKE 'mixedcase';" \
    "MixedCase|int|YES||NULL|"
check_columns_output \
    "show columns explicit schema like" \
    "SHOW COLUMNS FROM alpha FROM ${DATABASE} LIKE 'name%';" \
    "name_id|bigint|YES||NULL|"
check_columns_output \
    "show columns qualified like" \
    "SHOW COLUMNS FROM ${DATABASE}.alpha LIKE 'literal\\_%';" \
    "literal_percent|int|YES||NULL|"

status=$(run_mysql "USE ${DATABASE}; SHOW TABLES LIKE 'a%'; SELECT @@warning_count, ROW_COUNT();" \
    | tail -n 1)
expect_value "show like status" "0	-1" "$status"

nul_hex=$(run_mysql "SELECT HEX('a\\0%');")
expect_value "mysql nul-producing pattern decode" "610025" "$nul_hex"
nul_status=$(run_mysql \
    "USE ${DATABASE}; SHOW TABLES LIKE 'a\\0%'; SELECT @@warning_count, ROW_COUNT();" \
    | tail -n 1)
expect_value "mysql accepts nul-producing show like pattern" "0	-1" "$nul_status"

accepted_but_deferred=$(run_mysql_with_headers \
    "USE ${DATABASE};
     SHOW TABLES WHERE Tables_in_${DATABASE} LIKE 'a%';
     SHOW DATABASES WHERE \`Database\` LIKE '${DATABASE_PATTERN}%';
     SHOW COLUMNS FROM alpha WHERE Field LIKE 'i%';
     DESCRIBE alpha 'i%';
     EXPLAIN alpha 'i%';"
)
expect_value \
    "deferred show tables where header" \
    "Tables_in_${DATABASE}" \
    "$(printf '%s\n' "$accepted_but_deferred" | sed -n '1p')"
expect_value \
    "deferred show databases where header" \
    "Database" \
    "$(printf '%s\n' "$accepted_but_deferred" | awk '/^Database$/ { print; exit }')"
expect_value \
    "deferred show columns where header" \
    "Field	Type	Null	Key	Default	Extra" \
    "$(printf '%s\n' "$accepted_but_deferred" | awk -F '\t' '$1 == "Field" && $2 == "Type" { print; exit }')"

expect_error \
    "missing default schema show tables like" \
    1046 \
    3D000 \
    "No database selected" \
    "SHOW TABLES LIKE 'a%';"

expect_error \
    "unknown schema show tables like" \
    1049 \
    42000 \
    "Unknown database 'missing_schema'" \
    "SHOW TABLES FROM missing_schema LIKE 'a%';"

expect_error \
    "unknown schema show columns like" \
    1049 \
    42000 \
    "Unknown database 'missing_schema'" \
    "SHOW COLUMNS FROM missing_schema.alpha LIKE 'i%';"

expect_error \
    "unknown table show columns like" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_table' doesn't exist" \
    "USE ${DATABASE}; SHOW COLUMNS FROM missing_table LIKE 'i%';"

expect_error \
    "numeric show tables like" \
    1064 \
    42000 \
    "near '1'" \
    "USE ${DATABASE}; SHOW TABLES LIKE 1;"

expect_error \
    "null show tables like" \
    1064 \
    42000 \
    "near 'NULL'" \
    "USE ${DATABASE}; SHOW TABLES LIKE NULL;"

expect_error \
    "national string show tables like" \
    1064 \
    42000 \
    "near 'N'a%''" \
    "USE ${DATABASE}; SHOW TABLES LIKE N'a%';"
