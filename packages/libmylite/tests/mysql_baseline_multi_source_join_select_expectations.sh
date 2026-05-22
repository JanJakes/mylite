#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_multi_source_join_select_expectations_$$"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_multi_source_join_select_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
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

run_mysql_with_headers() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --batch --raw "$@"
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
                --batch --raw "$@"
    fi
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

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

expect_contains() {
    label=$1
    actual=$2
    needle=$3

    case "$actual" in
        *"$needle"*) ;;
        *) fail "$label: expected [$actual] to contain [$needle]" ;;
    esac
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
run_mysql "CREATE DATABASE ${DATABASE} CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci;" \
    >/dev/null
run_mysql \
    "CREATE TABLE a (id INT NOT NULL, k INT NULL, v INT NULL, name VARCHAR(20)); "\
"CREATE TABLE b (id INT NOT NULL, a_k INT NULL, k INT NULL, w INT NULL, name VARCHAR(20)); "\
"CREATE TABLE c (id INT NOT NULL, b_k INT NULL, z INT NULL, name VARCHAR(20)); "\
"CREATE TABLE d (id INT NOT NULL, c_k INT NULL, q INT NULL); "\
"INSERT INTO a VALUES (1,10,100,'alpha'),(2,20,200,'Beta'),(3,NULL,300,'none'); "\
"INSERT INTO b VALUES (7,10,70,700,'ALPHA'),(8,10,80,800,'beta'),(9,99,90,900,'none'); "\
"INSERT INTO c VALUES (30,70,3000,'alpha'),(31,80,3100,'BETA'),(32,NULL,3200,'none'); "\
"INSERT INTO d VALUES (40,70,4000),(41,80,4100),(42,90,4200),(43,99,100);" \
    "$DATABASE" >/dev/null

expect_output_with_headers \
    "three-source explicit join" \
    "id	id	id
1	7	30
1	8	31
@@warning_count	ROW_COUNT()
0	-1" \
    "DO 0; SELECT a.id, b.id, c.id FROM a JOIN b ON a.k = b.a_k "\
"JOIN c ON b.k = c.b_k ORDER BY c.id; SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output_with_headers \
    "cross edge cartesian in chain" \
    "id	id	id
1	7	30
1	8	30" \
    "SELECT a.id, b.id, c.id FROM a JOIN b ON a.k = b.a_k "\
"CROSS JOIN c WHERE c.id = 30 ORDER BY b.id;" \
    "$DATABASE"

expect_output_with_headers \
    "pure comma list with where equality" \
    "id	id	id
1	7	30
1	8	31" \
    "SELECT a.id, b.id, c.id FROM a, b, c "\
"WHERE a.k = b.a_k AND b.k = c.b_k ORDER BY c.id;" \
    "$DATABASE"

expect_output_with_headers \
    "four-source explicit join" \
    "id	id	id	id
1	7	30	40
1	8	31	41" \
    "SELECT a.id, b.id, c.id, d.id FROM a JOIN b ON a.k = b.a_k "\
"JOIN c ON b.k = c.b_k JOIN d ON c.b_k = d.c_k ORDER BY d.id;" \
    "$DATABASE"

expect_output_with_headers \
    "later on references earlier non-immediate source" \
    "id	id	id	id
1	7	30	43
1	8	31	43" \
    "SELECT a.id, b.id, c.id, d.id FROM a JOIN b ON a.k = b.a_k "\
"JOIN c ON b.k = c.b_k JOIN d ON a.v = d.q ORDER BY b.id;" \
    "$DATABASE"

expect_output_with_headers \
    "star expands all sources in order" \
    "id	k	v	name	id	a_k	k	w	name	id	b_k	z	name
1	10	100	alpha	7	10	70	700	ALPHA	30	70	3000	alpha
1	10	100	alpha	8	10	80	800	beta	31	80	3100	BETA" \
    "SELECT * FROM a JOIN b ON a.k = b.a_k JOIN c ON b.k = c.b_k ORDER BY c.id;" \
    "$DATABASE"

expect_output_with_headers \
    "qualified wildcards over multi-source join" \
    "id	k	v	name	z
1	10	100	alpha	3000
1	10	100	alpha	3100" \
    "SELECT a.*, c.z FROM a JOIN b ON a.k = b.a_k JOIN c ON b.k = c.b_k "\
"ORDER BY c.id;" \
    "$DATABASE"

expect_output_with_headers \
    "schema-qualified sources and columns" \
    "id	id	id
1	7	30
1	8	31" \
    "SELECT ${DATABASE}.a.id, ${DATABASE}.b.id, ${DATABASE}.c.id "\
"FROM ${DATABASE}.a JOIN ${DATABASE}.b ON ${DATABASE}.a.k = ${DATABASE}.b.a_k "\
"JOIN ${DATABASE}.c ON ${DATABASE}.b.k = ${DATABASE}.c.b_k ORDER BY ${DATABASE}.c.id;" \
    "$DATABASE"

expect_output \
    "limit zero" \
    "0	-1" \
    "DO 0; SELECT a.id FROM a JOIN b ON a.k = b.a_k JOIN c ON b.k = c.b_k "\
"ORDER BY c.id LIMIT 0; SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output_with_headers \
    "limit offset" \
    "id	id	id
1	8	31" \
    "SELECT a.id, b.id, c.id FROM a JOIN b ON a.k = b.a_k "\
"JOIN c ON b.k = c.b_k ORDER BY c.id LIMIT 1 OFFSET 1;" \
    "$DATABASE"

multi_join_calc=$(run_mysql \
    "SELECT SQL_CALC_FOUND_ROWS a.id, b.id, c.id FROM a JOIN b ON a.k = b.a_k "\
"JOIN c ON b.k = c.b_k ORDER BY c.id LIMIT 1; "\
"SHOW WARNINGS; SELECT FOUND_ROWS(), @@warning_count, ROW_COUNT();" \
    "$DATABASE"
)
expect_value "sql calc row" "1	7	30" "$(printf '%s\n' "$multi_join_calc" | sed -n '1p')"
expect_contains \
    "sql calc warning" \
    "$(printf '%s\n' "$multi_join_calc" | sed -n '2p')" \
    "SQL_CALC_FOUND_ROWS is deprecated"
expect_value \
    "sql calc found rows" \
    "2	1	-1" \
    "$(printf '%s\n' "$multi_join_calc" | sed -n '3p')"

expect_error \
    "ambiguous selected column" \
    1052 \
    23000 \
    "Column 'id' in field list is ambiguous" \
    "SELECT id FROM a JOIN b ON a.k = b.a_k JOIN c ON b.k = c.b_k;" \
    "$DATABASE"

expect_error \
    "ambiguous on column" \
    1052 \
    23000 \
    "Column 'k' in on clause is ambiguous" \
    "SELECT a.id FROM a JOIN b ON a.k = b.a_k JOIN c ON k = b_k;" \
    "$DATABASE"

expect_error \
    "alias hides original table in on" \
    1054 \
    42S22 \
    "Unknown column 'a.k' in 'on clause'" \
    "SELECT x.id FROM a AS x JOIN b AS y ON a.k = y.a_k JOIN c AS z ON y.k = z.b_k;" \
    "$DATABASE"

expect_error \
    "duplicate alias" \
    1066 \
    42000 \
    "Not unique table/alias: 'x'" \
    "SELECT x.id FROM a AS x JOIN b AS x ON x.k = x.a_k JOIN c AS z ON x.k = z.b_k;" \
    "$DATABASE"

expect_error \
    "unknown on column" \
    1054 \
    42S22 \
    "Unknown column 'a.missing' in 'on clause'" \
    "SELECT a.id FROM a JOIN b ON a.missing = b.a_k JOIN c ON b.k = c.b_k;" \
    "$DATABASE"

expect_error \
    "unknown order column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'order clause'" \
    "SELECT a.id FROM a JOIN b ON a.k = b.a_k JOIN c ON b.k = c.b_k ORDER BY missing;" \
    "$DATABASE"

expect_error \
    "unknown table" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing' doesn't exist" \
    "SELECT a.id FROM a JOIN b ON a.k = b.a_k JOIN missing ON b.k = missing.b_k;" \
    "$DATABASE"

expect_error \
    "unknown schema" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "SELECT a.id FROM a JOIN b ON a.k = b.a_k JOIN ${MISSING_DATABASE}.c "\
"ON b.k = ${MISSING_DATABASE}.c.b_k;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_multi_source_join_select_expectations: ok"
