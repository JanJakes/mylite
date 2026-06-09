#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_parser_corpus_show_grants_surfaces_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw "$@"
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

version=$(run_mysql "SELECT VERSION();" --skip-column-names)
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

root_grants=$(run_mysql "SHOW GRANTS;" --skip-column-names)

expect_output \
    "unquoted root defaults host" \
    "$root_grants" \
    "SHOW GRANTS FOR root;" \
    --skip-column-names

expect_output \
    "root quoted wildcard host" \
    "$root_grants" \
    "SHOW GRANTS FOR root@'%';" \
    --skip-column-names

expect_error \
    "missing account omitted host" \
    1141 \
    42000 \
    "There is no such grant defined for user 'missing' on host '%'" \
    "SHOW GRANTS FOR missing;"

expect_error \
    "missing account empty host" \
    1141 \
    42000 \
    "There is no such grant defined for user 'mysqltest_7' on host ''" \
    "SHOW GRANTS FOR mysqltest_7@;"

expect_error \
    "current user role not granted" \
    3530 \
    HY000 \
    "\`r1\`@\`%\` is not granted to \`root\`@\`%\`" \
    "SHOW GRANTS FOR CURRENT_USER() USING r1;"

expect_error \
    "missing account before using role" \
    1141 \
    42000 \
    "There is no such grant defined for user 'missing' on host '%'" \
    "SHOW GRANTS FOR missing USING r1;"

printf '%s\n' "mysql_parser_corpus_show_grants_surfaces_expectations: ok"
