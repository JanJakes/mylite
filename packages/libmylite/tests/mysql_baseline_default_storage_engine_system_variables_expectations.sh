#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_ARGS="--protocol=TCP -h127.0.0.1 -uroot --batch --raw --default-character-set=utf8mb4"

fail() {
    printf '%s\n' "mysql_baseline_default_storage_engine_system_variables_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS "$@"
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

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

expected_values="InnoDB	InnoDB	InnoDB	InnoDB	0	-1"
values=$(run_mysql \
    "SELECT 1; SELECT @@default_storage_engine, @@global.default_storage_engine, \
     @@session.default_storage_engine, @@local.default_storage_engine, \
     @@warning_count, ROW_COUNT();" \
    | tail -n 1)
expect_value "default storage engine variables and diagnostics" "$expected_values" "$values"

expected_headers=$(cat <<EOF
@@default_storage_engine	@@global.default_storage_engine	@@session.\`default_storage_engine\`	@@\`default_storage_engine\`
InnoDB	InnoDB	InnoDB	InnoDB
EOF
)
expect_output_with_headers \
    "default storage engine labels preserve source text" \
    "$expected_headers" \
    "SELECT @@default_storage_engine, @@global.default_storage_engine, \
     @@session.\`default_storage_engine\`, @@\`default_storage_engine\`;"

expect_output \
    "case-insensitive default storage engine variables" \
    "InnoDB	InnoDB" \
    "SELECT @@DEFAULT_STORAGE_ENGINE, @@Global.Default_Storage_Engine;"

expect_output \
    "from dual returns default storage engine" \
    "InnoDB" \
    "SELECT @@default_storage_engine FROM DUAL;"

mutable_values=$(run_mysql \
    "SELECT @@default_storage_engine, @@global.default_storage_engine; \
     SET SESSION default_storage_engine=MEMORY; \
     SELECT @@default_storage_engine, @@global.default_storage_engine, \
            @@session.default_storage_engine, @@local.default_storage_engine, \
            @@warning_count, ROW_COUNT(); \
     SET SESSION default_storage_engine=InnoDB;" \
    | tail -n 1)
expect_value \
    "mysql session default storage engine is mutable upstream" \
    "MEMORY	InnoDB	MEMORY	MEMORY	0	0" \
    "$mutable_values"

warning_values=$(run_mysql \
    "SELECT 1; SHOW PROCESSLIST; \
     SELECT @@default_storage_engine, @@warning_count, @@error_count, ROW_COUNT(); \
     SHOW COUNT(*) WARNINGS;" \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "default storage engine variable reads and clears warning diagnostics" \
    "InnoDB	1	0	-1|0|" \
    "$warning_values"

parse_error_values=$(printf '%s\n' \
    'BAD SQL; SELECT @@default_storage_engine, @@warning_count, @@error_count, ROW_COUNT(); SHOW COUNT(*) WARNINGS;' \
    | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS --skip-column-names --force 2>/dev/null \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "default storage engine variable reads and clears error diagnostics" \
    "InnoDB	1	1	-1|0|" \
    "$parse_error_values"

expect_error \
    "unknown unscoped default storage engine variable" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_default_storage_engine_variable'" \
    "SELECT @@no_such_default_storage_engine_variable;"

expect_error \
    "unknown scoped default storage engine variable" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_default_storage_engine_variable'" \
    "SELECT @@global.no_such_default_storage_engine_variable;"

expect_error \
    "quoted default storage engine variable scope is syntax error" \
    1064 \
    42000 \
    "syntax" \
    "SELECT @@\`session\`.default_storage_engine;"

expect_output \
    "mysql accepts expressions outside this mylite slice" \
    "1" \
    "SELECT @@default_storage_engine + 1;"
