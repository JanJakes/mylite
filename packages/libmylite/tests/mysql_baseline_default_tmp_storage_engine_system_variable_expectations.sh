#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_ARGS="--protocol=TCP -h127.0.0.1 -uroot --batch --raw --default-character-set=utf8mb4"
DATABASE="mylite_default_tmp_storage_engine_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_default_tmp_storage_engine_system_variable_expectations: $1" >&2
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

cleanup() {
    printf 'DROP DATABASE IF EXISTS `%s`;\n' "$DATABASE" \
        | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS --skip-column-names >/dev/null 2>&1 || true
}

trap cleanup EXIT HUP INT TERM

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

expected_values="InnoDB	InnoDB	InnoDB	InnoDB	0	-1"
values=$(run_mysql \
    "SELECT 1; SELECT @@default_tmp_storage_engine, @@global.default_tmp_storage_engine, \
     @@session.default_tmp_storage_engine, @@local.default_tmp_storage_engine, \
     @@warning_count, ROW_COUNT();" \
    | tail -n 1)
expect_value "default temporary storage engine variables and diagnostics" "$expected_values" "$values"

expect_output \
    "show variables includes default temporary storage engine" \
    "default_tmp_storage_engine	InnoDB" \
    "SHOW VARIABLES LIKE 'default_tmp_storage_engine';"

expect_output \
    "show global variables includes default temporary storage engine" \
    "default_tmp_storage_engine	InnoDB" \
    "SHOW GLOBAL VARIABLES LIKE 'default_tmp_storage_engine';"

expected_headers=$(cat <<EOF
@@default_tmp_storage_engine	@@global.default_tmp_storage_engine	@@session.\`default_tmp_storage_engine\`	@@\`default_tmp_storage_engine\`
InnoDB	InnoDB	InnoDB	InnoDB
EOF
)
expect_output_with_headers \
    "default temporary storage engine labels preserve source text" \
    "$expected_headers" \
    "SELECT @@default_tmp_storage_engine, @@global.default_tmp_storage_engine, \
     @@session.\`default_tmp_storage_engine\`, @@\`default_tmp_storage_engine\`;"

expect_output \
    "case-insensitive default temporary storage engine variables" \
    "InnoDB	InnoDB" \
    "SELECT @@DEFAULT_TMP_STORAGE_ENGINE, @@Global.Default_Tmp_Storage_Engine;"

expect_output \
    "from dual returns default temporary storage engine" \
    "InnoDB" \
    "SELECT @@default_tmp_storage_engine FROM DUAL;"

mutable_values=$(run_mysql \
    "SELECT @@default_tmp_storage_engine, @@global.default_tmp_storage_engine; \
     SET SESSION default_tmp_storage_engine=MEMORY; \
     SELECT @@default_tmp_storage_engine, @@global.default_tmp_storage_engine, \
            @@session.default_tmp_storage_engine, @@local.default_tmp_storage_engine, \
            @@warning_count, ROW_COUNT(); \
     SET SESSION default_tmp_storage_engine=InnoDB;" \
    | tail -n 1)
expect_value \
    "mysql session default temporary storage engine is mutable upstream" \
    "MEMORY	InnoDB	MEMORY	MEMORY	0	0" \
    "$mutable_values"

no_op_values=$(run_mysql \
    "SET SESSION default_tmp_storage_engine=MEMORY; \
     SET SESSION default_tmp_storage_engine=DEFAULT; \
     SELECT 'default', @@default_tmp_storage_engine, @@warning_count, @@error_count, ROW_COUNT(); \
     SET @@default_tmp_storage_engine='InnoDB'; \
     SELECT 'direct', @@default_tmp_storage_engine, @@warning_count, @@error_count, ROW_COUNT(); \
     SET LOCAL default_tmp_storage_engine=InnoDB; \
     SELECT 'local', @@default_tmp_storage_engine, @@warning_count, @@error_count, ROW_COUNT();" \
    | tail -n 3)
expected_no_op_values=$(cat <<EOF
default	InnoDB	0	0	0
direct	InnoDB	0	0	0
local	InnoDB	0	0	0
EOF
)
expect_value \
    "mysql default temporary storage engine no-op assignments" \
    "$expected_no_op_values" \
    "$no_op_values"

temp_create_memory=$(run_mysql \
    "DROP DATABASE IF EXISTS \`${DATABASE}\`; CREATE DATABASE \`${DATABASE}\`; USE \`${DATABASE}\`; \
     SET SESSION default_tmp_storage_engine=MEMORY; \
     CREATE TEMPORARY TABLE t_memory(id INT); SHOW CREATE TABLE t_memory; \
     DROP TEMPORARY TABLE t_memory; SET SESSION default_tmp_storage_engine=InnoDB;" \
    | tail -n 4)
expected_temp_create_memory=$(cat <<EOF
t_memory	CREATE TEMPORARY TABLE \`t_memory\` (
  \`id\` int DEFAULT NULL
) ENGINE=MEMORY DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EOF
)
expect_value \
    "mysql default temporary storage engine affects implicit MEMORY temp table" \
    "$expected_temp_create_memory" \
    "$temp_create_memory"

temp_create_myisam=$(run_mysql \
    "USE \`${DATABASE}\`; SET SESSION default_tmp_storage_engine=MyISAM; \
     CREATE TEMPORARY TABLE t_myisam(id INT); SHOW CREATE TABLE t_myisam; \
     DROP TEMPORARY TABLE t_myisam; SET SESSION default_tmp_storage_engine=InnoDB;" \
    | tail -n 4)
expected_temp_create_myisam=$(cat <<EOF
t_myisam	CREATE TEMPORARY TABLE \`t_myisam\` (
  \`id\` int DEFAULT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EOF
)
expect_value \
    "mysql default temporary storage engine affects implicit MyISAM temp table" \
    "$expected_temp_create_myisam" \
    "$temp_create_myisam"

expect_error \
    "unknown default temporary storage engine rejected" \
    1286 \
    42000 \
    "Unknown storage engine 'NoSuchEngine'" \
    "SET SESSION default_tmp_storage_engine=NoSuchEngine;"

expect_error \
    "empty default temporary storage engine rejected" \
    1286 \
    42000 \
    "Unknown storage engine ''" \
    "SET SESSION default_tmp_storage_engine='';"

expect_error \
    "loose sql mode still rejects unknown default temporary storage engine" \
    1286 \
    42000 \
    "Unknown storage engine 'NoSuchEngine'" \
    "SET SESSION sql_mode=''; SET SESSION default_tmp_storage_engine=NoSuchEngine;"

warning_values=$(run_mysql \
    "SELECT 1; SHOW PROCESSLIST; \
     SELECT @@default_tmp_storage_engine, @@warning_count, @@error_count, ROW_COUNT(); \
     SHOW COUNT(*) WARNINGS;" \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "default temporary storage engine variable reads and clears warning diagnostics" \
    "InnoDB	1	0	-1|0|" \
    "$warning_values"

parse_error_values=$(printf '%s\n' \
    'BAD SQL; SELECT @@default_tmp_storage_engine, @@warning_count, @@error_count, ROW_COUNT(); SHOW COUNT(*) WARNINGS;' \
    | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS --skip-column-names --force 2>/dev/null \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "default temporary storage engine variable reads and clears error diagnostics" \
    "InnoDB	1	1	-1|0|" \
    "$parse_error_values"

expect_error \
    "unknown default temporary storage engine variable" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_default_tmp_storage_engine_variable'" \
    "SELECT @@no_such_default_tmp_storage_engine_variable;"

expect_error \
    "quoted default temporary storage engine variable scope is syntax error" \
    1064 \
    42000 \
    "syntax" \
    "SELECT @@\`session\`.default_tmp_storage_engine;"

expect_output \
    "mysql accepts expressions outside this mylite slice" \
    "1" \
    "SELECT @@default_tmp_storage_engine + 1;"

printf '%s\n' "mysql_baseline_default_tmp_storage_engine_system_variable_expectations: ok"
