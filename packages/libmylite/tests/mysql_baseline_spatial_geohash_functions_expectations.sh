#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_spatial_geohash_functions_expectations: $1" >&2
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

encode_expected=$(cat <<EXPECTED
xbpbpbpbpb	000000000000000	mh2n0p0581	mh2n0p0581	-20	45	POINT(45 -20)	0	100	s	s0	s000000000
EXPECTED
)
expect_output \
    "encode decode and length basics" \
    "$encode_expected" \
    "SELECT ST_GeoHash(180,0,10), ST_GeoHash(-180,-90,15), "\
"ST_GeoHash(45,-20,10), ST_GeoHash(Point(45,-20),10), "\
"ST_LatFromGeoHash(ST_GeoHash(45,-20,10)), ST_LongFromGeoHash(ST_GeoHash(45,-20,10)), "\
"ST_AsText(ST_PointFromGeoHash(ST_GeoHash(45,-20,10),0)), "\
"ST_SRID(ST_PointFromGeoHash(ST_GeoHash(45,-20,10),0)), "\
"LENGTH(ST_GeoHash(0,0,100)), ST_GeoHash(0,0,1), ST_GeoHash(0,0,2), "\
"ST_GeoHash(0,0,10);"

decode_expected=$(cat <<EXPECTED
-68	-158	POINT(-158 -68)	22	22	POINT(22 22)	3	6	POINT(6 3)	-20.000007	45
EXPECTED
)
expect_output \
    "decode midpoint display" \
    "$decode_expected" \
    "SELECT ST_LatFromGeoHash('0'), ST_LongFromGeoHash('0'), "\
"ST_AsText(ST_PointFromGeoHash('0',0)), ST_LatFromGeoHash('s'), ST_LongFromGeoHash('s'), "\
"ST_AsText(ST_PointFromGeoHash('s',0)), ST_LatFromGeoHash('s0'), ST_LongFromGeoHash('s0'), "\
"ST_AsText(ST_PointFromGeoHash('s0',0)), ST_LatFromGeoHash('mh2n0p0580'), "\
"ST_LongFromGeoHash('mh2n0p0580');"

srid_expected=$(cat <<EXPECTED
POINT(-20 45)	4326	mh2n0p0581	mh2n0p0581
EXPECTED
)
expect_output \
    "srid 4326 geohash axis handling" \
    "$srid_expected" \
    "SELECT ST_AsText(ST_PointFromGeoHash('mh2n0p0581',4326)), "\
"ST_SRID(ST_PointFromGeoHash('mh2n0p0581',4326)), "\
"ST_GeoHash(ST_PointFromGeoHash('mh2n0p0581',0),10), "\
"ST_GeoHash(ST_PointFromGeoHash('mh2n0p0581',4326),10);"

null_expected=$(cat <<EXPECTED
NULL	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "null propagation" \
    "$null_expected" \
    "SELECT ST_GeoHash(NULL,0,10), ST_GeoHash(0,NULL,10), "\
"ST_GeoHash(0,0,NULL), ST_GeoHash(NULL,10);"

expect_error \
    "max length zero rejected" \
    1690 \
    "22003" \
    "max geohash length value is out of range in 'st_geohash'" \
    "SELECT ST_GeoHash(0,0,0);"

expect_error \
    "longitude rejected" \
    1690 \
    "22003" \
    "longitude value is out of range in 'st_geohash'" \
    "SELECT ST_GeoHash(181,0,10);"

expect_error \
    "latitude rejected" \
    1690 \
    "22003" \
    "latitude value is out of range in 'st_geohash'" \
    "SELECT ST_GeoHash(0,91,10);"

expect_error \
    "point argument type rejected" \
    3064 \
    "HY000" \
    "Incorrect type for argument point in function st_geohash" \
    "SELECT ST_GeoHash(ST_GeomFromText('LINESTRING(0 0,1 1)'),10);"

expect_error \
    "fractional length rejected" \
    3064 \
    "HY000" \
    "Incorrect type for argument geohash max length in function st_geohash" \
    "SELECT ST_GeoHash(0,0,10.7);"

expect_error \
    "empty geohash rejected" \
    1411 \
    "HY000" \
    "Incorrect geohash value: '' for function ST_LATFROMGEOHASH" \
    "SELECT ST_LatFromGeoHash('');"

expect_error \
    "invalid geohash rejected" \
    1411 \
    "HY000" \
    "Incorrect geohash value: '!' for function ST_LONGFROMGEOHASH" \
    "SELECT ST_LongFromGeoHash('!');"

expect_error \
    "unknown srid rejected" \
    3548 \
    "SR001" \
    "There's no spatial reference system with SRID 999999." \
    "SELECT ST_AsText(ST_PointFromGeoHash('mh2n0p0581',999999));"

expect_error \
    "negative srid rejected" \
    1690 \
    "22003" \
    "SRID value is out of range in 'st_pointfromgeohash'" \
    "SELECT ST_AsText(ST_PointFromGeoHash('mh2n0p0581',-1));"

expect_error \
    "fractional srid rejected" \
    3064 \
    "HY000" \
    "Incorrect type for argument SRID in function st_pointfromgeohash" \
    "SELECT ST_AsText(ST_PointFromGeoHash('mh2n0p0581',1.5));"

printf '%s\n' "mysql_baseline_spatial_geohash_functions_expectations: ok"
