#!/usr/bin/env sh
set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_composite_string_primary_key_expectations_$$"

run_mysql() {
    docker exec -i "$MYSQL_CONTAINER" mysql -uroot --default-character-set=utf8mb4 --batch --raw --skip-column-names "$@"
}

run_sql() {
    printf '%s\n' "$1" | run_mysql "$DATABASE"
}

run_sql_expect() {
    label="$1"
    sql="$2"
    expected="$3"
    actual="$(run_sql "$sql")"
    if [ "$actual" != "$expected" ]; then
        printf 'Expectation failed: %s\n' "$label" >&2
        printf 'Expected:\n%s\n' "$expected" >&2
        printf 'Actual:\n%s\n' "$actual" >&2
        exit 1
    fi
}

run_sql_expect_error() {
    label="$1"
    sql="$2"
    expected="$3"
    set +e
    actual="$(run_sql "$sql" 2>&1 >/dev/null)"
    status=$?
    set -e
    if [ "$status" -eq 0 ]; then
        printf 'Expected error but statement succeeded: %s\n' "$label" >&2
        exit 1
    fi
    case "$actual" in
        *"$expected"*) ;;
        *)
            printf 'Expectation failed: %s\n' "$label" >&2
            printf 'Expected error containing:\n%s\n' "$expected" >&2
            printf 'Actual error:\n%s\n' "$actual" >&2
            exit 1
            ;;
    esac
}

run_sql_accepts() {
    label="$1"
    sql="$2"
    if ! run_sql "$sql" >/dev/null; then
        printf 'Expected MySQL to accept: %s\n' "$label" >&2
        exit 1
    fi
}

cleanup() {
    printf 'DROP DATABASE IF EXISTS `%s`;\n' "$DATABASE" | run_mysql >/dev/null 2>&1 || true
}
trap cleanup EXIT

printf 'CREATE DATABASE `%s` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci;\n' "$DATABASE" | run_mysql

run_sql_expect \
    "create-time composite varchar primary key metadata" \
    "CREATE TABLE cspk (a VARCHAR(10), b VARCHAR(10), v INT, PRIMARY KEY (a,b));
     SELECT COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE, COLUMN_KEY, IFNULL(COLUMN_DEFAULT, 'NULL')
       FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'cspk'
      ORDER BY ORDINAL_POSITION;
     SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
      WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'cspk'
      ORDER BY CONSTRAINT_NAME;
     SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION,
            IFNULL(POSITION_IN_UNIQUE_CONSTRAINT, 'NULL'), IFNULL(REFERENCED_TABLE_NAME, 'NULL')
       FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
      WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'cspk'
      ORDER BY ORDINAL_POSITION;
     SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, COLLATION,
            IFNULL(SUB_PART, 'NULL'), IF(NULLABLE = '', 'NO', NULLABLE),
            INDEX_TYPE, IS_VISIBLE, IFNULL(EXPRESSION, 'NULL')
       FROM INFORMATION_SCHEMA.STATISTICS
      WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'cspk'
      ORDER BY INDEX_NAME, SEQ_IN_INDEX;" \
    "a	varchar(10)	NO	PRI	NULL
b	varchar(10)	NO	PRI	NULL
v	int	YES		NULL
PRIMARY	PRIMARY KEY	YES
PRIMARY	a	1	NULL	NULL
PRIMARY	b	2	NULL	NULL
PRIMARY	0	1	a	A	NULL	NO	BTREE	YES	NULL
PRIMARY	0	2	b	A	NULL	NO	BTREE	YES	NULL"

run_sql_expect \
    "varchar trailing-space key tuples" \
    "INSERT INTO cspk VALUES ('a','b',1),('a','b ',2),('a ','b',3);
     SELECT ROW_COUNT(), @@warning_count;
     SELECT CONCAT(a, ':', b, ':', v, ':', CHAR_LENGTH(a), ':', CHAR_LENGTH(b), ':', HEX(a), ':', HEX(b))
       FROM cspk ORDER BY v;" \
    "3	0
a:b:1:1:1:61:62
a:b :2:1:2:61:6220
a :b:3:2:1:6120:62"

run_sql_expect_error \
    "case-insensitive duplicate insert" \
    "INSERT INTO cspk VALUES ('A','b',9);" \
    "ERROR 1062 (23000) at line 1: Duplicate entry 'A-b' for key 'cspk.PRIMARY'"

run_sql_expect \
    "insert ignore composite duplicate" \
    "INSERT IGNORE INTO cspk VALUES ('A','b',9),('z','q',4);
     SELECT ROW_COUNT(), @@warning_count;
     SELECT a, b, v FROM cspk ORDER BY v;" \
    "1	1
a	b	1
a	b 	2
a 	b	3
z	q	4"

run_sql_expect \
    "insert ignore duplicate warning detail" \
    "CREATE TABLE cspk_ignore_warning (a VARCHAR(10), b VARCHAR(10), PRIMARY KEY (a,b));
     INSERT INTO cspk_ignore_warning VALUES ('a','b');
     INSERT IGNORE INTO cspk_ignore_warning VALUES ('A','b');
     SHOW WARNINGS;" \
    "Warning	1062	Duplicate entry 'A-b' for key 'cspk_ignore_warning.PRIMARY'"

run_sql_expect_error \
    "update duplicate composite key tuple" \
    "UPDATE cspk SET b = 'b' WHERE v = 2;" \
    "ERROR 1062 (23000) at line 1: Duplicate entry 'a-b' for key 'cspk.PRIMARY'"

run_sql_expect \
    "mixed integer string primary key" \
    "CREATE TABLE mixed_pk (a INT, b VARCHAR(10), v INT, PRIMARY KEY (a,b));
     INSERT INTO mixed_pk VALUES (1,'a',1),(1,'b',2),(2,'A',3);
     SELECT a, b, v FROM mixed_pk ORDER BY a, b, v;" \
    "1	a	1
1	b	2
2	A	3"

run_sql_expect_error \
    "mixed integer string duplicate" \
    "INSERT INTO mixed_pk VALUES (1,'A',9);" \
    "ERROR 1062 (23000) at line 1: Duplicate entry '1-A' for key 'mixed_pk.PRIMARY'"

run_sql_expect_error \
    "explicit null primary part rejected" \
    "CREATE TABLE explicit_null_pk (a VARCHAR(10) NULL, b INT, PRIMARY KEY (a,b));" \
    "ERROR 1171 (42000) at line 1: All parts of a PRIMARY KEY must be NOT NULL; if you need NULL in a key, use UNIQUE instead"

run_sql_expect_error \
    "default null primary part rejected" \
    "CREATE TABLE default_null_pk (a VARCHAR(10) DEFAULT NULL, b INT, PRIMARY KEY (a,b));" \
    "ERROR 1171 (42000) at line 1: All parts of a PRIMARY KEY must be NOT NULL; if you need NULL in a key, use UNIQUE instead"

run_sql_accepts \
    "3072-byte composite varchar primary key" \
    "CREATE TABLE pk_len_ok (a VARCHAR(255), b VARCHAR(255), c VARCHAR(255), PRIMARY KEY (a,b,c));"

run_sql_expect_error \
    "overlength composite varchar primary key" \
    "CREATE TABLE pk_len_bad (a VARCHAR(255), b VARCHAR(255), c VARCHAR(255), d VARCHAR(255), PRIMARY KEY (a,b,c,d));" \
    "ERROR 1071 (42000) at line 1: Specified key was too long; max key length is 3072 bytes"

run_sql_expect \
    "alter table add composite varchar primary key" \
    "CREATE TABLE alter_ok (a VARCHAR(10) NOT NULL, b INT NOT NULL, v INT);
     INSERT INTO alter_ok VALUES ('a',1,1),('A',2,2),('a ',1,3);
     ALTER TABLE alter_ok ADD PRIMARY KEY (a,b);
     SELECT ROW_COUNT(), @@warning_count;
     SELECT COLUMN_NAME, IS_NULLABLE, COLUMN_KEY
       FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'alter_ok'
      ORDER BY ORDINAL_POSITION;
     SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, COLLATION,
            IFNULL(SUB_PART, 'NULL'), IF(NULLABLE = '', 'NO', NULLABLE)
       FROM INFORMATION_SCHEMA.STATISTICS
      WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'alter_ok'
      ORDER BY INDEX_NAME, SEQ_IN_INDEX;" \
    "0	0
a	NO	PRI
b	NO	PRI
v	YES	
PRIMARY	0	1	a	A	NULL	NO
PRIMARY	0	2	b	A	NULL	NO"

run_sql_expect_error \
    "alter table duplicate tuple rejected" \
    "CREATE TABLE alter_dup (a VARCHAR(10) NOT NULL, b INT NOT NULL);
     INSERT INTO alter_dup VALUES ('a',1),('A',1);
     ALTER TABLE alter_dup ADD PRIMARY KEY (a,b);" \
    "ERROR 1062 (23000) at line 3: Duplicate entry 'a-1' for key 'alter_dup.PRIMARY'"

run_sql_expect_error \
    "alter table existing null rejected" \
    "CREATE TABLE alter_null (a VARCHAR(10), b INT NOT NULL);
     INSERT INTO alter_null VALUES (NULL,1);
     ALTER TABLE alter_null ADD PRIMARY KEY (a,b);" \
    "ERROR 1138 (22004) at line 3: Invalid use of NULL value"

run_sql_expect_error \
    "alter table overlength composite varchar primary key" \
    "CREATE TABLE alter_len_bad (a VARCHAR(255) NOT NULL, b VARCHAR(255) NOT NULL, c VARCHAR(255) NOT NULL, d VARCHAR(255) NOT NULL);
     ALTER TABLE alter_len_bad ADD PRIMARY KEY (a,b,c,d);" \
    "ERROR 1071 (42000) at line 2: Specified key was too long; max key length is 3072 bytes"

run_sql_accepts \
    "mysql accepts primary prefix parts" \
    "CREATE TABLE prefix_pk (a VARCHAR(10), b INT, PRIMARY KEY (a(3), b));"

printf 'baseline composite string primary key MySQL 8.4.9 expectations verified\n'
