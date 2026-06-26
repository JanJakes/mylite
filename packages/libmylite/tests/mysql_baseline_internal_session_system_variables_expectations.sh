#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_internal_session_system_variables_expectations: $1" >&2
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
        *)
            fail "$label: expected error $code/$state containing [$message], got [$output]"
            ;;
    esac
}

normalize_tsv() {
    sed "s/${TAB}/|/g"
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

scalar_values=$(
    run_mysql \
        "SELECT @@original_commit_timestamp, @@SESSION.original_commit_timestamp,
                @@original_server_version, @@LOCAL.original_server_version,
                @@proxy_user, @@pseudo_replica_mode, @@rbr_exec_mode,
                @@GLOBAL.rbr_exec_mode, @@transaction_allow_batching;"
)
expect_value \
    "default/session/global scalar values" \
    "36028797018963968${TAB}36028797018963968${TAB}999999${TAB}999999${TAB}NULL${TAB}0${TAB}STRICT${TAB}STRICT${TAB}0" \
    "$scalar_values"

expected_session_show="original_commit_timestamp|36028797018963968
original_server_version|999999
proxy_user|
pseudo_replica_mode|OFF
pseudo_slave_mode|OFF
rbr_exec_mode|STRICT
transaction_allow_batching|OFF"

for scope in "" "SESSION"; do
    show_rows=$(
        run_mysql \
            "SHOW $scope VARIABLES WHERE Variable_name IN (
             'original_commit_timestamp','original_server_version','proxy_user',
             'pseudo_replica_mode','pseudo_slave_mode','rbr_exec_mode',
             'transaction_allow_batching');" \
            | normalize_tsv
    )
    expect_value "show ${scope:-default} rows" "$expected_session_show" "$show_rows"
done

global_show=$(
    run_mysql \
        "SHOW GLOBAL VARIABLES WHERE Variable_name IN (
         'original_commit_timestamp','original_server_version','proxy_user',
         'pseudo_replica_mode','pseudo_slave_mode','rbr_exec_mode',
         'transaction_allow_batching');" \
        | normalize_tsv
)
expect_value "show global rows" "rbr_exec_mode|STRICT" "$global_show"

for variable in \
    original_commit_timestamp \
    original_server_version \
    proxy_user \
    pseudo_replica_mode \
    pseudo_slave_mode \
    transaction_allow_batching
do
    expect_error \
        "scalar global $variable" \
        1238 \
        HY000 \
        "Variable '$variable' is a SESSION variable" \
        "SELECT @@GLOBAL.$variable;"
done

for variable in \
    original_commit_timestamp \
    original_server_version \
    pseudo_replica_mode \
    pseudo_slave_mode \
    transaction_allow_batching
do
    expect_error \
        "set global $variable" \
        1228 \
        HY000 \
        "Variable '$variable' is a SESSION variable and can't be used with SET GLOBAL" \
        "SET GLOBAL $variable = DEFAULT;"
done

expect_error \
    "set global rbr_exec_mode" \
    1228 \
    HY000 \
    "Variable 'rbr_exec_mode' is a SESSION variable and can't be used with SET GLOBAL" \
    "SET GLOBAL rbr_exec_mode = DEFAULT;"

mutated_values=$(
    run_mysql \
        "SET SESSION original_commit_timestamp = 0;
         SET SESSION original_server_version = 80000;
         SET SESSION pseudo_replica_mode = ON;
         SET SESSION rbr_exec_mode = IDEMPOTENT;
         SET SESSION transaction_allow_batching = ON;
         SELECT @@original_commit_timestamp, @@original_server_version,
                @@pseudo_replica_mode, @@rbr_exec_mode,
                @@GLOBAL.rbr_exec_mode, @@transaction_allow_batching;"
)
expect_value \
    "mutated session values" \
    "0${TAB}80000${TAB}1${TAB}IDEMPOTENT${TAB}STRICT${TAB}1" \
    "$mutated_values"

reset_values=$(
    run_mysql \
        "SET SESSION original_commit_timestamp = DEFAULT;
         SET SESSION original_server_version = DEFAULT;
         SET SESSION pseudo_replica_mode = DEFAULT;
         SET SESSION rbr_exec_mode = DEFAULT;
         SET SESSION transaction_allow_batching = DEFAULT;
         SELECT @@original_commit_timestamp, @@original_server_version,
                @@pseudo_replica_mode, @@rbr_exec_mode, @@transaction_allow_batching;"
)
expect_value \
    "reset session values" \
    "36028797018963968${TAB}999999${TAB}0${TAB}STRICT${TAB}0" \
    "$reset_values"

warning=$(
    run_mysql \
        "SET SESSION pseudo_slave_mode = ON;
         SHOW WARNINGS LIMIT 1;" \
        | normalize_tsv
)
case "$warning" in
    *"Warning|1287|'@@pseudo_slave_mode' is deprecated"*"pseudo_replica_mode"*) ;;
    *) fail "pseudo_slave_mode SET warning: got [$warning]" ;;
esac

read_warning=$(
    run_mysql \
        "SELECT @@pseudo_slave_mode;
         SHOW WARNINGS LIMIT 1;" \
        | normalize_tsv
)
case "$read_warning" in
    "0"*"$(
        printf '\n'
    )"*"Warning|1287|'@@pseudo_slave_mode' is deprecated"*"pseudo_replica_mode"*) ;;
    *) fail "pseudo_slave_mode read warning: got [$read_warning]" ;;
esac

expect_error \
    "set proxy_user read only" \
    1238 \
    HY000 \
    "Variable 'proxy_user' is a read only variable" \
    "SET SESSION proxy_user = DEFAULT;"
expect_error \
    "set global proxy_user read only" \
    1238 \
    HY000 \
    "Variable 'proxy_user' is a read only variable" \
    "SET GLOBAL proxy_user = DEFAULT;"
expect_error \
    "set invalid rbr_exec_mode" \
    1231 \
    42000 \
    "Variable 'rbr_exec_mode' can't be set to the value of 'bad'" \
    "SET SESSION rbr_exec_mode = 'bad';"
expect_error \
    "set invalid pseudo_replica_mode" \
    1231 \
    42000 \
    "Variable 'pseudo_replica_mode' can't be set to the value of '2'" \
    "SET SESSION pseudo_replica_mode = 2;"
expect_error \
    "set invalid original_commit_timestamp" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'original_commit_timestamp'" \
    "SET SESSION original_commit_timestamp = 'bad';"

printf '%s\n' "mysql_baseline_internal_session_system_variables_expectations: ok"
