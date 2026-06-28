#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_internal_native_helper_rejections_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
}

expect_error() {
    label=$1
    code=$2
    state=$3
    message=$4
    sql=$5

    set +e
    output=$(run_mysql "$sql" 2>&1)
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

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

for name in \
    CAN_ACCESS_COLUMN \
    CAN_ACCESS_DATABASE \
    CAN_ACCESS_TABLE \
    CAN_ACCESS_USER \
    CAN_ACCESS_VIEW \
    GET_DD_COLUMN_PRIVILEGES \
    GET_DD_CREATE_OPTIONS \
    GET_DD_INDEX_SUB_PART_LENGTH \
    INTERNAL_AUTO_INCREMENT \
    INTERNAL_AVG_ROW_LENGTH \
    INTERNAL_CHECK_TIME \
    INTERNAL_CHECKSUM \
    INTERNAL_DATA_FREE \
    INTERNAL_DATA_LENGTH \
    INTERNAL_DD_CHAR_LENGTH \
    INTERNAL_GET_COMMENT_OR_ERROR \
    INTERNAL_GET_ENABLED_ROLE_JSON \
    INTERNAL_GET_HOSTNAME \
    INTERNAL_GET_USERNAME \
    INTERNAL_GET_VIEW_WARNING_OR_ERROR \
    INTERNAL_INDEX_COLUMN_CARDINALITY \
    INTERNAL_INDEX_LENGTH \
    INTERNAL_IS_ENABLED_ROLE \
    INTERNAL_IS_MANDATORY_ROLE \
    INTERNAL_KEYS_DISABLED \
    INTERNAL_MAX_DATA_LENGTH \
    INTERNAL_TABLE_ROWS \
    INTERNAL_UPDATE_TIME
do
    expect_error \
        "$name direct native rejection" \
        3566 \
        "HY000" \
        "Access to native function '$name' is rejected." \
        "SELECT $name('a','b','c');"
done

expect_error \
    "native rejection ignores zero arity" \
    3566 \
    "HY000" \
    "Access to native function 'CAN_ACCESS_TABLE' is rejected." \
    "SELECT CAN_ACCESS_TABLE();"

expect_error \
    "native rejection ignores too many arguments" \
    3566 \
    "HY000" \
    "Access to native function 'INTERNAL_TABLE_ROWS' is rejected." \
    "SELECT INTERNAL_TABLE_ROWS(1,2,3,4,5,6,7,8,9,10);"

expect_error \
    "native rejection ignores too few arguments" \
    3566 \
    "HY000" \
    "Access to native function 'GET_DD_CREATE_OPTIONS' is rejected." \
    "SELECT GET_DD_CREATE_OPTIONS('');"

expect_error \
    "native rejection from dual" \
    3566 \
    "HY000" \
    "Access to native function 'GET_DD_CREATE_OPTIONS' is rejected." \
    "SELECT GET_DD_CREATE_OPTIONS('',0,0) FROM DUAL;"

expect_error \
    "native rejection in DO" \
    3566 \
    "HY000" \
    "Access to native function 'INTERNAL_KEYS_DISABLED' is rejected." \
    "DO INTERNAL_KEYS_DISABLED('');"

expect_error \
    "native rejection in row-backed query" \
    3566 \
    "HY000" \
    "Access to native function 'CAN_ACCESS_DATABASE' is rejected." \
    "CREATE DATABASE IF NOT EXISTS mylite_probe;
     USE mylite_probe;
     DROP TABLE IF EXISTS native_rejection_probe;
     CREATE TABLE native_rejection_probe(id INT);
     INSERT INTO native_rejection_probe VALUES (1);
     SELECT CAN_ACCESS_DATABASE(id) FROM native_rejection_probe;"

printf '%s\n' "mysql_baseline_internal_native_helper_rejections_expectations: ok"
