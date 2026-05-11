#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_information_schema_constraints_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_constraints_expectations: $1" >&2
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

run_mysql \
    "CREATE TABLE constrained ("\
"id INT NOT NULL, "\
"v INT, "\
"n INT NOT NULL, "\
"PRIMARY KEY (id), "\
"UNIQUE KEY u_v (v), "\
"UNIQUE KEY u_n (n), "\
"KEY k_v (v)"\
"); "\
"CREATE TABLE inline_unique (a INT UNIQUE, b INT NOT NULL UNIQUE KEY); "\
"CREATE TABLE no_constraints (a INT, KEY k_a (a)); "\
"CREATE TABLE clone LIKE constrained; "\
"CREATE TABLE copied AS SELECT id, v FROM constrained;" \
    "$DATABASE" >/dev/null

table_constraints_expected=$(cat <<EXPECTED
def	${DATABASE}	PRIMARY	${DATABASE}	constrained	PRIMARY KEY	YES
def	${DATABASE}	u_n	${DATABASE}	constrained	UNIQUE	YES
def	${DATABASE}	u_v	${DATABASE}	constrained	UNIQUE	YES
def	${DATABASE}	a	${DATABASE}	inline_unique	UNIQUE	YES
def	${DATABASE}	b	${DATABASE}	inline_unique	UNIQUE	YES
EXPECTED
)
expect_output \
    "table constraints primary and unique rows" \
    "$table_constraints_expected" \
    "SELECT CONSTRAINT_CATALOG, CONSTRAINT_SCHEMA, CONSTRAINT_NAME, TABLE_SCHEMA, "\
"TABLE_NAME, CONSTRAINT_TYPE, ENFORCED FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' "\
"AND (TABLE_NAME = 'constrained' OR TABLE_NAME = 'inline_unique') "\
"ORDER BY TABLE_NAME, CONSTRAINT_TYPE, CONSTRAINT_NAME;" \
    "$DATABASE"

key_column_usage_expected=$(cat <<EXPECTED
def	${DATABASE}	PRIMARY	def	${DATABASE}	constrained	id	1	NULL	NULL	NULL	NULL
def	${DATABASE}	u_n	def	${DATABASE}	constrained	n	1	NULL	NULL	NULL	NULL
def	${DATABASE}	u_v	def	${DATABASE}	constrained	v	1	NULL	NULL	NULL	NULL
def	${DATABASE}	a	def	${DATABASE}	inline_unique	a	1	NULL	NULL	NULL	NULL
def	${DATABASE}	b	def	${DATABASE}	inline_unique	b	1	NULL	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "key column usage primary and unique rows" \
    "$key_column_usage_expected" \
    "SELECT CONSTRAINT_CATALOG, CONSTRAINT_SCHEMA, CONSTRAINT_NAME, TABLE_CATALOG, "\
"TABLE_SCHEMA, TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, POSITION_IN_UNIQUE_CONSTRAINT, "\
"REFERENCED_TABLE_SCHEMA, REFERENCED_TABLE_NAME, REFERENCED_COLUMN_NAME "\
"FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE WHERE TABLE_SCHEMA = '${DATABASE}' "\
"AND (TABLE_NAME = 'constrained' OR TABLE_NAME = 'inline_unique') "\
"ORDER BY TABLE_NAME, CONSTRAINT_NAME, ORDINAL_POSITION;" \
    "$DATABASE"

nonunique_omission_expected=$(printf '%b' "0\n0")
expect_output \
    "nonunique indexes do not create constraints" \
    "$nonunique_omission_expected" \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'no_constraints'; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'no_constraints';" \
    "$DATABASE"

clone_constraints_expected=$(cat <<EXPECTED
PRIMARY	PRIMARY KEY
u_n	UNIQUE
u_v	UNIQUE
EXPECTED
)
expect_output \
    "create table like clones constraints" \
    "$clone_constraints_expected" \
    "SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'clone' "\
"ORDER BY CONSTRAINT_TYPE, CONSTRAINT_NAME;" \
    "$DATABASE"

copied_omission_expected=$(printf '%b' "0\n0")
expect_output \
    "create table select omits constraints" \
    "$copied_omission_expected" \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'copied'; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'copied';" \
    "$DATABASE"

run_mysql "RENAME TABLE constrained TO renamed_constrained; TRUNCATE TABLE renamed_constrained;" \
    "$DATABASE" >/dev/null

rename_truncate_expected=$(cat <<EXPECTED
renamed_constrained	PRIMARY	PRIMARY KEY
renamed_constrained	u_n	UNIQUE
renamed_constrained	u_v	UNIQUE
EXPECTED
)
expect_output \
    "rename and truncate preserve constraint metadata" \
    "$rename_truncate_expected" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE "\
"FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS WHERE TABLE_SCHEMA = '${DATABASE}' "\
"AND TABLE_NAME = 'renamed_constrained' ORDER BY CONSTRAINT_TYPE, CONSTRAINT_NAME;" \
    "$DATABASE"

run_mysql "DROP TABLE renamed_constrained;" "$DATABASE" >/dev/null
expect_output \
    "drop removes constraint metadata" \
    "0" \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'renamed_constrained';" \
    "$DATABASE"

system_tables_expected=$(cat <<\EXPECTED
information_schema	KEY_COLUMN_USAGE	SYSTEM VIEW	NULL	10	NULL	0
information_schema	TABLE_CONSTRAINTS	SYSTEM VIEW	NULL	10	NULL	0
EXPECTED
)
expect_output \
    "information schema system table rows" \
    "$system_tables_expected" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS "\
"FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'information_schema' "\
"AND (TABLE_NAME = 'KEY_COLUMN_USAGE' OR TABLE_NAME = 'TABLE_CONSTRAINTS') "\
"ORDER BY TABLE_NAME;" \
    "$DATABASE"

system_columns_expected=$(cat <<\EXPECTED
KEY_COLUMN_USAGE	CONSTRAINT_CATALOG	1	NULL	NO	varchar	64	192	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)
KEY_COLUMN_USAGE	CONSTRAINT_SCHEMA	2	NULL	NO	varchar	64	192	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)
KEY_COLUMN_USAGE	CONSTRAINT_NAME	3	NULL	YES	varchar	64	192	NULL	NULL	utf8mb3	utf8mb3_tolower_ci	varchar(64)
KEY_COLUMN_USAGE	TABLE_CATALOG	4	NULL	NO	varchar	64	192	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)
KEY_COLUMN_USAGE	TABLE_SCHEMA	5	NULL	NO	varchar	64	192	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)
KEY_COLUMN_USAGE	TABLE_NAME	6	NULL	NO	varchar	64	192	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)
KEY_COLUMN_USAGE	COLUMN_NAME	7	NULL	YES	varchar	64	192	NULL	NULL	utf8mb3	utf8mb3_tolower_ci	varchar(64)
KEY_COLUMN_USAGE	ORDINAL_POSITION	8	0	NO	int	NULL	NULL	10	0	NULL	NULL	int unsigned
KEY_COLUMN_USAGE	POSITION_IN_UNIQUE_CONSTRAINT	9	NULL	YES	int	NULL	NULL	10	0	NULL	NULL	int unsigned
KEY_COLUMN_USAGE	REFERENCED_TABLE_SCHEMA	10	NULL	YES	varchar	64	192	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)
KEY_COLUMN_USAGE	REFERENCED_TABLE_NAME	11	NULL	YES	varchar	64	192	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)
KEY_COLUMN_USAGE	REFERENCED_COLUMN_NAME	12	NULL	YES	varchar	64	192	NULL	NULL	utf8mb3	utf8mb3_tolower_ci	varchar(64)
TABLE_CONSTRAINTS	CONSTRAINT_CATALOG	1	NULL	NO	varchar	64	192	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)
TABLE_CONSTRAINTS	CONSTRAINT_SCHEMA	2	NULL	NO	varchar	64	192	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)
TABLE_CONSTRAINTS	CONSTRAINT_NAME	3	NULL	YES	varchar	64	192	NULL	NULL	utf8mb3	utf8mb3_tolower_ci	varchar(64)
TABLE_CONSTRAINTS	TABLE_SCHEMA	4	NULL	NO	varchar	64	192	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)
TABLE_CONSTRAINTS	TABLE_NAME	5	NULL	NO	varchar	64	192	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)
TABLE_CONSTRAINTS	CONSTRAINT_TYPE	6		NO	varchar	11	33	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(11)
TABLE_CONSTRAINTS	ENFORCED	7		NO	varchar	3	9	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(3)
EXPECTED
)
expect_output \
    "information schema system column rows" \
    "$system_columns_expected" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, "\
"DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION, "\
"NUMERIC_SCALE, CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE "\
"FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'information_schema' "\
"AND (TABLE_NAME = 'KEY_COLUMN_USAGE' OR TABLE_NAME = 'TABLE_CONSTRAINTS') "\
"ORDER BY TABLE_NAME, ORDINAL_POSITION;" \
    "$DATABASE"

alias_limit_expected=$(cat <<EXPECTED
PRIMARY
u_n
EXPECTED
)
expect_output \
    "alias qualified ordered limited constraints" \
    "$alias_limit_expected" \
    "SELECT tc.CONSTRAINT_NAME FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS AS tc "\
"WHERE tc.TABLE_SCHEMA = '${DATABASE}' AND tc.TABLE_NAME = 'clone' "\
"ORDER BY tc.CONSTRAINT_NAME LIMIT 2;" \
    "$DATABASE"

expect_output \
    "metadata string predicate collation" \
    "clone" \
    "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'clone' "\
"AND CONSTRAINT_NAME = 'primary';" \
    "$DATABASE"

expect_output \
    "numeric metadata string coercion" \
    "id" \
    "SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'clone' "\
"AND ORDINAL_POSITION = '01' AND CONSTRAINT_NAME = 'PRIMARY';" \
    "$DATABASE"

expect_error \
    "unknown projection column" \
    1054 \
    42S22 \
    "Unknown column 'nope' in 'field list'" \
    "SELECT nope FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS;"

expect_error \
    "unknown where column" \
    1054 \
    42S22 \
    "Unknown column 'nope' in 'where clause'" \
    "SELECT CONSTRAINT_NAME FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE WHERE nope = 'x';"

printf '%s\n' "mysql_baseline_information_schema_constraints_expectations: ok"
