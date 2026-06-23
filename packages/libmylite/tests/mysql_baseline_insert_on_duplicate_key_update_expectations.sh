#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_odku_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_insert_on_duplicate_key_update_expectations: $1" >&2
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

expect_output \
    "literal duplicate update affected rows" \
    "2	0	0
1	20	N
0	0	0
1	20	N" \
    "CREATE TABLE t(id INT PRIMARY KEY, v INT NOT NULL, n INT NULL); "\
"INSERT INTO t VALUES (1,10,NULL); "\
"INSERT INTO t VALUES (1,20,30) ON DUPLICATE KEY UPDATE v=20; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id,v,IFNULL(n,'N') FROM t ORDER BY id; "\
"INSERT INTO t VALUES (1,20,30) ON DUPLICATE KEY UPDATE v=20; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id,v,IFNULL(n,'N') FROM t ORDER BY id;" \
    "$DATABASE"
run_mysql "DROP TABLE t;" "$DATABASE" >/dev/null

expect_output \
    "values function duplicate update" \
    "2	1	0
1	20" \
    "CREATE TABLE t(id INT PRIMARY KEY, v INT NOT NULL); "\
"INSERT INTO t VALUES (1,10); "\
"INSERT INTO t VALUES (1,20) ON DUPLICATE KEY UPDATE v=VALUES(v); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id,v FROM t ORDER BY id;" \
    "$DATABASE"
run_mysql "DROP TABLE t;" "$DATABASE" >/dev/null

expect_output \
    "values function warning text" \
    "Warning	1287	'VALUES function' is deprecated and will be removed in a future release. Please use an alias (INSERT INTO ... VALUES (...) AS alias) and replace VALUES(col) in the ON DUPLICATE KEY UPDATE clause with alias.col instead" \
    "CREATE TABLE t(id INT PRIMARY KEY, v INT NOT NULL); "\
"INSERT INTO t VALUES (1,10); "\
"INSERT INTO t VALUES (1,20) ON DUPLICATE KEY UPDATE v=VALUES(v); "\
"SHOW WARNINGS;" \
    "$DATABASE"
run_mysql "DROP TABLE t;" "$DATABASE" >/dev/null

expect_output \
    "cross-column values function duplicate update" \
    "2	2	0
1	30	1" \
    "CREATE TABLE t(id INT PRIMARY KEY, v INT, n INT); "\
"INSERT INTO t VALUES (1,10,20); "\
"INSERT INTO t VALUES (1,40,30) ON DUPLICATE KEY UPDATE n=VALUES(id), v=VALUES(n); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id,v,n FROM t;" \
    "$DATABASE"
run_mysql "DROP TABLE t;" "$DATABASE" >/dev/null

expect_output \
    "values function row-scalar arithmetic duplicate update" \
    "2	2	0
0	2	0
1	20	21" \
"CREATE TABLE t(id INT PRIMARY KEY, n INT, out_n INT); "\
"INSERT INTO t VALUES (1,10,0); "\
"INSERT INTO t VALUES (1,19,0) ON DUPLICATE KEY UPDATE "\
"n=GREATEST(VALUES(n)+1,0), out_n=GREATEST(VALUES(n)+2,0); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"INSERT INTO t VALUES (1,19,0) ON DUPLICATE KEY UPDATE "\
"n=GREATEST(VALUES(n)+1,0), out_n=GREATEST(VALUES(n)+2,0); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id,n,out_n FROM t;" \
    "$DATABASE"
run_mysql "DROP TABLE t;" "$DATABASE" >/dev/null

expect_output \
    "values function row-scalar string duplicate update" \
    "2	1	0
0	1	0
1	base	new:base" \
    "CREATE TABLE s(id INT PRIMARY KEY, txt VARCHAR(32), out_txt VARCHAR(32)); "\
"INSERT INTO s VALUES (1,'base',''); "\
"INSERT INTO s VALUES (1,'new','') ON DUPLICATE KEY UPDATE out_txt=CONCAT(VALUES(txt),':',txt); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"INSERT INTO s VALUES (1,'new','') ON DUPLICATE KEY UPDATE out_txt=CONCAT(VALUES(txt),':',txt); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id,txt,out_txt FROM s;" \
    "$DATABASE"
run_mysql "DROP TABLE s;" "$DATABASE" >/dev/null

expect_output \
    "row-scalar string duplicate update assignment" \
    "2	0	0
0	0	0
1	Alpha Beta	2	lp" \
    "CREATE TABLE s(id INT PRIMARY KEY, txt VARCHAR(32), n INT, out_txt VARCHAR(32)); "\
"INSERT INTO s VALUES (1,'Alpha Beta',2,NULL); "\
"INSERT INTO s VALUES (1,'ignored',9,NULL) ON DUPLICATE KEY UPDATE out_txt=SUBSTRING(txt,2,n); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"INSERT INTO s VALUES (1,'ignored',9,NULL) ON DUPLICATE KEY UPDATE out_txt=SUBSTRING(txt,2,n); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id,txt,n,out_txt FROM s;" \
    "$DATABASE"
run_mysql "DROP TABLE s;" "$DATABASE" >/dev/null

expect_output \
    "row-scalar temporal duplicate update assignment" \
    "2	0	0
0	0	0
1	12	12:34:56" \
    "CREATE TABLE tm(id INT PRIMARY KEY, h INT, out_tm VARCHAR(32)); "\
"INSERT INTO tm VALUES (1,12,NULL); "\
"INSERT INTO tm VALUES (1,99,NULL) ON DUPLICATE KEY UPDATE out_tm=MAKETIME(h,34,56); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"INSERT INTO tm VALUES (1,99,NULL) ON DUPLICATE KEY UPDATE out_tm=MAKETIME(h,34,56); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id,h,out_tm FROM tm;" \
    "$DATABASE"
run_mysql "DROP TABLE tm;" "$DATABASE" >/dev/null

expect_output \
    "multi-row row-by-row duplicate handling" \
    "4	1	0
1	20
2	30" \
    "CREATE TABLE t(id INT PRIMARY KEY, v INT NOT NULL); "\
"INSERT INTO t VALUES (1,10),(1,20),(2,30) ON DUPLICATE KEY UPDATE v=VALUES(v); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id,v FROM t ORDER BY id;" \
    "$DATABASE"
run_mysql "DROP TABLE t;" "$DATABASE" >/dev/null

expect_output \
    "no-key table inserts normally" \
    "1	0	0
1	10
1	20" \
    "CREATE TABLE nk(id INT, v INT); "\
"INSERT INTO nk VALUES (1,10); "\
"INSERT INTO nk VALUES (1,20) ON DUPLICATE KEY UPDATE v=20; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id,v FROM nk ORDER BY v;" \
    "$DATABASE"
run_mysql "DROP TABLE nk;" "$DATABASE" >/dev/null

expect_output \
    "unique-key duplicate update" \
    "2	1	0
1	10	200" \
    "CREATE TABLE u(id INT, email INT UNIQUE, v INT); "\
"INSERT INTO u VALUES (1,10,100); "\
"INSERT INTO u VALUES (2,10,200) ON DUPLICATE KEY UPDATE v=VALUES(v); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id,email,v FROM u ORDER BY id;" \
    "$DATABASE"
run_mysql "DROP TABLE u;" "$DATABASE" >/dev/null

expect_output \
    "insert set duplicate update" \
    "2	1	0
1	20" \
    "CREATE TABLE t(id INT PRIMARY KEY, v INT NOT NULL); "\
"INSERT INTO t SET id=1, v=10; "\
"INSERT INTO t SET id=1, v=20 ON DUPLICATE KEY UPDATE v=VALUES(v); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id,v FROM t ORDER BY id;" \
    "$DATABASE"
run_mysql "DROP TABLE t;" "$DATABASE" >/dev/null

expect_output \
    "delayed duplicate update warning count" \
    "1	1	0
1	10" \
    "CREATE TABLE t(id INT PRIMARY KEY, v INT NOT NULL); "\
"INSERT DELAYED INTO t VALUES (1,10) ON DUPLICATE KEY UPDATE v=20; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id,v FROM t ORDER BY id;" \
    "$DATABASE"
run_mysql "DROP TABLE t;" "$DATABASE" >/dev/null

expect_output \
    "default duplicate assignment" \
    "2	0	0
1	7" \
    "CREATE TABLE t(id INT PRIMARY KEY, v INT NOT NULL DEFAULT 7); "\
"INSERT INTO t VALUES (1,10); "\
"INSERT INTO t VALUES (1,20) ON DUPLICATE KEY UPDATE v=DEFAULT; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id,v FROM t ORDER BY id;" \
    "$DATABASE"
run_mysql "DROP TABLE t;" "$DATABASE" >/dev/null

expect_output \
    "priority modifiers duplicate update" \
    "1	0	0	10
2	0	0	30" \
    "CREATE TABLE priority_t(id INT PRIMARY KEY, v INT); "\
"INSERT LOW_PRIORITY INTO priority_t VALUES (1,10) ON DUPLICATE KEY UPDATE v=20; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count, v FROM priority_t; "\
"INSERT HIGH_PRIORITY INTO priority_t VALUES (1,10) ON DUPLICATE KEY UPDATE v=30; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count, v FROM priority_t;" \
    "$DATABASE"
run_mysql "DROP TABLE priority_t;" "$DATABASE" >/dev/null

expect_output \
    "typed duplicate assignments" \
    "2	0	0
2	0	0
2	0	0
2	0	0
2	0	0
2	0	0
9.99	new	2024-02-29	-12:34:56	2024-05-06 07:08:09	2024-05-06 07:08:09" \
    "CREATE TABLE typed_t(id INT PRIMARY KEY, amount DECIMAL(5,2), name VARCHAR(10), "\
"d DATE, tm TIME, dt DATETIME, ts TIMESTAMP NULL); "\
"INSERT INTO typed_t VALUES (1,1.25,'old','2024-01-01','01:02:03',"\
"'2024-01-01 01:02:03','2024-01-01 01:02:03'); "\
"INSERT INTO typed_t VALUES (1,9.99,'proposed','2024-03-04','04:05:06',"\
"'2024-03-04 04:05:06','2024-03-04 04:05:06') ON DUPLICATE KEY UPDATE amount=9.99; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"INSERT INTO typed_t VALUES (1,9.99,'proposed','2024-03-04','04:05:06',"\
"'2024-03-04 04:05:06','2024-03-04 04:05:06') ON DUPLICATE KEY UPDATE name='new'; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"INSERT INTO typed_t VALUES (1,9.99,'proposed','2024-03-04','04:05:06',"\
"'2024-03-04 04:05:06','2024-03-04 04:05:06') ON DUPLICATE KEY UPDATE d='2024-02-29'; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"INSERT INTO typed_t VALUES (1,9.99,'proposed','2024-03-04','04:05:06',"\
"'2024-03-04 04:05:06','2024-03-04 04:05:06') ON DUPLICATE KEY UPDATE tm='-12:34:56'; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"INSERT INTO typed_t VALUES (1,9.99,'proposed','2024-03-04','04:05:06',"\
"'2024-03-04 04:05:06','2024-03-04 04:05:06') ON DUPLICATE KEY UPDATE dt='2024-05-06 07:08:09'; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"INSERT INTO typed_t VALUES (1,9.99,'proposed','2024-03-04','04:05:06',"\
"'2024-03-04 04:05:06','2024-03-04 04:05:06') ON DUPLICATE KEY UPDATE ts='2024-05-06 07:08:09'; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT amount,name,d,tm,dt,ts FROM typed_t;" \
    "$DATABASE"
run_mysql "DROP TABLE typed_t;" "$DATABASE" >/dev/null

expect_output \
    "generated auto increment duplicate update" \
    "2	0	0	1
1	0	0	3
1	10	200
3	20	300" \
    "CREATE TABLE auto_t(id INT AUTO_INCREMENT, email INT UNIQUE, v INT, KEY(id)); "\
"INSERT INTO auto_t(email,v) VALUES (10,100); "\
"INSERT INTO auto_t(email,v) VALUES (10,200) ON DUPLICATE KEY UPDATE v=200; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count, LAST_INSERT_ID(); "\
"INSERT INTO auto_t(email,v) VALUES (20,300); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count, LAST_INSERT_ID(); "\
"SELECT id,email,v FROM auto_t ORDER BY id;" \
    "$DATABASE"
run_mysql "DROP TABLE auto_t;" "$DATABASE" >/dev/null

expect_output \
    "same-column arithmetic duplicate update" \
    "2	0	0
1	15	18	33	44	45	66	N" \
    "CREATE TABLE arith(id INT PRIMARY KEY, i INT, ii INTEGER, bi BIGINT, "\
"ui INT UNSIGNED, uii INTEGER UNSIGNED, ubi BIGINT UNSIGNED, n INT NULL); "\
"INSERT INTO arith VALUES(1, 10, 20, 30, 40, 50, 60, NULL); "\
"INSERT INTO arith VALUES(1, 0, 0, 0, 0, 0, 0, 1) ON DUPLICATE KEY UPDATE "\
"i=i+5, ii=ii-2, bi=bi+3, ui=ui+4, uii=uii-5, ubi=ubi+6, n=n+7; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id,i,ii,bi,ui,uii,ubi,IFNULL(n,'N') FROM arith;" \
    "$DATABASE"
run_mysql "DROP TABLE arith;" "$DATABASE" >/dev/null

expect_output \
    "same-column arithmetic insert set duplicate update" \
    "2	0	0
1	14" \
    "CREATE TABLE arith_set(id INT PRIMARY KEY, n INT); "\
"INSERT INTO arith_set SET id=1, n=10; "\
"INSERT INTO arith_set SET id=1, n=0 ON DUPLICATE KEY UPDATE n=n+4; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id,n FROM arith_set;" \
    "$DATABASE"
run_mysql "DROP TABLE arith_set;" "$DATABASE" >/dev/null

expect_error \
    "same-column arithmetic insert select ambiguity" \
    1052 \
    23000 \
    "Column 'n' in field list is ambiguous" \
    "CREATE TABLE select_dst(id INT PRIMARY KEY, n INT); "\
"CREATE TABLE select_src(id INT, n INT); "\
"INSERT INTO select_dst VALUES(1, 10); "\
"INSERT INTO select_src VALUES(1, 20); "\
"INSERT INTO select_dst SELECT id,n FROM select_src ON DUPLICATE KEY UPDATE n=n+3;" \
    "$DATABASE"
run_mysql "DROP TABLE select_src;" "$DATABASE" >/dev/null
run_mysql "DROP TABLE select_dst;" "$DATABASE" >/dev/null

expect_output \
    "same-column arithmetic no-op and null" \
    "0	0	0	8	N
0	0	0	8	N" \
    "CREATE TABLE noop(id INT PRIMARY KEY, n INT NULL, m INT NULL); "\
"INSERT INTO noop VALUES(1, 8, NULL); "\
"INSERT INTO noop VALUES(1, 0, 0) ON DUPLICATE KEY UPDATE n=n+0; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count, n, IFNULL(m,'N') FROM noop; "\
"INSERT INTO noop VALUES(1, 0, 0) ON DUPLICATE KEY UPDATE m=m+1; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count, n, IFNULL(m,'N') FROM noop;" \
    "$DATABASE"
run_mysql "DROP TABLE noop;" "$DATABASE" >/dev/null

expect_output \
    "same-column arithmetic multi-row accumulation" \
    "5	0	0
1	12
2	20" \
    "CREATE TABLE multi(id INT PRIMARY KEY, n INT); "\
"INSERT INTO multi VALUES(1, 10); "\
"INSERT INTO multi VALUES(2,20),(1,0),(1,0) ON DUPLICATE KEY UPDATE n=n+1; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id,n FROM multi ORDER BY id;" \
    "$DATABASE"
run_mysql "DROP TABLE multi;" "$DATABASE" >/dev/null

expect_output \
    "same-column arithmetic mixed with values and literal" \
    "2	1	0
1	12	88	5" \
    "CREATE TABLE mixed(id INT PRIMARY KEY, a INT, b INT, c INT); "\
"INSERT INTO mixed VALUES(1, 10, 20, 30); "\
"INSERT INTO mixed VALUES(1, 99, 88, 77) "\
"ON DUPLICATE KEY UPDATE a=a+2, b=VALUES(b), c=5; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT id,a,b,c FROM mixed;" \
    "$DATABASE"
run_mysql "DROP TABLE mixed;" "$DATABASE" >/dev/null

expect_output \
    "same-column arithmetic null huge delta no-op" \
    "0	0	0	N" \
    "CREATE TABLE null_big(id INT PRIMARY KEY, b BIGINT NULL); "\
"INSERT INTO null_big VALUES(1, NULL); "\
"INSERT INTO null_big VALUES(1, 0) "\
"ON DUPLICATE KEY UPDATE b = b + 9223372036854775808; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count, IFNULL(b,'N') FROM null_big;" \
    "$DATABASE"
run_mysql "DROP TABLE null_big;" "$DATABASE" >/dev/null

expect_error \
    "same-column arithmetic int overflow" \
    1264 \
    22003 \
    "Out of range value for column 'n' at row 1" \
    "CREATE TABLE signed_i(id INT PRIMARY KEY, n INT); "\
"INSERT INTO signed_i VALUES(1, 2147483647); "\
"INSERT INTO signed_i VALUES(1, 0) ON DUPLICATE KEY UPDATE n = n + 1;" \
    "$DATABASE"
run_mysql "DROP TABLE signed_i;" "$DATABASE" >/dev/null

expect_error \
    "same-column arithmetic unsigned int underflow" \
    1690 \
    22003 \
    "BIGINT UNSIGNED value is out of range" \
    "CREATE TABLE unsigned_i(id INT PRIMARY KEY, u INT UNSIGNED); "\
"INSERT INTO unsigned_i VALUES(1, 0); "\
"INSERT INTO unsigned_i VALUES(1, 0) ON DUPLICATE KEY UPDATE u = u - 1;" \
    "$DATABASE"
run_mysql "DROP TABLE unsigned_i;" "$DATABASE" >/dev/null

expect_error \
    "same-column arithmetic signed bigint overflow" \
    1690 \
    22003 \
    "BIGINT value is out of range" \
    "CREATE TABLE signed_b(id INT PRIMARY KEY, b BIGINT); "\
"INSERT INTO signed_b VALUES(1, 9223372036854775807); "\
"INSERT INTO signed_b VALUES(1, 0) ON DUPLICATE KEY UPDATE b = b + 1;" \
    "$DATABASE"
run_mysql "DROP TABLE signed_b;" "$DATABASE" >/dev/null

expect_error \
    "same-column arithmetic unsigned bigint underflow" \
    1690 \
    22003 \
    "BIGINT UNSIGNED value is out of range" \
    "CREATE TABLE unsigned_b(id INT PRIMARY KEY, bu BIGINT UNSIGNED); "\
"INSERT INTO unsigned_b VALUES(1, 0); "\
"INSERT INTO unsigned_b VALUES(1, 0) ON DUPLICATE KEY UPDATE bu = bu - 1;" \
    "$DATABASE"
run_mysql "DROP TABLE unsigned_b;" "$DATABASE" >/dev/null

run_mysql \
    "CREATE TABLE rollback_t(id INT PRIMARY KEY, ti TINYINT, v INT); "\
"INSERT INTO rollback_t VALUES (1,1,10);" \
    "$DATABASE" >/dev/null
expect_error \
    "duplicate branch range failure" \
    1264 \
    22003 \
    "Out of range value for column 'ti' at row 2" \
    "INSERT INTO rollback_t VALUES (2,2,20),(1,3,30) "\
"ON DUPLICATE KEY UPDATE ti=128;" \
    "$DATABASE"
expect_output \
    "duplicate branch failure rolls back statement" \
    "1	1	10" \
    "SELECT id,ti,v FROM rollback_t ORDER BY id;" \
    "$DATABASE"
run_mysql "DROP TABLE rollback_t;" "$DATABASE" >/dev/null

expect_error \
    "unknown duplicate assignment column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'field list'" \
    "CREATE TABLE t(id INT PRIMARY KEY, v INT); "\
"INSERT INTO t VALUES (1,10) ON DUPLICATE KEY UPDATE missing=1;" \
    "$DATABASE"
run_mysql "DROP TABLE t;" "$DATABASE" >/dev/null

expect_error \
    "unknown values function column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'field list'" \
    "CREATE TABLE t(id INT PRIMARY KEY, v INT); "\
"INSERT INTO t VALUES (1,10) ON DUPLICATE KEY UPDATE v=VALUES(missing);" \
    "$DATABASE"
run_mysql "DROP TABLE t;" "$DATABASE" >/dev/null

expect_error \
    "null into not null duplicate assignment" \
    1048 \
    23000 \
    "Column 'v' cannot be null" \
    "CREATE TABLE t(id INT PRIMARY KEY, v INT NOT NULL); "\
"INSERT INTO t VALUES (1,10); "\
"INSERT INTO t VALUES (1,20) ON DUPLICATE KEY UPDATE v=NULL;" \
    "$DATABASE"
run_mysql "DROP TABLE t;" "$DATABASE" >/dev/null

expect_error \
    "default without explicit default duplicate assignment" \
    1364 \
    HY000 \
    "Field 'v' doesn't have a default value" \
    "CREATE TABLE t(id INT PRIMARY KEY, v INT NOT NULL); "\
"INSERT INTO t VALUES (1,10); "\
"INSERT INTO t VALUES (1,20) ON DUPLICATE KEY UPDATE v=DEFAULT;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_insert_on_duplicate_key_update_expectations: ok"
