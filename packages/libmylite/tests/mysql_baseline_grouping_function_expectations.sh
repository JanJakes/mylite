#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_grouping_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_grouping_function_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "${MYSQL_BIN:-mysql}" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --default-character-set=utf8mb4 --batch --raw --skip-column-names "$@"
        return
    fi
    if [ -n "$MYSQL_BIN" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=TCP -h127.0.0.1 -uroot --default-character-set=utf8mb4 \
                --batch --raw --skip-column-names "$@"
        return
    fi
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --default-character-set=utf8mb4 \
            --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "${MYSQL_BIN:-mysql}" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --default-character-set=utf8mb4 --batch --raw "$@"
        return
    fi
    if [ -n "$MYSQL_BIN" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=TCP -h127.0.0.1 -uroot --default-character-set=utf8mb4 \
                --batch --raw "$@"
        return
    fi
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --default-character-set=utf8mb4 \
            --batch --raw "$@"
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
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE};
CREATE TABLE t(id INT NOT NULL, g INT NULL, n INT NULL);
INSERT INTO t VALUES (1, NULL, NULL), (2, 1, 10), (3, 1, NULL), (4, 2, 20);" \
    >/dev/null

expect_output \
    "marker values" \
    "$(cat <<EXPECTED
NULL	0	1
1	0	2
2	0	1
NULL	1	4
EXPECTED
)" \
    "USE ${DATABASE};
     SELECT g, GROUPING(g), COUNT(*) FROM t GROUP BY g WITH ROLLUP;"

expect_output_with_headers \
    "default marker label" \
    "$(cat <<EXPECTED
g	GROUPING(g)	COUNT(*)
NULL	0	1
1	0	2
2	0	1
NULL	1	4
EXPECTED
)" \
    "USE ${DATABASE};
     SELECT g, GROUPING(g), COUNT(*) FROM t GROUP BY g WITH ROLLUP;"

expect_output_with_headers \
    "marker alias" \
    "$(cat <<EXPECTED
marker
0
0
0
1
EXPECTED
)" \
    "USE ${DATABASE};
     SELECT GROUPING(g) AS marker FROM t GROUP BY g WITH ROLLUP;"

expect_output \
    "empty source emits no rollup marker" \
    "" \
    "USE ${DATABASE};
     SELECT g, GROUPING(g), COUNT(*) FROM t WHERE n > 100 GROUP BY g WITH ROLLUP;"

expect_error \
    "no rollup" \
    1111 \
    HY000 \
    "Invalid use of group function" \
    "USE ${DATABASE}; SELECT g, GROUPING(g), COUNT(*) FROM t GROUP BY g;"

expect_error \
    "non-grouped argument" \
    3602 \
    HY000 \
    "Argument #1 of GROUPING function is not in GROUP BY" \
    "USE ${DATABASE}; SELECT g, GROUPING(n), COUNT(*) FROM t GROUP BY g WITH ROLLUP;"

expect_error \
    "non-column argument" \
    1210 \
    HY000 \
    "Incorrect arguments to GROUPING function" \
    "USE ${DATABASE}; SELECT g, GROUPING(1), COUNT(*) FROM t GROUP BY g WITH ROLLUP;"

expect_error \
    "multi-argument single-key rollup" \
    3602 \
    HY000 \
    "Argument #2 of GROUPING function is not in GROUP BY" \
    "USE ${DATABASE}; SELECT g, GROUPING(g, n), COUNT(*) FROM t GROUP BY g WITH ROLLUP;"

printf '%s\n' "mysql_baseline_grouping_function_expectations: ok"
