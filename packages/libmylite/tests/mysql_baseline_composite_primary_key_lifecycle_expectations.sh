#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_composite_primary_key_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_composite_primary_key_lifecycle_expectations: $1" >&2
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

metadata_columns=$(printf '%b' 'a\tint\tNO\tPRI\tNULL\t\nb\tint\tNO\tPRI\tNULL\t\nv\tint\tYES\t\tNULL\t')
metadata_rest=$(cat <<\EXPECTED
cpk	0	PRIMARY	1	a	A	0	NULL	NULL		BTREE			YES	NULL
cpk	0	PRIMARY	2	b	A	0	NULL	NULL		BTREE			YES	NULL
cpk	CREATE TABLE `cpk` (
  `a` int NOT NULL,
  `b` int NOT NULL,
  `v` int DEFAULT NULL,
  PRIMARY KEY (`a`,`b`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
a	PRI	NO	NULL
b	PRI	NO	NULL
v		YES	NULL
PRIMARY	PRIMARY KEY	YES
PRIMARY	a	1	NULL
PRIMARY	b	2	NULL
PRIMARY	0	1	a	NO
PRIMARY	0	2	b	NO
EXPECTED
)
metadata_expected=$(cat <<EXPECTED
$metadata_columns
$metadata_rest
EXPECTED
)
expect_output \
    "composite primary key metadata" \
    "$metadata_expected" \
    "CREATE TABLE cpk (a INT, b INT, v INT, PRIMARY KEY (a,b)); "\
"SHOW COLUMNS FROM cpk; "\
"SHOW INDEX FROM cpk; "\
"SHOW CREATE TABLE cpk; "\
"SELECT COLUMN_NAME, COLUMN_KEY, IS_NULLABLE, IFNULL(COLUMN_DEFAULT, 'NULL') "\
"FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'cpk' "\
"ORDER BY ORDINAL_POSITION; "\
"SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'cpk' ORDER BY CONSTRAINT_NAME; "\
"SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION, IFNULL(REFERENCED_TABLE_NAME, 'NULL') "\
"FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'cpk' "\
"ORDER BY CONSTRAINT_NAME, ORDINAL_POSITION; "\
"SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, IF(NULLABLE = '', 'NO', NULLABLE) "\
"FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'cpk' "\
"ORDER BY NON_UNIQUE, INDEX_NAME, SEQ_IN_INDEX;" \
    "$DATABASE"

insert_expected=$(cat <<\EXPECTED
2	0
1:2:10,1:3:20
EXPECTED
)
expect_output \
    "insert rows with composite primary key" \
    "$insert_expected" \
    "INSERT INTO cpk VALUES (1, 2, 10), (1, 3, 20); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT GROUP_CONCAT(CONCAT(a, ':', b, ':', v) ORDER BY a, b) FROM cpk;" \
    "$DATABASE"

expect_error \
    "insert duplicate composite primary key" \
    1062 \
    23000 \
    "Duplicate entry '1-2' for key 'cpk.PRIMARY'" \
    "INSERT INTO cpk VALUES (1, 2, 99);" \
    "$DATABASE"

expect_error \
    "update duplicate composite primary key" \
    1062 \
    23000 \
    "Duplicate entry '1-2' for key 'cpk.PRIMARY'" \
    "UPDATE cpk SET b = 2 WHERE b = 3;" \
    "$DATABASE"

expect_error \
    "update duplicate composite primary key after unchanged matched rows" \
    1062 \
    23000 \
    "Duplicate entry '2-2' for key 'update_diag.PRIMARY'" \
    "CREATE TABLE update_diag (a INT, b INT, v INT, PRIMARY KEY (a,b)); "\
"INSERT INTO update_diag VALUES (1, 2, 10), (2, 2, 20), (2, 3, 30); "\
"UPDATE update_diag SET b = 2 WHERE b >= 2;" \
    "$DATABASE"

expect_error \
    "update duplicate composite primary key with order limit target" \
    1062 \
    23000 \
    "Duplicate entry '2-2' for key 'update_limit_diag.PRIMARY'" \
    "CREATE TABLE update_limit_diag (a INT, b INT, v INT, PRIMARY KEY (a,b)); "\
"INSERT INTO update_limit_diag VALUES (1, 2, 10), (1, 3, 30), (2, 2, 20), (2, 3, 40); "\
"UPDATE update_limit_diag SET b = 2 WHERE b >= 2 ORDER BY v DESC LIMIT 1;" \
    "$DATABASE"

expect_error \
    "insert null first primary-key part" \
    1048 \
    23000 \
    "Column 'a' cannot be null" \
    "INSERT INTO cpk VALUES (NULL, 4, 40);" \
    "$DATABASE"

expect_error \
    "update null second primary-key part" \
    1048 \
    23000 \
    "Column 'b' cannot be null" \
    "UPDATE cpk SET b = NULL WHERE a = 1;" \
    "$DATABASE"

ignore_expected=$(cat <<\EXPECTED
1	1
1:2:10,1:3:20,2:1:30
EXPECTED
)
expect_output \
    "insert ignore duplicate composite primary key" \
    "$ignore_expected" \
    "INSERT IGNORE INTO cpk VALUES (1, 2, 99), (2, 1, 30); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT GROUP_CONCAT(CONCAT(a, ':', b, ':', v) ORDER BY a, b) FROM cpk;" \
    "$DATABASE"

warning_expected=$(cat <<\EXPECTED
Warning	1062	Duplicate entry '1-2' for key 'warn_pk.PRIMARY'
EXPECTED
)
expect_output \
    "insert ignore composite primary key warning detail" \
    "$warning_expected" \
    "CREATE TABLE warn_pk (a INT, b INT, PRIMARY KEY (a,b)); "\
"INSERT INTO warn_pk VALUES (1, 2); "\
"INSERT IGNORE INTO warn_pk VALUES (1, 2); "\
"SHOW WARNINGS;" \
    "$DATABASE"

defaults_columns=$(printf '%b' 'a\tint\tNO\tPRI\t7\t\nb\tint\tNO\tPRI\t8\t\nv\tint\tYES\t\tNULL\t')
defaults_rest=$(cat <<\EXPECTED
defaults_pk	CREATE TABLE `defaults_pk` (
  `a` int NOT NULL DEFAULT '7',
  `b` int NOT NULL DEFAULT '8',
  `v` int DEFAULT NULL,
  PRIMARY KEY (`a`,`b`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
1	0	7:8:10
EXPECTED
)
defaults_expected=$(cat <<EXPECTED
$defaults_columns
$defaults_rest
EXPECTED
)
expect_output \
    "composite primary key defaults" \
    "$defaults_expected" \
    "CREATE TABLE defaults_pk (a INT DEFAULT 7, b INT DEFAULT 8, v INT, PRIMARY KEY (a,b)); "\
"SHOW COLUMNS FROM defaults_pk; "\
"SHOW CREATE TABLE defaults_pk; "\
"INSERT INTO defaults_pk (v) VALUES (10); "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(a, ':', b, ':', v) ORDER BY a, b) "\
"FROM defaults_pk;" \
    "$DATABASE"

clone_expected=$(cat <<\EXPECTED
clone	CREATE TABLE `clone` (
  `a` int NOT NULL,
  `b` int NOT NULL,
  `v` int DEFAULT NULL,
  PRIMARY KEY (`a`,`b`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
clone	0	PRIMARY	1	a	A	0	NULL	NULL		BTREE			YES	NULL
clone	0	PRIMARY	2	b	A	0	NULL	NULL		BTREE			YES	NULL
ctas	CREATE TABLE `ctas` (
  `a` int NOT NULL,
  `b` int NOT NULL,
  `v` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "composite primary key create like and ctas" \
    "$clone_expected" \
    "CREATE TABLE clone LIKE cpk; "\
"SHOW CREATE TABLE clone; "\
"SHOW INDEX FROM clone; "\
"CREATE TABLE ctas AS SELECT * FROM cpk; "\
"SHOW CREATE TABLE ctas; "\
"SHOW INDEX FROM ctas;" \
    "$DATABASE"

expect_error \
    "composite primary key default null" \
    1171 \
    42000 \
    "All parts of a PRIMARY KEY must be NOT NULL" \
    "CREATE TABLE bad_default_null (a INT DEFAULT NULL, b INT, PRIMARY KEY (a,b));" \
    "$DATABASE"

expect_error \
    "composite primary key explicit null" \
    1171 \
    42000 \
    "All parts of a PRIMARY KEY must be NOT NULL" \
    "CREATE TABLE bad_explicit_null (a INT NULL, b INT, PRIMARY KEY (a,b));" \
    "$DATABASE"

expect_error \
    "composite primary key duplicate part" \
    1060 \
    42S21 \
    "Duplicate column name 'a'" \
    "CREATE TABLE bad_duplicate_part (a INT, b INT, PRIMARY KEY (a,a));" \
    "$DATABASE"

expect_error \
    "composite primary key missing part" \
    1072 \
    42000 \
    "Key column 'missing' doesn't exist in table" \
    "CREATE TABLE bad_missing_part (a INT, b INT, PRIMARY KEY (a,missing));" \
    "$DATABASE"

expect_upstream_accepts \
    "string composite primary key remains deferred in MyLite" \
    "CREATE TABLE upstream_string_pk (a VARCHAR(3), b INT, PRIMARY KEY (a,b));" \
    "$DATABASE"

expect_upstream_accepts \
    "composite auto-increment first part remains deferred in MyLite" \
    "CREATE TABLE upstream_auto_pk (a INT AUTO_INCREMENT, b INT, PRIMARY KEY (a,b));" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_composite_primary_key_lifecycle_expectations: ok"
