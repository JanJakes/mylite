#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_json_row_scalar_contexts_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_json_row_scalar_contexts_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
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
run_mysql "CREATE DATABASE ${DATABASE} DEFAULT CHARACTER SET utf8mb4 "\
"COLLATE utf8mb4_0900_ai_ci;" >/dev/null

setup_sql="CREATE TABLE t("\
"id INT, j JSON, s VARCHAR(64), n INT, b BOOLEAN, "\
"out_text VARCHAR(255), out_json JSON, out_int INT); "\
"INSERT INTO t(id, j, s, n, b, out_text, out_json, out_int) VALUES "\
"(1, '{\"a\":2,\"b\":\"x\",\"obj\":{\"k\":1}}', 'plain', 7, TRUE, NULL, NULL, NULL), "\
"(2, '{\"a\":1,\"b\":\"y\",\"obj\":{\"m\":2}}', 'quoted', 3, FALSE, NULL, NULL, NULL), "\
"(3, NULL, NULL, NULL, NULL, NULL, NULL, NULL);"
run_mysql "$setup_sql" "$DATABASE" >/dev/null

order_expected=$(cat <<EXPECTED
3	NULL
2	1
1	2
3	NULL
1	x
2	y
3	NULL
1	OBJECT
2	OBJECT
3	NULL
1	3
2	3
3	NULL
1	"plain"
2	"quoted"
EXPECTED
)
expect_output \
    "JSON row-scalar ORDER BY contexts" \
    "$order_expected" \
    "SELECT id, JSON_EXTRACT(j, '$.a') FROM t ORDER BY JSON_EXTRACT(j, '$.a'), id; "\
"SELECT id, JSON_UNQUOTE(JSON_EXTRACT(j, '$.b')) FROM t "\
"ORDER BY JSON_UNQUOTE(JSON_EXTRACT(j, '$.b')), id; "\
"SELECT id, JSON_TYPE(j) FROM t ORDER BY JSON_TYPE(j), id; "\
"SELECT id, JSON_LENGTH(j) FROM t ORDER BY JSON_LENGTH(j), id; "\
"SELECT id, JSON_QUOTE(s) FROM t ORDER BY JSON_QUOTE(s), id;" \
    "$DATABASE"

update_expected=$(cat <<EXPECTED
1	x	3	NULL
2	OBJECT	1	["a", "b", "obj"]
3	NULL	NULL	NULL
1	{"a": 2, "b": "x", "n": 7, "obj": {"k": 1}}
2	[3, "quoted", {"a": 1, "b": "y", "obj": {"m": 2}}]
3	{"s": null, "id": 3}
1	{"a": 2, "b": "x", "obj": {"k": 1}, "inserted": 7}
2	{"a": 3, "b": "y", "obj": {"m": 2}}
3	NULL
-1	0
EXPECTED
)
expect_output \
    "JSON row-scalar UPDATE assignment contexts" \
    "$update_expected" \
    "UPDATE t SET out_text = JSON_UNQUOTE(JSON_EXTRACT(j, '$.b')) WHERE id = 1; "\
"UPDATE t SET out_text = JSON_QUOTE(s) WHERE id = 2; "\
"UPDATE t SET out_int = JSON_LENGTH(j) WHERE id = 1; "\
"UPDATE t SET out_json = JSON_KEYS(j) WHERE id = 2; "\
"UPDATE t SET out_text = JSON_TYPE(j) WHERE id = 2; "\
"UPDATE t SET out_int = JSON_EXTRACT(j, '$.a') WHERE id = 2; "\
"SELECT id, out_text, out_int, out_json FROM t ORDER BY id; "\
"UPDATE t SET out_json = JSON_SET(j, '$.n', n) WHERE id = 1; "\
"UPDATE t SET out_json = JSON_ARRAY(n, s, j) WHERE id = 2; "\
"UPDATE t SET out_json = JSON_OBJECT('id', id, 's', s) WHERE id = 3; "\
"SELECT id, out_json FROM t ORDER BY id; "\
"UPDATE t SET out_json = JSON_INSERT(j, '$.inserted', n) WHERE id = 1; "\
"UPDATE t SET out_json = JSON_REPLACE(j, '$.a', n) WHERE id = 2; "\
"UPDATE t SET out_json = JSON_REMOVE(j, '$.b') WHERE id = 3; "\
"SELECT id, out_json FROM t ORDER BY id; "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_json_row_scalar_contexts_expectations: ok"
