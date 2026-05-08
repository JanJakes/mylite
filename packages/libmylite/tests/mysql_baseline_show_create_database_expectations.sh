#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_show_create_db_expectations_$$"
QUOTED_DATABASE=$(printf '%s_a`b' "$DATABASE")
QUOTED_DATABASE_ESCAPED=$(printf '%s' "$QUOTED_DATABASE" | sed 's/`/``/g')

fail() {
    printf '%s\n' "mysql_baseline_show_create_database_expectations: $1" >&2
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
    run_mysql "DROP DATABASE IF EXISTS \`${DATABASE}\`; DROP DATABASE IF EXISTS \`${QUOTED_DATABASE_ESCAPED}\`;" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup

run_mysql "CREATE DATABASE \`${DATABASE}\`; CREATE DATABASE \`${QUOTED_DATABASE_ESCAPED}\`;" >/dev/null

expected_headers="Database	Create Database"
expected_create="CREATE DATABASE \`${DATABASE}\` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */"
expected_if_not_exists="CREATE DATABASE /*!32312 IF NOT EXISTS*/ \`${DATABASE}\` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */"
expected_unquoted_create="CREATE DATABASE ${DATABASE} /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */"
expected_quoted_create="CREATE DATABASE \`${QUOTED_DATABASE_ESCAPED}\` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */"

show_database_output=$(run_mysql_with_headers "SHOW CREATE DATABASE \`${DATABASE}\`;")
expect_value "show create database headers" "$expected_headers" "$(printf '%s\n' "$show_database_output" | sed -n '1p')"
expect_value "show create database row" "${DATABASE}	${expected_create}" "$(printf '%s\n' "$show_database_output" | sed -n '2p')"

status=$(run_mysql "SHOW CREATE DATABASE \`${DATABASE}\`; SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "show create database status" "0	-1" "$status"

show_schema_output=$(run_mysql_with_headers "SHOW CREATE SCHEMA \`${DATABASE}\`;")
expect_value "show create schema headers" "$expected_headers" "$(printf '%s\n' "$show_schema_output" | sed -n '1p')"
expect_value "show create schema row" "${DATABASE}	${expected_create}" "$(printf '%s\n' "$show_schema_output" | sed -n '2p')"

show_if_not_exists_output=$(run_mysql_with_headers "SHOW CREATE DATABASE IF NOT EXISTS \`${DATABASE}\`;")
expect_value \
    "show create database if not exists row" \
    "${DATABASE}	${expected_if_not_exists}" \
    "$(printf '%s\n' "$show_if_not_exists_output" | sed -n '2p')"

show_unquoted_output=$(run_mysql_with_headers "SET SESSION sql_quote_show_create = 0; SHOW CREATE DATABASE \`${DATABASE}\`;")
expect_value \
    "show create database sql_quote_show_create off row" \
    "${DATABASE}	${expected_unquoted_create}" \
    "$(printf '%s\n' "$show_unquoted_output" | sed -n '2p')"

show_quoted_output=$(run_mysql_with_headers "SHOW CREATE DATABASE \`${QUOTED_DATABASE_ESCAPED}\`;")
expect_value "show create database quoted row" "${QUOTED_DATABASE}	${expected_quoted_create}" "$(printf '%s\n' "$show_quoted_output" | sed -n '2p')"

expect_error \
    "unknown schema show create database" \
    1049 \
    42000 \
    "Unknown database 'missing_show_create_schema'" \
    "SHOW CREATE DATABASE missing_show_create_schema;"

expect_error \
    "unknown schema show create schema" \
    1049 \
    42000 \
    "Unknown database 'missing_show_create_schema'" \
    "SHOW CREATE SCHEMA missing_show_create_schema;"

expect_error \
    "missing name show create database" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW CREATE DATABASE;"

expect_error \
    "unsupported if exists show create database" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW CREATE DATABASE IF EXISTS \`${DATABASE}\`;"

expect_error \
    "unsupported like show create database" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW CREATE DATABASE \`${DATABASE}\` LIKE 'x';"

expect_error \
    "unsupported options show create database" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW CREATE DATABASE \`${DATABASE}\` DEFAULT CHARACTER SET utf8mb4;"

printf '%s\n' "baseline-show-create-database MySQL 8.4.9 expectations verified"
