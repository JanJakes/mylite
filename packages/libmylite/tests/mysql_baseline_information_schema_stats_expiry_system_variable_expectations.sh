#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DEFAULT_VALUE=86400
MAX_VALUE=31536000

fail() {
    printf '%s\n' \
        "mysql_baseline_information_schema_stats_expiry_system_variable_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
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
        *) fail "$label: expected error $code/$state containing [$message], got [$output]" ;;
    esac
}

cleanup() {
    run_mysql "SET GLOBAL information_schema_stats_expiry = DEFAULT;" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup

readback_expected=$(cat <<EXPECTED
default	${DEFAULT_VALUE}	${DEFAULT_VALUE}	${DEFAULT_VALUE}	${DEFAULT_VALUE}
information_schema_stats_expiry	${DEFAULT_VALUE}
information_schema_stats_expiry	${DEFAULT_VALUE}
information_schema_stats_expiry	${DEFAULT_VALUE}
session0	0	${DEFAULT_VALUE}	0	0	0
default_again	${DEFAULT_VALUE}	0
local_plus7	7
direct8	8
direct_session9	9
direct_local10	10
paren11	11
true1	1
false0	0
usermax	${MAX_VALUE}
global_noop	${MAX_VALUE}	${DEFAULT_VALUE}
EXPECTED
)
expect_output \
    "readback and supported assignments" \
    "$readback_expected" \
    "SELECT 'default', @@information_schema_stats_expiry, "\
"@@global.information_schema_stats_expiry, @@session.information_schema_stats_expiry, "\
"@@local.information_schema_stats_expiry; "\
"SHOW VARIABLES LIKE 'information_schema_stats_expiry'; "\
"SHOW SESSION VARIABLES LIKE 'information_schema_stats_expiry'; "\
"SHOW GLOBAL VARIABLES LIKE 'information_schema_stats_expiry'; "\
"SET SESSION information_schema_stats_expiry = 0; "\
"SELECT 'session0', @@information_schema_stats_expiry, "\
"@@GLOBAL.information_schema_stats_expiry, @@SESSION.information_schema_stats_expiry, "\
"@@LOCAL.information_schema_stats_expiry, @@warning_count; "\
"SET SESSION information_schema_stats_expiry = DEFAULT; "\
"SELECT 'default_again', @@information_schema_stats_expiry, @@warning_count; "\
"SET LOCAL information_schema_stats_expiry = +7; "\
"SELECT 'local_plus7', @@information_schema_stats_expiry; "\
"SET @@information_schema_stats_expiry = 8; "\
"SELECT 'direct8', @@information_schema_stats_expiry; "\
"SET @@SESSION.information_schema_stats_expiry = 9; "\
"SELECT 'direct_session9', @@information_schema_stats_expiry; "\
"SET @@LOCAL.information_schema_stats_expiry = 10; "\
"SELECT 'direct_local10', @@information_schema_stats_expiry; "\
"SET SESSION information_schema_stats_expiry = (11); "\
"SELECT 'paren11', @@information_schema_stats_expiry; "\
"SET information_schema_stats_expiry = TRUE; "\
"SELECT 'true1', @@information_schema_stats_expiry; "\
"SET information_schema_stats_expiry = FALSE; "\
"SELECT 'false0', @@information_schema_stats_expiry; "\
"SET @stats_expiry = ${MAX_VALUE}; "\
"SET information_schema_stats_expiry = @stats_expiry; "\
"SELECT 'usermax', @@information_schema_stats_expiry; "\
"SET GLOBAL information_schema_stats_expiry = DEFAULT; "\
"SET @@GLOBAL.information_schema_stats_expiry = ${DEFAULT_VALUE}; "\
"SELECT 'global_noop', @@information_schema_stats_expiry, "\
"@@GLOBAL.information_schema_stats_expiry;"

clamp_expected=$(cat <<EXPECTED
Warning	1292	Truncated incorrect information_schema_stats_expiry value: '-1'
clamp_neg1	0
Warning	1292	Truncated incorrect information_schema_stats_expiry value: '$((MAX_VALUE + 1))'
clamp_max_plus1	${MAX_VALUE}
Warning	1292	Truncated incorrect information_schema_stats_expiry value: '-2'
clamp_user_neg	0
EXPECTED
)
expect_output \
    "clamp warnings" \
    "$clamp_expected" \
    "SET SESSION information_schema_stats_expiry = -1; SHOW WARNINGS; "\
"SELECT 'clamp_neg1', @@information_schema_stats_expiry; "\
"SET SESSION information_schema_stats_expiry = $((MAX_VALUE + 1)); SHOW WARNINGS; "\
"SELECT 'clamp_max_plus1', @@information_schema_stats_expiry; "\
"SET @stats_expiry = -2; "\
"SET SESSION information_schema_stats_expiry = @stats_expiry; SHOW WARNINGS; "\
"SELECT 'clamp_user_neg', @@information_schema_stats_expiry;"

expect_error \
    "string rejected" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'information_schema_stats_expiry'" \
    "SET SESSION information_schema_stats_expiry = '5';"
expect_error \
    "decimal rejected" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'information_schema_stats_expiry'" \
    "SET SESSION information_schema_stats_expiry = 1.5;"
expect_error \
    "null rejected" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'information_schema_stats_expiry'" \
    "SET SESSION information_schema_stats_expiry = NULL;"
expect_error \
    "on rejected" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'information_schema_stats_expiry'" \
    "SET SESSION information_schema_stats_expiry = ON;"
expect_error \
    "off rejected" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'information_schema_stats_expiry'" \
    "SET SESSION information_schema_stats_expiry = OFF;"
expect_error \
    "unsigned overflow rejected" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'information_schema_stats_expiry'" \
    "SET SESSION information_schema_stats_expiry = 18446744073709551616;"
expect_error \
    "string user variable rejected" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'information_schema_stats_expiry'" \
    "SET @stats_expiry = '5'; SET SESSION information_schema_stats_expiry = @stats_expiry;"

printf '%s\n' "mysql_baseline_information_schema_stats_expiry_system_variable_expectations: ok"
