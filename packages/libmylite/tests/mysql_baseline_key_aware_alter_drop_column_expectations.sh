#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_key_aware_drop_column_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_key_aware_alter_drop_column_expectations: $1" >&2
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

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

run_mysql \
    "CREATE TABLE single_keys ("\
"id INT NOT NULL PRIMARY KEY, "\
"uniq_col INT UNIQUE, "\
"plain_col INT, "\
"pref_col VARCHAR(20), "\
"keep_col INT, "\
"KEY k_plain(plain_col), "\
"KEY k_pref(pref_col(5))"\
"); "\
"INSERT INTO single_keys VALUES "\
"(1, 10, 100, 'alpha', 1000), "\
"(2, 20, 200, 'bravo', 2000);" \
    "$DATABASE" >/dev/null

expect_output \
    "drop single-column secondary and unique keys" \
    "0	0	0	0	1:1000,2:2000" \
    "ALTER TABLE single_keys DROP COLUMN plain_col; "\
"ALTER TABLE single_keys DROP COLUMN uniq_col; "\
"SELECT ROW_COUNT(), @@warning_count, "\
"(SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "\
" WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'single_keys' "\
" AND INDEX_NAME IN ('k_plain','uniq_col')), "\
"(SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "\
" WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'single_keys' "\
" AND COLUMN_NAME IN ('plain_col','uniq_col')), "\
"GROUP_CONCAT(CONCAT(id, ':', keep_col) ORDER BY id) FROM single_keys;" \
    "$DATABASE"

expect_output \
    "drop prefix key" \
    "0	0	0	1:1000,2:2000" \
    "ALTER TABLE single_keys DROP COLUMN pref_col; "\
"SELECT ROW_COUNT(), @@warning_count, "\
"(SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "\
" WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'single_keys' "\
" AND INDEX_NAME = 'k_pref'), "\
"GROUP_CONCAT(CONCAT(id, ':', keep_col) ORDER BY id) FROM single_keys;" \
    "$DATABASE"

expect_output \
    "drop one-column primary key" \
    "2	0	0	1000,2000" \
    "ALTER TABLE single_keys DROP COLUMN id; "\
"SELECT ROW_COUNT(), @@warning_count, "\
"(SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "\
" WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'single_keys'), "\
"GROUP_CONCAT(keep_col ORDER BY keep_col) FROM single_keys;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE fulltext_keys ("\
"id INT, "\
"body TEXT, "\
"keep_col INT, "\
"FULLTEXT KEY ft_body(body)"\
"); "\
"INSERT INTO fulltext_keys VALUES (1, 'hello world', 10), (2, 'other text', 20);" \
    "$DATABASE" >/dev/null

expect_output \
    "drop fulltext key column" \
    "0	0	0	10,20" \
    "ALTER TABLE fulltext_keys DROP COLUMN body; "\
"SELECT ROW_COUNT(), @@warning_count, "\
"(SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "\
" WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'fulltext_keys'), "\
"GROUP_CONCAT(keep_col ORDER BY keep_col) FROM fulltext_keys;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE composite_keys ("\
"a INT NOT NULL, "\
"b INT NOT NULL, "\
"c INT, "\
"name VARCHAR(20), "\
"PRIMARY KEY(a, b), "\
"UNIQUE KEY u_ab(a, b), "\
"KEY k_ab(a, b), "\
"KEY k_prefix(name(4), c)"\
"); "\
"INSERT INTO composite_keys VALUES (1, 10, 100, 'alpha'), (2, 20, 200, 'bravo');" \
    "$DATABASE" >/dev/null

composite_stats_expected=$(cat <<'EXPECTED'
k_ab	1	1	b	NULL
k_prefix	1	1	name	4
k_prefix	1	2	c	NULL
PRIMARY	0	1	b	NULL
u_ab	0	1	b	NULL
EXPECTED
)
expect_output \
    "drop composite key part shrinks descriptors" \
    "$composite_stats_expected" \
    "ALTER TABLE composite_keys DROP COLUMN a; "\
"SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, SUB_PART "\
"FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'composite_keys' "\
"ORDER BY INDEX_NAME, SEQ_IN_INDEX;" \
    "$DATABASE"

expect_output \
    "composite rows after key shrink" \
    "10:100:alpha,20:200:bravo" \
    "SELECT GROUP_CONCAT(CONCAT(b, ':', c, ':', name) ORDER BY b) FROM composite_keys;" \
    "$DATABASE"

expect_error \
    "drop composite unique part causing duplicate" \
    1062 \
    23000 \
    "Duplicate entry '1' for key 'unique_duplicate.u_ab'" \
    "CREATE TABLE unique_duplicate (a INT, b INT, UNIQUE KEY u_ab(a, b)); "\
"INSERT INTO unique_duplicate VALUES (1, 1), (2, 1); "\
"ALTER TABLE unique_duplicate DROP COLUMN a;" \
    "$DATABASE"

expect_error \
    "drop composite primary part causing duplicate" \
    1062 \
    23000 \
    "Duplicate entry '1' for key 'primary_duplicate.PRIMARY'" \
    "CREATE TABLE primary_duplicate (a INT NOT NULL, b INT NOT NULL, PRIMARY KEY(a, b)); "\
"INSERT INTO primary_duplicate VALUES (1, 1), (2, 1); "\
"ALTER TABLE primary_duplicate DROP COLUMN a;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE auto_drop ("\
"id INT AUTO_INCREMENT, "\
"keep_col INT, "\
"KEY id_key(id)"\
"); "\
"INSERT INTO auto_drop(keep_col) VALUES (10), (20);" \
    "$DATABASE" >/dev/null
expect_output \
    "drop auto-increment column" \
    "0	0	0	10,20" \
    "ALTER TABLE auto_drop DROP COLUMN id; "\
"SELECT ROW_COUNT(), @@warning_count, "\
"(SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "\
" WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'auto_drop'), "\
"GROUP_CONCAT(keep_col ORDER BY keep_col) FROM auto_drop;" \
    "$DATABASE"

expect_error \
    "drop child foreign-key column" \
    1828 \
    HY000 \
    "Cannot drop column 'parent_id': needed in a foreign key constraint 'fk_parent'" \
    "CREATE TABLE parent (id INT PRIMARY KEY); "\
"CREATE TABLE child ("\
"id INT PRIMARY KEY, "\
"parent_id INT, "\
"KEY p_idx(parent_id), "\
"CONSTRAINT fk_parent FOREIGN KEY(parent_id) REFERENCES parent(id)"\
"); "\
"ALTER TABLE child DROP COLUMN parent_id;" \
    "$DATABASE"

expect_error \
    "drop parent foreign-key column" \
    1829 \
    HY000 \
    "Cannot drop column 'id': needed in a foreign key constraint 'fk_parent_ref' of table 'child_ref'" \
    "CREATE TABLE parent_ref (id INT PRIMARY KEY, keep_col INT); "\
"CREATE TABLE child_ref ("\
"id INT PRIMARY KEY, "\
"parent_id INT, "\
"KEY p_idx(parent_id), "\
"CONSTRAINT fk_parent_ref FOREIGN KEY(parent_id) REFERENCES parent_ref(id)"\
"); "\
"ALTER TABLE parent_ref DROP COLUMN id;" \
    "$DATABASE"

cleanup
