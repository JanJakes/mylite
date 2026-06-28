#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_spatial_geojson_functions_expectations: $1" >&2
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

as_basic_expected=$(cat <<EXPECTED
{"type": "Point", "coordinates": [1.0, 2.0]}	{"type": "LineString", "coordinates": [[0.0, 0.0], [1.0, 1.0]]}	{"type": "Polygon", "coordinates": [[[0.0, 0.0], [1.0, 0.0], [1.0, 1.0], [0.0, 0.0]]]}
EXPECTED
)
expect_output \
    "basic GeoJSON output" \
    "$as_basic_expected" \
    "SELECT ST_AsGeoJSON(Point(1,2)), "\
"ST_AsGeoJSON(ST_GeomFromText('LINESTRING(0 0,1 1)')), "\
"ST_AsGeoJSON(ST_GeomFromText('POLYGON((0 0,1 0,1 1,0 0))'));"

as_collections_expected=$(cat <<EXPECTED
{"type": "MultiPoint", "coordinates": [[0.0, 0.0], [1.0, 1.0]]}	{"type": "MultiLineString", "coordinates": [[[0.0, 0.0], [1.0, 1.0]], [[2.0, 2.0], [3.0, 3.0]]]}	{"type": "MultiPolygon", "coordinates": [[[[0.0, 0.0], [1.0, 0.0], [1.0, 1.0], [0.0, 0.0]]]]}	{"type": "GeometryCollection", "geometries": [{"type": "Point", "coordinates": [1.0, 2.0]}, {"type": "LineString", "coordinates": [[0.0, 0.0], [1.0, 1.0]]}]}	{"type": "GeometryCollection", "geometries": []}
EXPECTED
)
expect_output \
    "collection GeoJSON output" \
    "$as_collections_expected" \
    "SELECT ST_AsGeoJSON(ST_GeomFromText('MULTIPOINT((0 0),(1 1))')), "\
"ST_AsGeoJSON(ST_GeomFromText('MULTILINESTRING((0 0,1 1),(2 2,3 3))')), "\
"ST_AsGeoJSON(ST_GeomFromText('MULTIPOLYGON(((0 0,1 0,1 1,0 0)))')), "\
"ST_AsGeoJSON(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(1 2),LINESTRING(0 0,1 1))')), "\
"ST_AsGeoJSON(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'));"

as_options_expected=$(cat <<EXPECTED
{"type": "Point", "coordinates": [11.11, 12.22]}	{"type": "Point", "coordinates": [11.0, 12.0]}	{"bbox": [1.0, 2.0, 1.0, 2.0], "type": "Point", "coordinates": [1.0, 2.0]}	{"crs": {"type": "name", "properties": {"name": "EPSG:4326"}}, "type": "Point", "coordinates": [102.0, 0.0]}	{"crs": {"type": "name", "properties": {"name": "urn:ogc:def:crs:EPSG::4326"}}, "bbox": [102.0, 0.0, 102.0, 0.0], "type": "Point", "coordinates": [102.0, 0.0]}
EXPECTED
)
expect_output \
    "GeoJSON output precision and options" \
    "$as_options_expected" \
    "SELECT ST_AsGeoJSON(ST_GeomFromText('POINT(11.11111 12.22222)'),2), "\
"ST_AsGeoJSON(ST_GeomFromText('POINT(11.11111 12.22222)'),0), "\
"ST_AsGeoJSON(ST_GeomFromText('POINT(1 2)',0),4,1), "\
"ST_AsGeoJSON(ST_GeomFromGeoJSON('{\"type\":\"Point\",\"coordinates\":[102.0,0.0]}'),4,2), "\
"ST_AsGeoJSON(ST_GeomFromGeoJSON('{\"type\":\"Point\",\"coordinates\":[102.0,0.0]}'),4,5);"

from_basic_expected=$(cat <<EXPECTED
POINT(0 102)	4326	POINT(102 0)	0	POINT(102.5 0)	POINT(100 0)
EXPECTED
)
expect_output \
    "GeoJSON input default and explicit SRIDs" \
    "$from_basic_expected" \
    "SELECT ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"Point\",\"coordinates\":[102.0,0.0]}')), "\
"ST_SRID(ST_GeomFromGeoJSON('{\"type\":\"Point\",\"coordinates\":[102.0,0.0]}')), "\
"ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"Point\",\"coordinates\":[102.0,0.0]}',1,0)), "\
"ST_SRID(ST_GeomFromGeoJSON('{\"type\":\"Point\",\"coordinates\":[102.0,0.0]}',1,0)), "\
"ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"Point\",\"coordinates\":[102.5,0]}',1,0)), "\
"ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"Point\",\"coordinates\":[1e2,0]}',1,0));"

from_types_expected=$(cat <<EXPECTED
LINESTRING(0 0,1 1)	POLYGON((0 0,1 0,1 1,0 0))	MULTIPOINT((0 0),(1 1))	MULTILINESTRING((0 0,1 1))	MULTIPOLYGON(((0 0,1 0,1 1,0 0)))	GEOMETRYCOLLECTION(POINT(1 2),LINESTRING(0 0,1 1))
EXPECTED
)
expect_output \
    "GeoJSON input geometry types" \
    "$from_types_expected" \
    "SELECT ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"LineString\",\"coordinates\":[[0,0],[1,1]]}',1,0)), "\
"ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"Polygon\",\"coordinates\":[[[0,0],[1,0],[1,1],[0,0]]]}',1,0)), "\
"ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"MultiPoint\",\"coordinates\":[[0,0],[1,1]]}',1,0)), "\
"ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"MultiLineString\",\"coordinates\":[[[0,0],[1,1]]]}',1,0)), "\
"ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"MultiPolygon\",\"coordinates\":[[[[0,0],[1,0],[1,1],[0,0]]]]}',1,0)), "\
"ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"GeometryCollection\",\"geometries\":[{\"type\":\"Point\",\"coordinates\":[1,2]},{\"type\":\"LineString\",\"coordinates\":[[0,0],[1,1]]}]}',1,0));"

from_feature_expected=$(cat <<EXPECTED
POINT(1 2)	GEOMETRYCOLLECTION(POINT(1 2),LINESTRING(0 0,1 1))	POINT(1 2)	LINESTRING(0 0,1 1)	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "GeoJSON features dimensions and nulls" \
    "$from_feature_expected" \
    "SELECT ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"Feature\",\"geometry\":{\"type\":\"Point\",\"coordinates\":[1,2]},\"properties\":{\"a\":1}}',1,0)), "\
"ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"FeatureCollection\",\"features\":[{\"type\":\"Feature\",\"geometry\":{\"type\":\"Point\",\"coordinates\":[1,2]},\"properties\":{}},{\"type\":\"Feature\",\"geometry\":{\"type\":\"LineString\",\"coordinates\":[[0,0],[1,1]]},\"properties\":{}}]}',1,0)), "\
"ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"Point\",\"coordinates\":[1,2,3]}',2,0)), "\
"ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"LineString\",\"coordinates\":[[0,0,9],[1,1,8]]}',2,0)), "\
"ST_AsGeoJSON(NULL), ST_AsText(ST_GeomFromGeoJSON(NULL)), "\
"ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"Feature\",\"geometry\":null,\"properties\":{}}',1,0));"

expect_error \
    "invalid JSON text rejected" \
    3141 \
    "22032" \
    "Invalid JSON text in argument 1 to function st_geomfromgeojson" \
    "SELECT ST_AsText(ST_GeomFromGeoJSON('{bad}',1,0));"

expect_error \
    "invalid GeoJSON type rejected" \
    3072 \
    "HY000" \
    "Invalid GeoJSON data provided to function st_geomfromgeojson" \
    "SELECT ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"point\",\"coordinates\":[1,2]}',1,0));"

expect_error \
    "missing coordinates rejected" \
    3070 \
    "HY000" \
    "Missing required member 'coordinates'" \
    "SELECT ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"Point\"}',1,0));"

expect_error \
    "higher dimensions rejected by default" \
    3073 \
    "HY000" \
    "Unsupported number of coordinate dimensions in function st_geomfromgeojson" \
    "SELECT ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"Point\",\"coordinates\":[1,2,3]}',1,0));"

expect_error \
    "ST_GeomFromGeoJSON option rejected" \
    1411 \
    "HY000" \
    "Incorrect option value: '0' for function st_geomfromgeojson" \
    "SELECT ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"Point\",\"coordinates\":[1,2]}',0,0));"

expect_error \
    "ST_GeomFromGeoJSON null input invalid option rejected" \
    1411 \
    "HY000" \
    "Incorrect option value: '0' for function st_geomfromgeojson" \
    "SELECT ST_AsText(ST_GeomFromGeoJSON(NULL,0,0));"

expect_error \
    "ST_AsGeoJSON option rejected" \
    1411 \
    "HY000" \
    "Incorrect options value: '8' for function st_asgeojson" \
    "SELECT ST_AsGeoJSON(Point(1,2),2,8);"

expect_error \
    "ST_AsGeoJSON fractional option rejected" \
    3064 \
    "HY000" \
    "Incorrect type for argument options in function st_asgeojson" \
    "SELECT ST_AsGeoJSON(Point(1,2),2,1.5);"

expect_error \
    "ST_AsGeoJSON negative precision rejected" \
    1411 \
    "HY000" \
    "Incorrect max decimal digits value: '-1' for function st_asgeojson" \
    "SELECT ST_AsGeoJSON(Point(1,2),-1);"

expect_error \
    "unknown SRID rejected" \
    3548 \
    "SR001" \
    "There's no spatial reference system with SRID 999999." \
    "SELECT ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"Point\",\"coordinates\":[1,2]}',1,999999));"

expect_error \
    "negative SRID rejected" \
    1690 \
    "22003" \
    "SRID value is out of range in 'st_geomfromgeojson'" \
    "SELECT ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"Point\",\"coordinates\":[1,2]}',1,-1));"

expect_error \
    "longitude range rejected" \
    3616 \
    "22S02" \
    "Longitude 181.000000 is out of range in function st_geomfromgeojson" \
    "SELECT ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"Point\",\"coordinates\":[181,0]}'));"

expect_error \
    "latitude range rejected" \
    3617 \
    "22S03" \
    "Latitude 91.000000 is out of range in function st_geomfromgeojson" \
    "SELECT ST_AsText(ST_GeomFromGeoJSON('{\"type\":\"Point\",\"coordinates\":[0,91]}'));"

printf '%s\n' "mysql_baseline_spatial_geojson_functions_expectations: ok"
