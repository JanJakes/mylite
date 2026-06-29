#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_spatial_constructive_operator_functions_expectations: $1" >&2
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

strategy_expected=$(cat <<EXPECTED
point_square	060000000000000000000000
end_flat	020000000000000000000000
point_circle	050000003333333333330740
join_round	030000000000000000004040
join_miter	040000000000000000004040
end_round	010000000000000000004040
upper	060000000000000000000000
null_first	NULL
null_second	NULL
EXPECTED
)
expect_output \
    "buffer strategy bytes" \
    "$strategy_expected" \
    "SELECT 'point_square', HEX(ST_Buffer_Strategy('point_square')) UNION ALL "\
"SELECT 'end_flat', HEX(ST_Buffer_Strategy('end_flat')) UNION ALL "\
"SELECT 'point_circle', HEX(ST_Buffer_Strategy('point_circle', 2.9)) UNION ALL "\
"SELECT 'join_round', HEX(ST_Buffer_Strategy('join_round', 32)) UNION ALL "\
"SELECT 'join_miter', HEX(ST_Buffer_Strategy('join_miter', 32)) UNION ALL "\
"SELECT 'end_round', HEX(ST_Buffer_Strategy('end_round', 32)) UNION ALL "\
"SELECT 'upper', HEX(ST_Buffer_Strategy('POINT_SQUARE')) UNION ALL "\
"SELECT 'null_first', HEX(ST_Buffer_Strategy(NULL)) UNION ALL "\
"SELECT 'null_second', HEX(ST_Buffer_Strategy('point_circle', NULL));"

buffer_expected=$(cat <<EXPECTED
zero	POINT(0 0)
nonnumeric_zero	POINT(0 0)
square_zero	POINT(0 0)
null_strategy	NULL
null_distance	NULL
null_geometry	NULL
geographic_zero	4326
EXPECTED
)
expect_output \
    "buffer zero identity" \
    "$buffer_expected" \
    "SELECT 'zero', ST_AsText(ST_Buffer(Point(0,0), 0)) UNION ALL "\
"SELECT 'nonnumeric_zero', ST_AsText(ST_Buffer(Point(0,0), 'abc')) UNION ALL "\
"SELECT 'square_zero', ST_AsText(ST_Buffer(Point(0,0), 0, ST_Buffer_Strategy('point_square'))) "\
"UNION ALL "\
"SELECT 'null_strategy', ST_AsText(ST_Buffer(Point(0,0), 0, NULL)) UNION ALL "\
"SELECT 'null_distance', ST_AsText(ST_Buffer(Point(0,0), NULL)) UNION ALL "\
"SELECT 'null_geometry', ST_AsText(ST_Buffer(NULL, 0)) UNION ALL "\
"SELECT 'geographic_zero', ST_SRID(ST_Buffer(ST_GeomFromText('POINT(1 1)', 4326), 0));"

operators_expected=$(cat <<EXPECTED
difference_point	POINT(1 1)
difference_empty	GEOMETRYCOLLECTION EMPTY
intersection_point	POINT(1 1)
intersection_empty	GEOMETRYCOLLECTION EMPTY
union_same	POINT(1 1)
union_distinct	MULTIPOINT((1 1),(2 2))
symdiff_distinct	MULTIPOINT((1 1),(2 2))
symdiff_empty	GEOMETRYCOLLECTION EMPTY
multi_difference	MULTIPOINT((1 1),(3 3))
multi_intersection	POINT(2 2)
multi_union	MULTIPOINT((1 1),(2 2),(3 3),(4 4))
multi_symdiff	MULTIPOINT((1 1),(3 3),(4 4))
point_multi_union	MULTIPOINT((1 1),(2 2))
multi_point_symdiff	MULTIPOINT((3 3),(1 1),(2 2))
null_left	NULL
EXPECTED
)
expect_output \
    "point set operator results" \
    "$operators_expected" \
    "SELECT 'difference_point', ST_AsText(ST_Difference(Point(1,1), Point(2,2))) UNION ALL "\
"SELECT 'difference_empty', ST_AsText(ST_Difference(Point(1,1), Point(1,1))) UNION ALL "\
"SELECT 'intersection_point', ST_AsText(ST_Intersection(Point(1,1), Point(1,1))) UNION ALL "\
"SELECT 'intersection_empty', ST_AsText(ST_Intersection(Point(1,1), Point(2,2))) UNION ALL "\
"SELECT 'union_same', ST_AsText(ST_Union(Point(1,1), Point(1,1))) UNION ALL "\
"SELECT 'union_distinct', ST_AsText(ST_Union(Point(1,1), Point(2,2))) UNION ALL "\
"SELECT 'symdiff_distinct', ST_AsText(ST_SymDifference(Point(1,1), Point(2,2))) UNION ALL "\
"SELECT 'symdiff_empty', ST_AsText(ST_SymDifference(Point(1,1), Point(1,1))) UNION ALL "\
"SELECT 'multi_difference', ST_AsText(ST_Difference("\
"ST_GeomFromText('MULTIPOINT((1 1),(2 2),(2 2),(3 3))'), "\
"ST_GeomFromText('MULTIPOINT((2 2),(4 4))'))) UNION ALL "\
"SELECT 'multi_intersection', ST_AsText(ST_Intersection("\
"ST_GeomFromText('MULTIPOINT((1 1),(2 2),(2 2),(3 3))'), "\
"ST_GeomFromText('MULTIPOINT((2 2),(4 4))'))) UNION ALL "\
"SELECT 'multi_union', ST_AsText(ST_Union("\
"ST_GeomFromText('MULTIPOINT((1 1),(2 2),(2 2),(3 3))'), "\
"ST_GeomFromText('MULTIPOINT((2 2),(4 4))'))) UNION ALL "\
"SELECT 'multi_symdiff', ST_AsText(ST_SymDifference("\
"ST_GeomFromText('MULTIPOINT((1 1),(2 2),(2 2),(3 3))'), "\
"ST_GeomFromText('MULTIPOINT((2 2),(4 4))'))) UNION ALL "\
"SELECT 'point_multi_union', ST_AsText(ST_Union(Point(1,1), "\
"ST_GeomFromText('MULTIPOINT((1 1),(2 2))'))) UNION ALL "\
"SELECT 'multi_point_symdiff', ST_AsText(ST_SymDifference(Point(3,3), "\
"ST_GeomFromText('MULTIPOINT((1 1),(2 2))'))) UNION ALL "\
"SELECT 'null_left', ST_AsText(ST_Union(NULL, Point(1,1)));"

transform_expected=$(cat <<EXPECTED
zero	POINT(1 1)
zero_text	POINT(1 1)
null_geometry	NULL
null_target	NULL
EXPECTED
)
expect_output \
    "transform identity results" \
    "$transform_expected" \
    "SELECT 'zero', ST_AsText(ST_Transform(Point(1,1), 0)) UNION ALL "\
"SELECT 'zero_text', ST_AsText(ST_Transform(Point(1,1), '0abc')) UNION ALL "\
"SELECT 'null_geometry', ST_AsText(ST_Transform(NULL, 0)) UNION ALL "\
"SELECT 'null_target', ST_AsText(ST_Transform(Point(1,1), NULL));"

expect_error \
    "buffer strategy arity zero" \
    1582 \
    42000 \
    "Incorrect parameter count" \
    "SELECT ST_Buffer_Strategy();"

expect_error \
    "buffer strategy arity three" \
    1582 \
    42000 \
    "Incorrect parameter count" \
    "SELECT ST_Buffer_Strategy('point_circle', 1, 2);"

expect_error \
    "buffer strategy invalid name" \
    1210 \
    HY000 \
    "Incorrect arguments to st_buffer_strategy" \
    "SELECT ST_Buffer_Strategy('bad', NULL);"

expect_error \
    "buffer strategy missing points" \
    1210 \
    HY000 \
    "Incorrect arguments to st_buffer_strategy" \
    "SELECT ST_Buffer_Strategy('point_circle');"

expect_error \
    "buffer strategy point square extra" \
    1210 \
    HY000 \
    "Incorrect arguments to st_buffer_strategy" \
    "SELECT ST_Buffer_Strategy('point_square', NULL);"

expect_error \
    "buffer strategy too many points" \
    3134 \
    HY000 \
    "Parameter points_per_circle exceeds the maximum number of points in a geometry" \
    "SELECT ST_Buffer_Strategy('point_circle', @@max_points_in_geometry + 1);"

expect_error \
    "transform from srid zero" \
    3741 \
    22S00 \
    "Transformation from SRID 0 is not supported" \
    "SELECT ST_Transform(Point(1,1), 4326);"

printf '%s\n' "mysql_baseline_spatial_constructive_operator_functions_expectations: ok"
