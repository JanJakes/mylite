#!/usr/bin/env bash
set -euo pipefail

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_baseline_where_scalar_literal_predicates"

fail() {
    printf '%s\n' "mysql_baseline_where_scalar_literal_predicates_expectations: $1" >&2
    exit 1
}

run_mysql() {
    local sql="$1"
    shift || true

    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --batch --raw --skip-column-names "$@"
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
                --batch --raw --skip-column-names "$@"
    fi
}

expect_output() {
    local label="$1"
    local expected="$2"
    local sql="$3"
    local actual

    actual="$(run_mysql "$sql" "$DATABASE")"
    if [ "$actual" != "$expected" ]; then
        printf 'case: %s\n' "$label" >&2
        printf 'sql: %s\n' "$sql" >&2
        printf 'expected:\n%s\n' "$expected" >&2
        printf 'actual:\n%s\n' "$actual" >&2
        fail "unexpected output"
    fi
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS \`$DATABASE\`;" >/dev/null || true
}
trap cleanup EXIT

run_mysql "DROP DATABASE IF EXISTS \`$DATABASE\`; CREATE DATABASE \`$DATABASE\`;" >/dev/null
run_mysql "
CREATE TABLE numbers (
  id INT,
  i INT NULL,
  n INT NULL,
  s VARCHAR(16) NULL
);
INSERT INTO numbers VALUES
  (1, 1, NULL, 'a'),
  (2, 2, 0, 'b'),
  (3, NULL, 3, NULL),
  (4, -1, 3, 'c');
" "$DATABASE" >/dev/null

expect_output \
    "WHERE TRUE matches all rows" \
    "1,2,3,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE TRUE;"

expect_output \
    "WHERE FALSE matches no rows" \
    "0" \
    "SELECT COUNT(*) FROM numbers WHERE FALSE;"

expect_output \
    "WHERE NULL matches no rows" \
    "0" \
    "SELECT COUNT(*) FROM numbers WHERE NULL;"

expect_output \
    "WHERE signed nonzero integer is true" \
    "1,2,3,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE -1;"

expect_output \
    "scalar literal true comparison" \
    "1,2,3,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE 1 = 1;"

expect_output \
    "scalar literal false comparison" \
    "0" \
    "SELECT COUNT(*) FROM numbers WHERE 1 = 0;"

expect_output \
    "scalar literal null-safe comparison" \
    "1,2,3,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE NULL <=> NULL;"

expect_output \
    "ordinary NULL scalar comparison is unknown" \
    "0" \
    "SELECT COUNT(*) FROM numbers WHERE NULL = NULL;"

expect_output \
    "literal-left equality" \
    "1" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE 1 = i;"

expect_output \
    "literal-left less-than flips to column greater-than" \
    "1,2" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE 0 < i;"

expect_output \
    "literal-left greater-than flips to column less-than" \
    "1,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE 2 > i;"

expect_output \
    "literal-left NULL-safe descriptor comparison" \
    "1" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE NULL <=> n;"

expect_output \
    "ordinary literal-left NULL comparison is unknown" \
    "0" \
    "SELECT COUNT(*) FROM numbers WHERE NULL = n;"

expect_output \
    "descriptor NULL-safe comparison" \
    "1" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE n <=> NULL;"

expect_output \
    "ordinary descriptor NULL comparison is unknown" \
    "0" \
    "SELECT COUNT(*) FROM numbers WHERE n = NULL;"

expect_output \
    "scalar literal IS TRUE" \
    "1,2,3,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE 1 IS TRUE;"

expect_output \
    "scalar literal IS FALSE" \
    "1,2,3,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE 0 IS FALSE;"

expect_output \
    "scalar literal IS UNKNOWN" \
    "1,2,3,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE NULL IS UNKNOWN;"

expect_output \
    "scalar literal IS NOT TRUE" \
    "1,2,3,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE 0 IS NOT TRUE;"

expect_output \
    "scalar literal IS NOT FALSE" \
    "1,2,3,4" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE 2 IS NOT FALSE;"

expect_output \
    "literal predicates compose with existing boolean grammar" \
    "1" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE TRUE AND 1 = i OR FALSE;"

expect_output \
    "scalar literal predicate updates" \
    "1	9" \
    "UPDATE numbers SET n = 9 WHERE 1 = i; SELECT ROW_COUNT(), n FROM numbers WHERE id = 1;"

expect_output \
    "scalar literal predicate deletes" \
    "1	3" \
    "DELETE FROM numbers WHERE NULL <=> i; SELECT ROW_COUNT(), COUNT(*) FROM numbers;"

printf '%s\n' "mysql_baseline_where_scalar_literal_predicates_expectations: ok"
