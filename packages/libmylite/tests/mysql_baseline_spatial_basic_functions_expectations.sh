#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_spatial_basic_functions_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_spatial_basic_functions_expectations: $1" >&2
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

scalar_expected=$(cat <<EXPECTED
POINT(1 2)	000000000101000000000000000000F03F0000000000000040	0101000000000000000000F03F0000000000000040	POINT	0	1	2
EXPECTED
)
expect_output \
    "scalar point values and bytes" \
    "$scalar_expected" \
    "SELECT ST_AsText(Point(1, 2)), HEX(Point(1, 2)), "\
"HEX(ST_AsWKB(Point(1, 2))), ST_GeometryType(Point(1, 2)), "\
"ST_SRID(Point(1, 2)), ST_X(Point(1, 2)), ST_Y(Point(1, 2));" \
    "$DATABASE"

constructors_expected=$(cat <<EXPECTED
LINESTRING(0 0,1 1)	POLYGON((0 0,1 0,1 1,0 0))	MULTIPOINT((0 0),(1 1))	MULTILINESTRING((0 0,1 1))	MULTIPOLYGON(((0 0,1 0,1 1,0 0)))	GEOMETRYCOLLECTION(POINT(1 2),LINESTRING(0 0,1 1))
EXPECTED
)
expect_output \
    "spatial constructors" \
    "$constructors_expected" \
    "SELECT ST_AsText(LineString(Point(0,0), Point(1,1))), "\
"ST_AsText(Polygon(LineString(Point(0,0), Point(1,0), Point(1,1), Point(0,0)))), "\
"ST_AsText(MultiPoint(Point(0,0), Point(1,1))), "\
"ST_AsText(MultiLineString(LineString(Point(0,0), Point(1,1)))), "\
"ST_AsText(MultiPolygon(Polygon(LineString(Point(0,0), Point(1,0), Point(1,1), Point(0,0))))), "\
"ST_AsText(GeometryCollection(Point(1,2), LineString(Point(0,0), Point(1,1))));" \
    "$DATABASE"

from_text_wkb_expected=$(cat <<EXPECTED
GEOMETRYCOLLECTION EMPTY	GEOMETRYCOLLECTION EMPTY	POINT(3 4)	01020000000200000000000000000000000000000000000000000000000000F03F000000000000F03F
EXPECTED
)
expect_output \
    "WKT and WKB constructors" \
    "$from_text_wkb_expected" \
    "SELECT ST_AsText(ST_GeomFromText('GEOMETRYCOLLECTION()')), "\
"ST_AsText(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY')), "\
"ST_AsText(ST_GeomFromWKB(ST_AsWKB(Point(3, 4)))), "\
"HEX(ST_AsWKB(ST_GeomFromText('LINESTRING(0 0,1 1)')));" \
    "$DATABASE"

null_expected=$(cat <<EXPECTED
NULL	NULL	NULL	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "spatial NULL propagation" \
    "$null_expected" \
    "SELECT ST_AsText(NULL), ST_AsWKB(NULL), ST_GeometryType(NULL), "\
"ST_SRID(NULL), ST_X(NULL), ST_Y(NULL);" \
    "$DATABASE"

table_expected=$(cat <<EXPECTED
1	POINT(1 2)	000000000101000000000000000000F03F0000000000000040	POINT	0	POINT(1 2)
2	LINESTRING(0 0,1 1)	0000000001020000000200000000000000000000000000000000000000000000000000F03F000000000000F03F	LINESTRING	0	LINESTRING(0 0,1 1)
3	NULL	NULL	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "table-backed spatial values" \
    "$table_expected" \
    "CREATE TABLE spatial_values(id INT PRIMARY KEY, g GEOMETRY, txt VARCHAR(80)); "\
"INSERT INTO spatial_values VALUES "\
"(1, Point(1, 2), NULL), "\
"(2, ST_GeomFromText('LINESTRING(0 0,1 1)'), NULL), "\
"(3, NULL, NULL); "\
"UPDATE spatial_values SET txt = ST_AsText(g) WHERE g IS NOT NULL; "\
"SELECT id, ST_AsText(g), HEX(g), ST_GeometryType(g), ST_SRID(g), txt "\
"FROM spatial_values ORDER BY id;" \
    "$DATABASE"

updated_expected=$(cat <<EXPECTED
3	POINT(5 6)	5	6
EXPECTED
)
expect_output \
    "updated spatial value" \
    "$updated_expected" \
    "UPDATE spatial_values SET g = ST_GeomFromWKB(ST_AsWKB(Point(5, 6))) "\
"WHERE id = 3; "\
"SELECT id, ST_AsText(g), ST_X(g), ST_Y(g) FROM spatial_values WHERE id = 3;" \
    "$DATABASE"

expect_error \
    "invalid WKT rejected" \
    3037 \
    "22023" \
    "Invalid GIS data provided to function st_geomfromtext" \
    "SELECT ST_GeomFromText('bad');" \
    "$DATABASE"

expect_error \
    "wrong typed WKT constructor rejected" \
    3516 \
    "22S01" \
    "WKT value is a geometry of unexpected type LINESTRING in st_pointfromtext" \
    "SELECT ST_PointFromText('LINESTRING(0 0,1 1)');" \
    "$DATABASE"

expect_error \
    "invalid WKB rejected" \
    3037 \
    "22023" \
    "Invalid GIS data provided to function st_geomfromwkb" \
    "SELECT ST_GeomFromWKB(X'00');" \
    "$DATABASE"

expect_error \
    "unknown SRID rejected" \
    3548 \
    "SR001" \
    "There's no spatial reference system with SRID 1" \
    "SELECT ST_GeomFromText('POINT(1 2)', 1);" \
    "$DATABASE"

expect_error \
    "raw geometry column value rejected" \
    1416 \
    "22003" \
    "Cannot get geometry object from data you send to the GEOMETRY field" \
    "INSERT INTO spatial_values VALUES (4, 'bad', NULL);" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_spatial_basic_functions_expectations: ok"
