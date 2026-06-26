#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_innodb_fulltext_system_variables_expectations: $1" >&2
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

global_dynamic_variables() {
    cat <<'EOF'
innodb_ft_aux_table|NULL||NULL|nullable
innodb_ft_enable_diag_print|0|OFF|OFF|boolean
innodb_ft_num_word_optimize|2000|2000|2000|integer
innodb_ft_result_cache_limit|2000000000|2000000000|2000000000|integer
innodb_ft_server_stopword_table|NULL||NULL|nullable
EOF
}

read_only_variables() {
    cat <<'EOF'
innodb_ft_cache_size|8000000|8000000
innodb_ft_max_token_size|84|84
innodb_ft_min_token_size|3|3
innodb_ft_sort_pll_degree|2|2
innodb_ft_total_cache_size|640000000|640000000
EOF
}

session_variables() {
    cat <<'EOF'
innodb_ft_enable_stopword|1|ON|ON|boolean
innodb_ft_user_stopword_table|NULL||NULL|nullable
EOF
}

reset_defaults() {
    global_dynamic_variables | while IFS='|' read -r variable _scalar _show _exact _kind; do
        run_mysql "SET GLOBAL $variable = DEFAULT;" >/dev/null
    done
    session_variables | while IFS='|' read -r variable _scalar _show _exact _kind; do
        run_mysql "SET GLOBAL $variable = DEFAULT; SET SESSION $variable = DEFAULT;" >/dev/null
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

global_dynamic_variables | while IFS='|' read -r variable scalar show exact kind; do
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
    if [ "$kind" = "nullable" ]; then
        expect_error \
            "$variable rejects unresolved table" \
            1231 \
            42000 \
            "can't be set to the value" \
            "SET GLOBAL $variable = 'test/no_such_table';"
    fi
done

read_only_variables | while IFS='|' read -r variable scalar show; do
    check_show_values "$variable" "$scalar" "$show"
    expect_error \
        "$variable session scalar" \
        1238 \
        HY000 \
        "Variable '$variable' is a GLOBAL variable" \
        "SELECT @@SESSION.$variable;"
    expect_error \
        "$variable read-only SET" \
        1238 \
        HY000 \
        "Variable '$variable' is a read only variable" \
        "SET GLOBAL $variable = DEFAULT;"
done

session_variables | while IFS='|' read -r variable scalar show exact kind; do
    check_show_values "$variable" "$scalar" "$show"
    actual_session=$(run_mysql "SELECT @@SESSION.$variable, @@LOCAL.$variable;" | normalize_tsv)
    expect_value "$variable session scalar" "$scalar|$scalar" "$actual_session"

    run_mysql "SET $variable = DEFAULT; SET SESSION $variable = DEFAULT; SET LOCAL $variable = DEFAULT;" \
        >/dev/null
    run_mysql "SET GLOBAL $variable = DEFAULT; SET GLOBAL $variable = $exact;" >/dev/null
    if [ "$kind" = "boolean" ]; then
        actual_mutation=$(
            run_mysql "SET SESSION $variable = OFF; \
                       SELECT @@$variable, @@SESSION.$variable, @@GLOBAL.$variable; \
                       SHOW VARIABLES LIKE '$variable'; \
                       SHOW GLOBAL VARIABLES LIKE '$variable'; \
                       SET SESSION $variable = DEFAULT;" \
                | normalize_tsv
        )
        expect_value \
            "$variable session mutation" \
            "0|0|1
$variable|OFF
$variable|ON" \
            "$actual_mutation"
    else
        run_mysql "SET SESSION $variable = NULL; SET GLOBAL $variable = NULL;" >/dev/null
        expect_error \
            "$variable rejects unresolved table" \
            1231 \
            42000 \
            "can't be set to the value" \
            "SET SESSION $variable = 'test/no_such_table';"
    fi
done

printf '%s\n' "mysql_baseline_innodb_fulltext_system_variables_expectations: ok"
