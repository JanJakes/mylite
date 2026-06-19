#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_current_database_expectations_$$"
OTHER_DATABASE="${DATABASE}_other"

fail() {
    printf '%s\n' "mysql_baseline_current_database_function_expectations: $1" >&2
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
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup

expect_output \
    "no selected database returns nulls and no warnings" \
    "NULL	NULL	0" \
    "DO 0; SELECT DATABASE(), SCHEMA(), @@warning_count;"

expected_no_default_headers=$(cat <<EOF
DATABASE()	SCHEMA()
NULL	NULL
EOF
)
expect_output_with_headers \
    "no selected database column names" \
    "$expected_no_default_headers" \
    "SELECT DATABASE(), SCHEMA();"

expect_output \
    "use updates current database functions" \
    "${DATABASE}	${DATABASE}	0" \
    "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; SELECT DATABASE(), SCHEMA(), @@warning_count;"

expected_selected_headers=$(cat <<EOF
database()	schema()
${DATABASE}	${DATABASE}
EOF
)
expect_output_with_headers \
    "lower-case function names remain result labels" \
    "$expected_selected_headers" \
    "USE ${DATABASE}; SELECT database(), schema();"

expected_spaced_headers=$(cat <<EOF
DATABASE ()	SCHEMA ()
${DATABASE}	${DATABASE}
EOF
)
expect_output_with_headers \
    "spaced function names remain result labels" \
    "$expected_spaced_headers" \
    "USE ${DATABASE}; SELECT DATABASE (), SCHEMA ();"

expected_parenthesized_headers=$(cat <<EOF
(DATABASE())
${DATABASE}
EOF
)
expect_output_with_headers \
    "parenthesized function name remains result label" \
    "$expected_parenthesized_headers" \
    "USE ${DATABASE}; SELECT (DATABASE());"

expect_output \
    "from dual returns selected database" \
    "${DATABASE}" \
    "USE ${DATABASE}; SELECT DATABASE() FROM DUAL;"

expected_table_headers=$(cat <<EOF
DATABASE()	SCHEMA()	id
${DATABASE}	${DATABASE}	1
${DATABASE}	${DATABASE}	2
EOF
)
expect_output_with_headers \
    "table-backed current database projection predicate and order" \
    "$expected_table_headers" \
    "USE ${DATABASE}; CREATE TABLE t (id INT); INSERT INTO t VALUES (2), (1); \
     SELECT DATABASE(), SCHEMA(), id FROM t WHERE DATABASE() = '${DATABASE}' \
     ORDER BY SCHEMA(), id;"

expect_output \
    "drop selected database clears current database" \
    "NULL	NULL	0" \
    "USE ${DATABASE}; DROP DATABASE ${DATABASE}; SELECT DATABASE(), SCHEMA(), @@warning_count;"

expect_error \
    "database function rejects arguments" \
    1064 \
    42000 \
    "near '1)'" \
    "SELECT DATABASE(1);"

expect_error \
    "schema function rejects arguments" \
    1064 \
    42000 \
    "near '1)'" \
    "SELECT SCHEMA(1);"

expect_error \
    "bare database keyword is syntax error" \
    1064 \
    42000 \
    "near ''" \
    "SELECT DATABASE;"

expect_error \
    "bare schema keyword is syntax error" \
    1064 \
    42000 \
    "near ''" \
    "SELECT SCHEMA;"

expect_output \
    "mysql accepts limit outside this mylite slice" \
    "${DATABASE}" \
    "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; SELECT DATABASE() LIMIT 1;"
