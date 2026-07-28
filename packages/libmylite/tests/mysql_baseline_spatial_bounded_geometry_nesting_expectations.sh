#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_spatial_bounded_geometry_nesting_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names
}

expect_output() {
    label=$1
    expected=$2
    sql=$3

    output=$(run_mysql "$sql")
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

    set +e
    output=$(run_mysql "$sql" 2>&1)
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

wrap_wkt_geometry() {
    depth=$1
    geometry='POINT(0 0)'
    index=0

    while [ "$index" -lt "$depth" ]; do
        geometry="GEOMETRYCOLLECTION($geometry)"
        index=$((index + 1))
    done
    printf '%s' "$geometry"
}

wrap_wkb_geometry() {
    depth=$1
    geometry='010100000000000000000000000000000000000000'
    index=0

    while [ "$index" -lt "$depth" ]; do
        geometry="010700000001000000$geometry"
        index=$((index + 1))
    done
    printf '%s' "$geometry"
}

wrap_geojson_geometry() {
    depth=$1
    geometry='{"type":"Point","coordinates":[0,0]}'
    index=0

    while [ "$index" -lt "$depth" ]; do
        geometry='{"type":"GeometryCollection","geometries":['"$geometry"']}'
        index=$((index + 1))
    done
    printf '%s' "$geometry"
}

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

wkt_49=$(wrap_wkt_geometry 49)
wkt_50=$(wrap_wkt_geometry 50)
wkt_51=$(wrap_wkt_geometry 51)
wkt_sql="SELECT LENGTH(ST_AsWKB(ST_GeomFromText('$wkt_49'))), "
wkt_sql="${wkt_sql}LENGTH(ST_AsWKB(ST_GeomFromText('$wkt_50'))), "
wkt_sql="${wkt_sql}LENGTH(ST_AsWKB(ST_GeomFromText('$wkt_51')));"
expect_output \
    "WKT geometry depths 50 through 52" \
    "462	471	480" \
    "$wkt_sql"

wkb_49=$(wrap_wkb_geometry 49)
wkb_50=$(wrap_wkb_geometry 50)
wkb_51=$(wrap_wkb_geometry 51)
wkb_sql="SELECT LENGTH(ST_AsWKB(ST_GeomFromWKB(UNHEX('$wkb_49')))), "
wkb_sql="${wkb_sql}LENGTH(ST_AsWKB(ST_GeomFromWKB(UNHEX('$wkb_50')))), "
wkb_sql="${wkb_sql}LENGTH(ST_AsWKB(ST_GeomFromWKB(UNHEX('$wkb_51'))));"
expect_output \
    "WKB geometry depths 50 through 52" \
    "462	471	480" \
    "$wkb_sql"

geojson_49=$(wrap_geojson_geometry 49)
geojson_50=$(wrap_geojson_geometry 50)
expect_output \
    "GeoJSON geometry depth 50" \
    "462" \
    "SELECT LENGTH(ST_AsWKB(ST_GeomFromGeoJSON('$geojson_49',1,0)));"
expect_error \
    "GeoJSON geometry depth 51" \
    3157 \
    "22032" \
    "The JSON document exceeds the maximum depth." \
    "SELECT ST_AsWKB(ST_GeomFromGeoJSON('$geojson_50',1,0));"

printf '%s\n' "mysql_baseline_spatial_bounded_geometry_nesting_expectations: ok"
