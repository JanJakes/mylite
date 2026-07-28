#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_robust_spatial_topology_metrics_expectations: $1" >&2
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

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

threshold_expected=$(cat <<'EXPECTED'
point_line_5e-13	0.0000000000005	0	1
point_line_1e-12	0.000000000001	0	1
point_line_2e-12	0.000000000002	0	1
line_line_5e-13	0.0000000000005	0	1
line_line_1e-12	0.000000000001	0	1
line_line_2e-12	0.000000000002	0	1
polygon_out_5e-13	0.0000000000005	0	1	0	0
polygon_boundary	0	1	0	0	1
polygon_in_5e-13	0	1	0	1	0
near_endpoint_5e-13	0.0000000000005000444502911705	0	1
near_cross_5e-13	0	1	1
point_point_5e-13	0.0000000000005	0	0	1
tiny_line_5e-13	0.0000000000005	1	1
EXPECTED
)
threshold_sql=$(cat <<'SQL'
SELECT 'point_line_5e-13',
       ST_Distance(ST_GeomFromText('POINT(0.5 0.0000000000005)'),
                   ST_GeomFromText('LINESTRING(0 0,1 0)')),
       ST_Intersects(ST_GeomFromText('POINT(0.5 0.0000000000005)'),
                     ST_GeomFromText('LINESTRING(0 0,1 0)')),
       ST_Disjoint(ST_GeomFromText('POINT(0.5 0.0000000000005)'),
                   ST_GeomFromText('LINESTRING(0 0,1 0)'));
SELECT 'point_line_1e-12',
       ST_Distance(ST_GeomFromText('POINT(0.5 0.000000000001)'),
                   ST_GeomFromText('LINESTRING(0 0,1 0)')),
       ST_Intersects(ST_GeomFromText('POINT(0.5 0.000000000001)'),
                     ST_GeomFromText('LINESTRING(0 0,1 0)')),
       ST_Disjoint(ST_GeomFromText('POINT(0.5 0.000000000001)'),
                   ST_GeomFromText('LINESTRING(0 0,1 0)'));
SELECT 'point_line_2e-12',
       ST_Distance(ST_GeomFromText('POINT(0.5 0.000000000002)'),
                   ST_GeomFromText('LINESTRING(0 0,1 0)')),
       ST_Intersects(ST_GeomFromText('POINT(0.5 0.000000000002)'),
                     ST_GeomFromText('LINESTRING(0 0,1 0)')),
       ST_Disjoint(ST_GeomFromText('POINT(0.5 0.000000000002)'),
                   ST_GeomFromText('LINESTRING(0 0,1 0)'));
SELECT 'line_line_5e-13',
       ST_Distance(ST_GeomFromText('LINESTRING(0 0,1 0)'),
                   ST_GeomFromText('LINESTRING(0 0.0000000000005,1 0.0000000000005)')),
       ST_Intersects(ST_GeomFromText('LINESTRING(0 0,1 0)'),
                     ST_GeomFromText('LINESTRING(0 0.0000000000005,1 0.0000000000005)')),
       ST_Disjoint(ST_GeomFromText('LINESTRING(0 0,1 0)'),
                   ST_GeomFromText('LINESTRING(0 0.0000000000005,1 0.0000000000005)'));
SELECT 'line_line_1e-12',
       ST_Distance(ST_GeomFromText('LINESTRING(0 0,1 0)'),
                   ST_GeomFromText('LINESTRING(0 0.000000000001,1 0.000000000001)')),
       ST_Intersects(ST_GeomFromText('LINESTRING(0 0,1 0)'),
                     ST_GeomFromText('LINESTRING(0 0.000000000001,1 0.000000000001)')),
       ST_Disjoint(ST_GeomFromText('LINESTRING(0 0,1 0)'),
                   ST_GeomFromText('LINESTRING(0 0.000000000001,1 0.000000000001)'));
SELECT 'line_line_2e-12',
       ST_Distance(ST_GeomFromText('LINESTRING(0 0,1 0)'),
                   ST_GeomFromText('LINESTRING(0 0.000000000002,1 0.000000000002)')),
       ST_Intersects(ST_GeomFromText('LINESTRING(0 0,1 0)'),
                     ST_GeomFromText('LINESTRING(0 0.000000000002,1 0.000000000002)')),
       ST_Disjoint(ST_GeomFromText('LINESTRING(0 0,1 0)'),
                   ST_GeomFromText('LINESTRING(0 0.000000000002,1 0.000000000002)'));
SELECT 'polygon_out_5e-13',
       ST_Distance(ST_GeomFromText('POLYGON((0 0,1 0,1 1,0 1,0 0))'),
                   ST_GeomFromText('POINT(0.5 -0.0000000000005)')),
       ST_Intersects(ST_GeomFromText('POLYGON((0 0,1 0,1 1,0 1,0 0))'),
                     ST_GeomFromText('POINT(0.5 -0.0000000000005)')),
       ST_Disjoint(ST_GeomFromText('POLYGON((0 0,1 0,1 1,0 1,0 0))'),
                   ST_GeomFromText('POINT(0.5 -0.0000000000005)')),
       ST_Contains(ST_GeomFromText('POLYGON((0 0,1 0,1 1,0 1,0 0))'),
                   ST_GeomFromText('POINT(0.5 -0.0000000000005)')),
       ST_Touches(ST_GeomFromText('POLYGON((0 0,1 0,1 1,0 1,0 0))'),
                  ST_GeomFromText('POINT(0.5 -0.0000000000005)'));
SELECT 'polygon_boundary',
       ST_Distance(ST_GeomFromText('POLYGON((0 0,1 0,1 1,0 1,0 0))'),
                   ST_GeomFromText('POINT(0.5 0)')),
       ST_Intersects(ST_GeomFromText('POLYGON((0 0,1 0,1 1,0 1,0 0))'),
                     ST_GeomFromText('POINT(0.5 0)')),
       ST_Disjoint(ST_GeomFromText('POLYGON((0 0,1 0,1 1,0 1,0 0))'),
                   ST_GeomFromText('POINT(0.5 0)')),
       ST_Contains(ST_GeomFromText('POLYGON((0 0,1 0,1 1,0 1,0 0))'),
                   ST_GeomFromText('POINT(0.5 0)')),
       ST_Touches(ST_GeomFromText('POLYGON((0 0,1 0,1 1,0 1,0 0))'),
                  ST_GeomFromText('POINT(0.5 0)'));
SELECT 'polygon_in_5e-13',
       ST_Distance(ST_GeomFromText('POLYGON((0 0,1 0,1 1,0 1,0 0))'),
                   ST_GeomFromText('POINT(0.5 0.0000000000005)')),
       ST_Intersects(ST_GeomFromText('POLYGON((0 0,1 0,1 1,0 1,0 0))'),
                     ST_GeomFromText('POINT(0.5 0.0000000000005)')),
       ST_Disjoint(ST_GeomFromText('POLYGON((0 0,1 0,1 1,0 1,0 0))'),
                   ST_GeomFromText('POINT(0.5 0.0000000000005)')),
       ST_Contains(ST_GeomFromText('POLYGON((0 0,1 0,1 1,0 1,0 0))'),
                   ST_GeomFromText('POINT(0.5 0.0000000000005)')),
       ST_Touches(ST_GeomFromText('POLYGON((0 0,1 0,1 1,0 1,0 0))'),
                  ST_GeomFromText('POINT(0.5 0.0000000000005)'));
SELECT 'near_endpoint_5e-13',
       ST_Distance(ST_GeomFromText('LINESTRING(0 0,1 0)'),
                   ST_GeomFromText('LINESTRING(1.0000000000005 -1,1.0000000000005 1)')),
       ST_Intersects(ST_GeomFromText('LINESTRING(0 0,1 0)'),
                     ST_GeomFromText('LINESTRING(1.0000000000005 -1,1.0000000000005 1)')),
       ST_Disjoint(ST_GeomFromText('LINESTRING(0 0,1 0)'),
                   ST_GeomFromText('LINESTRING(1.0000000000005 -1,1.0000000000005 1)'));
SELECT 'near_cross_5e-13',
       ST_Distance(ST_GeomFromText('LINESTRING(0 0,1 0)'),
                   ST_GeomFromText('LINESTRING(0.5 -0.0000000000005,0.5 0.0000000000005)')),
       ST_Intersects(ST_GeomFromText('LINESTRING(0 0,1 0)'),
                     ST_GeomFromText('LINESTRING(0.5 -0.0000000000005,0.5 0.0000000000005)')),
       ST_Crosses(ST_GeomFromText('LINESTRING(0 0,1 0)'),
                  ST_GeomFromText('LINESTRING(0.5 -0.0000000000005,0.5 0.0000000000005)'));
SELECT 'point_point_5e-13',
       ST_Distance(ST_GeomFromText('POINT(0 0)'),
                   ST_GeomFromText('POINT(0.0000000000005 0)')),
       ST_Equals(ST_GeomFromText('POINT(0 0)'),
                 ST_GeomFromText('POINT(0.0000000000005 0)')),
       ST_Intersects(ST_GeomFromText('POINT(0 0)'),
                     ST_GeomFromText('POINT(0.0000000000005 0)')),
       ST_Disjoint(ST_GeomFromText('POINT(0 0)'),
                   ST_GeomFromText('POINT(0.0000000000005 0)'));
SELECT 'tiny_line_5e-13',
       ST_Length(ST_GeomFromText('LINESTRING(0 0,0.0000000000005 0)')),
       ST_IsValid(ST_GeomFromText('LINESTRING(0 0,0.0000000000005 0)')),
       ST_IsSimple(ST_GeomFromText('LINESTRING(0 0,0.0000000000005 0)'));
SQL
)
expect_output "threshold topology and metric matrix" "$threshold_expected" "$threshold_sql"

transform_expected=$(cat <<'EXPECTED'
base	0.0000000000005	0	1
half	0.00000000000025	0	1
double	0.000000000001	0	1
translated	0.0000000000004547473508864641	0	1
half_polygon	0.00000000000025	0	0
double_polygon	0.000000000001	0	0
translated_polygon	0.0000000000004547473508864641	0	0
EXPECTED
)
transform_sql=$(cat <<'SQL'
SELECT 'base',
       ST_Distance(ST_GeomFromText('POINT(0.5 0.0000000000005)'),
                   ST_GeomFromText('LINESTRING(0 0,1 0)')),
       ST_Intersects(ST_GeomFromText('POINT(0.5 0.0000000000005)'),
                     ST_GeomFromText('LINESTRING(0 0,1 0)')),
       ST_Disjoint(ST_GeomFromText('POINT(0.5 0.0000000000005)'),
                   ST_GeomFromText('LINESTRING(0 0,1 0)'));
SELECT 'half',
       ST_Distance(ST_GeomFromText('POINT(0.25 0.00000000000025)'),
                   ST_GeomFromText('LINESTRING(0 0,0.5 0)')),
       ST_Intersects(ST_GeomFromText('POINT(0.25 0.00000000000025)'),
                     ST_GeomFromText('LINESTRING(0 0,0.5 0)')),
       ST_Disjoint(ST_GeomFromText('POINT(0.25 0.00000000000025)'),
                   ST_GeomFromText('LINESTRING(0 0,0.5 0)'));
SELECT 'double',
       ST_Distance(ST_GeomFromText('POINT(1 0.000000000001)'),
                   ST_GeomFromText('LINESTRING(0 0,2 0)')),
       ST_Intersects(ST_GeomFromText('POINT(1 0.000000000001)'),
                     ST_GeomFromText('LINESTRING(0 0,2 0)')),
       ST_Disjoint(ST_GeomFromText('POINT(1 0.000000000001)'),
                   ST_GeomFromText('LINESTRING(0 0,2 0)'));
SELECT 'translated',
       ST_Distance(ST_GeomFromText('POINT(1000.5 1000.0000000000005)'),
                   ST_GeomFromText('LINESTRING(1000 1000,1001 1000)')),
       ST_Intersects(ST_GeomFromText('POINT(1000.5 1000.0000000000005)'),
                     ST_GeomFromText('LINESTRING(1000 1000,1001 1000)')),
       ST_Disjoint(ST_GeomFromText('POINT(1000.5 1000.0000000000005)'),
                   ST_GeomFromText('LINESTRING(1000 1000,1001 1000)'));
SELECT 'half_polygon',
       ST_Distance(ST_GeomFromText('POLYGON((0 0,0.5 0,0.5 0.5,0 0.5,0 0))'),
                   ST_GeomFromText('POINT(0.25 -0.00000000000025)')),
       ST_Intersects(ST_GeomFromText('POLYGON((0 0,0.5 0,0.5 0.5,0 0.5,0 0))'),
                     ST_GeomFromText('POINT(0.25 -0.00000000000025)')),
       ST_Contains(ST_GeomFromText('POLYGON((0 0,0.5 0,0.5 0.5,0 0.5,0 0))'),
                   ST_GeomFromText('POINT(0.25 -0.00000000000025)'));
SELECT 'double_polygon',
       ST_Distance(ST_GeomFromText('POLYGON((0 0,2 0,2 2,0 2,0 0))'),
                   ST_GeomFromText('POINT(1 -0.000000000001)')),
       ST_Intersects(ST_GeomFromText('POLYGON((0 0,2 0,2 2,0 2,0 0))'),
                     ST_GeomFromText('POINT(1 -0.000000000001)')),
       ST_Contains(ST_GeomFromText('POLYGON((0 0,2 0,2 2,0 2,0 0))'),
                   ST_GeomFromText('POINT(1 -0.000000000001)'));
SELECT 'translated_polygon',
       ST_Distance(ST_GeomFromText(
                       'POLYGON((1000 1000,1001 1000,1001 1001,1000 1001,1000 1000))'),
                   ST_GeomFromText('POINT(1000.5 999.9999999999995)')),
       ST_Intersects(ST_GeomFromText(
                         'POLYGON((1000 1000,1001 1000,1001 1001,1000 1001,1000 1000))'),
                     ST_GeomFromText('POINT(1000.5 999.9999999999995)')),
       ST_Contains(ST_GeomFromText(
                       'POLYGON((1000 1000,1001 1000,1001 1001,1000 1001,1000 1000))'),
                   ST_GeomFromText('POINT(1000.5 999.9999999999995)'));
SQL
)
expect_output "translation and scale matrix" "$transform_expected" "$transform_sql"

precision_expected=$(cat <<'EXPECTED'
unrepresentable_translation	POINT(1000000000.5 1000000000)	0	1
extreme_relative_scale	5e-19	1	0
tiny_square	2.5e-25	0
EXPECTED
)
precision_sql=$(cat <<'SQL'
SELECT 'unrepresentable_translation',
       ST_AsText(ST_GeomFromText('POINT(1000000000.5 1000000000.0000000000005)')),
       ST_Distance(ST_GeomFromText('POINT(1000000000.5 1000000000.0000000000005)'),
                   ST_GeomFromText(
                       'LINESTRING(1000000000 1000000000,1000000001 1000000000)')),
       ST_Intersects(ST_GeomFromText('POINT(1000000000.5 1000000000.0000000000005)'),
                     ST_GeomFromText(
                         'LINESTRING(1000000000 1000000000,1000000001 1000000000)'));
SELECT 'extreme_relative_scale',
       ST_Distance(ST_GeomFromText('POINT(0.0000005 0.0000000000000000005)'),
                   ST_GeomFromText('LINESTRING(0 0,0.000001 0)')),
       ST_Intersects(ST_GeomFromText('POINT(0.0000005 0.0000000000000000005)'),
                     ST_GeomFromText('LINESTRING(0 0,0.000001 0)')),
       ST_Disjoint(ST_GeomFromText('POINT(0.0000005 0.0000000000000000005)'),
                   ST_GeomFromText('LINESTRING(0 0,0.000001 0)'));
SELECT 'tiny_square',
       ST_Area(ST_GeomFromText(
           'POLYGON((0 0,0.0000000000005 0,0.0000000000005 0.0000000000005,'
           '0 0.0000000000005,0 0))')),
       ST_IsValid(ST_GeomFromText(
           'POLYGON((0 0,0.0000000000005 0,0.0000000000005 0.0000000000005,'
           '0 0.0000000000005,0 0))'));
SQL
)
expect_output "represented and runtime precision boundaries" "$precision_expected" "$precision_sql"

printf '%s\n' "mysql_robust_spatial_topology_metrics_expectations: ok"
