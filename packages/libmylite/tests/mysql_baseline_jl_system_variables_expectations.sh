#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_jl_system_variables_expectations: $1" >&2
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

    set +e
    output=$(run_mysql "$sql" 2>&1)
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

normalize_tsv() {
    sed "s/${TAB}/|/g"
}

reset_defaults() {
    run_mysql \
        "SET GLOBAL internal_tmp_mem_storage_engine = DEFAULT;
         SET internal_tmp_mem_storage_engine = DEFAULT;
         SET GLOBAL join_buffer_size = DEFAULT;
         SET join_buffer_size = DEFAULT;
         SET GLOBAL key_buffer_size = DEFAULT;
         SET GLOBAL key_cache_age_threshold = DEFAULT;
         SET GLOBAL key_cache_block_size = DEFAULT;
         SET GLOBAL key_cache_division_limit = DEFAULT;
         SET GLOBAL keyring_operations = DEFAULT;
         SET GLOBAL lc_messages = DEFAULT;
         SET lc_messages = DEFAULT;
         SET GLOBAL local_infile = DEFAULT;
         SET GLOBAL mandatory_roles = DEFAULT;" >/dev/null
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

trap reset_defaults EXIT
reset_defaults

defaults=$(
    run_mysql \
        "SELECT @@internal_tmp_mem_storage_engine, @@GLOBAL.internal_tmp_mem_storage_engine,
                @@SESSION.internal_tmp_mem_storage_engine,
                @@join_buffer_size, @@GLOBAL.join_buffer_size, @@SESSION.join_buffer_size,
                @@key_buffer_size, @@key_cache_age_threshold, @@key_cache_block_size,
                @@key_cache_division_limit, @@keyring_operations, @@large_files_support,
                @@large_page_size, @@large_pages, @@lc_messages, @@GLOBAL.lc_messages,
                @@local_infile, @@mandatory_roles;"
)
expect_value \
    "J/L defaults" \
    "TempTable${TAB}TempTable${TAB}TempTable${TAB}262144${TAB}262144${TAB}262144${TAB}8388608${TAB}300${TAB}1024${TAB}100${TAB}1${TAB}1${TAB}0${TAB}0${TAB}en_US${TAB}en_US${TAB}0${TAB}" \
    "$defaults"

while IFS='|' read -r variable show; do
    [ -n "$variable" ] || continue

    expected_show="$variable|$show"
    actual_show=$(run_mysql "SHOW VARIABLES LIKE '$variable';" | normalize_tsv)
    expect_value "$variable show" "$expected_show" "$actual_show"
    actual_global_show=$(run_mysql "SHOW GLOBAL VARIABLES LIKE '$variable';" | normalize_tsv)
    expect_value "$variable show global" "$expected_show" "$actual_global_show"
    actual_session_show=$(run_mysql "SHOW SESSION VARIABLES LIKE '$variable';" | normalize_tsv)
    expect_value "$variable show session" "$expected_show" "$actual_session_show"
done <<EOF
internal_tmp_mem_storage_engine|TempTable
join_buffer_size|262144
key_buffer_size|8388608
key_cache_age_threshold|300
key_cache_block_size|1024
key_cache_division_limit|100
keyring_operations|ON
large_files_support|ON
large_page_size|0
large_pages|OFF
lc_messages|en_US
lc_messages_dir|/usr/share/mysql-8.4/
local_infile|OFF
locked_in_memory|OFF
mandatory_roles|
EOF

for variable in \
    key_buffer_size \
    key_cache_age_threshold \
    key_cache_block_size \
    key_cache_division_limit \
    keyring_operations \
    large_files_support \
    large_page_size \
    large_pages \
    lc_messages_dir \
    local_infile \
    locked_in_memory \
    mandatory_roles
do
    expect_error \
        "$variable session scalar" \
        1238 \
        HY000 \
        "Variable '$variable' is a GLOBAL variable" \
        "SELECT @@SESSION.$variable;"
done

internal_tmp_sets=$(
    run_mysql \
        "SET internal_tmp_mem_storage_engine = MEMORY;
         SELECT @@internal_tmp_mem_storage_engine, @@GLOBAL.internal_tmp_mem_storage_engine;
         SET internal_tmp_mem_storage_engine = 1;
         SELECT @@internal_tmp_mem_storage_engine;
         SET internal_tmp_mem_storage_engine = 0;
         SELECT @@internal_tmp_mem_storage_engine;
         SET internal_tmp_mem_storage_engine = DEFAULT;
         SELECT @@internal_tmp_mem_storage_engine;" \
        | normalize_tsv
)
expect_value \
    "internal_tmp_mem_storage_engine assignments" \
    "MEMORY|TempTable
TempTable
MEMORY
TempTable" \
    "$internal_tmp_sets"
expect_error \
    "internal_tmp_mem_storage_engine invalid" \
    1231 \
    42000 \
    "Variable 'internal_tmp_mem_storage_engine' can't be set to the value of 'InnoDB'" \
    "SET internal_tmp_mem_storage_engine = InnoDB;"

join_sets=$(
    run_mysql \
        "SET join_buffer_size = 128;
         SELECT @@join_buffer_size, @@warning_count;
         SET join_buffer_size = 129;
         SHOW WARNINGS LIMIT 1;
         SELECT @@join_buffer_size, @@warning_count;
         SET join_buffer_size = TRUE;
         SHOW WARNINGS LIMIT 1;
         SELECT @@join_buffer_size, @@warning_count;
         SET @join_value = 262272;
         SET join_buffer_size = @join_value;
         SELECT @@join_buffer_size, @@warning_count;" \
        | normalize_tsv
)
expect_value \
    "join_buffer_size assignments" \
    "128|0
Warning|1292|Truncated incorrect join_buffer_size value: '129'
128|1
Warning|1292|Truncated incorrect join_buffer_size value: '1'
128|1
262272|0" \
    "$join_sets"
expect_error \
    "join_buffer_size string assignment" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'join_buffer_size'" \
    "SET join_buffer_size = '262144';"
expect_error \
    "join_buffer_size ON assignment" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'join_buffer_size'" \
    "SET join_buffer_size = ON;"

lc_messages_sets=$(
    run_mysql \
        "SET lc_messages = en_US;
         SET GLOBAL lc_messages = en_US;
         SET lc_messages = DEFAULT;
         SELECT @@lc_messages, @@GLOBAL.lc_messages;"
)
expect_value "lc_messages no-op assignments" "en_US${TAB}en_US" "$lc_messages_sets"
expect_error \
    "lc_messages unknown locale" \
    1649 \
    HY000 \
    "Unknown locale: 'bogus'" \
    "SET lc_messages = bogus;"

expect_error \
    "key_buffer_size set global-only" \
    1229 \
    HY000 \
    "Variable 'key_buffer_size' is a GLOBAL variable and should be set with SET GLOBAL" \
    "SET key_buffer_size = DEFAULT;"
expect_error \
    "large_files_support read-only SET" \
    1238 \
    HY000 \
    "Variable 'large_files_support' is a read only variable" \
    "SET GLOBAL large_files_support = DEFAULT;"
expect_error \
    "large_page_size read-only SET" \
    1238 \
    HY000 \
    "Variable 'large_page_size' is a read only variable" \
    "SET GLOBAL large_page_size = DEFAULT;"
expect_error \
    "large_pages read-only SET" \
    1238 \
    HY000 \
    "Variable 'large_pages' is a read only variable" \
    "SET GLOBAL large_pages = DEFAULT;"
expect_error \
    "lc_messages_dir read-only SET" \
    1238 \
    HY000 \
    "Variable 'lc_messages_dir' is a read only variable" \
    "SET GLOBAL lc_messages_dir = DEFAULT;"
expect_error \
    "locked_in_memory read-only SET" \
    1238 \
    HY000 \
    "Variable 'locked_in_memory' is a read only variable" \
    "SET GLOBAL locked_in_memory = DEFAULT;"

global_noops=$(
    run_mysql \
        "SET GLOBAL key_buffer_size = DEFAULT;
         SET GLOBAL key_buffer_size = 8388608;
         SET GLOBAL key_cache_age_threshold = DEFAULT;
         SET GLOBAL key_cache_block_size = DEFAULT;
         SET GLOBAL key_cache_division_limit = DEFAULT;
         SET GLOBAL keyring_operations = DEFAULT;
         SET GLOBAL local_infile = DEFAULT;
         SET GLOBAL mandatory_roles = DEFAULT;
         SELECT @@GLOBAL.key_buffer_size, @@GLOBAL.key_cache_age_threshold,
                @@GLOBAL.key_cache_block_size, @@GLOBAL.key_cache_division_limit,
                @@GLOBAL.keyring_operations, @@GLOBAL.local_infile, @@GLOBAL.mandatory_roles;"
)
expect_value \
    "fixed placeholder global no-ops" \
    "8388608${TAB}300${TAB}1024${TAB}100${TAB}1${TAB}0${TAB}" \
    "$global_noops"

global_mutation=$(
    run_mysql \
        "SET GLOBAL internal_tmp_mem_storage_engine = MEMORY;
         SET GLOBAL join_buffer_size = 262272;
         SELECT @@GLOBAL.internal_tmp_mem_storage_engine, @@GLOBAL.join_buffer_size,
                @@warning_count, @@error_count;"
)
expect_value "mysql mutable global observation" "MEMORY${TAB}262272${TAB}0${TAB}0" "$global_mutation"

reset_defaults

printf '%s\n' "mysql_baseline_jl_system_variables_expectations: ok"
