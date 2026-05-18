#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_show_full_tables_$$"
OTHER_DATABASE="${DATABASE}_other"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_show_full_tables_expectations: $1" >&2
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
            "$@"
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
    status=$?
    set -e

    if [ "$status" -eq 0 ]; then
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

check_full_tables_output() {
    label=$1
    sql=$2
    expected_header=$3
    expected_rows=$4
    shift 4

    output=$(run_mysql_with_headers "$sql" "$@")
    header=$(printf '%s\n' "$output" | sed -n '1p' | normalize_tsv)
    rows=$(printf '%s\n' "$output" | sed '1d' | normalize_tsv)

    expect_value "$label header" "$expected_header" "$header"
    expect_value "$label rows" "$expected_rows" "$rows"
}

check_full_tables_empty_output() {
    label=$1
    sql=$2
    shift 2

    rows=$(run_mysql "$sql" "$@" | normalize_tsv)
    expect_value "$label rows" "" "$rows"
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

case "$(run_mysql 'SELECT @@lower_case_table_names;')" in
    0) ;;
    *) fail "expected @@lower_case_table_names=0 for SHOW FULL TABLES LIKE probes" ;;
esac

cleanup
run_mysql \
    "CREATE DATABASE ${DATABASE};
     CREATE DATABASE ${OTHER_DATABASE};
     USE ${DATABASE};
     CREATE TABLE alpha(id INT);
     CREATE TABLE beta(id INT);
     CREATE TEMPORARY TABLE temp_only(id INT);
     CREATE TABLE ${OTHER_DATABASE}.other_alpha(id INT);" >/dev/null

check_full_tables_output \
    "show full tables selected schema" \
    "USE ${DATABASE}; SHOW FULL TABLES;" \
    "Tables_in_${DATABASE}|Table_type" \
    "alpha|BASE TABLE
beta|BASE TABLE"

check_full_tables_output \
    "show full tables from schema" \
    "SHOW FULL TABLES FROM ${DATABASE};" \
    "Tables_in_${DATABASE}|Table_type" \
    "alpha|BASE TABLE
beta|BASE TABLE"

check_full_tables_output \
    "show full tables in schema" \
    "SHOW FULL TABLES IN ${OTHER_DATABASE};" \
    "Tables_in_${OTHER_DATABASE}|Table_type" \
    "other_alpha|BASE TABLE"

check_full_tables_output \
    "show full tables like" \
    "USE ${DATABASE}; SHOW FULL TABLES LIKE 'a%';" \
    "Tables_in_${DATABASE} (a%)|Table_type" \
    "alpha|BASE TABLE"

check_full_tables_empty_output \
    "show full tables no match" \
    "USE ${DATABASE}; SHOW FULL TABLES LIKE 'ALPHA';"

check_full_tables_empty_output \
    "show full tables omits temporary tables" \
    "USE ${DATABASE}; SHOW FULL TABLES LIKE 'temp%';"

status=$(
    run_mysql \
        "USE ${DATABASE};
         SHOW FULL TABLES LIKE 'a%';
         SELECT @@warning_count, @@error_count, ROW_COUNT();" \
        | tail -n 1 \
        | normalize_tsv
)
expect_value "status after successful show full tables" "0|0|-1" "$status"

expect_error \
    "missing default schema show full tables" \
    1046 \
    3D000 \
    "No database selected" \
    "SHOW FULL TABLES;"

expect_error \
    "unknown schema show full tables" \
    1049 \
    42000 \
    "Unknown database 'missing_full_tables'" \
    "SHOW FULL TABLES FROM missing_full_tables;" \
    "$DATABASE"

expect_error \
    "numeric show full tables like" \
    1064 \
    42000 \
    "near '1'" \
    "SHOW FULL TABLES LIKE 1;" \
    "$DATABASE"

expect_error \
    "null show full tables like" \
    1064 \
    42000 \
    "near 'NULL'" \
    "SHOW FULL TABLES LIKE NULL;" \
    "$DATABASE"

expect_error \
    "national string show full tables like" \
    1064 \
    42000 \
    "near 'N'a%''" \
    "SHOW FULL TABLES LIKE N'a%';" \
    "$DATABASE"

printf '%s\n' "baseline-show-full-tables MySQL 8.4.9 expectations verified"
