#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_show_databases_where_$$"
OTHER_DATABASE="${DATABASE}_other"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_show_databases_where_expectations: $1" >&2
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

expect_positive_integer() {
    label=$1
    value=$2

    case "$value" in
        ''|*[!0-9]*) fail "$label: expected positive integer, got [$value]" ;;
    esac
    if [ "$value" -eq 0 ]; then
        fail "$label: expected positive integer, got [$value]"
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

run_mysql "CREATE DATABASE ${DATABASE}; CREATE DATABASE ${OTHER_DATABASE};" >/dev/null

headers=$(run_mysql_with_headers "SHOW DATABASES WHERE \`Database\` = 'mysql';" | sed -n '1p')
expect_value "show databases where headers" "Database" "$headers"

schema_headers=$(run_mysql_with_headers "SHOW SCHEMAS WHERE \`Database\` = 'mysql';" | sed -n '1p')
expect_value "show schemas where headers" "Database" "$schema_headers"

schema_synonym=$(run_mysql "SHOW SCHEMAS WHERE \`Database\` = 'mysql';")
expect_value "show schemas synonym" "mysql" "$schema_synonym"

identifier_case=$(
    run_mysql "SHOW DATABASES WHERE \`database\` = 'mysql'; SHOW DATABASES WHERE \`DATABASE\` = 'mysql';"
)
expect_value "database output column case" "mysql
mysql" "$identifier_case"

case_sensitive_equal=$(run_mysql "SHOW DATABASES WHERE \`Database\` = 'MYSQL';")
expect_value "case-sensitive equality" "" "$case_sensitive_equal"

like_case_sensitive=$(run_mysql "SHOW DATABASES WHERE \`Database\` LIKE 'M%';")
expect_value "case-sensitive like" "" "$like_case_sensitive"

like_user=$(run_mysql "SHOW DATABASES WHERE \`Database\` LIKE '${DATABASE}%';")
expect_value "user database like" "${DATABASE}
${OTHER_DATABASE}" "$like_user"

regexp_user=$(run_mysql "SHOW DATABASES WHERE \`Database\` REGEXP '^${DATABASE}';")
expect_value "user database regexp" "${DATABASE}
${OTHER_DATABASE}" "$regexp_user"

in_rows=$(run_mysql "SHOW DATABASES WHERE \`Database\` IN ('mysql','sys','${DATABASE}');")
expect_value "database in rows" "${DATABASE}
mysql
sys" "$in_rows"

or_not=$(
    run_mysql \
        "SHOW DATABASES WHERE (\`Database\` = '${DATABASE}' OR \`Database\` = '${OTHER_DATABASE}')
         AND NOT \`Database\` = '${OTHER_DATABASE}';"
)
expect_value "or and not" "$DATABASE" "$or_not"

not_in_null_status=$(
    run_mysql \
        "SHOW DATABASES WHERE \`Database\` NOT IN (NULL, 'mysql')
             AND \`Database\` IN ('mysql','sys');
         SELECT @@warning_count, @@error_count, ROW_COUNT();" \
        | tail -n 1 \
        | normalize_tsv
)
expect_value "not in null status" "0|0|-1" "$not_in_null_status"

is_not_null=$(run_mysql "SHOW DATABASES WHERE \`Database\` IS NOT NULL AND \`Database\` = 'sys';")
expect_value "is not null" "sys" "$is_not_null"

numeric_warning_count=$(
    run_mysql "SHOW DATABASES WHERE \`Database\` = 1; SELECT @@warning_count;" | tail -n 1
)
expect_positive_integer "numeric comparison warning count" "$numeric_warning_count"

expect_error \
    "unknown where column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'where clause'" \
    "SHOW DATABASES WHERE missing = 'x';"

expect_error \
    "qualified where column" \
    1054 \
    42S22 \
    "Unknown column 'schemas.Database' in 'where clause'" \
    "SHOW DATABASES WHERE \`schemas\`.\`Database\` = 'mysql';"

expect_error \
    "unquoted database keyword" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW DATABASES WHERE Database = 'mysql';"

expect_error \
    "like then where" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW DATABASES LIKE 'mysql' WHERE \`Database\` = 'mysql';"

expect_error \
    "order by after where" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW DATABASES WHERE \`Database\` = 'mysql' ORDER BY \`Database\`;"

expect_error \
    "limit after where" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW DATABASES WHERE \`Database\` = 'mysql' LIMIT 1;"

printf '%s\n' "baseline-show-databases-where MySQL 8.4.9 expectations verified"
