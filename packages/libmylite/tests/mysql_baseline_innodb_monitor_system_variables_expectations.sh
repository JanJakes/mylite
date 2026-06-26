#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_innodb_monitor_system_variables_expectations: $1" >&2
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

variables() {
    cat <<'EOF'
innodb_monitor_disable
innodb_monitor_enable
innodb_monitor_reset
innodb_monitor_reset_all
EOF
}

reset_defaults() {
    variables | while IFS= read -r variable; do
        run_mysql "SET GLOBAL $variable = DEFAULT;" >/dev/null
    done
}

check_default_readbacks() {
    variable=$1
    expected_show="$variable|"

    actual_scalar=$(run_mysql "SELECT @@$variable, @@GLOBAL.$variable;" | normalize_tsv)
    expect_value "$variable scalar default" "NULL|NULL" "$actual_scalar"
    actual_show=$(run_mysql "SHOW VARIABLES LIKE '$variable';" | normalize_tsv)
    expect_value "$variable show" "$expected_show" "$actual_show"
    actual_global_show=$(run_mysql "SHOW GLOBAL VARIABLES LIKE '$variable';" | normalize_tsv)
    expect_value "$variable show global" "$expected_show" "$actual_global_show"
    actual_session_show=$(run_mysql "SHOW SESSION VARIABLES LIKE '$variable';" | normalize_tsv)
    expect_value "$variable show session" "$expected_show" "$actual_session_show"
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

trap reset_defaults EXIT
reset_defaults

variables | while IFS= read -r variable; do
    check_default_readbacks "$variable"
    expect_error \
        "$variable session scalar" \
        1238 \
        HY000 \
        "Variable '$variable' is a GLOBAL variable" \
        "SELECT @@SESSION.$variable;"
    expect_error \
        "$variable local scalar" \
        1238 \
        HY000 \
        "Variable '$variable' is a GLOBAL variable" \
        "SELECT @@LOCAL.$variable;"
    expect_error \
        "$variable global-only unscoped SET" \
        1229 \
        HY000 \
        "Variable '$variable' is a GLOBAL variable and should be set with SET GLOBAL" \
        "SET $variable = DEFAULT;"
    expect_error \
        "$variable global-only session SET" \
        1229 \
        HY000 \
        "Variable '$variable' is a GLOBAL variable and should be set with SET GLOBAL" \
        "SET SESSION $variable = DEFAULT;"
    expect_error \
        "$variable global-only local SET" \
        1229 \
        HY000 \
        "Variable '$variable' is a GLOBAL variable and should be set with SET GLOBAL" \
        "SET LOCAL $variable = DEFAULT;"

    run_mysql "SET GLOBAL $variable = DEFAULT;" >/dev/null
    check_default_readbacks "$variable"
    run_mysql "SET GLOBAL $variable = 'all';" >/dev/null
    run_mysql "SET GLOBAL $variable = 'latch';" >/dev/null
    run_mysql "SET GLOBAL $variable = 'module_buffer';" >/dev/null
    run_mysql "SET GLOBAL $variable = 'buffer%';" >/dev/null
    run_mysql "SET GLOBAL $variable = all;" >/dev/null
    run_mysql "SET GLOBAL $variable = latch;" >/dev/null
    run_mysql "SET GLOBAL $variable = module_buffer;" >/dev/null
    expect_error \
        "$variable null SET" \
        1231 \
        42000 \
        "Variable '$variable' can't be set to the value of 'NULL'" \
        "SET GLOBAL $variable = NULL;"
    expect_error \
        "$variable integer SET" \
        1232 \
        42000 \
        "Incorrect argument type to variable '$variable'" \
        "SET GLOBAL $variable = 1;"
    expect_error \
        "$variable bogus SET" \
        1231 \
        42000 \
        "Variable '$variable' can't be set to the value of 'bogus'" \
        "SET GLOBAL $variable = 'bogus';"
    run_mysql "SET GLOBAL $variable = DEFAULT;" >/dev/null
    check_default_readbacks "$variable"
done

printf '%s\n' "mysql_baseline_innodb_monitor_system_variables_expectations: ok"
