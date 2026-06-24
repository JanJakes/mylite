#!/usr/bin/env sh
set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_ARGS="--protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names"
DATABASE="mylite_baseline_named_lock_info"

run_mysql() {
    sql="$1"
    printf '%s\n' "$sql" | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT
cleanup

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *)
        echo "expected MySQL 8.4.9, got: $version" >&2
        exit 1
        ;;
esac

run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; CREATE TABLE locks(id INT PRIMARY KEY); INSERT INTO locks VALUES (1),(2);" >/dev/null

icu=$(run_mysql "SELECT ICU_VERSION();")
if [ "$icu" != "77.1" ]; then
    echo "unexpected ICU_VERSION(): $icu" >&2
    exit 1
fi

benchmark_values=$(run_mysql "SELECT BENCHMARK(0,1+1), BENCHMARK(3,1+1), BENCHMARK(NULL,1), BENCHMARK(-1,1); SHOW WARNINGS;")
expected_benchmark_values="0	0	NULL	NULL
Warning	1411	Incorrect count value: '-1' for function benchmark"
if [ "$benchmark_values" != "$expected_benchmark_values" ]; then
    echo "unexpected BENCHMARK() values:" >&2
    printf '%s\n' "$benchmark_values" >&2
    exit 1
fi

benchmark_zero_side_effects=$(run_mysql "SELECT RELEASE_ALL_LOCKS(); SELECT BENCHMARK(0, GET_LOCK('mylite_benchmark_zero_side_effect', 0)); SELECT IS_USED_LOCK('mylite_benchmark_zero_side_effect'); SELECT RELEASE_ALL_LOCKS();")
expected_benchmark_zero_side_effects="0
0
NULL
0"
if [ "$benchmark_zero_side_effects" != "$expected_benchmark_zero_side_effects" ]; then
    echo "unexpected BENCHMARK(0) side effects:" >&2
    printf '%s\n' "$benchmark_zero_side_effects" >&2
    exit 1
fi

benchmark_repeat_side_effects=$(run_mysql "SELECT RELEASE_ALL_LOCKS(), BENCHMARK(3, GET_LOCK('mylite_benchmark_repeat_side_effect', 0)), RELEASE_ALL_LOCKS();")
expected_benchmark_repeat_side_effects="0	0	3"
if [ "$benchmark_repeat_side_effects" != "$expected_benchmark_repeat_side_effects" ]; then
    echo "unexpected BENCHMARK repeated side effects:" >&2
    printf '%s\n' "$benchmark_repeat_side_effects" >&2
    exit 1
fi

lock_values=$(run_mysql "SELECT CONNECTION_ID(); SELECT GET_LOCK('mylite_probe_lock', 0); SELECT GET_LOCK('mylite_probe_lock', 0); SELECT IS_FREE_LOCK('mylite_probe_lock'); SELECT IS_USED_LOCK('mylite_probe_lock'); SELECT RELEASE_LOCK('mylite_probe_lock'); SELECT IS_FREE_LOCK('mylite_probe_lock'); SELECT IS_USED_LOCK('mylite_probe_lock'); SELECT RELEASE_LOCK('mylite_probe_lock'); SELECT RELEASE_LOCK('mylite_probe_lock'); SELECT RELEASE_ALL_LOCKS();")
connection_id=$(printf '%s\n' "$lock_values" | sed -n '1p')
expected_lock_values="${connection_id}
1
1
0
${connection_id}
1
0
${connection_id}
1
NULL
0"
if [ "$lock_values" != "$expected_lock_values" ]; then
    echo "unexpected named-lock lifecycle values:" >&2
    printf '%s\n' "$lock_values" >&2
    exit 1
fi

row_values=$(run_mysql "USE ${DATABASE}; SELECT CONNECTION_ID(); SELECT id, GET_LOCK(CONCAT('mylite_row_lock_', id), 0), IS_USED_LOCK(CONCAT('mylite_row_lock_', id)), RELEASE_LOCK(CONCAT('mylite_row_lock_', id)) FROM locks ORDER BY id;")
row_connection_id=$(printf '%s\n' "$row_values" | sed -n '1p')
expected_row_values="${row_connection_id}
1	1	${row_connection_id}	1
2	1	${row_connection_id}	1"
if [ "$row_values" != "$expected_row_values" ]; then
    echo "unexpected row named-lock values:" >&2
    printf '%s\n' "$row_values" >&2
    exit 1
fi

echo "mysql_baseline_named_lock_and_info_functions_expectations: ok"
