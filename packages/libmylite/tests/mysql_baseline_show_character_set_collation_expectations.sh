#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_show_character_set_collation_expectations: $1" >&2
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

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

server_defaults=$(run_mysql 'SELECT @@character_set_server, @@collation_server;')
expect_value "server defaults" "utf8mb4	utf8mb4_0900_ai_ci" "$server_defaults"

charset_headers="Charset	Description	Default collation	Maxlen"
charset_row="utf8mb4	UTF-8 Unicode	utf8mb4_0900_ai_ci	4"
collation_headers="Collation	Charset	Id	Default	Compiled	Sortlen	Pad_attribute"
collation_row="utf8mb4_0900_ai_ci	utf8mb4	255	Yes	Yes	0	NO PAD"

charset_output=$(run_mysql_with_headers "SHOW CHARACTER SET LIKE 'utf8mb4';")
expect_value "character set exact row" "$charset_headers
$charset_row" "$charset_output"

charset_alias_output=$(run_mysql_with_headers "SHOW CHARSET LIKE 'utf8mb4';")
expect_value "charset alias exact row" "$charset_headers
$charset_row" "$charset_alias_output"

charset_upper_output=$(run_mysql "SHOW CHARACTER SET LIKE 'UTF8MB4';")
expect_value "character set uppercase like" "$charset_row" "$charset_upper_output"

charset_wildcard_output=$(run_mysql "SHOW CHARACTER SET LIKE 'utf%mb4';")
expect_value "character set wildcard like" "$charset_row" "$charset_wildcard_output"

charset_single_wildcard_output=$(run_mysql "SHOW CHARACTER SET LIKE 'utf_mb4';")
expect_value "character set underscore like" "$charset_row" "$charset_single_wildcard_output"

charset_no_match_status=$(run_mysql "SHOW CHARACTER SET LIKE 'missing%'; SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "character set no-match status" "0	-1" "$charset_no_match_status"

charset_status=$(run_mysql "SHOW CHARACTER SET LIKE 'utf8mb4'; SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "character set status" "0	-1" "$charset_status"

charset_where_output=$(run_mysql "SHOW CHARACTER SET WHERE Charset = 'utf8mb4';")
expect_value "character set where accepted row" "$charset_row" "$charset_where_output"

collation_output=$(run_mysql_with_headers "SHOW COLLATION LIKE 'utf8mb4_0900_ai_ci';")
expect_value "collation exact row" "$collation_headers
$collation_row" "$collation_output"

collation_upper_output=$(run_mysql "SHOW COLLATION LIKE 'UTF8MB4_0900_AI_CI';")
expect_value "collation uppercase like" "$collation_row" "$collation_upper_output"

collation_escaped_output=$(run_mysql "SHOW COLLATION LIKE 'utf8mb4\\_0900\\_ai\\_ci';")
expect_value "collation escaped underscore like" "$collation_row" "$collation_escaped_output"

collation_no_match_status=$(run_mysql "SHOW COLLATION LIKE 'missing%'; SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "collation no-match status" "0	-1" "$collation_no_match_status"

collation_status=$(run_mysql "SHOW COLLATION LIKE 'utf8mb4_0900_ai_ci'; SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "collation status" "0	-1" "$collation_status"

collation_where_output=$(run_mysql "SHOW COLLATION WHERE Collation = 'utf8mb4_0900_ai_ci';")
expect_value "collation where accepted row" "$collation_row" "$collation_where_output"

charset_count=$(run_mysql "SHOW CHARACTER SET;" | wc -l | tr -d ' ')
if [ "$charset_count" -le 1 ]; then
    fail "expected MySQL full character-set catalog to contain more than one row, got [$charset_count]"
fi

collation_count=$(run_mysql "SHOW COLLATION;" | wc -l | tr -d ' ')
if [ "$collation_count" -le 1 ]; then
    fail "expected MySQL full collation catalog to contain more than one row, got [$collation_count]"
fi

expect_error \
    "plural character sets" \
    1064 \
    42000 \
    "near 'SETS'" \
    "SHOW CHARACTER SETS;"

expect_error \
    "plural charsets" \
    1064 \
    42000 \
    "near 'CHARSETS'" \
    "SHOW CHARSETS;"

expect_error \
    "plural collations" \
    1064 \
    42000 \
    "near 'COLLATIONS'" \
    "SHOW COLLATIONS;"

expect_error \
    "character set numeric like" \
    1064 \
    42000 \
    "near '1'" \
    "SHOW CHARACTER SET LIKE 1;"

expect_error \
    "character set null like" \
    1064 \
    42000 \
    "near 'NULL'" \
    "SHOW CHARACTER SET LIKE NULL;"

expect_error \
    "character set national like" \
    1064 \
    42000 \
    "near 'N'utf8mb4''" \
    "SHOW CHARACTER SET LIKE N'utf8mb4';"

expect_error \
    "character set introducer like" \
    1064 \
    42000 \
    "near '_utf8mb4'utf8mb4''" \
    "SHOW CHARACTER SET LIKE _utf8mb4'utf8mb4';"

expect_error \
    "collation numeric like" \
    1064 \
    42000 \
    "near '1'" \
    "SHOW COLLATION LIKE 1;"

expect_error \
    "collation null like" \
    1064 \
    42000 \
    "near 'NULL'" \
    "SHOW COLLATION LIKE NULL;"

expect_error \
    "collation national like" \
    1064 \
    42000 \
    "near 'N'utf8mb4_0900_ai_ci''" \
    "SHOW COLLATION LIKE N'utf8mb4_0900_ai_ci';"

expect_error \
    "collation introducer like" \
    1064 \
    42000 \
    "near '_utf8mb4'utf8mb4_0900_ai_ci''" \
    "SHOW COLLATION LIKE _utf8mb4'utf8mb4_0900_ai_ci';"
