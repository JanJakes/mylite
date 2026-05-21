#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_spatial_index_metadata_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_spatial_index_metadata_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "${MYSQL_BIN:-mysql}" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --default-character-set=utf8mb4 --batch --raw --skip-column-names "$@"
        return
    fi
    if [ -n "$MYSQL_BIN" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=TCP -h127.0.0.1 -uroot --default-character-set=utf8mb4 \
                --batch --raw --skip-column-names "$@"
        return
    fi
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --default-character-set=utf8mb4 \
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
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

spatial_metadata_expected=$(cat <<\EXPECTED
0	1
spatial_meta	CREATE TABLE `spatial_meta` (
  `g` geometry DEFAULT NULL,
  `p` point NOT NULL,
  `l` linestring DEFAULT NULL,
  `poly` polygon DEFAULT NULL,
  `mp` multipoint DEFAULT NULL,
  `ml` multilinestring DEFAULT NULL,
  `mpoly` multipolygon DEFAULT NULL,
  `gc` geomcollection DEFAULT NULL,
  SPATIAL KEY `sp` (`p`) COMMENT 'geo' /*!80000 INVISIBLE */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
g	geometry	YES		NULL	
p	point	NO	MUL	NULL	
l	linestring	YES		NULL	
poly	polygon	YES		NULL	
mp	multipoint	YES		NULL	
ml	multilinestring	YES		NULL	
mpoly	multipolygon	YES		NULL	
gc	geomcollection	YES		NULL	
spatial_meta	1	sp	1	p	A	0	32	NULL		SPATIAL		geo	NO	NULL
g	geometry	geometry	YES	NULL		NULL
p	point	point	NO	NULL	MUL	NULL
l	linestring	linestring	YES	NULL		NULL
poly	polygon	polygon	YES	NULL		NULL
mp	multipoint	multipoint	YES	NULL		NULL
ml	multilinestring	multilinestring	YES	NULL		NULL
mpoly	multipolygon	multipolygon	YES	NULL		NULL
gc	geomcollection	geomcollection	YES	NULL		NULL
sp	1	1	p	A	32		SPATIAL	geo	NO	NULL
EXPECTED
)
expect_output \
    "spatial column and index metadata" \
    "$spatial_metadata_expected" \
    "USE ${DATABASE}; "\
"CREATE TABLE spatial_meta ("\
"g GEOMETRY, p POINT NOT NULL, l LINESTRING, poly POLYGON, mp MULTIPOINT, "\
"ml MULTILINESTRING, mpoly MULTIPOLYGON, gc GEOMETRYCOLLECTION, "\
"SPATIAL KEY sp (p) COMMENT 'geo' INVISIBLE"\
"); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE spatial_meta; "\
"SHOW COLUMNS FROM spatial_meta; "\
"SHOW INDEX FROM spatial_meta; "\
"SELECT COLUMN_NAME, DATA_TYPE, COLUMN_TYPE, IS_NULLABLE, COLUMN_DEFAULT, COLUMN_KEY, SRS_ID "\
"FROM information_schema.columns "\
"WHERE table_schema = '${DATABASE}' AND table_name = 'spatial_meta' "\
"ORDER BY ORDINAL_POSITION; "\
"SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, COLLATION, SUB_PART, "\
"NULLABLE, INDEX_TYPE, INDEX_COMMENT, IS_VISIBLE, EXPRESSION "\
"FROM information_schema.statistics "\
"WHERE table_schema = '${DATABASE}' AND table_name = 'spatial_meta' "\
"ORDER BY INDEX_NAME, SEQ_IN_INDEX;"

index_forms_expected=$(cat <<\EXPECTED
alter_spatial	CREATE TABLE `alter_spatial` (
  `g` geometry NOT NULL,
  `p` point NOT NULL,
  SPATIAL KEY `sg` (`g`),
  SPATIAL KEY `sp` (`p`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
2
implicit_spatial	CREATE TABLE `implicit_spatial` (
  `g` geometry NOT NULL,
  SPATIAL KEY `kg` (`g`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
1
EXPECTED
)
expect_output \
    "alter standalone and implicit spatial index forms" \
    "$index_forms_expected" \
    "USE ${DATABASE}; "\
"CREATE TABLE alter_spatial (g GEOMETRY NOT NULL, p POINT NOT NULL); "\
"ALTER TABLE alter_spatial ADD SPATIAL INDEX sg (g); "\
"CREATE SPATIAL INDEX sp ON alter_spatial (p); "\
"SHOW CREATE TABLE alter_spatial; "\
"SELECT COUNT(*) FROM information_schema.statistics "\
"WHERE table_schema = '${DATABASE}' AND table_name = 'alter_spatial' "\
"AND INDEX_TYPE = 'SPATIAL'; "\
"CREATE TABLE implicit_spatial (g GEOMETRY NOT NULL, KEY kg (g)); "\
"SHOW CREATE TABLE implicit_spatial; "\
"SELECT COUNT(*) FROM information_schema.statistics "\
"WHERE table_schema = '${DATABASE}' AND table_name = 'implicit_spatial' "\
"AND INDEX_TYPE = 'SPATIAL';"

rtree_type_options_expected=$(cat <<\EXPECTED
Warning	3674	The spatial index on column 'g' will not be used by the query optimizer since the column does not have an SRID attribute. Consider adding an SRID attribute to the column.
1
rtree_leading	CREATE TABLE `rtree_leading` (
  `g` geometry NOT NULL,
  SPATIAL KEY `kg` (`g`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
rtree_trailing	CREATE TABLE `rtree_trailing` (
  `g` geometry NOT NULL,
  SPATIAL KEY `kg` (`g`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
1
1
rtree_added	CREATE TABLE `rtree_added` (
  `g` geometry NOT NULL,
  `p` point NOT NULL,
  SPATIAL KEY `kg` (`g`),
  SPATIAL KEY `kp` (`p`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
kg	SPATIAL	g
kp	SPATIAL	p
EXPECTED
)
expect_output \
    "ordinary RTREE spatial index type options" \
    "$rtree_type_options_expected" \
    "USE ${DATABASE}; "\
"CREATE TABLE rtree_leading (g GEOMETRY NOT NULL, KEY kg USING RTREE (g)); "\
"SHOW WARNINGS; "\
"SELECT @@warning_count; "\
"SHOW CREATE TABLE rtree_leading; "\
"CREATE TABLE rtree_trailing (g GEOMETRY NOT NULL, KEY kg (g) USING RTREE); "\
"SHOW CREATE TABLE rtree_trailing; "\
"CREATE TABLE rtree_added (g GEOMETRY NOT NULL, p POINT NOT NULL); "\
"ALTER TABLE rtree_added ADD INDEX kg USING RTREE (g); "\
"SELECT @@warning_count; "\
"CREATE INDEX kp ON rtree_added (p) USING RTREE; "\
"SELECT @@warning_count; "\
"SHOW CREATE TABLE rtree_added; "\
"SELECT INDEX_NAME, INDEX_TYPE, COLUMN_NAME FROM information_schema.statistics "\
"WHERE table_schema = '${DATABASE}' AND table_name = 'rtree_added' "\
"ORDER BY INDEX_NAME;"

create_like_expected=$(cat <<\EXPECTED
0
spatial_like_clone	CREATE TABLE `spatial_like_clone` (
  `g` geometry DEFAULT NULL,
  `p` point NOT NULL,
  SPATIAL KEY `sp` (`p`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
sp	SPATIAL	p
EXPECTED
)
expect_output \
    "spatial create table like metadata" \
    "$create_like_expected" \
    "USE ${DATABASE}; "\
"CREATE TABLE spatial_like_source (g GEOMETRY, p POINT NOT NULL, SPATIAL KEY sp (p)); "\
"CREATE TABLE spatial_like_clone LIKE spatial_like_source; "\
"SELECT @@warning_count; "\
"SHOW CREATE TABLE spatial_like_clone; "\
"SELECT INDEX_NAME, INDEX_TYPE, COLUMN_NAME FROM information_schema.statistics "\
"WHERE table_schema = '${DATABASE}' AND table_name = 'spatial_like_clone';"

dml_expected=$(cat <<\EXPECTED
1	1
2	1
3	1
EXPECTED
)
expect_output \
    "nullable spatial null/default row values" \
    "$dml_expected" \
    "USE ${DATABASE}; "\
"CREATE TABLE nullable_spatial (id INT, g GEOMETRY); "\
"INSERT INTO nullable_spatial (id) VALUES (1); "\
"INSERT INTO nullable_spatial VALUES (2, DEFAULT); "\
"INSERT INTO nullable_spatial VALUES (3, NULL); "\
"SELECT id, g IS NULL FROM nullable_spatial ORDER BY id;"

expect_error \
    "nullable spatial index rejected" \
    1252 \
    42000 \
    "All parts of a SPATIAL index must be NOT NULL" \
    "USE ${DATABASE}; CREATE TABLE spatial_nullable_index (g GEOMETRY, SPATIAL KEY sg (g));"

expect_error \
    "spatial index on non-spatial column rejected" \
    1687 \
    42000 \
    "A SPATIAL index may only contain a geometrical type column" \
    "USE ${DATABASE}; CREATE TABLE spatial_nonspatial (id INT NOT NULL); "\
"CREATE SPATIAL INDEX sid ON spatial_nonspatial (id);"

expect_error \
    "ordinary BTREE spatial type option rejected" \
    3729 \
    HY000 \
    "The index type BTREE is not supported for spatial indexes." \
    "USE ${DATABASE}; CREATE TABLE ordinary_btree_spatial ("\
"g GEOMETRY NOT NULL, KEY kg USING BTREE (g));"

expect_error \
    "ordinary HASH spatial type option rejected" \
    3729 \
    HY000 \
    "The index type HASH is not supported for spatial indexes." \
    "USE ${DATABASE}; CREATE TABLE ordinary_hash_spatial ("\
"g GEOMETRY NOT NULL, KEY kg (g) USING HASH);"

expect_error \
    "ordinary RTREE nonspatial type option rejected" \
    1687 \
    42000 \
    "A SPATIAL index may only contain a geometrical type column" \
    "USE ${DATABASE}; CREATE TABLE ordinary_rtree_nonspatial ("\
"id INT NOT NULL, KEY kid USING RTREE (id));"

expect_error \
    "unique RTREE spatial type option rejected" \
    3728 \
    HY000 \
    "Spatial indexes can't be primary or unique indexes." \
    "USE ${DATABASE}; CREATE TABLE unique_rtree_spatial ("\
"g GEOMETRY NOT NULL, UNIQUE KEY ug USING RTREE (g));"

expect_error \
    "ALTER ADD BTREE spatial type option rejected" \
    3729 \
    HY000 \
    "The index type BTREE is not supported for spatial indexes." \
    "USE ${DATABASE}; CREATE TABLE alter_btree_spatial_type (g GEOMETRY NOT NULL); "\
"ALTER TABLE alter_btree_spatial_type ADD INDEX bad_btree USING BTREE (g);"

expect_error \
    "ALTER ADD HASH spatial type option rejected" \
    3729 \
    HY000 \
    "The index type HASH is not supported for spatial indexes." \
    "USE ${DATABASE}; CREATE TABLE alter_hash_spatial_type (g GEOMETRY NOT NULL); "\
"ALTER TABLE alter_hash_spatial_type ADD INDEX bad_hash (g) USING HASH;"

expect_error \
    "ALTER ADD RTREE nonspatial type option rejected" \
    1687 \
    42000 \
    "A SPATIAL index may only contain a geometrical type column" \
    "USE ${DATABASE}; CREATE TABLE alter_rtree_nonspatial_type (id INT NOT NULL); "\
"ALTER TABLE alter_rtree_nonspatial_type ADD INDEX bad_rtree USING RTREE (id);"

expect_error \
    "standalone BTREE spatial type option rejected" \
    3729 \
    HY000 \
    "The index type BTREE is not supported for spatial indexes." \
    "USE ${DATABASE}; CREATE TABLE standalone_btree_spatial_type (g GEOMETRY NOT NULL); "\
"CREATE INDEX bad_btree USING BTREE ON standalone_btree_spatial_type (g);"

expect_error \
    "standalone HASH spatial type option rejected" \
    3729 \
    HY000 \
    "The index type HASH is not supported for spatial indexes." \
    "USE ${DATABASE}; CREATE TABLE standalone_hash_spatial_type (g GEOMETRY NOT NULL); "\
"CREATE INDEX bad_hash ON standalone_hash_spatial_type (g) USING HASH;"

expect_error \
    "standalone RTREE nonspatial type option rejected" \
    1687 \
    42000 \
    "A SPATIAL index may only contain a geometrical type column" \
    "USE ${DATABASE}; CREATE TABLE standalone_rtree_nonspatial_type (id INT NOT NULL); "\
"CREATE INDEX bad_rtree USING RTREE ON standalone_rtree_nonspatial_type (id);"

expect_error \
    "explicit SPATIAL RTREE type option rejected" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "USE ${DATABASE}; CREATE TABLE explicit_spatial_rtree ("\
"g GEOMETRY NOT NULL, SPATIAL KEY sg (g) USING RTREE);"

expect_error \
    "standalone explicit SPATIAL RTREE type option rejected" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "USE ${DATABASE}; CREATE TABLE standalone_spatial_rtree (g GEOMETRY NOT NULL); "\
"CREATE SPATIAL INDEX sg ON standalone_spatial_rtree (g) USING RTREE;"

expect_error \
    "spatial index missing column rejected" \
    1072 \
    42000 \
    "Key column 'missing' doesn't exist in table" \
    "USE ${DATABASE}; CREATE TABLE spatial_missing_index_column (id INT NOT NULL); "\
"CREATE SPATIAL INDEX smissing ON spatial_missing_index_column (missing);"

expect_error \
    "spatial index rejects multiple key parts" \
    1070 \
    42000 \
    "Too many key parts specified; max 1 parts allowed" \
    "USE ${DATABASE}; CREATE TABLE spatial_multi (g GEOMETRY NOT NULL, p POINT NOT NULL); "\
"CREATE SPATIAL INDEX sm ON spatial_multi (g, p);"

expect_error \
    "spatial prefix rejected" \
    1089 \
    HY000 \
    "Incorrect prefix key" \
    "USE ${DATABASE}; CREATE TABLE spatial_prefix (g GEOMETRY NOT NULL); "\
"CREATE SPATIAL INDEX sprefix ON spatial_prefix (g(4));"

expect_error \
    "spatial order rejected" \
    1221 \
    HY000 \
    "Incorrect usage of spatial/fulltext/hash index and explicit index order" \
    "USE ${DATABASE}; CREATE TABLE spatial_order (g GEOMETRY NOT NULL); "\
"CREATE SPATIAL INDEX sorder ON spatial_order (g DESC);"

expect_error \
    "unique spatial rejected" \
    3728 \
    HY000 \
    "Spatial indexes can't be primary or unique indexes." \
    "USE ${DATABASE}; CREATE TABLE unique_spatial (g GEOMETRY NOT NULL, UNIQUE KEY ug (g));"

expect_error \
    "inline unique spatial rejected" \
    3728 \
    HY000 \
    "Spatial indexes can't be primary or unique indexes." \
    "USE ${DATABASE}; CREATE TABLE inline_unique_spatial (g GEOMETRY UNIQUE);"

expect_error \
    "primary spatial rejected" \
    3728 \
    HY000 \
    "Spatial indexes can't be primary or unique indexes." \
    "USE ${DATABASE}; CREATE TABLE primary_spatial (g GEOMETRY NOT NULL, PRIMARY KEY (g));"

expect_error \
    "inline primary spatial rejected" \
    3728 \
    HY000 \
    "Spatial indexes can't be primary or unique indexes." \
    "USE ${DATABASE}; CREATE TABLE inline_primary_spatial (g GEOMETRY PRIMARY KEY);"

expect_error \
    "duplicate spatial key name rejected" \
    1061 \
    42000 \
    "Duplicate key name 'sg'" \
    "USE ${DATABASE}; CREATE TABLE spatial_duplicate (g GEOMETRY NOT NULL, SPATIAL KEY sg (g)); "\
"ALTER TABLE spatial_duplicate ADD SPATIAL INDEX sg (g);"

expect_error \
    "non-null spatial default rejected" \
    1101 \
    42000 \
    "BLOB, TEXT, GEOMETRY or JSON column 'g' can't have a default value" \
    "USE ${DATABASE}; CREATE TABLE spatial_default (g GEOMETRY DEFAULT 'x');"

expect_error \
    "not null default null rejected" \
    1067 \
    42000 \
    "Invalid default value for 'g'" \
    "USE ${DATABASE}; CREATE TABLE spatial_not_null_default (g GEOMETRY NOT NULL DEFAULT NULL);"

expect_error \
    "not null spatial missing default rejected" \
    1364 \
    HY000 \
    "Field 'g' doesn't have a default value" \
    "USE ${DATABASE}; CREATE TABLE spatial_missing_default (id INT, g GEOMETRY NOT NULL); "\
"INSERT INTO spatial_missing_default (id) VALUES (1);"

expect_error \
    "not null spatial explicit null rejected" \
    3673 \
    23000 \
    "Column 'g' cannot be null" \
    "USE ${DATABASE}; CREATE TABLE spatial_not_null_values (id INT, g GEOMETRY NOT NULL); "\
"INSERT INTO spatial_not_null_values VALUES (1, NULL);"

printf '%s\n' "baseline spatial index metadata MySQL 8.4.9 expectations verified"
