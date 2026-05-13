#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_inner_join_select_expectations_$$"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_inner_join_select_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw "$@"
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
        *)
            fail "$label: expected error $code/$state containing [$message], got [$output]"
            ;;
    esac
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null
run_mysql \
    "CREATE TABLE lefts (id INT NOT NULL, k INT NULL, v INT NULL, name VARCHAR(20)); "\
"CREATE TABLE rights (id INT NOT NULL, k INT NULL, w INT NULL, name VARCHAR(20)); "\
"INSERT INTO lefts VALUES (1,10,100,'alpha'),(2,20,200,'Beta'),(3,NULL,300,'none'); "\
"INSERT INTO rights VALUES (7,10,700,'ALPHA'),(8,10,800,'beta'),(9,NULL,900,'none');" \
    "$DATABASE" >/dev/null

expect_output_with_headers \
    "star expands left then right" \
    "id	k	v	name	id	k	w	name
1	10	100	alpha	7	10	700	ALPHA
1	10	100	alpha	8	10	800	beta
@@warning_count	ROW_COUNT()
0	-1" \
    "DO 0; SELECT * FROM lefts JOIN rights ON lefts.k = rights.k "\
"ORDER BY lefts.id, rights.id; SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output_with_headers \
    "inner join aliases" \
    "id	w
1	700
1	800" \
    "SELECT l.id, r.w FROM lefts AS l INNER JOIN rights AS r "\
"ON l.k = r.k ORDER BY l.id, r.id;" \
    "$DATABASE"

expect_output_with_headers \
    "cross join cartesian" \
    "id	id
1	7
1	8
1	9
2	7" \
    "SELECT l.id, r.id FROM lefts l CROSS JOIN rights r ORDER BY l.id, r.id LIMIT 4;" \
    "$DATABASE"

expect_output_with_headers \
    "cross join with on" \
    "id	id
1	7
1	8" \
    "SELECT l.id, r.id FROM lefts l CROSS JOIN rights r ON l.k = r.k "\
"ORDER BY l.id, r.id;" \
    "$DATABASE"

expect_output_with_headers \
    "join without on" \
    "id	id
1	7
1	8
1	9
2	7" \
    "SELECT l.id, r.id FROM lefts l JOIN rights r ORDER BY l.id, r.id LIMIT 4;" \
    "$DATABASE"

expect_output_with_headers \
    "where order limit offset" \
    "id	w
1	800" \
    "SELECT l.id, r.w FROM lefts AS l JOIN rights AS r ON l.k = r.k "\
"WHERE l.v = 100 ORDER BY r.w LIMIT 1 OFFSET 1;" \
    "$DATABASE"

expect_output_with_headers \
    "order by explicit alias" \
    "left_id	right_id
1	8
1	7" \
    "SELECT l.id AS left_id, r.id AS right_id FROM lefts AS l JOIN rights AS r "\
"ON l.k = r.k ORDER BY right_id DESC;" \
    "$DATABASE"

expect_output_with_headers \
    "string equality uses collation" \
    "id	id
1	7
2	8
3	9" \
    "SELECT lefts.id, rights.id FROM lefts JOIN rights ON lefts.name = rights.name "\
"ORDER BY lefts.id, rights.id;" \
    "$DATABASE"

expect_output \
    "limit zero" \
    "0	-1" \
    "DO 0; SELECT l.id FROM lefts AS l JOIN rights AS r ON l.k = r.k "\
"ORDER BY r.id LIMIT 0; SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_error \
    "ambiguous selected column" \
    1052 \
    23000 \
    "Column 'id' in field list is ambiguous" \
    "SELECT id FROM lefts JOIN rights ON lefts.k = rights.k;" \
    "$DATABASE"

expect_error \
    "ambiguous on column" \
    1052 \
    23000 \
    "Column 'k' in on clause is ambiguous" \
    "SELECT lefts.id FROM lefts JOIN rights ON k = k;" \
    "$DATABASE"

expect_error \
    "ambiguous where column" \
    1052 \
    23000 \
    "Column 'id' in where clause is ambiguous" \
    "SELECT lefts.v FROM lefts JOIN rights ON lefts.k = rights.k WHERE id = 1;" \
    "$DATABASE"

expect_error \
    "ambiguous order column" \
    1052 \
    23000 \
    "Column 'id' in order clause is ambiguous" \
    "SELECT lefts.v FROM lefts JOIN rights ON lefts.k = rights.k ORDER BY id;" \
    "$DATABASE"

expect_error \
    "alias hides original table in on" \
    1054 \
    42S22 \
    "Unknown column 'lefts.k' in 'on clause'" \
    "SELECT l.id FROM lefts AS l JOIN rights AS r ON lefts.k = r.k;" \
    "$DATABASE"

expect_error \
    "duplicate alias" \
    1066 \
    42000 \
    "Not unique table/alias: 'x'" \
    "SELECT x.id FROM lefts AS x JOIN rights AS x ON x.k = x.k;" \
    "$DATABASE"

expect_error \
    "unknown selected qualifier" \
    1054 \
    42S22 \
    "Unknown column 'missing.id' in 'field list'" \
    "SELECT missing.id FROM lefts JOIN rights ON lefts.k = rights.k;" \
    "$DATABASE"

expect_error \
    "unknown on column" \
    1054 \
    42S22 \
    "Unknown column 'lefts.missing' in 'on clause'" \
    "SELECT lefts.id FROM lefts JOIN rights ON lefts.missing = rights.k;" \
    "$DATABASE"

expect_error \
    "unknown table" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing' doesn't exist" \
    "SELECT lefts.id FROM lefts JOIN missing ON lefts.k = missing.k;" \
    "$DATABASE"

expect_error \
    "unknown schema" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "SELECT lefts.id FROM lefts JOIN ${MISSING_DATABASE}.rights "\
"ON lefts.k = ${MISSING_DATABASE}.rights.k;" \
    "$DATABASE"
