#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_scalable_cancellable_spatial_validation_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
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

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

topology_expected=$(cat <<EXPECTED
1
1
1
0
EXPECTED
)
topology_sql=$(cat <<'SQL'
SELECT ST_IsValid(ST_GeomFromText(
    'POLYGON((0 0,8 0,8 8,0 8,0 0),(0 0,2 1,1 2,0 0))'
));
SELECT ST_IsValid(ST_GeomFromText(
    'POLYGON((0 0,8 0,8 8,0 8,0 0),(0 1,2 1,1 2,0 1))'
));
SELECT ST_IsValid(ST_GeomFromText(
    'MULTIPOLYGON(((0 0,2 0,2 2,0 2,0 0)),((2 2,4 2,4 4,2 4,2 2)))'
));
SELECT ST_IsValid(ST_GeomFromText(
    'MULTIPOLYGON(((0 0,2 0,2 2,0 2,0 0)),((2 0,4 0,4 2,2 2,2 0)))'
));
SQL
)
expect_output \
    "touching boundary validity" \
    "$topology_expected" \
    "$topology_sql"

deadline_sql=$(cat <<'SQL'
SET SESSION max_execution_time = 1;
SELECT COUNT(*)
FROM information_schema.columns AS a
CROSS JOIN information_schema.columns AS b
CROSS JOIN information_schema.columns AS c;
SQL
)
expect_error \
    "maximum execution time diagnostic" \
    3024 \
    HY000 \
    "Query execution was interrupted, maximum statement execution time exceeded" \
    "$deadline_sql"

printf '%s\n' "mysql_scalable_cancellable_spatial_validation_expectations: ok"
