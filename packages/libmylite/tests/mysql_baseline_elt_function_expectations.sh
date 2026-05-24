#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_elt_function_expectations: $1" >&2
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

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

scalar=$(
    run_mysql \
        "DO 0;
         SELECT ELT(1,'Aa','Bb','Cc'), ELT(3,'Aa','Bb','Cc'),
                ELT(0,'Aa'), ELT(-1,'Aa'), ELT(4,'Aa','Bb'),
                ELT(NULL,'Aa'), ELT(TRUE,'no','yes'), ELT(FALSE,'zero'),
                ELT(1, 10, TRUE, NULL), ELT(2, 10, TRUE, NULL),
                ELT(3, 10, TRUE, NULL), @@warning_count;"
)
expect_value \
    "scalar values" \
    "Aa${TAB}Cc${TAB}NULL${TAB}NULL${TAB}NULL${TAB}NULL${TAB}no${TAB}NULL${TAB}10${TAB}1${TAB}NULL${TAB}0" \
    "$scalar"

dual=$(
    run_mysql \
        "SELECT ELT (2,'first','second') AS elt_alias, ELT(+1,'x') FROM DUAL;"
)
expect_value "dual values" "second${TAB}x" "$dual"

status=$(
    run_mysql \
        "DO ELT(2,'a','b'), ELT(NULL,'a');
         SELECT ROW_COUNT(), @@warning_count;"
)
expect_value "do status" "0${TAB}0" "$status"

expect_error \
    "empty argument list" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'ELT'" \
    "SELECT ELT();"

expect_error \
    "index only" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'ELT'" \
    "SELECT ELT(1);"

printf '%s\n' "mysql_baseline_elt_function_expectations: ok"
