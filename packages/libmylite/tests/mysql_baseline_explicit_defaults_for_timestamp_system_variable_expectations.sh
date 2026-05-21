#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"

fail() {
    printf '%s\n' "mysql_baseline_explicit_defaults_for_timestamp_system_variable_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_BIN" ]; then
        if [ -n "$MYSQL_SOCKET" ]; then
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                    --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
        else
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=TCP -h127.0.0.1 -uroot \
                    --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
        fi
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
                --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
    fi
}

run_mysql_with_headers() {
    sql=$1
    shift
    if [ -n "$MYSQL_BIN" ]; then
        if [ -n "$MYSQL_SOCKET" ]; then
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                    --batch --raw --default-character-set=utf8mb4 "$@"
        else
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=TCP -h127.0.0.1 -uroot \
                    --batch --raw --default-character-set=utf8mb4 "$@"
        fi
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
                --batch --raw --default-character-set=utf8mb4 "$@"
    fi
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

run_mysql "SET SESSION explicit_defaults_for_timestamp=DEFAULT;" >/dev/null

expected_values="1	1	1	1	0	0	-1"
values=$(run_mysql \
    "SELECT 1; SELECT @@explicit_defaults_for_timestamp, \
     @@global.explicit_defaults_for_timestamp, \
     @@session.explicit_defaults_for_timestamp, \
     @@local.explicit_defaults_for_timestamp, @@warning_count, @@error_count, ROW_COUNT();" \
    | tail -n 1)
expect_value "explicit_defaults_for_timestamp variables and diagnostics" "$expected_values" "$values"

expected_headers=$(cat <<EOF
@@explicit_defaults_for_timestamp	@@global.explicit_defaults_for_timestamp	@@session.\`explicit_defaults_for_timestamp\`	@@\`explicit_defaults_for_timestamp\`
1	1	1	1
EOF
)
expect_output_with_headers \
    "explicit_defaults_for_timestamp labels preserve source text" \
    "$expected_headers" \
    "SELECT @@explicit_defaults_for_timestamp, @@global.explicit_defaults_for_timestamp, \
     @@session.\`explicit_defaults_for_timestamp\`, @@\`explicit_defaults_for_timestamp\`;"

expect_output \
    "case-insensitive explicit_defaults_for_timestamp variables" \
    "1	1" \
    "SELECT @@EXPLICIT_DEFAULTS_FOR_TIMESTAMP, @@Global.Explicit_Defaults_For_Timestamp;"

expect_output \
    "from dual returns explicit_defaults_for_timestamp" \
    "1" \
    "SELECT @@explicit_defaults_for_timestamp FROM DUAL;"

expect_output \
    "show variables returns explicit_defaults_for_timestamp" \
    "explicit_defaults_for_timestamp	ON" \
    "SHOW VARIABLES LIKE 'explicit_defaults_for_timestamp';"

expect_output \
    "show global variables returns explicit_defaults_for_timestamp" \
    "explicit_defaults_for_timestamp	ON" \
    "SHOW GLOBAL VARIABLES LIKE 'explicit_defaults_for_timestamp';"

expect_output \
    "show variables where returns explicit_defaults_for_timestamp" \
    "explicit_defaults_for_timestamp	ON" \
    "SHOW VARIABLES WHERE Variable_name = 'explicit_defaults_for_timestamp';"

no_op_values=$(run_mysql \
    "SET SESSION explicit_defaults_for_timestamp=DEFAULT; \
     SET SESSION explicit_defaults_for_timestamp=ON; \
     SELECT @@explicit_defaults_for_timestamp, @@warning_count, @@error_count, ROW_COUNT(); \
     SET SESSION explicit_defaults_for_timestamp=1; \
     SELECT @@explicit_defaults_for_timestamp, @@warning_count, @@error_count, ROW_COUNT(); \
     SET SESSION explicit_defaults_for_timestamp=TRUE; \
     SELECT @@explicit_defaults_for_timestamp, @@warning_count, @@error_count, ROW_COUNT(); \
     SET SESSION explicit_defaults_for_timestamp=DEFAULT; \
     SELECT @@explicit_defaults_for_timestamp, @@warning_count, @@error_count, ROW_COUNT();" \
    | tail -n 4 \
    | tr '\n' '|')
expect_value \
    "explicit_defaults_for_timestamp no-op assignments preserve ON" \
    "1	0	0	0|1	0	0	0|1	0	0	0|1	0	0	0|" \
    "$no_op_values"

off_values=$(run_mysql \
    "SET SESSION explicit_defaults_for_timestamp=DEFAULT; \
     SET SESSION explicit_defaults_for_timestamp=OFF; \
     SHOW WARNINGS; \
     SELECT @@explicit_defaults_for_timestamp, @@warning_count, @@error_count, ROW_COUNT(); \
     SET SESSION explicit_defaults_for_timestamp=DEFAULT;" \
    | tr '\n' '|')
expect_value \
    "mysql accepts deprecated explicit_defaults_for_timestamp OFF with warning" \
    "Warning	1287	'explicit_defaults_for_timestamp' is deprecated and will be removed in a future release.|0	1	0	-1|" \
    "$off_values"

expect_error \
    "explicit_defaults_for_timestamp rejects integer 2" \
    1231 \
    42000 \
    "Variable 'explicit_defaults_for_timestamp' can't be set to the value of '2'" \
    "SET SESSION explicit_defaults_for_timestamp=2;"

expect_error \
    "explicit_defaults_for_timestamp rejects NULL" \
    1231 \
    42000 \
    "Variable 'explicit_defaults_for_timestamp' can't be set to the value of 'NULL'" \
    "SET SESSION explicit_defaults_for_timestamp=NULL;"

expect_error \
    "unknown unscoped explicit_defaults_for_timestamp variable" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_explicit_defaults_for_timestamp_variable'" \
    "SELECT @@no_such_explicit_defaults_for_timestamp_variable;"

expect_error \
    "unknown scoped explicit_defaults_for_timestamp variable" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_explicit_defaults_for_timestamp_variable'" \
    "SELECT @@global.no_such_explicit_defaults_for_timestamp_variable;"

expect_error \
    "quoted explicit_defaults_for_timestamp variable scope is syntax error" \
    1064 \
    42000 \
    "syntax" \
    "SELECT @@\`session\`.explicit_defaults_for_timestamp;"

expect_output \
    "mysql accepts expressions outside this mylite slice" \
    "2" \
    "SELECT @@explicit_defaults_for_timestamp + 1;"

printf '%s\n' "mysql_baseline_explicit_defaults_for_timestamp_system_variable_expectations: ok"
