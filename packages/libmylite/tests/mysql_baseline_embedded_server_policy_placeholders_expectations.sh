#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_embedded_server_policy_placeholders_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
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

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

expect_error \
    "CREATE LOGFILE GROUP with default/InnoDB engine" \
    3658 \
    "HY000" \
    "Feature LOGFILE GROUP is unsupported" \
    "CREATE LOGFILE GROUP mylite_lg ADD UNDOFILE 'mylite_undo.dat' INITIAL_SIZE=1M;"

expect_error \
    "CREATE LOGFILE GROUP with explicit NDB engine on non-NDB target" \
    1286 \
    "42000" \
    "Unknown storage engine 'NDB'" \
    "CREATE LOGFILE GROUP mylite_lg ADD UNDOFILE 'mylite_undo.dat' ENGINE=NDB;"

expect_error \
    "ALTER LOGFILE GROUP with explicit NDB engine on non-NDB target" \
    1286 \
    "42000" \
    "Unknown storage engine 'NDB'" \
    "ALTER LOGFILE GROUP mylite_lg ADD UNDOFILE 'mylite_undo2.dat' ENGINE=NDB;"

expect_error \
    "DROP LOGFILE GROUP with explicit NDB engine on non-NDB target" \
    1286 \
    "42000" \
    "Unknown storage engine 'NDB'" \
    "DROP LOGFILE GROUP mylite_lg ENGINE=NDB;"

printf '%s\n' "mysql_baseline_embedded_server_policy_placeholders_expectations: ok"
