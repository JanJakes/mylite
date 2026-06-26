#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_innodb_dirty_purge_system_variables_expectations: $1" >&2
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
innodb_max_dirty_pages_pct|90.000000|90.000000|90|91|decimal
innodb_max_dirty_pages_pct_lwm|10.000000|10.000000|10|11|decimal
innodb_max_purge_lag|0|0|0|1|integer
innodb_max_purge_lag_delay|0|0|0|1|integer
innodb_max_undo_log_size|1073741824|1073741824|1073741824|1073741825|integer
EOF
}

reset_defaults() {
    variables | while IFS='|' read -r variable _scalar _show _exact _bad _kind; do
        run_mysql "SET GLOBAL $variable = DEFAULT;" >/dev/null
    done
}

check_show_values() {
    variable=$1
    scalar=$2
    show=$3
    expected_pair="$scalar|$scalar"
    expected_show="$variable|$show"

    actual_pair=$(run_mysql "SELECT @@$variable, @@GLOBAL.$variable;" | normalize_tsv)
    expect_value "$variable scalar" "$expected_pair" "$actual_pair"
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

variables | while IFS='|' read -r variable scalar show exact bad kind; do
    check_show_values "$variable" "$scalar" "$show"
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
    run_mysql "SET GLOBAL $variable = $exact;" >/dev/null
    run_mysql "SET GLOBAL $variable = $bad; SET GLOBAL $variable = DEFAULT;" >/dev/null
    if [ "$kind" = "decimal" ]; then
        decimal_exact="${exact}.000000"
        actual_decimal=$(
            run_mysql "SET GLOBAL $variable = $decimal_exact; \
                       SET @mylite_v = $decimal_exact; \
                       SET GLOBAL $variable = @mylite_v; \
                       SELECT @@GLOBAL.$variable; \
                       SET GLOBAL $variable = DEFAULT;" \
                | normalize_tsv
        )
        expect_value "$variable decimal exact assignment" "$decimal_exact" "$actual_decimal"
    fi
done

printf '%s\n' "mysql_baseline_innodb_dirty_purge_system_variables_expectations: ok"
