#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_show_table_status_where_$$"
OTHER_DATABASE="${DATABASE}_other"
EMPTY_DATABASE="${DATABASE}_empty"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_show_table_status_where_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql \
            --protocol=TCP \
            -h127.0.0.1 \
            -uroot \
            --batch \
            --raw \
            --skip-column-names \
            --default-character-set=utf8mb4 \
            "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql \
            --protocol=TCP \
            -h127.0.0.1 \
            -uroot \
            --batch \
            --raw \
            --default-character-set=utf8mb4 \
            "$@"
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

row_names() {
    awk -F '\t' 'NF > 0 { print $1 }'
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE}; DROP DATABASE IF EXISTS ${OTHER_DATABASE};
               DROP DATABASE IF EXISTS ${EMPTY_DATABASE};" >/dev/null 2>&1 || true
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
     CREATE DATABASE ${EMPTY_DATABASE};
     USE ${DATABASE};
     CREATE TABLE numbers(id INT NOT NULL, i INT NULL) ENGINE=InnoDB;
     CREATE TABLE auto_numbers(id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, i INT NULL) ENGINE=InnoDB;
     INSERT INTO numbers VALUES (1, NULL), (2, 20), (3, 30);
     INSERT INTO auto_numbers(i) VALUES (10), (20);
     CREATE TABLE ${OTHER_DATABASE}.only_other(id INT NOT NULL) ENGINE=InnoDB;" >/dev/null

expected_columns="Name${TAB}Engine${TAB}Version${TAB}Row_format${TAB}Rows${TAB}Avg_row_length${TAB}Data_length${TAB}Max_data_length${TAB}Index_length${TAB}Data_free${TAB}Auto_increment${TAB}Create_time${TAB}Update_time${TAB}Check_time${TAB}Collation${TAB}Checksum${TAB}Create_options${TAB}Comment"
headers=$(run_mysql_with_headers "SHOW TABLE STATUS FROM ${DATABASE} WHERE Name = 'numbers';" | sed -n '1p')
expect_value "headers" "$expected_columns" "$headers"

case_name=$(run_mysql "SHOW TABLE STATUS FROM ${DATABASE} WHERE Name = 'NUMBERS';" | row_names)
expect_value "case-sensitive name equality" "" "$case_name"

backtick_like=$(run_mysql "SHOW TABLE STATUS FROM ${DATABASE} WHERE \`Name\` LIKE 'num%';" | row_names)
expect_value "backticked name like" "numbers" "$backtick_like"

engine_in=$(
    run_mysql \
        "SHOW TABLE STATUS FROM ${DATABASE} WHERE Engine = 'INNODB' AND Name IN ('numbers','auto_numbers');" \
        | row_names
)
expect_value "engine case-insensitive and name in" "auto_numbers
numbers" "$engine_in"

rows_string=$(run_mysql "SHOW TABLE STATUS FROM ${DATABASE} WHERE \`Rows\` = '3';" | row_names)
expect_value "rows string comparison" "numbers" "$rows_string"

rows_leading_zero=$(run_mysql "SHOW TABLE STATUS FROM ${DATABASE} WHERE \`Rows\` = '03';" | row_names)
expect_value "rows leading-zero numeric string comparison" "numbers" "$rows_leading_zero"

rows_ordering=$(
    run_mysql "SHOW TABLE STATUS FROM ${DATABASE} WHERE \`Rows\` > '10' AND Name = 'numbers';" \
        | row_names
)
expect_value "rows numeric ordering comparison" "" "$rows_ordering"

data_length_leading_zero=$(
    run_mysql \
        "SHOW TABLE STATUS FROM ${DATABASE} WHERE Data_length = '016384' AND Name = 'numbers';" \
        | row_names
)
expect_value "data length leading-zero numeric string comparison" "numbers" "$data_length_leading_zero"

auto_is_null=$(
    run_mysql \
        "SHOW TABLE STATUS FROM ${DATABASE} WHERE Auto_increment IS NULL AND Name IN ('numbers','auto_numbers');" \
        | row_names
)
expect_value "auto increment is null" "numbers" "$auto_is_null"

auto_is_not_null=$(
    run_mysql \
        "SHOW TABLE STATUS FROM ${DATABASE} WHERE Auto_increment IS NOT NULL AND Name IN ('numbers','auto_numbers');" \
        | row_names
)
expect_value "auto increment is not null" "auto_numbers" "$auto_is_not_null"

auto_null_safe=$(
    run_mysql \
        "SHOW TABLE STATUS FROM ${DATABASE} WHERE Auto_increment <=> NULL AND Name IN ('numbers','auto_numbers');" \
        | row_names
)
expect_value "auto increment null safe" "numbers" "$auto_null_safe"

auto_in_leading_zero=$(
    run_mysql \
        "SHOW TABLE STATUS FROM ${DATABASE} WHERE Auto_increment IN (NULL, '03') AND Name IN ('numbers','auto_numbers');" \
        | row_names
)
expect_value "auto increment numeric in leading zero" "auto_numbers" "$auto_in_leading_zero"

not_in_null_status=$(
    run_mysql \
        "SHOW TABLE STATUS FROM ${DATABASE} WHERE Name NOT IN (NULL, 'numbers') AND Name IN ('numbers','auto_numbers');
         SELECT @@warning_count, @@error_count, ROW_COUNT();" \
        | tail -n 1 \
        | normalize_tsv
)
expect_value "not in null status" "0|0|-1" "$not_in_null_status"

or_not=$(
    run_mysql \
        "SHOW TABLE STATUS FROM ${DATABASE} WHERE (Name = 'numbers' OR Name = 'missing') AND NOT Engine = 'memory';" \
        | row_names
)
expect_value "or and not" "numbers" "$or_not"

explicit_schema=$(run_mysql "SHOW TABLE STATUS IN ${OTHER_DATABASE} WHERE Name = 'only_other';" | row_names)
expect_value "explicit schema where" "only_other" "$explicit_schema"

no_match_status=$(
    run_mysql \
        "USE ${DATABASE}; SHOW TABLE STATUS WHERE Name = 'missing';
         SELECT @@warning_count, @@error_count, ROW_COUNT();" \
        | tail -n 1 \
        | normalize_tsv
)
expect_value "no match status" "0|0|-1" "$no_match_status"

numeric_name_status=$(
    run_mysql \
        "SHOW TABLE STATUS FROM ${DATABASE} WHERE Name = 1;
         SELECT @@warning_count, @@error_count, ROW_COUNT();" \
        | tail -n 1 \
        | normalize_tsv
)
expect_value "mysql numeric name comparison warnings" "2|0|-1" "$numeric_name_status"

expect_error \
    "unknown where column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'where clause'" \
    "SHOW TABLE STATUS FROM ${DATABASE} WHERE missing = 'x';"

expect_error \
    "unknown where column in empty schema" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'where clause'" \
    "SHOW TABLE STATUS FROM ${EMPTY_DATABASE} WHERE missing = 'x';"

expect_error \
    "qualified where column" \
    1054 \
    42S22 \
    "Unknown column 'tables.Name' in 'where clause'" \
    "SHOW TABLE STATUS FROM ${DATABASE} WHERE tables.Name = 'numbers';"

expect_error \
    "unquoted rows keyword" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW TABLE STATUS FROM ${DATABASE} WHERE Rows = 3;"

expect_error \
    "order by after where" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW TABLE STATUS FROM ${DATABASE} WHERE Name = 'numbers' ORDER BY Name;"

expect_error \
    "like then where" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW TABLE STATUS FROM ${DATABASE} LIKE 'numbers' WHERE Name = 'numbers';"

printf '%s\n' "baseline-show-table-status-where MySQL 8.4.9 expectations verified"
