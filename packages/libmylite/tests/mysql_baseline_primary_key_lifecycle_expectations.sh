#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_primary_key_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_primary_key_lifecycle_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw --skip-column-names "$@"
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
        *)
            fail "$label: expected error $code/$state containing [$message], got [$output]"
            ;;
    esac
}

expect_upstream_accepts() {
    label=$1
    sql=$2
    shift 2

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status_code=$?
    set -e

    if [ "$status_code" -ne 0 ]; then
        fail "$label: expected MySQL to accept deferred behavior, got [$output]"
    fi
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

show_columns_expected=$(printf '%b' 'id\tint\tNO\tPRI\tNULL\t\nv\tint\tYES\t\tNULL\t\nn\tint\tYES\t\tNULL\t')
show_index_expected=$(cat <<\EXPECTED
inline_pk	0	PRIMARY	1	id	A	0	NULL	NULL		BTREE			YES	NULL
EXPECTED
)
show_create_expected=$(cat <<\EXPECTED
inline_pk	CREATE TABLE `inline_pk` (
  `id` int NOT NULL,
  `v` int DEFAULT NULL,
  `n` int DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "inline primary key metadata" \
    "$show_columns_expected
$show_index_expected
$show_create_expected" \
    "CREATE TABLE inline_pk (id INT PRIMARY KEY, v INT, n INT NULL); "\
"SHOW COLUMNS FROM inline_pk; "\
"SHOW INDEX FROM inline_pk; "\
"SHOW CREATE TABLE inline_pk;" \
    "$DATABASE"

table_primary_expected=$(cat <<\EXPECTED
table_pk	CREATE TABLE `table_pk` (
  `id` int NOT NULL,
  `v` int DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
table_pk	0	PRIMARY	1	id	A	0	NULL	NULL		BTREE			YES	NULL
EXPECTED
)
expect_output \
    "table-level primary key metadata" \
    "$table_primary_expected" \
    "CREATE TABLE table_pk (id INT, v INT, PRIMARY KEY (id)); "\
"SHOW CREATE TABLE table_pk; "\
"SHOW INDEX FROM table_pk;" \
    "$DATABASE"

unsigned_columns_expected=$(printf '%b' 'u\tint unsigned\tNO\tPRI\tNULL\t\nb\tbigint unsigned\tYES\t\tNULL\t')
unsigned_create_expected=$(cat <<\EXPECTED
unsigned_pk	CREATE TABLE `unsigned_pk` (
  `u` int unsigned NOT NULL,
  `b` bigint unsigned DEFAULT NULL,
  PRIMARY KEY (`u`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
unsigned_expected="${unsigned_columns_expected}
${unsigned_create_expected}"
expect_output \
    "unsigned integer primary key metadata" \
    "$unsigned_expected" \
    "CREATE TABLE unsigned_pk (u INT UNSIGNED PRIMARY KEY, b BIGINT UNSIGNED); "\
"SHOW COLUMNS FROM unsigned_pk; "\
"SHOW CREATE TABLE unsigned_pk;" \
    "$DATABASE"

default_columns_expected=$(printf '%b' 'id\tint\tNO\tPRI\t7\t\nv\tint\tYES\t\tNULL\t')
default_rest_expected=$(cat <<\EXPECTED
default_pk	CREATE TABLE `default_pk` (
  `id` int NOT NULL DEFAULT '7',
  `v` int DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
1	0	7:10
EXPECTED
)
default_expected="${default_columns_expected}
${default_rest_expected}"
expect_output \
    "integer default on primary key" \
    "$default_expected" \
    "CREATE TABLE default_pk (id INT DEFAULT 7 PRIMARY KEY, v INT); "\
"SHOW COLUMNS FROM default_pk; "\
"SHOW CREATE TABLE default_pk; "\
"INSERT INTO default_pk (v) VALUES (10); "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM default_pk;" \
    "$DATABASE"

insert_expected=$(cat <<\EXPECTED
2	0
1:10:N,2:20:5
EXPECTED
)
expect_output \
    "insert rows with primary key" \
    "$insert_expected" \
    "INSERT INTO inline_pk VALUES (1, 10, NULL), (2, 20, 5); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v, ':', IFNULL(n, 'N')) ORDER BY id) FROM inline_pk;" \
    "$DATABASE"

expect_error \
    "insert duplicate primary key" \
    1062 \
    23000 \
    "Duplicate entry '1' for key 'inline_pk.PRIMARY'" \
    "INSERT INTO inline_pk VALUES (1, 99, 9);" \
    "$DATABASE"

expect_error \
    "update duplicate primary key" \
    1062 \
    23000 \
    "Duplicate entry '1' for key 'inline_pk.PRIMARY'" \
    "UPDATE inline_pk SET id = 1 WHERE id = 2;" \
    "$DATABASE"

expect_error \
    "insert null primary key" \
    1048 \
    23000 \
    "Column 'id' cannot be null" \
    "INSERT INTO inline_pk VALUES (NULL, 99, 9);" \
    "$DATABASE"

expect_error \
    "update null primary key" \
    1048 \
    23000 \
    "Column 'id' cannot be null" \
    "UPDATE inline_pk SET id = NULL WHERE id = 1;" \
    "$DATABASE"

ignore_row_count_expected=$(cat <<\EXPECTED
1	1
1:10,2:20,3:30
EXPECTED
)
expect_output \
    "insert ignore duplicate affected rows and warning count" \
    "$ignore_row_count_expected" \
    "INSERT IGNORE INTO inline_pk VALUES (1, 11, 1), (3, 30, 3); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM inline_pk;" \
    "$DATABASE"

ignore_warning_expected=$(cat <<\EXPECTED
Warning	1062	Duplicate entry '1' for key 'ignore_warning.PRIMARY'
EXPECTED
)
expect_output \
    "insert ignore duplicate warning" \
    "$ignore_warning_expected" \
    "CREATE TABLE ignore_warning (id INT PRIMARY KEY, v INT); "\
"INSERT INTO ignore_warning VALUES (1, 10); "\
"INSERT IGNORE INTO ignore_warning VALUES (1, 11); "\
"SHOW WARNINGS;" \
    "$DATABASE"

pk_type_expected=$(cat <<\EXPECTED
1:10
-2147483648:20
9223372036854775807:30
4294967295:40
4294967295:50
9223372036854775807:60
EXPECTED
)
expect_output \
    "integer-family primary key values" \
    "$pk_type_expected" \
    "CREATE TABLE pk_int (id INT PRIMARY KEY, v INT); "\
"CREATE TABLE pk_integer (id INTEGER PRIMARY KEY, v INT); "\
"CREATE TABLE pk_bigint (id BIGINT PRIMARY KEY, v INT); "\
"CREATE TABLE pk_int_unsigned (id INT UNSIGNED PRIMARY KEY, v INT); "\
"CREATE TABLE pk_integer_unsigned (id INTEGER UNSIGNED PRIMARY KEY, v INT); "\
"CREATE TABLE pk_bigint_unsigned (id BIGINT UNSIGNED PRIMARY KEY, v INT); "\
"INSERT INTO pk_int SET id = 1, v = 10; "\
"INSERT INTO pk_integer VALUES (-2147483648, 20); "\
"INSERT INTO pk_bigint VALUES (9223372036854775807, 30); "\
"INSERT INTO pk_int_unsigned VALUES (4294967295, 40); "\
"INSERT INTO pk_integer_unsigned VALUES (4294967295, 50); "\
"INSERT INTO pk_bigint_unsigned VALUES (9223372036854775807, 60); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM pk_int; "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM pk_integer; "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM pk_bigint; "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM pk_int_unsigned; "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM pk_integer_unsigned; "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM pk_bigint_unsigned;" \
    "$DATABASE"

expect_error \
    "insert set duplicate primary key" \
    1062 \
    23000 \
    "Duplicate entry '1' for key 'pk_int.PRIMARY'" \
    "INSERT INTO pk_int SET id = 1, v = 11;" \
    "$DATABASE"

insert_set_ignore_expected=$(printf '%b' '0\t1\n1:10')
expect_output \
    "insert ignore set duplicate primary key" \
    "$insert_set_ignore_expected" \
    "INSERT IGNORE INTO pk_int SET id = 1, v = 12; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM pk_int;" \
    "$DATABASE"

expect_error \
    "integer duplicate primary key" \
    1062 \
    23000 \
    "Duplicate entry '-2147483648' for key 'pk_integer.PRIMARY'" \
    "INSERT INTO pk_integer VALUES (-2147483648, 21);" \
    "$DATABASE"

expect_error \
    "bigint duplicate primary key" \
    1062 \
    23000 \
    "Duplicate entry '9223372036854775807' for key 'pk_bigint.PRIMARY'" \
    "INSERT INTO pk_bigint VALUES (9223372036854775807, 31);" \
    "$DATABASE"

expect_error \
    "int unsigned duplicate primary key" \
    1062 \
    23000 \
    "Duplicate entry '4294967295' for key 'pk_int_unsigned.PRIMARY'" \
    "INSERT INTO pk_int_unsigned VALUES (4294967295, 41);" \
    "$DATABASE"

expect_error \
    "integer unsigned duplicate primary key" \
    1062 \
    23000 \
    "Duplicate entry '4294967295' for key 'pk_integer_unsigned.PRIMARY'" \
    "INSERT INTO pk_integer_unsigned VALUES (4294967295, 51);" \
    "$DATABASE"

expect_error \
    "bigint unsigned duplicate primary key" \
    1062 \
    23000 \
    "Duplicate entry '9223372036854775807' for key 'pk_bigint_unsigned.PRIMARY'" \
    "INSERT INTO pk_bigint_unsigned VALUES (9223372036854775807, 61);" \
    "$DATABASE"

delete_truncate_expected=$(cat <<\EXPECTED
1:15,2:20
delete_pk	0	PRIMARY	1	id	A	0	NULL	NULL		BTREE			YES	NULL
1:30
EXPECTED
)
expect_output \
    "delete and truncate preserve primary key" \
    "$delete_truncate_expected" \
    "CREATE TABLE delete_pk (id INT PRIMARY KEY, v INT); "\
"INSERT INTO delete_pk VALUES (1, 10), (2, 20); "\
"DELETE FROM delete_pk WHERE id = 1; "\
"INSERT INTO delete_pk VALUES (1, 15); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM delete_pk; "\
"TRUNCATE TABLE delete_pk; "\
"SHOW INDEX FROM delete_pk; "\
"INSERT INTO delete_pk VALUES (1, 30); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM delete_pk;" \
    "$DATABASE"

expect_error \
    "truncate duplicate primary key" \
    1062 \
    23000 \
    "Duplicate entry '1' for key 'delete_pk.PRIMARY'" \
    "INSERT INTO delete_pk VALUES (1, 31);" \
    "$DATABASE"

like_expected=$(cat <<\EXPECTED
like_pk	CREATE TABLE `like_pk` (
  `id` int NOT NULL,
  `v` int DEFAULT NULL,
  `n` int DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
like_pk	0	PRIMARY	1	id	A	0	NULL	NULL		BTREE			YES	NULL
EXPECTED
)
expect_output \
    "create table like clones primary key" \
    "$like_expected" \
    "CREATE TABLE like_pk LIKE inline_pk; "\
"SHOW CREATE TABLE like_pk; "\
"SHOW INDEX FROM like_pk;" \
    "$DATABASE"

ctas_expected=$(cat <<\EXPECTED
ctas_pk	CREATE TABLE `ctas_pk` (
  `id` int NOT NULL,
  `v` int DEFAULT NULL,
  `n` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "create table select does not copy primary key" \
    "$ctas_expected" \
    "CREATE TABLE ctas_pk AS SELECT * FROM inline_pk; "\
"SHOW CREATE TABLE ctas_pk; "\
"SHOW INDEX FROM ctas_pk;" \
    "$DATABASE"

expect_error \
    "explicit null primary key definition" \
    1171 \
    42000 \
    "All parts of a PRIMARY KEY must be NOT NULL" \
    "CREATE TABLE explicit_null_pk (id INT NULL PRIMARY KEY);" \
    "$DATABASE"

expect_error \
    "explicit null table primary key definition" \
    1171 \
    42000 \
    "All parts of a PRIMARY KEY must be NOT NULL" \
    "CREATE TABLE explicit_null_table_pk (id INT NULL, PRIMARY KEY (id));" \
    "$DATABASE"

expect_error \
    "default null primary key definition" \
    1067 \
    42000 \
    "Invalid default value for 'id'" \
    "CREATE TABLE default_null_pk (id INT PRIMARY KEY DEFAULT NULL);" \
    "$DATABASE"

expect_error \
    "duplicate primary key definitions" \
    1068 \
    42000 \
    "Multiple primary key defined" \
    "CREATE TABLE duplicate_pk (id INT PRIMARY KEY, v INT PRIMARY KEY);" \
    "$DATABASE"

expect_error \
    "unknown primary key column" \
    1072 \
    42000 \
    "Key column 'missing' doesn't exist in table" \
    "CREATE TABLE unknown_pk (id INT, PRIMARY KEY (missing));" \
    "$DATABASE"

expect_error \
    "table-qualified primary key column syntax" \
    1064 \
    42000 \
    "right syntax to use near '.id))'" \
    "CREATE TABLE qualified_pk (id INT, PRIMARY KEY (qualified_pk.id));" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts composite primary keys deferred by MyLite" \
    "CREATE TABLE composite_pk (id INT, v INT, PRIMARY KEY (id, v));" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts varchar primary keys deferred by MyLite" \
    "CREATE TABLE varchar_pk (v VARCHAR(3) PRIMARY KEY);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts named primary key constraints deferred by MyLite" \
    "CREATE TABLE constraint_pk (id INT, CONSTRAINT c PRIMARY KEY (id));" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts column key shorthand deferred by MyLite" \
    "CREATE TABLE key_synonym_pk (id INT KEY);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts alter add and drop primary key deferred by MyLite" \
    "CREATE TABLE alter_pk (id INT NOT NULL); "\
"ALTER TABLE alter_pk ADD PRIMARY KEY (id); "\
"ALTER TABLE alter_pk DROP PRIMARY KEY;" \
    "$DATABASE"
