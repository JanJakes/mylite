#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_schema_expectations_$$"
OTHER_DATABASE="${DATABASE}_other"
DROP_DATABASE="${DATABASE}_drop"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_schema_lifecycle_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw "$@"
}

run_mysql_verbose() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot -vvv "$@"
}

expect_output() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    if [ "$output" != "$expected" ]; then
        fail "$label: expected [$expected], got [$output]"
    fi
}

expect_output_with_headers() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql_with_headers "$sql" "$@")
    if [ "$output" != "$expected" ]; then
        fail "$label: expected [$expected], got [$output]"
    fi
}

expect_verbose_contains() {
    label=$1
    needle=$2
    sql=$3
    shift 3

    output=$(run_mysql_verbose "$sql" "$@")
    case "$output" in
        *"$needle"*) ;;
        *) fail "$label: expected verbose output containing [$needle], got [$output]" ;;
    esac
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

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
    run_mysql "DROP DATABASE IF EXISTS ${OTHER_DATABASE};" >/dev/null 2>&1 || true
    run_mysql "DROP DATABASE IF EXISTS ${DROP_DATABASE};" >/dev/null 2>&1 || true
    run_mysql "DROP DATABASE IF EXISTS ${MISSING_DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup

expect_output \
    "create database status and selected database" \
    "1	0	NULL" \
    "CREATE DATABASE ${DATABASE}; SELECT ROW_COUNT(), @@warning_count, DATABASE();"

expect_error \
    "duplicate create database" \
    1007 \
    HY000 \
    "Can't create database '${DATABASE}'; database exists" \
    "CREATE DATABASE ${DATABASE};"

expect_output \
    "create schema alias status" \
    "1	0" \
    "CREATE SCHEMA ${OTHER_DATABASE}; SELECT ROW_COUNT(), @@warning_count;"

expect_error \
    "duplicate create schema alias" \
    1007 \
    HY000 \
    "Can't create database '${OTHER_DATABASE}'; database exists" \
    "CREATE SCHEMA ${OTHER_DATABASE};"

expect_output \
    "use selects created schema" \
    "${DATABASE}	0	0" \
    "USE ${DATABASE}; SELECT DATABASE(), ROW_COUNT(), @@warning_count;"

expected_show_like=$(cat <<EOF
Database (${DATABASE}%)
${DATABASE}
${OTHER_DATABASE}
EOF
)
expect_output_with_headers \
    "show databases result shape" \
    "$expected_show_like" \
    "SHOW DATABASES LIKE '${DATABASE}%';"

expect_output_with_headers \
    "show schemas alias result shape" \
    "$expected_show_like" \
    "SHOW SCHEMAS LIKE '${DATABASE}%';"

expect_verbose_contains \
    "drop empty database affected rows" \
    "Query OK, 0 rows affected" \
    "DROP DATABASE ${OTHER_DATABASE};"

expect_output \
    "drop selected database clears selected database" \
    "NULL" \
    "USE ${DATABASE}; DROP DATABASE ${DATABASE}; SELECT DATABASE();"

expect_error \
    "drop missing database" \
    1008 \
    HY000 \
    "Can't drop database '${MISSING_DATABASE}'; database doesn't exist" \
    "DROP DATABASE ${MISSING_DATABASE};"

expect_error \
    "drop missing schema alias" \
    1008 \
    HY000 \
    "Can't drop database '${MISSING_DATABASE}'; database doesn't exist" \
    "DROP SCHEMA ${MISSING_DATABASE};"

expect_error \
    "use missing database" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "USE ${MISSING_DATABASE};"

expect_verbose_contains \
    "drop database reports removed table count" \
    "Query OK, 2 rows affected" \
    "CREATE DATABASE ${DROP_DATABASE}; "\
"USE ${DROP_DATABASE}; "\
"CREATE TABLE a (id INT); "\
"CREATE TABLE b (id INT); "\
"DROP DATABASE ${DROP_DATABASE};"

expect_output \
    "create database if not exists duplicate warning" \
    "1	1" \
    "CREATE DATABASE ${DATABASE}; "\
"CREATE DATABASE IF NOT EXISTS ${DATABASE}; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"DROP DATABASE ${DATABASE};"

expect_output \
    "drop database if exists missing status" \
    "-1	0" \
    "DROP DATABASE IF EXISTS ${MISSING_DATABASE}; SELECT ROW_COUNT(), @@warning_count;"

expect_output \
    "create database options are accepted by mysql" \
    "1	0" \
    "CREATE DATABASE ${DATABASE} DEFAULT CHARACTER SET utf8mb4; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"DROP DATABASE ${DATABASE};"

expect_error \
    "qualified schema name syntax" \
    1064 \
    42000 \
    "near '.b'" \
    "CREATE DATABASE a.b;"

expected_show_where=$(cat <<EOF
Database
${DATABASE}
EOF
)
expect_output_with_headers \
    "show databases where is accepted by mysql" \
    "$expected_show_where" \
    "CREATE DATABASE ${DATABASE}; "\
"SHOW DATABASES WHERE \`Database\` LIKE '${DATABASE}%'; "\
"DROP DATABASE ${DATABASE};"
