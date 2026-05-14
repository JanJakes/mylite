#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_left_join_select_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_left_join_select_expectations: $1" >&2
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
"INSERT INTO lefts VALUES "\
"(1,10,100,'alpha'),(2,20,200,'Beta'),(3,NULL,300,'none'),(4,40,400,'onlyleft'); "\
"INSERT INTO rights VALUES "\
"(7,10,700,'ALPHA'),(8,10,800,'beta'),(9,NULL,900,'none'),(10,50,1000,'onlyright');" \
    "$DATABASE" >/dev/null

expect_output_with_headers \
    "left join preserves left rows and null-extends right rows" \
    "id	k	id	k	w
1	10	7	10	700
1	10	8	10	800
2	20	NULL	NULL	NULL
3	NULL	NULL	NULL	NULL
4	40	NULL	NULL	NULL
@@warning_count	ROW_COUNT()
0	-1" \
    "DO 0; SELECT l.id, l.k, r.id, r.k, r.w "\
"FROM lefts AS l LEFT JOIN rights AS r ON l.k = r.k "\
"ORDER BY l.id, r.id; SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output_with_headers \
    "left outer join synonym" \
    "id	id
1	7
1	8
2	NULL
3	NULL
4	NULL" \
    "SELECT l.id, r.id FROM lefts l LEFT OUTER JOIN rights r ON l.k = r.k "\
"ORDER BY l.id, r.id;" \
    "$DATABASE"

expect_output_with_headers \
    "star expands left then right" \
    "id	k	v	name	id	k	w	name
1	10	100	alpha	7	10	700	ALPHA
1	10	100	alpha	8	10	800	beta
2	20	200	Beta	NULL	NULL	NULL	NULL
3	NULL	300	none	NULL	NULL	NULL	NULL
4	40	400	onlyleft	NULL	NULL	NULL	NULL" \
    "SELECT * FROM lefts LEFT JOIN rights ON lefts.k = rights.k "\
"ORDER BY lefts.id, rights.id;" \
    "$DATABASE"

expect_output_with_headers \
    "where sees null-extended right rows" \
    "id	id
2	NULL
3	NULL
4	NULL" \
    "SELECT l.id, r.id FROM lefts l LEFT JOIN rights r ON l.k = r.k "\
"WHERE r.id IS NULL ORDER BY l.id;" \
    "$DATABASE"

expect_output_with_headers \
    "order by right nullable key asc" \
    "id	id
4	NULL
1	7
1	8" \
    "SELECT l.id, r.id FROM lefts l LEFT JOIN rights r ON l.k = r.k "\
"WHERE l.id = 1 OR l.id = 4 ORDER BY r.id;" \
    "$DATABASE"

expect_output_with_headers \
    "order by right nullable key desc" \
    "id	id
1	8
1	7
4	NULL" \
    "SELECT l.id, r.id FROM lefts l LEFT JOIN rights r ON l.k = r.k "\
"WHERE l.id = 1 OR l.id = 4 ORDER BY r.id DESC;" \
    "$DATABASE"

expect_output_with_headers \
    "string equality uses collation" \
    "id	id
1	7
2	8
3	9
4	NULL" \
    "SELECT lefts.id, rights.id FROM lefts LEFT JOIN rights "\
"ON lefts.name = rights.name ORDER BY lefts.id;" \
    "$DATABASE"

expect_output \
    "limit zero" \
    "0	-1" \
    "DO 0; SELECT l.id FROM lefts AS l LEFT JOIN rights AS r ON l.k = r.k "\
"ORDER BY l.id LIMIT 0; SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output_with_headers \
    "exact limit" \
    "id	id
4	NULL
1	7
1	8" \
    "SELECT l.id, r.id FROM lefts AS l LEFT JOIN rights AS r ON l.k = r.k "\
"WHERE l.id = 1 OR l.id = 4 ORDER BY r.id LIMIT 3;" \
    "$DATABASE"

expect_output_with_headers \
    "oversized limit" \
    "id	id
4	NULL
1	7
1	8" \
    "SELECT l.id, r.id FROM lefts AS l LEFT JOIN rights AS r ON l.k = r.k "\
"WHERE l.id = 1 OR l.id = 4 ORDER BY r.id LIMIT 10;" \
    "$DATABASE"

expect_output_with_headers \
    "mysql accepts using but mylite defers it" \
    "id
1
1
2
3
4" \
    "SELECT l.id FROM lefts l LEFT JOIN rights r USING (k) ORDER BY l.id, r.id;" \
    "$DATABASE"

expect_error \
    "left join without condition is syntax error" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT l.id FROM lefts l LEFT JOIN rights r;" \
    "$DATABASE"

expect_error \
    "left join without condition is syntax before table lookup" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT * FROM missing LEFT JOIN also_missing;" \
    "$DATABASE"

expect_error \
    "ambiguous selected column" \
    1052 \
    23000 \
    "Column 'id' in field list is ambiguous" \
    "SELECT id FROM lefts LEFT JOIN rights ON lefts.k = rights.k;" \
    "$DATABASE"

expect_error \
    "ambiguous on column" \
    1052 \
    23000 \
    "Column 'k' in on clause is ambiguous" \
    "SELECT lefts.id FROM lefts LEFT JOIN rights ON k = k;" \
    "$DATABASE"

expect_error \
    "alias hides original table in on" \
    1054 \
    42S22 \
    "Unknown column 'lefts.k' in 'on clause'" \
    "SELECT l.id FROM lefts AS l LEFT JOIN rights AS r ON lefts.k = r.k;" \
    "$DATABASE"

expect_error \
    "duplicate alias" \
    1066 \
    42000 \
    "Not unique table/alias: 'x'" \
    "SELECT x.id FROM lefts AS x LEFT JOIN rights AS x ON x.k = x.k;" \
    "$DATABASE"

expect_error \
    "unknown on column" \
    1054 \
    42S22 \
    "Unknown column 'lefts.missing' in 'on clause'" \
    "SELECT lefts.id FROM lefts LEFT JOIN rights ON lefts.missing = rights.k;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_left_join_select_expectations: ok"
