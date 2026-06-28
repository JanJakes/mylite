#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_spatial_collect_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_spatial_collect_aggregate_expectations: $1" >&2
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

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS \`$DATABASE\`;" >/dev/null 2>&1 || true
}

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

run_mysql "DROP DATABASE IF EXISTS \`$DATABASE\`; CREATE DATABASE \`$DATABASE\`;"
trap cleanup EXIT INT TERM

run_mysql "CREATE TABLE shapes (grp INT, id INT, g GEOMETRY); \
INSERT INTO shapes VALUES \
(1,1,Point(0,0)), \
(1,2,Point(1,1)), \
(1,3,NULL), \
(2,1,ST_GeomFromText('LINESTRING(0 0,1 1)')), \
(2,2,ST_GeomFromText('LINESTRING(2 2,3 3)')), \
(3,1,ST_GeomFromText('POLYGON((0 0,1 0,1 1,0 0))')), \
(3,2,ST_GeomFromText('POLYGON((2 2,3 2,3 3,2 2))')), \
(4,1,Point(0,0)), \
(4,2,ST_GeomFromText('LINESTRING(0 0,1 1)')), \
(5,1,NULL), \
(6,1,ST_SRID(Point(0,0),3857)), \
(7,1,ST_SRID(Point(0,0),3857)), \
(7,2,Point(1,1)), \
(8,1,Point(0,0)), \
(8,2,Point(0,0)), \
(8,3,Point(1,1)), \
(9,1,ST_GeomFromText('MULTIPOINT(0 0,1 1)')), \
(10,1,ST_GeomFromText('GEOMETRYCOLLECTION(POINT(2 2),LINESTRING(0 0,1 1))'));" \
    "$DATABASE" >/dev/null

point_expected=$(cat <<EXPECTED
MULTIPOINT((0 0),(1 1))	MULTIPOINT	0
EXPECTED
)
expect_output \
    "point collection wrappers" \
    "$point_expected" \
    "SELECT ST_AsText(ST_Collect(g)), ST_GeometryType(ST_Collect(g)), ST_SRID(ST_Collect(g)) \
FROM shapes WHERE grp = 1;" \
    "$DATABASE"

type_expected=$(cat <<EXPECTED
1	MULTIPOINT((0 0),(1 1))	MULTIPOINT
2	MULTILINESTRING((0 0,1 1),(2 2,3 3))	MULTILINESTRING
3	MULTIPOLYGON(((0 0,1 0,1 1,0 0)),((2 2,3 2,3 3,2 2)))	MULTIPOLYGON
4	GEOMETRYCOLLECTION(POINT(0 0),LINESTRING(0 0,1 1))	GEOMCOLLECTION
5	NULL	NULL
EXPECTED
)
expect_output \
    "grouped collection wrappers" \
    "$type_expected" \
    "SELECT grp, ST_AsText(ST_Collect(g)), ST_GeometryType(ST_Collect(g)) \
FROM shapes WHERE grp IN (1,2,3,4,5) GROUP BY grp ORDER BY grp;" \
    "$DATABASE"

distinct_expected=$(cat <<EXPECTED
MULTIPOINT((0 0),(1 1))
EXPECTED
)
expect_output \
    "distinct collection" \
    "$distinct_expected" \
    "SELECT ST_AsText(ST_Collect(DISTINCT g)) FROM shapes WHERE grp = 8;" \
    "$DATABASE"

grouped_distinct_expected=$(cat <<EXPECTED
8	MULTIPOINT((0 0),(1 1))
EXPECTED
)
expect_output \
    "grouped distinct collection" \
    "$grouped_distinct_expected" \
    "SELECT grp, ST_AsText(ST_Collect(DISTINCT g)) \
FROM shapes WHERE grp = 8 GROUP BY grp;" \
    "$DATABASE"

collection_input_expected=$(cat <<EXPECTED
9	GEOMETRYCOLLECTION(MULTIPOINT((0 0),(1 1)))	GEOMCOLLECTION
10	GEOMETRYCOLLECTION(GEOMETRYCOLLECTION(POINT(2 2),LINESTRING(0 0,1 1)))	GEOMCOLLECTION
EXPECTED
)
expect_output \
    "multi and collection input wrappers" \
    "$collection_input_expected" \
    "SELECT grp, ST_AsText(ST_Collect(g)), ST_GeometryType(ST_Collect(g)) \
FROM shapes WHERE grp IN (9,10) GROUP BY grp ORDER BY grp;" \
    "$DATABASE"

srid_expected=$(cat <<EXPECTED
MULTIPOINT((0 0))	3857
EXPECTED
)
expect_output \
    "srid preserving collection" \
    "$srid_expected" \
    "SELECT ST_AsText(ST_Collect(g)), ST_SRID(ST_Collect(g)) FROM shapes WHERE grp = 6;" \
    "$DATABASE"

expect_error \
    "mixed SRID collection" \
    4034 \
    22S05 \
    "different SRIDs: 3857 and 0" \
    "SELECT ST_AsText(ST_Collect(g)) FROM shapes WHERE grp = 7;" \
    "$DATABASE"

expect_error \
    "empty argument list" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT ST_Collect() FROM shapes;" \
    "$DATABASE"

expect_error \
    "too many arguments" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT ST_Collect(g, g) FROM shapes;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_spatial_collect_aggregate_expectations: ok"
