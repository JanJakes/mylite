#include "mylite/parser.h"

#include <stdio.h>
#include <string.h>

typedef struct ExpectedAstTarget {
  MyliteStatementTargetRole role;
  MyliteStatementTargetKind kind;
  const char *target;
  const char *schema;
  const char *name;
} ExpectedAstTarget;

typedef struct ExpectedCreateTableColumn {
  const char *definition;
  const char *name;
  const char *type;
  const char *options;
  MyliteCreateTableColumnTypeFamily type_family;
  unsigned int flags;
  const char *type_name;
  const char *type_parameters;
  size_t type_numeric_parameter_count;
  unsigned long long type_numeric_parameters[2];
  size_t type_element_count;
  const char *type_attributes;
  const char *default_span;
  const char *default_value;
  const char *on_update;
  const char *on_update_value;
  const char *generated;
  const char *generated_expression;
  const char *generated_storage;
  const char *comment;
  const char *comment_value;
  const char *check_span;
  const char *check_expression;
  MyliteCreateTableCheckEnforcement check_enforcement;
  const char *check_enforcement_span;
  const char *reference;
  MyliteCreateTableColumnTypeKind type_kind;
  MyliteCreateTableColumnStorageClass storage_class;
  const char *type_node_symbol;
  const char *options_node_symbol;
  const char *default_node_symbol;
  const char *default_value_node_symbol;
  const char *on_update_value_node_symbol;
  const char *generated_expression_node_symbol;
  const char *generated_storage_node_symbol;
  const char *check_expression_node_symbol;
  const char *check_enforcement_node_symbol;
  const char *reference_node_symbol;
} ExpectedCreateTableColumn;

typedef struct ExpectedCreateTableKeyPart {
  const char *definition;
  const char *name;
  MyliteCreateTableKeyPartKind kind;
  const char *expression;
  const char *prefix;
  const char *prefix_value;
  MyliteCreateTableKeyPartOrder order;
  const char *order_span;
} ExpectedCreateTableKeyPart;

typedef struct ExpectedCreateTableKeyOption {
  MyliteCreateTableKeyOptionKind kind;
  const char *definition;
  const char *name;
  const char *value;
} ExpectedCreateTableKeyOption;

typedef struct ExpectedCreateTableKey {
  MyliteCreateTableKeyKind kind;
  const char *definition;
  const char *constraint_name;
  const char *name;
  const ExpectedCreateTableKeyPart *columns;
  size_t column_count;
  const char *referenced_table;
  const char *referenced_schema;
  const char *referenced_name;
  const ExpectedCreateTableKeyPart *referenced_columns;
  size_t referenced_column_count;
  const char *index_type;
  MyliteCreateTableForeignMatchKind foreign_match;
  const char *foreign_match_span;
  MyliteCreateTableForeignAction foreign_on_delete;
  const char *foreign_on_delete_span;
  MyliteCreateTableForeignAction foreign_on_update;
  const char *foreign_on_update_span;
  const char *check_expression;
  MyliteCreateTableCheckEnforcement check_enforcement;
  const char *check_enforcement_span;
  const ExpectedCreateTableKeyOption *options;
  size_t option_count;
} ExpectedCreateTableKey;

typedef struct ExpectedCreateTableOption {
  MyliteCreateTableOptionKind kind;
  const char *definition;
  const char *name;
  const char *value;
} ExpectedCreateTableOption;

static int expect_parse_ok(const char *sql);
static int expect_ast_ok(const char *sql, const char *root_symbol);
static int expect_ast_statements(const char *sql, size_t count,
                                 const MyliteStatementKind *kinds);
static int expect_ast_target(const char *sql, MyliteStatementKind statement_kind,
                             MyliteStatementTargetKind target_kind,
                             const char *target, const char *schema,
                             const char *name);
static int expect_ast_targets(const char *sql, MyliteStatementKind statement_kind,
                              const ExpectedAstTarget *targets,
                              size_t target_count);
static int expect_create_table_columns(const char *sql,
                                       const ExpectedCreateTableColumn *columns,
                                       size_t column_count);
static int expect_create_table_keys(const char *sql,
                                    const ExpectedCreateTableKey *keys,
                                    size_t key_count);
static int expect_create_table_key_parts(
    const char *sql, const MyliteAst *ast, size_t key_index,
    const ExpectedCreateTableKeyPart *parts, size_t part_count,
    int referenced);
static int expect_create_table_key_options(
    const char *sql, const MyliteAst *ast, size_t key_index,
    const ExpectedCreateTableKeyOption *options, size_t option_count);
static int expect_create_table_options(const char *sql,
                                       const ExpectedCreateTableOption *options,
                                       size_t option_count);
static int span_matches(const char *sql, size_t start, size_t end,
                        const char *expected);
static int span_matches_when_expected(const char *sql, size_t start, size_t end,
                                      const char *expected);
static int node_symbol_matches_when_expected(const MyliteAstNode *node,
                                             const char *expected);

int main(void) {
  int failures = 0;

  failures += expect_parse_ok("SELECT schema_name FROM information_schema.schemata ORDER BY schema_name");
  failures += expect_parse_ok("SHOW TABLES IN mysql WHERE Tables_in_mysql != 'ndb_binlog_index'");
  failures += expect_parse_ok("DROP TABLE IF EXISTS t1");
  failures += expect_parse_ok("RENAME TABLES child1 TO child");
  failures += expect_parse_ok("SET sql_mode = 'NO_ENGINE_SUBSTITUTION'");
  failures += expect_parse_ok(
      "CREATE TABLE t1 ("
      "id int(11) NOT NULL auto_increment,"
      "name varchar(50) NOT NULL default '',"
      "created_at timestamp NOT NULL,"
      "PRIMARY KEY (id),"
      "KEY name_idx(name)"
      ")");
  failures += expect_parse_ok("CREATE TABLE t_user_key (User INT, KEY User (User))");
  failures += expect_parse_ok(
      "CREATE TABLE t_check (c1 INT DEFAULT 2 PRIMARY KEY CHECK(c1 > 1 OR c1 IS NOT NULL))");
  failures += expect_parse_ok("CREATE TABLE 1ea10 (1a20 int, 1e int)");
  failures += expect_parse_ok("CREATE TABLE table_28127_a (0b02 INT)");
  failures += expect_parse_ok(
      "CREATE TABLE t3 (SELECT GROUP_CONCAT(a) AS a FROM t1 WHERE a = 'a') "
      "UNION (SELECT GROUP_CONCAT(b) AS a FROM t1 WHERE a = 'b')");
  failures += expect_parse_ok("CREATE TABLE trx_option (f1 INT) START TRANSACTION");
  failures += expect_parse_ok("CREATE TABLE ascii_col (c CHAR(32) ASCII NOT NULL, v VARCHAR(10) BINARY ASCII)");
  failures += expect_parse_ok("SELECT 1ea10.1a20, 1e + 1e+10 FROM 1ea10");
  failures += expect_parse_ok("SELECT \"$id2\", \"$$$\" FROM t WHERE t.\"$id\" = 0");
  failures += expect_parse_ok(
      "INSERT INTO t1 VALUES "
      "(1,'alpha',20010202105916),"
      "(2,'beta',20010202105917)");
  failures += expect_parse_ok("INSERT INTO t1 VALUES() AS f2 ON DUPLICATE KEY UPDATE f1=1");
  failures += expect_parse_ok("INSERT INTO t0 SET a=1, b=20 AS n ON DUPLICATE KEY UPDATE b=n.b");
  failures += expect_parse_ok("INSERT INTO t1 VALUES(1, 10) AS n");
  failures += expect_parse_ok(
      "SELECT SQL_CALC_FOUND_ROWS t1.id, COUNT(*) AS c "
      "FROM `t1` LEFT JOIN t2 ON t2.id = t1.id "
      "WHERE t1.name LIKE 'a%' GROUP BY t1.id HAVING c > 0 ORDER BY c DESC LIMIT 10");
  failures += expect_parse_ok(
      "SELECT * FROM t1 AS a STRAIGHT_JOIN t2 AS b INNER JOIN t3 AS c "
      "ON c.x = b.x ON c.y = b.y");
  failures += expect_parse_ok("SELECT * FROM t1 t11 NATURAL INNER JOIN t1 t12");
  failures += expect_parse_ok("SELECT t1.*,t2.* FROM { OJ t2 LEFT OUTER JOIN t1 ON (t1.a=t2.a) }");
  failures += expect_parse_ok("SELECT STD(0), STDDEV(0), VARIANCE(0) FROM t2");
  failures += expect_parse_ok("SELECT myfunc_int(a AS attr_name) FROM t1");
  failures += expect_parse_ok("SELECT myfunc_int(fn(MIN(b)) xx) AS c FROM t1 GROUP BY a");
  failures += expect_parse_ok("SELECT sequence() AS seq, a FROM t1 ORDER BY seq ASC");
  failures += expect_parse_ok("EXPLAIN FORMAT=tree SELECT col_varchar_key FROM t1");
  failures += expect_parse_ok("SELECT HEX(CAST(_koi8r x'D4C5D3D4' AS CHAR CHARACTER SET cp1251))");
  failures += expect_parse_ok("SELECT CAST(_koi8r x'C6C7' AS NCHAR(2))");
  failures += expect_parse_ok("SELECT HEX(a) FROM t1 WHERE a = _big5 0xF9DC");
  failures += expect_parse_ok(
      "SELECT HEX(c) FROM t1 WHERE c LIKE CONCAT('%', _gb18030 0x8130963781309636) "
      "ESCAPE _gb18030 0x81309637");
  failures += expect_parse_ok("SELECT '' LIKE '' ESCAPE EXPORT_SET(1, 1, 1, 1, '')");
  failures += expect_parse_ok("SELECT ('a%b' LIKE 'a\\%b' ESCAPE (SELECT x FROM t1))");
  failures += expect_parse_ok(
      "SELECT SYSDATE(6) NOT LIKE '%.000000' || SYSDATE(6) NOT LIKE '%.000000'");
  failures += expect_parse_ok("SELECT * FROM t1 STRAIGHT_JOIN t2 FOR SHARE OF t1 FOR UPDATE OF t2");
  failures += expect_parse_ok("DO LEAD(1, n) OVER()");
  failures += expect_parse_ok("DO LAG(1, @v) OVER()");
  failures += expect_parse_ok(
      "SELECT 1 FROM DUAL WHERE 1 GROUP BY 1 HAVING 1 ORDER BY 1 FOR UPDATE");
  failures += expect_parse_ok(
      "SELECT 1 FROM (SELECT 1 FROM DUAL WHERE 1 GROUP BY 1 HAVING 1 "
      "ORDER BY 1 FOR UPDATE) a");
  failures += expect_parse_ok(
      "(SELECT 1 FROM t1) UNION "
      "(SELECT 1 FROM DUAL WHERE 1 GROUP BY 1 HAVING 1 ORDER BY 1 FOR UPDATE)");
  failures += expect_parse_ok("DO(SELECT 1 c GROUP BY 1 HAVING 1 ORDER BY COUNT(1))");
  failures += expect_parse_ok(
      "DO(SELECT 1 c FROM DUAL GROUP BY 1 HAVING 1 ORDER BY COUNT(1))");
  failures += expect_parse_ok(
      "SELECT ((SELECT 1 AS f FROM DUAL HAVING EXISTS(SELECT 1 FROM t1) IS TRUE "
      "ORDER BY f))");
  failures += expect_parse_ok("SELECT 1 WHERE TRUE HAVING COUNT(*) = 1");
  failures += expect_parse_ok("SELECT 'mood' SOUNDS LIKE 'mud'");
  failures += expect_parse_ok("SELECT * FROM t1 WHERE MATCH a,b AGAINST ('+call* +coll*' IN BOOLEAN MODE)");
  failures += expect_parse_ok(
      "SELECT ST_AsText(ST_GeomFromWKB(ST_AsWKB("
      "ST_Intersection(LineString(Point(-59,82), Point(32,29)), Point(2,-5))))) AS result");
  failures += expect_parse_ok("SELECT CONVERT(103, CHAR(50) UNICODE)");
  failures += expect_parse_ok("SELECT CAST(1/3 AS DOUBLE PRECISION), CAST(1/3 AS REAL)");
  failures += expect_parse_ok(
      "SELECT CAST(TIMESTAMP'2019-10-10 10:11:12' AT TIME ZONE 'UTC' AS DATETIME)");
  failures += expect_parse_ok("SELECT CAST(NULL AT TIME ZONE 'UTC' AS DATETIME)");
  failures += expect_parse_ok("SELECT CAST(CURRENT_TIMESTAMP AT TIME ZONE 'UTC' AS DATETIME)");
  failures += expect_parse_ok("DO ('1985-10-19' - INTERVAL(0x1446) DAY_MICROSECOND)");
  failures += expect_parse_ok("SELECT CAST(a AT TIME ZONE '+00:00' AS DATETIME) FROM t1");
  failures += expect_parse_ok(
      "SELECT REGEXP_INSTR(e, 'pattern') "
      "FROM (VALUES ROW('Find pattern'), ROW(NULL), ROW('Find pattern')) AS v(e)");
  failures += expect_parse_ok("SELECT * FROM t1 WHERE CAST(f1->>\"$.id\" AS CHAR(10)) = \"\\\"n\\\"\"");
  failures += expect_parse_ok("SELECT * FROM \"full\"");
  failures += expect_parse_ok("SELECT HEX(WEIGHT_STRING(a COLLATE utf8mb4_0900_ai_ci, 3, 3, 0xC0)) FROM t1");
  failures += expect_parse_ok("SELECT UNIQUE_CONSTRAINT_NAME FROM information_schema.referential_constraints WHERE constraint_schema = schema()");
  failures += expect_parse_ok(
      "SELECT * FROM JSON_TABLE('[]', '$[*]' "
      "COLUMNS (p CHAR(1) CHARACTER SET utf8 PATH '$.a')) AS t");
  failures += expect_parse_ok(
      "SELECT JSON_VALUE('{\"data\": \"2019-01-01 11:11::11\"}', '$.data' "
      "RETURNING DATETIME) AS v");
  failures += expect_parse_ok(
      "CREATE TABLE t_roles AS "
      "SELECT CURRENT_ROLE() AS CURRENT_ROLE, ROLES_GRAPHML() AS ROLES_GRAPHML");
  failures += expect_parse_ok(
      "CREATE DEFINER=baz@localhost PROCEDURE my_db.baz_proc() "
      "BEGIN "
      "SET ROLE ALL; "
      "INSERT INTO my_db.t1 VALUES(4) ON DUPLICATE KEY UPDATE id = VALUES(id) + 400; "
      "END");
  failures += expect_parse_ok("CREATE PROCEDURE p2(n INT) DO LEAD(1, n) OVER()");
  failures += expect_parse_ok("CREATE TABLE t1 (g GEOMCOLLECTION)");
  failures += expect_parse_ok("CREATE TABLE \" quoted name\" (i INT)");
  failures += expect_parse_ok("CREATE TABLE t_quoted_col (\"blah\" INT)");
  failures += expect_parse_ok(
      "CREATE TABLE t_part(col1 INT, col2 DATE) "
      "ENGINE=INNODB "
      "PARTITION BY RANGE(YEAR(\"col2\")) "
      "SUBPARTITION BY HASH(TO_DAYS(\"col2\"))("
      "PARTITION p0 VALUES LESS THAN (1990)("
      "SUBPARTITION s0,"
      "SUBPARTITION s1 TABLESPACE=`innodb_file_per_table`"
      "),"
      "PARTITION p1 VALUES LESS THAN MAXVALUE("
      "SUBPARTITION s2,"
      "SUBPARTITION s3 TABLESPACE=\"innodb_file_per_table\""
      ")"
      ")");
  failures += expect_parse_ok("CREATE TABLE BIT_AND (a INT)");
  failures += expect_parse_ok("SELECT 1 /*!99999 /* */ */");
  failures += expect_parse_ok("WITH cte AS (SELECT 0 /*! ) */ SELECT * FROM cte a, cte b");
  failures += expect_parse_ok("WITH cte AS /*! ( */ SELECT 0) SELECT * FROM cte a, cte b");
  failures += expect_parse_ok("SELECT @a := 1, @@session.sql_mode");
  failures += expect_parse_ok("SET @@SESSION.binlog_format=ROW");
  failures += expect_parse_ok("SET @`a b`='hello'");
  failures += expect_parse_ok("SET PERSIST innodb_monitor_enable=all");
  failures += expect_parse_ok(
      "SET PERSIST max_user_connections=10, PERSIST max_allowed_packet=8388608");
  failures += expect_parse_ok(
      "SET @@persist.max_user_connections=10, PERSIST max_allowed_packet=8388608");
  failures += expect_parse_ok("SET GLOBAL autocommit=0, PERSIST max_user_connections=10");
  failures += expect_parse_ok("SET PASSWORD = '' REPLACE ''");
  failures += expect_parse_ok("SET PASSWORD FOR 'usr1'@'localhost' TO RANDOM");
  failures += expect_parse_ok("SET LOCAL TRANSACTION READ ONLY");
  failures += expect_parse_ok("PREPARE stmt FROM \"SELECT 'x' AS 'alias'\"");
  failures += expect_parse_ok("SELECT CONNECTION_ID() INTO @id1");
  failures += expect_parse_ok("SELECT 1 FROM DUAL LIMIT 1 INTO @var FOR UPDATE");
  failures += expect_parse_ok("SELECT 1 UNION SELECT 1 INTO @var FOR UPDATE");
  failures += expect_parse_ok("(SELECT 1 UNION SELECT 1 INTO @var FOR UPDATE)");
  failures += expect_parse_ok("(SELECT 1) LIMIT 1 INTO @var");
  failures += expect_parse_ok("(SELECT 2 AS c) ORDER BY c INTO @var");
  failures += expect_parse_ok("(SELECT 4) INTO @var");
  failures += expect_parse_ok("DO (!(SECOND(0xb16beeb7)))");
  failures += expect_parse_ok("DO ST_ASTEXT(ST_UNION(ST_GEOMFROMTEXT('POINT(1 1)'), ST_GEOMFROMTEXT('POINT(2 2)'))) st_u");
  failures += expect_parse_ok("SELECT * INTO OUTFILE 'test/t1.txt' FROM t1");
  failures += expect_parse_ok("SELECT * FROM t1 INTO OUTFILE 'tmp1.txt' CHARACTER SET binary");
  failures += expect_parse_ok("SELECT 1 INTO @var17727401 FROM DUAL");
  failures += expect_parse_ok("SELECT '00' UNION SELECT '10' INTO OUTFILE 'tmpp.txt'");
  failures += expect_parse_ok("SELECT '00' UNION SELECT '10' INTO OUTFILE 'tmpp2.txt' CHARACTER SET ucs2");
  failures += expect_parse_ok("SELECT user, plugin, authentication_string FROM mysql.user");
  failures += expect_parse_ok("BEGIN; INSERT INTO t1 VALUES (3, 'gamma', NOW()); COMMIT;");
  failures += expect_parse_ok("BEGIN WORK");
  failures += expect_parse_ok("START TRANSACTION READ ONLY, WITH CONSISTENT SNAPSHOT");
  failures += expect_parse_ok("COMMIT WORK AND CHAIN");
  failures += expect_parse_ok("ROLLBACK WORK TO SAVEPOINT A");
  failures += expect_parse_ok("LOCK TABLES t1 LOW_PRIORITY WRITE");
  failures += expect_parse_ok("LOCK INSTANCE FOR BACKUP");
  failures += expect_parse_ok("UNLOCK INSTANCE");
  failures += expect_parse_ok("FLUSH RELAY LOGS");
  failures += expect_parse_ok("FLUSH OPTIMIZER_COSTS");
  failures += expect_parse_ok("FLUSH USER_RESOURCES");
  failures += expect_parse_ok("FLUSH STATUS,USER_RESOURCES");
  failures += expect_parse_ok("FLUSH TABLE WITH READ LOCK");
  failures += expect_parse_ok("FLUSH TABLES t1 FOR EXPORT");
  failures += expect_parse_ok("CACHE INDEX t1 IN new_cache");
  failures += expect_parse_ok("LOAD INDEX INTO CACHE t1 PARTITION (ALL)");
  failures += expect_parse_ok("LOAD INDEX INTO CACHE t1, t2 KEY (PRIMARY,b) IGNORE LEAVES");
  failures += expect_parse_ok(
      "LOAD XML INFILE '../../std_data/loadxml.dat' INTO TABLE t1 "
      "ROWS IDENTIFIED BY '<row>'");
  failures += expect_parse_ok(
      "LOAD XML INFILE '../../std_data/loadxml.dat' INTO TABLE t1 "
      "ROWS IDENTIFIED BY '<row>' IGNORE 4 ROWS");
  failures += expect_parse_ok(
      "LOAD DATA INFILE 'loadtest.txt' INTO TABLE t1 PARTITION (pNeg, subp4, subp5)");
  failures += expect_parse_ok(
      "LOAD DATA INFILE '../../std_data/words.dat' INTO TABLE t1 (a) SET b:= f1()");
  failures += expect_parse_ok("IMPORT TABLE FROM 't1_*.sdi', 't2_*.sdi'");
  failures += expect_parse_ok("SHOW SLAVE HOSTS");
  failures += expect_parse_ok("SHOW BINLOG EVENTS");
  failures += expect_parse_ok("SHOW MASTER LOGS");
  failures += expect_parse_ok("SHOW WARNINGS LIMIT 1");
  failures += expect_parse_ok("SHOW ERRORS LIMIT 1");
  failures += expect_parse_ok("INSTALL PLUGIN archive SONAME 'ha_archive.so'");
  failures += expect_parse_ok("CREATE SERVER s FOREIGN DATA WRAPPER mysql OPTIONS (DATABASE 'test')");
  failures += expect_parse_ok("ALTER SERVER s OPTIONS (USER 'sally')");
  failures += expect_parse_ok("DROP SERVER 'server_one'");
  failures += expect_parse_ok("HANDLER t1 READ FIRST");
  failures += expect_parse_ok("HANDLER t1 READ `PRIMARY` PREV LIMIT 3");
  failures += expect_parse_ok("HANDLER t1 READ a = (49)");
  failures += expect_parse_ok("HANDLER t1 OPEN t");
  failures += expect_parse_ok("HELP no_such_topic");
  failures += expect_parse_ok("CALL avg ()");
  failures += expect_parse_ok("CALL count ()");
  failures += expect_parse_ok("XA START 'test3','xx',5");
  failures += expect_parse_ok("XA COMMIT 'test3','xx',5 ONE PHASE");
  failures += expect_parse_ok("XA RECOVER CONVERT XID");
  failures += expect_parse_ok("INSTALL COMPONENT 'file://component_validate_password' SET length = 8 + 8");
  failures += expect_parse_ok(
      "INSTALL COMPONENT \"file://component_validate_password\" "
      "SET validate_password.length = 16, PERSIST validate_password.number_count = 13");
  failures += expect_parse_ok("UNINSTALL COMPONENT 'file://component_example_component1', 'file://component_example_component2'");
  failures += expect_parse_ok(
      "CHANGE REPLICATION SOURCE TO SOURCE_PASSWORD='secret', "
      "IGNORE_SERVER_IDS=(99,100) FOR CHANNEL 'chan_jackie'");
  failures += expect_parse_ok("PURGE BINARY LOGS TO 'source-bin.000002'");
  failures += expect_parse_ok("PURGE MASTER LOGS BEFORE NOW()");
  failures += expect_parse_ok("RESET PERSIST IF EXISTS default.key_buffer_size");
  failures += expect_parse_ok("CREATE USER 'u1'@'%.com'");
  failures += expect_parse_ok(
      "CREATE USER u_worldrou@localhost IDENTIFIED BY 'xxx' DEFAULT ROLE r_worldrou");
  failures += expect_parse_ok("CREATE USER 'usr1'@'localhost' IDENTIFIED BY RANDOM PASSWORD");
  failures += expect_parse_ok("CREATE ROLE skip");
  failures += expect_parse_ok("CREATE ROLE locked");
  failures += expect_parse_ok("CREATE ROLE nowait");
  failures += expect_parse_ok("CREATE ROLE role");
  failures += expect_parse_ok("ALTER USER USER() IDENTIFIED BY 'abc'");
  failures += expect_parse_ok("ALTER USER 'usr1'@'localhost' IDENTIFIED BY RANDOM PASSWORD");
  failures += expect_parse_ok("ALTER USER u1@localhost DEFAULT ROLE r1, r2");
  failures += expect_parse_ok("ALTER USER CURRENT_USER() DEFAULT ROLE NONE");
  failures += expect_parse_ok(
      "ALTER USER u1 IDENTIFIED BY '123' REPLACE '', u2 IDENTIFIED BY '456' "
      "PASSWORD REQUIRE CURRENT OPTIONAL");
  failures += expect_parse_ok(
      "ALTER USER redqueen@localhost IDENTIFIED BY 'madness' RETAIN CURRENT PASSWORD");
  failures += expect_parse_ok(
      "ALTER USER redqueen@localhost DISCARD OLD PASSWORD COMMENT 'Dropped old password'");
  failures += expect_parse_ok("GRANT r2 TO u1@localhost WITH ADMIN OPTION");
  failures += expect_parse_ok("REVOKE IF EXISTS SELECT ON wl14690.* FROM u1");
  failures += expect_parse_ok("REVOKE IF EXISTS role1 FROM u1");
  failures += expect_parse_ok("REVOKE ALL ON *.* FROM unknown_user IGNORE UNKNOWN USER");
  failures += expect_parse_ok(
      "REVOKE ALL PRIVILEGES, GRANT OPTION FROM unknown_user IGNORE UNKNOWN USER");
  failures += expect_parse_ok("REVOKE PROXY ON u1 FROM unknown_user IGNORE UNKNOWN USER");
  failures += expect_parse_ok("REVOKE IF EXISTS PROXY ON u3 FROM u1");
  failures += expect_parse_ok(
      "CREATE RESOURCE GROUP cafe TYPE=USER VCPU=1-3 THREAD_PRIORITY=5");
  failures += expect_parse_ok("DROP RESOURCE GROUP rg1 FORCE");
  failures += expect_parse_ok("ALTER TABLESPACE mysql ENCRYPTION='N'");
  failures += expect_parse_ok("ALTER TABLESPACE ts1 RENAME TO ts2");
  failures += expect_parse_ok("CREATE TABLESPACE ts1 ENGINE_ATTRIBUTE=''");
  failures += expect_parse_ok("CREATE UNDO TABLESPACE undo_003 ADD DATAFILE 'undo_003.ibu' ENGINE InnoDB");
  failures += expect_parse_ok("ALTER UNDO TABLESPACE undo_003 SET ACTIVE ENGINE InnoDB");
  failures += expect_parse_ok("ALTER UNDO TABLESPACE undo_003 SET INACTIVE ENGINE InnoDB");
  failures += expect_parse_ok("DROP UNDO TABLESPACE undo_003 ENGINE InnoDB");
  failures += expect_parse_ok("ALTER TABLE t1 MODIFY COLUMN c1 FLOAT(10.3)");
  failures += expect_parse_ok("ALTER TABLE tst MODIFY COLUMN scol TIME DEFAULT(CURTIME())");
  failures += expect_parse_ok(
      "ANALYZE TABLE tbl_int UPDATE HISTOGRAM ON col1 "
      "USING DATA '{\"buckets\": [], \"histogram-type\": \"singleton\"}'");
  failures += expect_parse_ok("ANALYZE TABLE foo, foo2 UPDATE HISTOGRAM ON bar WITH 100 BUCKETS");
  failures += expect_parse_ok("ANALYZE TABLE t1, t2 DROP HISTOGRAM ON col1");
  failures += expect_parse_ok("ANALYZE TABLES c, cc");
  failures += expect_parse_ok("CREATE TABLE tst (scol INT DEFAULT(col * col), col INT)");
  failures += expect_parse_ok("CREATE TABLE t3 (a INT PRIMARY KEY, d INT DEFAULT (-a + 1), c INT DEFAULT (-d))");
  failures += expect_parse_ok("CREATE TABLE t_dec (d DEC(10))");
  failures += expect_parse_ok("CREATE TABLE t_dec_default (d DEC(6,6) DEFAULT .000001)");
  failures += expect_parse_ok("CREATE TABLE t1 (f1 DATE NOT SECONDARY)");
  failures += expect_parse_ok("CREATE TABLE `t1` (i varchar(200) DEFAULT (_utf8mb4\"abc\"))");
  failures += expect_parse_ok("CREATE TABLE t_time (a datetime DEFAULT (current_time()))");
  failures += expect_parse_ok("CREATE TABLE t_utc (a datetime, b datetime DEFAULT (utc_date()))");
  failures += expect_parse_ok("CREATE TABLE t_sysdate (b varchar(100) DEFAULT (sysdate()))");
  failures += expect_parse_ok("CREATE TABLE t_database (a varchar(1024) DEFAULT (database()))");
  failures += expect_parse_ok("CREATE TABLE t_user (a varchar(288) DEFAULT (CURRENT_USER()), b varchar(288) DEFAULT (USER()))");
  failures += expect_parse_ok("CREATE TABLE t(i INT, b TINYBLOB DEFAULT (REPEAT('b', i)))");
  failures += expect_parse_ok("CREATE TABLE t_rand (i1 INTEGER, i2 INTEGER DEFAULT (i1 + RAND()))");
  failures += expect_parse_ok(
      "CREATE TABLE t1 (a INT, b TIMESTAMP DEFAULT (TIMESTAMPADD(MINUTE, 1, '2003-01-02')))");
  failures += expect_parse_ok("ALTER TABLE t ADD COLUMN b int DEFAULT(date_sub(a, INTERVAL A MONTH))");
  failures += expect_parse_ok(
      "ALTER TABLE t1 ENGINE='MYISAM', "
      "ADD COLUMN my_row_id bigint unsigned NOT NULL AUTO_INCREMENT INVISIBLE FIRST, "
      "ADD PRIMARY KEY(my_row_id)");
  failures += expect_parse_ok("ALTER TABLE t1 ENGINE='MYISAM', ALTER my_row_id SET VISIBLE");
  failures += expect_parse_ok("CREATE TABLE tst_div (i2 INT, d2 INT, def2 DOUBLE DEFAULT(i2 DIV d2))");
  failures += expect_parse_ok(
      "CREATE VIEW v1 AS SELECT f1, f2 FROM t1 "
      "WHERE f2 < '2019-01-01 00:00:00' WITH CHECK OPTION");
  failures += expect_parse_ok("ALTER VIEW v2 AS SELECT * FROM t2 GROUP BY(f2)");
  failures += expect_parse_ok(
      "CREATE FUNCTION f(a INTEGER) RETURNS INTEGER DETERMINISTIC "
      "RETURN IFNULL(a, 666)");
  failures += expect_parse_ok("SHOW CREATE FUNCTION sp.hello");
  failures += expect_parse_ok("SHOW FUNCTION CODE f1");
  failures += expect_parse_ok("SHOW CREATE SCHEMA foo");
  failures += expect_parse_ok("SHOW SCHEMAS LIKE 'foo'");
  failures += expect_parse_ok("SHOW STORAGE ENGINES");
  failures += expect_parse_ok("SHOW ENGINE csv STATUS");
  failures += expect_parse_ok("SHOW ENGINE csv LOGS");
  failures += expect_parse_ok("SHOW ENGINE csv MUTEX");
  failures += expect_parse_ok("SHOW LOCAL VARIABLES LIKE 'SQL_MODE'");
  failures += expect_parse_ok("SHOW EXTENDED INDEX FROM t1");
  failures += expect_parse_ok("SHOW PROCEDURE CODE signal_proc");
  failures += expect_parse_ok("ALTER SCHEMA s1 READ ONLY DEFAULT");
  failures += expect_parse_ok(
      "CREATE FUNCTION f1(a CHAR(1) CHARACTER SET ucs2 COLLATE ucs2_persian_ci) "
      "RETURNS INT RETURN 1");
  failures += expect_parse_ok("DROP FUNCTION IF EXISTS f");
  failures += expect_parse_ok("ALTER FUNCTION f COMMENT 'routine comment'");
  failures += expect_parse_ok("ALTER PROCEDURE bar");
  failures += expect_parse_ok(
      "CREATE TRIGGER before_t2_insert BEFORE INSERT ON t2 FOR EACH ROW "
      "BEGIN INSERT INTO t1 VALUES(NEW.c1, NEW.c2); END");
  failures += expect_parse_ok(
      "CREATE DEFINER = u1 TRIGGER trig1 BEFORE INSERT ON t1 FOR EACH ROW DELETE FROM t1");
  failures += expect_parse_ok("CREATE TRIGGER IF NOT EXISTS trg1 BEFORE INSERT ON t1 FOR EACH ROW BEGIN END");
  failures += expect_parse_ok(
      "CREATE TRIGGER trg1a0 BEFORE INSERT ON t1 FOR EACH ROW PRECEDES trg1a BEGIN END");
  failures += expect_parse_ok("SHOW CREATE TRIGGER trg1");
  failures += expect_parse_ok("SHOW CREATE EVENT eve");
  failures += expect_parse_ok(
      "CREATE DEFINER=CURRENT_USER EVENT ev_hourly "
      "ON SCHEDULE EVERY 1 HOUR STARTS CURRENT_TIMESTAMP "
      "ON COMPLETION NOT PRESERVE ENABLE COMMENT 'hourly' DO INSERT INTO t1 VALUES (1)");
  failures += expect_parse_ok(
      "CREATE EVENT ev_once ON SCHEDULE AT '2030-01-01 00:00:00' DO BEGIN SELECT 1; END");
  failures += expect_parse_ok(
      "CREATE EVENT event_name_with_a_very_long_identifier_"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      " ON SCHEDULE EVERY 2 YEAR DO SELECT 1");
  failures += expect_parse_ok(
      "ALTER EVENT ev_hourly ON SCHEDULE EVERY 1 YEAR ON COMPLETION PRESERVE "
      "RENAME TO ev_yearly COMMENT 'new comment' DO BEGIN SELECT 1; END");
  failures += expect_parse_ok("ALTER EVENT event1 RENAME TO event2 ENABLE");
  failures += expect_parse_ok("DROP EVENT IF EXISTS ev_yearly");
  failures += expect_parse_ok(
      "CREATE PROCEDURE proc1 (IN val1 INT) BEGIN "
      "IF val1 < 10 THEN SIGNAL SQLSTATE '45000' "
      "SET MESSAGE_TEXT = 'check constraint on c1 failed'; END IF; END");
  failures += expect_parse_ok(
      "CREATE PROCEDURE p1() SQL SECURITY INVOKER INSERT INTO t2 (fld1, fld2) VALUES (1, 2)");
  failures += expect_parse_ok("CREATE DEFINER = u1 PROCEDURE p1() DELETE FROM t1");
  failures += expect_parse_ok("GET DIAGNOSTICS @n = NUMBER");
  failures += expect_parse_ok("GET CURRENT DIAGNOSTICS CONDITION 1 @x = RETURNED_SQLSTATE");
  failures += expect_parse_ok("CREATE PROCEDURE p1() GET STACKED DIAGNOSTICS @var1 = NUMBER");
  failures += expect_parse_ok(
      "CREATE PROCEDURE pdiag() BEGIN "
      "DECLARE red CONDITION FOR 1051; "
      "DECLARE CONTINUE HANDLER FOR red BEGIN "
      "GET DIAGNOSTICS @n0 = NUMBER; "
      "GET DIAGNOSTICS CONDITION 1 @e0 = MYSQL_ERRNO, @t0 = MESSAGE_TEXT; "
      "END; "
      "SIGNAL SQLSTATE '02000' SET MESSAGE_TEXT = 'signal message'; "
      "END");
  failures += expect_parse_ok(
      "CREATE PROCEDURE pddl() BEGIN "
      "DROP TABLE no_such_table; "
      "CREATE TABLE t1 (f1 INT) START TRANSACTION; "
      "ALTER EVENT event1 RENAME TO event2 ENABLE; "
      "SELECT 'we should never get here'; "
      "END");
  failures += expect_parse_ok(
      "CREATE PROCEDURE pselect_into(INOUT ioid INTEGER) BEGIN "
      "SELECT id INTO ioid FROM t3 WHERE id = ioid; "
      "IF ioid IS NULL THEN SET ioid = 1; END IF; "
      "END");
  failures += expect_parse_ok(
      "CREATE PROCEDURE BatchInsert(IN row_count int) "
      "BEGIN "
      "START TRANSACTION; "
      "SET @n = 1; "
      "REPEAT "
      "SET @str = (CONCAT('test', CAST(@n AS CHAR))); "
      "INSERT INTO product(code, name) VALUES(@str, @str); "
      "SET @n = @n + 1; "
      "UNTIL @n > row_count "
      "END REPEAT; "
      "COMMIT; "
      "END");
  failures += expect_parse_ok(
      "CREATE PROCEDURE cursor1() BEGIN "
      "DECLARE v1 int; "
      "DECLARE done INT DEFAULT FALSE; "
      "DECLARE cur1 CURSOR FOR SELECT * FROM t1; "
      "DECLARE CONTINUE HANDLER FOR NOT FOUND SET done = TRUE; "
      "OPEN cur1; "
      "read_loop: LOOP "
      "FETCH cur1 INTO v1; "
      "IF done THEN LEAVE read_loop; END IF; "
      "END LOOP; "
      "CLOSE cur1; "
      "END");
  failures += expect_parse_ok(
      "CREATE PROCEDURE cursor_keyword_name() BEGIN "
      "DECLARE start_time CHAR(20); "
      "DECLARE cur1 CURSOR FOR SELECT * FROM mysql.slow_log; "
      "OPEN cur1; "
      "FETCH cur1 INTO start_time; "
      "END");
  failures += expect_parse_ok(
      "CREATE PROCEDURE p2() BEGIN "
      "DECLARE n INT DEFAULT 2; "
      "general: WHILE n > 0 DO SET n = n - 1; END WHILE general; "
      "END");
  failures += expect_parse_ok(
      "CREATE PROCEDURE plimit() BEGIN "
      "DECLARE a INTEGER; "
      "DECLARE b INTEGER; "
      "SELECT * FROM t1 LIMIT a, b; "
      "END");
  failures += expect_parse_ok(
      "CREATE FUNCTION kill_by_id(tid INT) RETURNS INT "
      "BEGIN KILL tid; RETURN tid; END");
  failures += expect_parse_ok(
      "CREATE PROCEDURE p1(IN param INT) "
      "LANGUAGE SQL "
      "BEGIN "
      "DECLARE v INT DEFAULT 0; "
      "DECLARE rcount_each INT; "
      "DECLARE rcount_total INT DEFAULT 0; "
      "WHILE v < 5 DO "
      "UPDATE t1 SET a = a * 1.1 WHERE b = param; "
      "GET DIAGNOSTICS rcount_each = ROW_COUNT; "
      "SET rcount_total = rcount_total + rcount_each; "
      "SET v = v + 1; "
      "END WHILE; "
      "SELECT rcount_total; "
      "END");
  failures += expect_parse_ok("CREATE PROCEDURE pmod() MODIFIES SQL DATA SET @a = 5");
  failures += expect_parse_ok("CREATE PROCEDURE pdet(OUT i INT) DETERMINISTIC NO SQL SET i = 3");
  failures += expect_parse_ok("CREATE PROCEDURE proc_1() RESET MASTER");
  failures += expect_parse_ok("CREATE PROCEDURE proc_start_slave() START SLAVE");
  failures += expect_parse_ok(
      "CREATE PROCEDURE proc_plugin() INSTALL PLUGIN my_plug SONAME 'some_plugin.so'");
  failures += expect_parse_ok("CREATE PROCEDURE proc_drop_trigger() DROP TRIGGER tr1");
  failures += expect_parse_ok("CREATE PROCEDURE proc_user() CREATE USER pstest_xyz@localhost");
  failures += expect_parse_ok("CREATE PROCEDURE proc_cache() CACHE INDEX t1 IN new_cache");
  failures += expect_parse_ok("CREATE PROCEDURE proc_load_index() LOAD INDEX INTO CACHE t1 IGNORE LEAVES");
  failures += expect_parse_ok(
      "CREATE PROCEDURE proc_tablespace() BEGIN "
      "CREATE TABLESPACE x; "
      "DROP TABLESPACE x; "
      "END");
  failures += expect_parse_ok("CREATE PROCEDURE bug14945() DETERMINISTIC TRUNCATE t3");
  failures += expect_parse_ok(
      "CREATE DEFINER = 'root'@'localhost' PROCEDURE p1() "
      "NOT DETERMINISTIC CONTAINS SQL SQL SECURITY DEFINER COMMENT '' "
      "BEGIN SHOW TABLE STATUS LIKE 't1'; END");
  failures += expect_parse_ok(
      "CREATE TRIGGER before_t1_insert BEFORE INSERT ON t1 FOR EACH ROW "
      "BEGIN CALL proc1(new.c1); END");
  failures += expect_parse_ok(
      "CALL mtr.add_suppression(\"Found wrong key definition in #sql.* Please do "
      "\\ALTER TABLE `#sql.*` FORCE \\\" to fix it!\"\")\"");
  failures += expect_ast_ok("SELECT 1 + 2 AS total", "input");
  failures += expect_ast_ok("SELECT 1 WHERE TRUE HAVING COUNT(*) = 1",
                            "nt_mylite_recognized_statement");
  {
    const MyliteStatementKind kinds[] = {MYLITE_STATEMENT_SET,
                                         MYLITE_STATEMENT_SELECT,
                                         MYLITE_STATEMENT_CREATE};
    failures += expect_ast_statements("SET @a = 1; SELECT @a; CREATE TABLE s (id INT)",
                                      3, kinds);
  }
  failures += expect_ast_target("CREATE TABLE db1.t1 (id INT)",
                                MYLITE_STATEMENT_CREATE,
                                MYLITE_STATEMENT_TARGET_TABLE, "db1.t1", "db1",
                                "t1");
  failures += expect_ast_target("INSERT INTO db1.t1 (id) VALUES (1)",
                                MYLITE_STATEMENT_INSERT,
                                MYLITE_STATEMENT_TARGET_TABLE, "db1.t1", "db1",
                                "t1");
  failures += expect_ast_target("UPDATE db1.t1 SET id = 2",
                                MYLITE_STATEMENT_UPDATE,
                                MYLITE_STATEMENT_TARGET_TABLE, "db1.t1", "db1",
                                "t1");
  failures += expect_ast_target("DELETE FROM db1.t1 WHERE id = 1",
                                MYLITE_STATEMENT_DELETE,
                                MYLITE_STATEMENT_TARGET_TABLE, "db1.t1", "db1",
                                "t1");
  failures += expect_ast_target("SET @a = 1", MYLITE_STATEMENT_SET,
                                MYLITE_STATEMENT_TARGET_VARIABLE, "@a", NULL,
                                "@a");
  {
    const ExpectedAstTarget targets[] = {
        {MYLITE_STATEMENT_TARGET_ROLE_PRIMARY, MYLITE_STATEMENT_TARGET_TABLE,
         "db1.t1", "db1", "t1"},
        {MYLITE_STATEMENT_TARGET_ROLE_PRIMARY, MYLITE_STATEMENT_TARGET_TABLE,
         "db2.t2", "db2", "t2"}};
    failures += expect_ast_targets("DROP TABLE db1.t1, db2.t2",
                                   MYLITE_STATEMENT_DROP, targets,
                                   sizeof(targets) / sizeof(targets[0]));
  }
  {
    const ExpectedAstTarget targets[] = {
        {MYLITE_STATEMENT_TARGET_ROLE_SOURCE, MYLITE_STATEMENT_TARGET_TABLE,
         "db1.t1", "db1", "t1"},
        {MYLITE_STATEMENT_TARGET_ROLE_DESTINATION, MYLITE_STATEMENT_TARGET_TABLE,
         "db1.t2", "db1", "t2"},
        {MYLITE_STATEMENT_TARGET_ROLE_SOURCE, MYLITE_STATEMENT_TARGET_TABLE,
         "db2.a", "db2", "a"},
        {MYLITE_STATEMENT_TARGET_ROLE_DESTINATION, MYLITE_STATEMENT_TARGET_TABLE,
         "db2.b", "db2", "b"}};
    failures += expect_ast_targets(
        "RENAME TABLE db1.t1 TO db1.t2, db2.a TO db2.b",
        MYLITE_STATEMENT_RENAME, targets,
        sizeof(targets) / sizeof(targets[0]));
  }
  {
    const ExpectedAstTarget targets[] = {
        {MYLITE_STATEMENT_TARGET_ROLE_PRIMARY, MYLITE_STATEMENT_TARGET_TABLE,
         "db1.t1", "db1", "t1"},
        {MYLITE_STATEMENT_TARGET_ROLE_PRIMARY, MYLITE_STATEMENT_TARGET_TABLE,
         "db2.t2", "db2", "t2"}};
    failures += expect_ast_targets(
        "UPDATE db1.t1 JOIN db2.t2 ON t1.id = t2.id SET t1.id = 1",
        MYLITE_STATEMENT_UPDATE, targets,
        sizeof(targets) / sizeof(targets[0]));
  }
  {
    const ExpectedAstTarget targets[] = {
        {MYLITE_STATEMENT_TARGET_ROLE_PRIMARY, MYLITE_STATEMENT_TARGET_TABLE,
         "db1.t1", "db1", "t1"},
        {MYLITE_STATEMENT_TARGET_ROLE_PRIMARY, MYLITE_STATEMENT_TARGET_TABLE,
         "db2.t2", "db2", "t2"}};
    failures += expect_ast_targets(
        "DELETE db1.t1, db2.t2 FROM db1.t1 JOIN db2.t2 ON t1.id = t2.id",
        MYLITE_STATEMENT_DELETE, targets,
        sizeof(targets) / sizeof(targets[0]));
  }
  {
    const ExpectedAstTarget targets[] = {
        {MYLITE_STATEMENT_TARGET_ROLE_PRIMARY, MYLITE_STATEMENT_TARGET_TABLE,
         "t1.*", NULL, "t1"}};
    failures += expect_ast_targets("DELETE t1.* FROM t1",
                                   MYLITE_STATEMENT_DELETE, targets,
                                   sizeof(targets) / sizeof(targets[0]));
  }
  {
    const ExpectedAstTarget targets[] = {
        {MYLITE_STATEMENT_TARGET_ROLE_PRIMARY, MYLITE_STATEMENT_TARGET_TABLE,
         "db1.t1", "db1", "t1"}};
    failures += expect_ast_targets(
        "DELETE FROM db1.t1 WHERE EXISTS (SELECT 1 FROM db2.t2)",
        MYLITE_STATEMENT_DELETE, targets,
        sizeof(targets) / sizeof(targets[0]));
  }
  {
    const ExpectedAstTarget targets[] = {
        {MYLITE_STATEMENT_TARGET_ROLE_PRIMARY, MYLITE_STATEMENT_TARGET_TABLE,
         "db1.t1", "db1", "t1"}};
    failures += expect_ast_targets(
        "UPDATE db1.t1 JOIN (SELECT * FROM db2.t2) dt ON t1.id = dt.id "
        "SET t1.id = 1",
        MYLITE_STATEMENT_UPDATE, targets,
        sizeof(targets) / sizeof(targets[0]));
  }
  {
    const ExpectedAstTarget targets[] = {
        {MYLITE_STATEMENT_TARGET_ROLE_PRIMARY, MYLITE_STATEMENT_TARGET_TABLE,
         "db1.t1", "db1", "t1"}};
    failures += expect_ast_targets(
        "WITH cte AS (SELECT * FROM db2.t2) UPDATE db1.t1 SET id = 1",
        MYLITE_STATEMENT_UPDATE, targets,
        sizeof(targets) / sizeof(targets[0]));
  }
  {
    const ExpectedCreateTableColumn columns[] = {
        {"id INT NOT NULL AUTO_INCREMENT", "id", "INT",
         "NOT NULL AUTO_INCREMENT", MYLITE_CREATE_TABLE_COLUMN_TYPE_NUMERIC,
         MYLITE_CREATE_TABLE_COLUMN_FLAG_NOT_NULL |
             MYLITE_CREATE_TABLE_COLUMN_FLAG_AUTO_INCREMENT},
        {"name VARCHAR(50) DEFAULT 'x'", "name", "VARCHAR(50)",
         "DEFAULT 'x'", MYLITE_CREATE_TABLE_COLUMN_TYPE_STRING,
         MYLITE_CREATE_TABLE_COLUMN_FLAG_DEFAULT}};
    failures += expect_create_table_columns(
        "CREATE TABLE db1.t1 (id INT NOT NULL AUTO_INCREMENT, "
        "name VARCHAR(50) DEFAULT 'x', PRIMARY KEY (id), KEY name_idx (name))",
        columns, sizeof(columns) / sizeof(columns[0]));
  }
  {
    const ExpectedCreateTableColumn columns[] = {
        {"n BIGINT UNSIGNED ZEROFILL", "n", "BIGINT UNSIGNED ZEROFILL", NULL,
         MYLITE_CREATE_TABLE_COLUMN_TYPE_NUMERIC,
         MYLITE_CREATE_TABLE_COLUMN_FLAG_UNSIGNED |
             MYLITE_CREATE_TABLE_COLUMN_FLAG_ZEROFILL},
        {"v VARCHAR(191) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL",
         "v", "VARCHAR(191) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci",
         "NOT NULL", MYLITE_CREATE_TABLE_COLUMN_TYPE_STRING,
         MYLITE_CREATE_TABLE_COLUMN_FLAG_CHARACTER_SET |
             MYLITE_CREATE_TABLE_COLUMN_FLAG_COLLATE |
             MYLITE_CREATE_TABLE_COLUMN_FLAG_NOT_NULL},
        {"j JSON", "j", "JSON", NULL, MYLITE_CREATE_TABLE_COLUMN_TYPE_JSON, 0},
        {"e ENUM('a','b')", "e", "ENUM('a','b')", NULL,
         MYLITE_CREATE_TABLE_COLUMN_TYPE_ENUM, 0},
        {"s SET('x','y')", "s", "SET('x','y')", NULL,
         MYLITE_CREATE_TABLE_COLUMN_TYPE_SET, 0},
        {"ts TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP", "ts",
         "TIMESTAMP", "DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP",
         MYLITE_CREATE_TABLE_COLUMN_TYPE_TEMPORAL,
         MYLITE_CREATE_TABLE_COLUMN_FLAG_DEFAULT |
             MYLITE_CREATE_TABLE_COLUMN_FLAG_ON_UPDATE},
        {"g POINT", "g", "POINT", NULL,
         MYLITE_CREATE_TABLE_COLUMN_TYPE_SPATIAL, 0},
        {"c INT CHECK (c > 0)", "c", "INT", "CHECK (c > 0)",
         MYLITE_CREATE_TABLE_COLUMN_TYPE_NUMERIC,
         MYLITE_CREATE_TABLE_COLUMN_FLAG_CHECK},
        {"r INT REFERENCES other(id)", "r", "INT", "REFERENCES other(id)",
         MYLITE_CREATE_TABLE_COLUMN_TYPE_NUMERIC,
         MYLITE_CREATE_TABLE_COLUMN_FLAG_REFERENCES},
        {"u INT UNIQUE COMMENT 'x'", "u", "INT", "UNIQUE COMMENT 'x'",
         MYLITE_CREATE_TABLE_COLUMN_TYPE_NUMERIC,
         MYLITE_CREATE_TABLE_COLUMN_FLAG_UNIQUE_KEY |
             MYLITE_CREATE_TABLE_COLUMN_FLAG_COMMENT},
        {"gen INT GENERATED ALWAYS AS (1 + 2) STORED", "gen", "INT",
         "GENERATED ALWAYS AS (1 + 2) STORED",
         MYLITE_CREATE_TABLE_COLUMN_TYPE_NUMERIC,
         MYLITE_CREATE_TABLE_COLUMN_FLAG_GENERATED |
             MYLITE_CREATE_TABLE_COLUMN_FLAG_STORED}};
    failures += expect_create_table_columns(
        "CREATE TABLE t (n BIGINT UNSIGNED ZEROFILL, "
        "v VARCHAR(191) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL, "
        "j JSON, e ENUM('a','b'), s SET('x','y'), "
        "ts TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, "
        "g POINT, c INT CHECK (c > 0), r INT REFERENCES other(id), "
        "u INT UNIQUE COMMENT 'x', "
        "gen INT GENERATED ALWAYS AS (1 + 2) STORED)",
        columns, sizeof(columns) / sizeof(columns[0]));
  }
  {
    const ExpectedCreateTableColumn columns[] = {
        {.definition = "d DECIMAL(10,2) UNSIGNED ZEROFILL",
         .name = "d",
         .type = "DECIMAL(10,2) UNSIGNED ZEROFILL",
         .type_family = MYLITE_CREATE_TABLE_COLUMN_TYPE_NUMERIC,
         .flags = MYLITE_CREATE_TABLE_COLUMN_FLAG_UNSIGNED |
                  MYLITE_CREATE_TABLE_COLUMN_FLAG_ZEROFILL,
         .type_name = "DECIMAL",
         .type_parameters = "(10,2)",
         .type_numeric_parameter_count = 2,
         .type_numeric_parameters = {10, 2},
         .type_attributes = "UNSIGNED ZEROFILL",
         .type_kind = MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_DECIMAL,
         .storage_class = MYLITE_CREATE_TABLE_COLUMN_STORAGE_DECIMAL,
         .type_node_symbol = "nt_type"},
        {.definition = "e ENUM('a','b') DEFAULT 'a'",
         .name = "e",
         .type = "ENUM('a','b')",
         .options = "DEFAULT 'a'",
         .type_family = MYLITE_CREATE_TABLE_COLUMN_TYPE_ENUM,
         .flags = MYLITE_CREATE_TABLE_COLUMN_FLAG_DEFAULT,
         .type_name = "ENUM",
         .type_parameters = "('a','b')",
         .type_element_count = 2,
         .default_span = "DEFAULT 'a'",
         .default_value = "'a'",
         .type_kind = MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_ENUM,
         .storage_class = MYLITE_CREATE_TABLE_COLUMN_STORAGE_ENUM,
         .type_node_symbol = "nt_type",
         .options_node_symbol = "nt_column_option_list_opt",
         .default_node_symbol = "nt_column_option",
         .default_value_node_symbol = "nt_default_value_expr"},
        {.definition = "v VARCHAR(50) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin COMMENT 'x'",
         .name = "v",
         .type = "VARCHAR(50) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin",
         .options = "COMMENT 'x'",
         .type_family = MYLITE_CREATE_TABLE_COLUMN_TYPE_STRING,
         .flags = MYLITE_CREATE_TABLE_COLUMN_FLAG_CHARACTER_SET |
                  MYLITE_CREATE_TABLE_COLUMN_FLAG_COLLATE |
                  MYLITE_CREATE_TABLE_COLUMN_FLAG_COMMENT,
         .type_name = "VARCHAR",
         .type_parameters = "(50)",
         .type_numeric_parameter_count = 1,
         .type_numeric_parameters = {50, 0},
         .type_attributes = "CHARACTER SET utf8mb4 COLLATE utf8mb4_bin",
         .comment = "COMMENT 'x'",
         .comment_value = "'x'",
         .type_kind = MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_VARCHAR,
         .storage_class = MYLITE_CREATE_TABLE_COLUMN_STORAGE_VARIABLE_STRING},
        {.definition = "n NATIONAL VARCHAR(10)",
         .name = "n",
         .type = "NATIONAL VARCHAR(10)",
         .type_family = MYLITE_CREATE_TABLE_COLUMN_TYPE_STRING,
         .type_name = "NATIONAL VARCHAR",
         .type_parameters = "(10)",
         .type_numeric_parameter_count = 1,
         .type_numeric_parameters = {10, 0},
         .type_kind = MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_NVARCHAR,
         .storage_class = MYLITE_CREATE_TABLE_COLUMN_STORAGE_VARIABLE_STRING},
        {.definition = "b LONG VARBINARY",
         .name = "b",
         .type = "LONG VARBINARY",
         .type_family = MYLITE_CREATE_TABLE_COLUMN_TYPE_STRING,
         .type_name = "LONG VARBINARY",
         .type_kind = MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_LONG_VARBINARY,
         .storage_class = MYLITE_CREATE_TABLE_COLUMN_STORAGE_BLOB},
        {.definition = "m LONG",
         .name = "m",
         .type = "LONG",
         .type_family = MYLITE_CREATE_TABLE_COLUMN_TYPE_STRING,
         .type_name = "LONG",
         .type_kind = MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_LONG,
         .storage_class = MYLITE_CREATE_TABLE_COLUMN_STORAGE_TEXT},
        {.definition = "l LONG VARCHAR",
         .name = "l",
         .type = "LONG VARCHAR",
         .type_family = MYLITE_CREATE_TABLE_COLUMN_TYPE_STRING,
         .type_name = "LONG VARCHAR",
         .type_kind = MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_LONG_VARCHAR,
         .storage_class = MYLITE_CREATE_TABLE_COLUMN_STORAGE_TEXT}};
    failures += expect_create_table_columns(
        "CREATE TABLE t (d DECIMAL(10,2) UNSIGNED ZEROFILL, "
        "e ENUM('a','b') DEFAULT 'a', "
        "v VARCHAR(50) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin COMMENT 'x', "
        "n NATIONAL VARCHAR(10), b LONG VARBINARY, m LONG, l LONG VARCHAR)",
        columns, sizeof(columns) / sizeof(columns[0]));
  }
  {
    const ExpectedCreateTableColumn columns[] = {
        {.definition = "ti TINYINT",
         .name = "ti",
         .type = "TINYINT",
         .type_family = MYLITE_CREATE_TABLE_COLUMN_TYPE_NUMERIC,
         .type_kind = MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_TINYINT,
         .storage_class = MYLITE_CREATE_TABLE_COLUMN_STORAGE_INTEGER},
        {.definition = "i8 INT8",
         .name = "i8",
         .type = "INT8",
         .type_family = MYLITE_CREATE_TABLE_COLUMN_TYPE_NUMERIC,
         .type_kind = MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_BIGINT,
         .storage_class = MYLITE_CREATE_TABLE_COLUMN_STORAGE_INTEGER},
        {.definition = "bo BOOLEAN",
         .name = "bo",
         .type = "BOOLEAN",
         .type_family = MYLITE_CREATE_TABLE_COLUMN_TYPE_NUMERIC,
         .type_kind = MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_BOOL,
         .storage_class = MYLITE_CREATE_TABLE_COLUMN_STORAGE_INTEGER},
        {.definition = "f FLOAT8",
         .name = "f",
         .type = "FLOAT8",
         .type_family = MYLITE_CREATE_TABLE_COLUMN_TYPE_NUMERIC,
         .type_kind = MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_DOUBLE,
         .storage_class = MYLITE_CREATE_TABLE_COLUMN_STORAGE_FLOAT},
        {.definition = "bt BIT(1)",
         .name = "bt",
         .type = "BIT(1)",
         .type_family = MYLITE_CREATE_TABLE_COLUMN_TYPE_NUMERIC,
         .type_kind = MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_BIT,
         .type_numeric_parameter_count = 1,
         .type_numeric_parameters = {1, 0},
         .storage_class = MYLITE_CREATE_TABLE_COLUMN_STORAGE_BIT},
        {.definition = "bn BINARY(2)",
         .name = "bn",
         .type = "BINARY(2)",
         .type_family = MYLITE_CREATE_TABLE_COLUMN_TYPE_STRING,
         .type_kind = MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_BINARY,
         .type_numeric_parameter_count = 1,
         .type_numeric_parameters = {2, 0},
         .storage_class = MYLITE_CREATE_TABLE_COLUMN_STORAGE_BINARY_STRING},
        {.definition = "tx MEDIUMTEXT",
         .name = "tx",
         .type = "MEDIUMTEXT",
         .type_family = MYLITE_CREATE_TABLE_COLUMN_TYPE_STRING,
         .type_kind = MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_MEDIUMTEXT,
         .storage_class = MYLITE_CREATE_TABLE_COLUMN_STORAGE_TEXT},
        {.definition = "dt DATETIME(6)",
         .name = "dt",
         .type = "DATETIME(6)",
         .type_family = MYLITE_CREATE_TABLE_COLUMN_TYPE_TEMPORAL,
         .type_kind = MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_DATETIME,
         .type_numeric_parameter_count = 1,
         .type_numeric_parameters = {6, 0},
         .storage_class = MYLITE_CREATE_TABLE_COLUMN_STORAGE_TEMPORAL},
        {.definition = "pt POINT",
         .name = "pt",
         .type = "POINT",
         .type_family = MYLITE_CREATE_TABLE_COLUMN_TYPE_SPATIAL,
         .type_kind = MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_POINT,
         .storage_class = MYLITE_CREATE_TABLE_COLUMN_STORAGE_SPATIAL},
        {.definition = "gc GEOMETRYCOLLECTION",
         .name = "gc",
         .type = "GEOMETRYCOLLECTION",
         .type_family = MYLITE_CREATE_TABLE_COLUMN_TYPE_SPATIAL,
         .type_kind = MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_GEOMETRYCOLLECTION,
         .storage_class = MYLITE_CREATE_TABLE_COLUMN_STORAGE_SPATIAL},
        {.definition = "ch CHAR(1)",
         .name = "ch",
         .type = "CHAR(1)",
         .type_family = MYLITE_CREATE_TABLE_COLUMN_TYPE_STRING,
         .type_kind = MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_CHAR,
         .type_numeric_parameter_count = 1,
         .type_numeric_parameters = {1, 0},
         .storage_class = MYLITE_CREATE_TABLE_COLUMN_STORAGE_FIXED_STRING},
        {.definition = "vb VARBINARY(2)",
         .name = "vb",
         .type = "VARBINARY(2)",
         .type_family = MYLITE_CREATE_TABLE_COLUMN_TYPE_STRING,
         .type_kind = MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_VARBINARY,
         .type_numeric_parameter_count = 1,
         .type_numeric_parameters = {2, 0},
         .storage_class = MYLITE_CREATE_TABLE_COLUMN_STORAGE_BINARY_STRING},
        {.definition = "js JSON",
         .name = "js",
         .type = "JSON",
         .type_family = MYLITE_CREATE_TABLE_COLUMN_TYPE_JSON,
         .type_kind = MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_JSON,
         .storage_class = MYLITE_CREATE_TABLE_COLUMN_STORAGE_JSON},
        {.definition = "st SET('x')",
         .name = "st",
         .type = "SET('x')",
         .type_family = MYLITE_CREATE_TABLE_COLUMN_TYPE_SET,
         .type_kind = MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_SET,
         .type_element_count = 1,
         .storage_class = MYLITE_CREATE_TABLE_COLUMN_STORAGE_SET},
        {.definition = "ve VECTOR(3)",
         .name = "ve",
         .type = "VECTOR(3)",
         .type_family = MYLITE_CREATE_TABLE_COLUMN_TYPE_STRING,
         .type_kind = MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_VECTOR,
         .type_numeric_parameter_count = 1,
         .type_numeric_parameters = {3, 0},
         .storage_class = MYLITE_CREATE_TABLE_COLUMN_STORAGE_VECTOR}};
    failures += expect_create_table_columns(
        "CREATE TABLE t (ti TINYINT, i8 INT8, bo BOOLEAN, f FLOAT8, "
        "bt BIT(1), bn BINARY(2), tx MEDIUMTEXT, dt DATETIME(6), "
        "pt POINT, gc GEOMETRYCOLLECTION, ch CHAR(1), vb VARBINARY(2), "
        "js JSON, st SET('x'), ve VECTOR(3))",
        columns, sizeof(columns) / sizeof(columns[0]));
  }
  {
    const ExpectedCreateTableColumn columns[] = {
        {.definition = "a INT DEFAULT 1",
         .name = "a",
         .type = "INT",
         .options = "DEFAULT 1",
         .type_family = MYLITE_CREATE_TABLE_COLUMN_TYPE_NUMERIC,
         .flags = MYLITE_CREATE_TABLE_COLUMN_FLAG_DEFAULT,
         .type_name = "INT",
         .default_span = "DEFAULT 1",
         .default_value = "1",
         .type_kind = MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_INT,
         .storage_class = MYLITE_CREATE_TABLE_COLUMN_STORAGE_INTEGER,
         .type_node_symbol = "nt_type",
         .options_node_symbol = "nt_column_option_list_opt",
         .default_node_symbol = "nt_column_option",
         .default_value_node_symbol = "nt_default_value_expr"},
        {.definition = "ts TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP",
         .name = "ts",
         .type = "TIMESTAMP",
         .options = "DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP",
         .type_family = MYLITE_CREATE_TABLE_COLUMN_TYPE_TEMPORAL,
         .flags = MYLITE_CREATE_TABLE_COLUMN_FLAG_DEFAULT |
                  MYLITE_CREATE_TABLE_COLUMN_FLAG_ON_UPDATE,
         .type_name = "TIMESTAMP",
         .default_span = "DEFAULT CURRENT_TIMESTAMP",
         .default_value = "CURRENT_TIMESTAMP",
         .on_update = "ON UPDATE CURRENT_TIMESTAMP",
         .on_update_value = "CURRENT_TIMESTAMP",
         .type_kind = MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_TIMESTAMP,
         .storage_class = MYLITE_CREATE_TABLE_COLUMN_STORAGE_TEMPORAL,
         .type_node_symbol = "nt_type",
         .options_node_symbol = "nt_column_option_list_opt",
         .default_value_node_symbol = "nt_default_value_expr",
         .on_update_value_node_symbol = "nt_now_sym_option_fraction"},
        {.definition = "g INT GENERATED ALWAYS AS (a + 1) STORED",
         .name = "g",
         .type = "INT",
         .options = "GENERATED ALWAYS AS (a + 1) STORED",
         .type_family = MYLITE_CREATE_TABLE_COLUMN_TYPE_NUMERIC,
         .flags = MYLITE_CREATE_TABLE_COLUMN_FLAG_GENERATED |
                  MYLITE_CREATE_TABLE_COLUMN_FLAG_STORED,
         .type_name = "INT",
         .generated = "GENERATED ALWAYS AS (a + 1) STORED",
         .generated_expression = "a + 1",
         .generated_storage = "STORED",
         .type_kind = MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_INT,
         .storage_class = MYLITE_CREATE_TABLE_COLUMN_STORAGE_INTEGER,
         .generated_expression_node_symbol = "nt_expression",
         .generated_storage_node_symbol = "nt_virtual_or_stored"},
        {.definition = "h INT AS (a + 2) VIRTUAL",
         .name = "h",
         .type = "INT",
         .options = "AS (a + 2) VIRTUAL",
         .type_family = MYLITE_CREATE_TABLE_COLUMN_TYPE_NUMERIC,
         .flags = MYLITE_CREATE_TABLE_COLUMN_FLAG_GENERATED |
                  MYLITE_CREATE_TABLE_COLUMN_FLAG_VIRTUAL,
         .type_name = "INT",
         .generated = "AS (a + 2) VIRTUAL",
         .generated_expression = "a + 2",
         .generated_storage = "VIRTUAL",
         .type_kind = MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_INT,
         .storage_class = MYLITE_CREATE_TABLE_COLUMN_STORAGE_INTEGER,
         .generated_expression_node_symbol = "nt_expression",
         .generated_storage_node_symbol = "nt_virtual_or_stored"},
        {.definition = "r INT REFERENCES parent(id)",
         .name = "r",
         .type = "INT",
         .options = "REFERENCES parent(id)",
         .type_family = MYLITE_CREATE_TABLE_COLUMN_TYPE_NUMERIC,
         .flags = MYLITE_CREATE_TABLE_COLUMN_FLAG_REFERENCES,
         .type_name = "INT",
         .reference = "REFERENCES parent(id)",
         .type_kind = MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_INT,
         .storage_class = MYLITE_CREATE_TABLE_COLUMN_STORAGE_INTEGER,
         .reference_node_symbol = "nt_refer_def"},
        {.definition = "c INT CHECK (c > 0) NOT ENFORCED",
         .name = "c",
         .type = "INT",
         .options = "CHECK (c > 0) NOT ENFORCED",
         .type_family = MYLITE_CREATE_TABLE_COLUMN_TYPE_NUMERIC,
         .flags = MYLITE_CREATE_TABLE_COLUMN_FLAG_CHECK,
         .type_name = "INT",
         .check_span = "CHECK (c > 0) NOT ENFORCED",
         .check_expression = "c > 0",
         .check_enforcement =
             MYLITE_CREATE_TABLE_CHECK_ENFORCEMENT_NOT_ENFORCED,
         .check_enforcement_span = "NOT ENFORCED",
         .type_kind = MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_INT,
         .storage_class = MYLITE_CREATE_TABLE_COLUMN_STORAGE_INTEGER,
         .check_expression_node_symbol = "nt_expression",
         .check_enforcement_node_symbol =
             "nt_enforced_or_not_or_not_null_opt"}};
    failures += expect_create_table_columns(
        "CREATE TABLE t (a INT DEFAULT 1, "
        "ts TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, "
        "g INT GENERATED ALWAYS AS (a + 1) STORED, "
        "h INT AS (a + 2) VIRTUAL, r INT REFERENCES parent(id), "
        "c INT CHECK (c > 0) NOT ENFORCED)",
        columns, sizeof(columns) / sizeof(columns[0]));
  }
  {
    const ExpectedCreateTableKeyPart pk_columns[] = {{"id", "id"}};
    const ExpectedCreateTableKeyPart slug_columns[] = {{"slug", "slug"}};
    const ExpectedCreateTableKeyPart id_slug_columns[] = {{"id", "id"},
                                                          {"slug", "slug"}};
    const ExpectedCreateTableKey keys[] = {
        {MYLITE_CREATE_TABLE_KEY_PRIMARY, "PRIMARY KEY (id)", NULL, NULL,
         pk_columns, sizeof(pk_columns) / sizeof(pk_columns[0]), NULL, NULL,
         NULL, NULL, 0},
        {MYLITE_CREATE_TABLE_KEY_INDEX, "KEY slug_idx (slug)", NULL,
         "slug_idx", slug_columns,
         sizeof(slug_columns) / sizeof(slug_columns[0]), NULL, NULL, NULL, NULL,
         0},
        {MYLITE_CREATE_TABLE_KEY_INDEX, "INDEX id_slug (id, slug)", NULL,
         "id_slug", id_slug_columns,
         sizeof(id_slug_columns) / sizeof(id_slug_columns[0]), NULL, NULL, NULL,
         NULL, 0}};
    failures += expect_create_table_keys(
        "CREATE TABLE t (id INT, slug VARCHAR(50), PRIMARY KEY (id), "
        "KEY slug_idx (slug), INDEX id_slug (id, slug))",
        keys, sizeof(keys) / sizeof(keys[0]));
  }
  {
    const ExpectedCreateTableKeyPart email_columns[] = {{"email", "email"}};
    const ExpectedCreateTableKey keys[] = {
        {MYLITE_CREATE_TABLE_KEY_UNIQUE, "UNIQUE KEY email_uq (email)", NULL,
         "email_uq", email_columns,
         sizeof(email_columns) / sizeof(email_columns[0]), NULL, NULL, NULL,
         NULL, 0},
        {MYLITE_CREATE_TABLE_KEY_FULLTEXT, "FULLTEXT KEY ft_email (email)",
         NULL, "ft_email", email_columns,
         sizeof(email_columns) / sizeof(email_columns[0]), NULL, NULL, NULL,
         NULL, 0},
        {MYLITE_CREATE_TABLE_KEY_SPATIAL, "SPATIAL KEY sp (email)", NULL,
         "sp", email_columns, sizeof(email_columns) / sizeof(email_columns[0]),
         NULL, NULL, NULL, NULL, 0}};
    failures += expect_create_table_keys(
        "CREATE TABLE t (id INT, email VARCHAR(100), "
        "UNIQUE KEY email_uq (email), FULLTEXT KEY ft_email (email), "
        "SPATIAL KEY sp (email))",
        keys, sizeof(keys) / sizeof(keys[0]));
  }
  {
    const ExpectedCreateTableKeyPart local_columns[] = {{"parent_id",
                                                         "parent_id"}};
    const ExpectedCreateTableKeyPart referenced_columns[] = {{"id", "id"}};
    const ExpectedCreateTableKey keys[] = {
        {MYLITE_CREATE_TABLE_KEY_FOREIGN,
         "CONSTRAINT fk_parent FOREIGN KEY fk_idx (parent_id) REFERENCES "
         "parent_db.parent(id)",
         "fk_parent", "fk_idx", local_columns,
         sizeof(local_columns) / sizeof(local_columns[0]), "parent_db.parent",
         "parent_db", "parent", referenced_columns,
         sizeof(referenced_columns) / sizeof(referenced_columns[0])},
        {MYLITE_CREATE_TABLE_KEY_CHECK, "CHECK (parent_id > 0)", NULL, NULL,
         NULL, 0, NULL, NULL, NULL, NULL, 0}};
    failures += expect_create_table_keys(
        "CREATE TABLE child (parent_id INT, CONSTRAINT fk_parent FOREIGN KEY "
        "fk_idx (parent_id) REFERENCES parent_db.parent(id), CHECK (parent_id "
        "> 0))",
        keys, sizeof(keys) / sizeof(keys[0]));
  }
  {
    const ExpectedCreateTableKeyPart ab_columns[] = {{"a", "a"}, {"b", "b"}};
    const ExpectedCreateTableKeyPart a_columns[] = {{"a", "a"}};
    const ExpectedCreateTableKey keys[] = {
        {MYLITE_CREATE_TABLE_KEY_UNIQUE,
         "CONSTRAINT uq_ab UNIQUE INDEX uq_ab USING BTREE (a, b)", "uq_ab",
         "uq_ab", ab_columns, sizeof(ab_columns) / sizeof(ab_columns[0]), NULL,
         NULL, NULL, NULL, 0},
        {MYLITE_CREATE_TABLE_KEY_PRIMARY, "CONSTRAINT pk PRIMARY KEY (a)", "pk",
         NULL, a_columns, sizeof(a_columns) / sizeof(a_columns[0]), NULL, NULL,
         NULL, NULL, 0}};
    failures += expect_create_table_keys(
        "CREATE TABLE t (a INT, b INT, CONSTRAINT uq_ab UNIQUE INDEX uq_ab "
        "USING BTREE (a, b), CONSTRAINT pk PRIMARY KEY (a))",
        keys, sizeof(keys) / sizeof(keys[0]));
  }
  {
    const ExpectedCreateTableKeyPart columns[] = {
        {"a(10) DESC", "a", MYLITE_CREATE_TABLE_KEY_PART_COLUMN, NULL,
         "(10)", "10", MYLITE_CREATE_TABLE_KEY_PART_ORDER_DESC, "DESC"},
        {"b ASC", "b", MYLITE_CREATE_TABLE_KEY_PART_COLUMN, NULL, NULL, NULL,
         MYLITE_CREATE_TABLE_KEY_PART_ORDER_ASC, "ASC"},
        {"((ABS(b))) DESC", NULL, MYLITE_CREATE_TABLE_KEY_PART_EXPRESSION,
         "(ABS(b))", NULL, NULL, MYLITE_CREATE_TABLE_KEY_PART_ORDER_DESC,
         "DESC"}};
    const ExpectedCreateTableKey keys[] = {{
        .kind = MYLITE_CREATE_TABLE_KEY_INDEX,
        .definition = "KEY k (a(10) DESC, b ASC, ((ABS(b))) DESC)",
        .name = "k",
        .columns = columns,
        .column_count = sizeof(columns) / sizeof(columns[0]),
    }};
    failures += expect_create_table_keys(
        "CREATE TABLE t (a VARCHAR(20), b INT, KEY k (a(10) DESC, b ASC, "
        "((ABS(b))) DESC))",
        keys, sizeof(keys) / sizeof(keys[0]));
  }
  {
    const ExpectedCreateTableKeyPart columns[] = {{"a", "a"}};
    const ExpectedCreateTableKeyOption options[] = {
        {MYLITE_CREATE_TABLE_KEY_OPTION_INDEX_TYPE, "USING BTREE", "USING",
         "BTREE"},
        {MYLITE_CREATE_TABLE_KEY_OPTION_COMMENT, "COMMENT 'idx'", "COMMENT",
         "'idx'"},
        {MYLITE_CREATE_TABLE_KEY_OPTION_KEY_BLOCK_SIZE, "KEY_BLOCK_SIZE=8",
         "KEY_BLOCK_SIZE", "8"},
        {MYLITE_CREATE_TABLE_KEY_OPTION_WITH_PARSER, "WITH PARSER ngram",
         "WITH PARSER", "ngram"},
        {MYLITE_CREATE_TABLE_KEY_OPTION_VISIBLE, "VISIBLE", "VISIBLE", NULL}};
    const ExpectedCreateTableKey keys[] = {{
        .kind = MYLITE_CREATE_TABLE_KEY_INDEX,
        .definition =
            "KEY k (a) USING BTREE COMMENT 'idx' KEY_BLOCK_SIZE=8 WITH PARSER "
            "ngram VISIBLE",
        .name = "k",
        .columns = columns,
        .column_count = sizeof(columns) / sizeof(columns[0]),
        .index_type = "USING BTREE",
        .options = options,
        .option_count = sizeof(options) / sizeof(options[0]),
    }};
    failures += expect_create_table_keys(
        "CREATE TABLE t (a INT, KEY k (a) USING BTREE COMMENT 'idx' "
        "KEY_BLOCK_SIZE=8 WITH PARSER ngram VISIBLE)",
        keys, sizeof(keys) / sizeof(keys[0]));
  }
  {
    const ExpectedCreateTableKeyPart local_columns[] = {{"pid", "pid"}};
    const ExpectedCreateTableKeyPart referenced_columns[] = {{"id", "id"}};
    const ExpectedCreateTableKey keys[] = {
        {.kind = MYLITE_CREATE_TABLE_KEY_FOREIGN,
         .definition = "CONSTRAINT fk FOREIGN KEY (pid) REFERENCES parent(id) "
                       "MATCH FULL ON DELETE CASCADE ON UPDATE SET NULL",
         .constraint_name = "fk",
         .columns = local_columns,
         .column_count = sizeof(local_columns) / sizeof(local_columns[0]),
         .referenced_table = "parent",
         .referenced_name = "parent",
         .referenced_columns = referenced_columns,
         .referenced_column_count =
             sizeof(referenced_columns) / sizeof(referenced_columns[0]),
         .foreign_match = MYLITE_CREATE_TABLE_FOREIGN_MATCH_FULL,
         .foreign_match_span = "MATCH FULL",
         .foreign_on_delete = MYLITE_CREATE_TABLE_FOREIGN_ACTION_CASCADE,
         .foreign_on_delete_span = "ON DELETE CASCADE",
         .foreign_on_update = MYLITE_CREATE_TABLE_FOREIGN_ACTION_SET_NULL,
         .foreign_on_update_span = "ON UPDATE SET NULL"},
        {.kind = MYLITE_CREATE_TABLE_KEY_CHECK,
         .definition = "CONSTRAINT ck CHECK (pid > 0) NOT ENFORCED",
         .constraint_name = "ck",
         .check_expression = "pid > 0",
         .check_enforcement =
             MYLITE_CREATE_TABLE_CHECK_ENFORCEMENT_NOT_ENFORCED,
         .check_enforcement_span = "NOT ENFORCED"}};
    failures += expect_create_table_keys(
        "CREATE TABLE child (pid INT, CONSTRAINT fk FOREIGN KEY (pid) "
        "REFERENCES parent(id) MATCH FULL ON DELETE CASCADE ON UPDATE SET "
        "NULL, CONSTRAINT ck CHECK (pid > 0) NOT ENFORCED)",
        keys, sizeof(keys) / sizeof(keys[0]));
  }
  {
    const ExpectedCreateTableOption options[] = {
        {MYLITE_CREATE_TABLE_OPTION_ENGINE, "ENGINE=InnoDB", "ENGINE",
         "InnoDB"},
        {MYLITE_CREATE_TABLE_OPTION_CHARSET, "DEFAULT CHARSET=utf8mb4",
         "CHARSET", "utf8mb4"},
        {MYLITE_CREATE_TABLE_OPTION_COLLATE, "COLLATE=utf8mb4_unicode_ci",
         "COLLATE", "utf8mb4_unicode_ci"},
        {MYLITE_CREATE_TABLE_OPTION_AUTO_INCREMENT, "AUTO_INCREMENT=10",
         "AUTO_INCREMENT", "10"},
        {MYLITE_CREATE_TABLE_OPTION_COMMENT, "COMMENT='hello'", "COMMENT",
         "'hello'"},
        {MYLITE_CREATE_TABLE_OPTION_ROW_FORMAT, "ROW_FORMAT=DYNAMIC",
         "ROW_FORMAT", "DYNAMIC"}};
    failures += expect_create_table_options(
        "CREATE TABLE t (id INT) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 "
        "COLLATE=utf8mb4_unicode_ci AUTO_INCREMENT=10 COMMENT='hello' "
        "ROW_FORMAT=DYNAMIC",
        options, sizeof(options) / sizeof(options[0]));
  }
  {
    const ExpectedCreateTableOption options[] = {
        {MYLITE_CREATE_TABLE_OPTION_CHARSET, "CHARACTER SET latin1",
         "CHARACTER SET", "latin1"},
        {MYLITE_CREATE_TABLE_OPTION_ENCRYPTION, "ENCRYPTION='Y'",
         "ENCRYPTION", "'Y'"},
        {MYLITE_CREATE_TABLE_OPTION_STATS_PERSISTENT, "STATS_PERSISTENT=0",
         "STATS_PERSISTENT", "0"},
        {MYLITE_CREATE_TABLE_OPTION_PACK_KEYS, "PACK_KEYS=1", "PACK_KEYS",
         "1"},
        {MYLITE_CREATE_TABLE_OPTION_TABLESPACE, "TABLESPACE ts", "TABLESPACE",
         "ts"},
        {MYLITE_CREATE_TABLE_OPTION_STORAGE, "STORAGE DISK", "STORAGE",
         "DISK"}};
    failures += expect_create_table_options(
        "CREATE TABLE t (id INT) CHARACTER SET latin1 ENCRYPTION='Y' "
        "STATS_PERSISTENT=0 PACK_KEYS=1 TABLESPACE ts STORAGE DISK",
        options, sizeof(options) / sizeof(options[0]));
  }
  {
    const ExpectedCreateTableOption options[] = {
        {MYLITE_CREATE_TABLE_OPTION_KEY_BLOCK_SIZE, "KEY_BLOCK_SIZE=8",
         "KEY_BLOCK_SIZE", "8"},
        {MYLITE_CREATE_TABLE_OPTION_AUTOEXTEND_SIZE, "AUTOEXTEND_SIZE=4M",
         "AUTOEXTEND_SIZE", "4M"},
        {MYLITE_CREATE_TABLE_OPTION_AVG_ROW_LENGTH, "AVG_ROW_LENGTH=100",
         "AVG_ROW_LENGTH", "100"},
        {MYLITE_CREATE_TABLE_OPTION_MAX_ROWS, "MAX_ROWS=1000", "MAX_ROWS",
         "1000"},
        {MYLITE_CREATE_TABLE_OPTION_MIN_ROWS, "MIN_ROWS=1", "MIN_ROWS", "1"},
        {MYLITE_CREATE_TABLE_OPTION_DELAY_KEY_WRITE, "DELAY_KEY_WRITE=1",
         "DELAY_KEY_WRITE", "1"}};
    failures += expect_create_table_options(
        "CREATE TABLE t (id INT) KEY_BLOCK_SIZE=8 AUTOEXTEND_SIZE=4M "
        "AVG_ROW_LENGTH=100 MAX_ROWS=1000 MIN_ROWS=1 DELAY_KEY_WRITE=1",
        options, sizeof(options) / sizeof(options[0]));
  }
  {
    const ExpectedCreateTableOption options[] = {
        {MYLITE_CREATE_TABLE_OPTION_DATA_DIRECTORY, "DATA DIRECTORY='data'",
         "DATA DIRECTORY", "'data'"},
        {MYLITE_CREATE_TABLE_OPTION_INDEX_DIRECTORY, "INDEX DIRECTORY='idx'",
         "INDEX DIRECTORY", "'idx'"},
        {MYLITE_CREATE_TABLE_OPTION_SECONDARY_ENGINE, "SECONDARY_ENGINE=NULL",
         "SECONDARY_ENGINE", "NULL"},
        {MYLITE_CREATE_TABLE_OPTION_ENGINE_ATTRIBUTE, "ENGINE_ATTRIBUTE='{}'",
         "ENGINE_ATTRIBUTE", "'{}'"},
        {MYLITE_CREATE_TABLE_OPTION_SECONDARY_ENGINE_ATTRIBUTE,
         "SECONDARY_ENGINE_ATTRIBUTE='{}'", "SECONDARY_ENGINE_ATTRIBUTE",
         "'{}'"}};
    failures += expect_create_table_options(
        "CREATE TABLE t (id INT) DATA DIRECTORY='data' INDEX DIRECTORY='idx' "
        "SECONDARY_ENGINE=NULL ENGINE_ATTRIBUTE='{}' "
        "SECONDARY_ENGINE_ATTRIBUTE='{}'",
        options, sizeof(options) / sizeof(options[0]));
  }
  {
    const ExpectedCreateTableOption options[] = {
        {MYLITE_CREATE_TABLE_OPTION_CONNECTION, "CONNECTION='conn'",
         "CONNECTION", "'conn'"},
        {MYLITE_CREATE_TABLE_OPTION_PASSWORD, "PASSWORD='pwd'", "PASSWORD",
         "'pwd'"},
        {MYLITE_CREATE_TABLE_OPTION_COMPRESSION, "COMPRESSION='zlib'",
         "COMPRESSION", "'zlib'"},
        {MYLITE_CREATE_TABLE_OPTION_INSERT_METHOD, "INSERT_METHOD=LAST",
         "INSERT_METHOD", "LAST"},
        {MYLITE_CREATE_TABLE_OPTION_UNION, "UNION=(t1,t2)", "UNION",
         "(t1,t2)"}};
    failures += expect_create_table_options(
        "CREATE TABLE t (id INT) CONNECTION='conn' PASSWORD='pwd' "
        "COMPRESSION='zlib' INSERT_METHOD=LAST UNION=(t1,t2)",
        options, sizeof(options) / sizeof(options[0]));
  }

  return failures == 0 ? 0 : 1;
}

static int expect_parse_ok(const char *sql) {
  MyliteParseResult result;
  MyliteParseStatus status = mylite_parse_sql(sql, &result);
  if (status == MYLITE_PARSE_OK) {
    return 0;
  }

  fprintf(stderr, "parse failed: %s\nstatus=%s offset=%zu token=%d message=%s\n",
          sql, mylite_parse_status_name(status), result.offset, result.token,
          result.message);
  return 1;
}

static int expect_ast_ok(const char *sql, const char *root_symbol) {
  MyliteParseResult result;
  MyliteAst *ast = NULL;
  MyliteParseStatus status = mylite_parse_sql_ast(sql, &ast, &result);
  if (status != MYLITE_PARSE_OK) {
    fprintf(stderr, "AST parse failed: %s\nstatus=%s offset=%zu token=%d message=%s\n",
            sql, mylite_parse_status_name(status), result.offset, result.token,
            result.message);
    return 1;
  }

  const MyliteAstNode *root = mylite_ast_root(ast);
  int failed = 0;
  if (root == NULL || strcmp(mylite_ast_node_symbol_name(root), root_symbol) != 0 ||
      mylite_ast_node_count(ast) == 0 ||
      mylite_ast_node_end(root) > strlen(sql)) {
    fprintf(stderr,
            "AST shape failed: %s\nroot=%s nodes=%zu span=%zu..%zu bytes=%zu\n",
            sql, root == NULL ? "<null>" : mylite_ast_node_symbol_name(root),
            mylite_ast_node_count(ast),
            root == NULL ? 0 : mylite_ast_node_start(root),
            root == NULL ? 0 : mylite_ast_node_end(root),
            mylite_ast_allocated_bytes(ast));
    failed = 1;
  }

  mylite_ast_free(ast);
  return failed;
}

static int expect_ast_target(const char *sql, MyliteStatementKind statement_kind,
                             MyliteStatementTargetKind target_kind,
                             const char *target, const char *schema,
                             const char *name) {
  MyliteParseResult result;
  MyliteAst *ast = NULL;
  MyliteParseStatus status = mylite_parse_sql_ast(sql, &ast, &result);
  if (status != MYLITE_PARSE_OK) {
    fprintf(stderr,
            "AST target parse failed: %s\nstatus=%s offset=%zu token=%d message=%s\n",
            sql, mylite_parse_status_name(status), result.offset, result.token,
            result.message);
    return 1;
  }

  int failed = 0;
  if (mylite_ast_statement_count(ast) != 1 ||
      mylite_ast_statement_kind(ast, 0) != statement_kind ||
      mylite_ast_statement_target_count(ast, 0) != 1 ||
      mylite_ast_statement_target_kind(ast, 0) != target_kind ||
      mylite_ast_statement_target_role_at(ast, 0, 0) !=
          MYLITE_STATEMENT_TARGET_ROLE_PRIMARY ||
      mylite_ast_statement_target_kind_at(ast, 0, 0) != target_kind ||
      !span_matches(sql, mylite_ast_statement_target_start(ast, 0),
                    mylite_ast_statement_target_end(ast, 0), target) ||
      !span_matches(sql, mylite_ast_statement_target_schema_start(ast, 0),
                    mylite_ast_statement_target_schema_end(ast, 0), schema) ||
      !span_matches(sql, mylite_ast_statement_target_name_start(ast, 0),
                    mylite_ast_statement_target_name_end(ast, 0), name)) {
    fprintf(stderr,
            "AST target failed: %s\nkind=%s target_kind=%s target=%zu..%zu "
            "schema=%zu..%zu name=%zu..%zu target_count=%zu\n",
            sql, mylite_statement_kind_name(mylite_ast_statement_kind(ast, 0)),
            mylite_statement_target_kind_name(mylite_ast_statement_target_kind(ast, 0)),
            mylite_ast_statement_target_start(ast, 0),
            mylite_ast_statement_target_end(ast, 0),
            mylite_ast_statement_target_schema_start(ast, 0),
            mylite_ast_statement_target_schema_end(ast, 0),
            mylite_ast_statement_target_name_start(ast, 0),
            mylite_ast_statement_target_name_end(ast, 0),
            mylite_ast_statement_target_count(ast, 0));
    failed = 1;
  }

  mylite_ast_free(ast);
  return failed;
}

static int expect_ast_targets(const char *sql, MyliteStatementKind statement_kind,
                              const ExpectedAstTarget *targets,
                              size_t target_count) {
  MyliteParseResult result;
  MyliteAst *ast = NULL;
  MyliteParseStatus status = mylite_parse_sql_ast(sql, &ast, &result);
  if (status != MYLITE_PARSE_OK) {
    fprintf(stderr,
            "AST targets parse failed: %s\nstatus=%s offset=%zu token=%d message=%s\n",
            sql, mylite_parse_status_name(status), result.offset, result.token,
            result.message);
    return 1;
  }

  int failed = 0;
  if (mylite_ast_statement_count(ast) != 1 ||
      mylite_ast_statement_kind(ast, 0) != statement_kind ||
      mylite_ast_statement_target_count(ast, 0) != target_count) {
    fprintf(stderr,
            "AST targets header failed: %s\nkind=%s target_count=%zu\n",
            sql, mylite_statement_kind_name(mylite_ast_statement_kind(ast, 0)),
            mylite_ast_statement_target_count(ast, 0));
    failed = 1;
  }

  size_t actual_count = mylite_ast_statement_target_count(ast, 0);
  for (size_t i = 0; i < target_count && i < actual_count; i++) {
    if (mylite_ast_statement_target_role_at(ast, 0, i) != targets[i].role ||
        mylite_ast_statement_target_kind_at(ast, 0, i) != targets[i].kind ||
        !span_matches(sql, mylite_ast_statement_target_start_at(ast, 0, i),
                      mylite_ast_statement_target_end_at(ast, 0, i),
                      targets[i].target) ||
        !span_matches(sql, mylite_ast_statement_target_schema_start_at(ast, 0, i),
                      mylite_ast_statement_target_schema_end_at(ast, 0, i),
                      targets[i].schema) ||
        !span_matches(sql, mylite_ast_statement_target_name_start_at(ast, 0, i),
                      mylite_ast_statement_target_name_end_at(ast, 0, i),
                      targets[i].name)) {
      fprintf(stderr,
              "AST target[%zu] failed: %s\nrole=%s kind=%s target=%zu..%zu "
              "schema=%zu..%zu name=%zu..%zu\n",
              i, sql,
              mylite_statement_target_role_name(
                  mylite_ast_statement_target_role_at(ast, 0, i)),
              mylite_statement_target_kind_name(
                  mylite_ast_statement_target_kind_at(ast, 0, i)),
              mylite_ast_statement_target_start_at(ast, 0, i),
              mylite_ast_statement_target_end_at(ast, 0, i),
              mylite_ast_statement_target_schema_start_at(ast, 0, i),
              mylite_ast_statement_target_schema_end_at(ast, 0, i),
              mylite_ast_statement_target_name_start_at(ast, 0, i),
              mylite_ast_statement_target_name_end_at(ast, 0, i));
      failed = 1;
    }
  }

  mylite_ast_free(ast);
  return failed;
}

static int expect_create_table_columns(const char *sql,
                                       const ExpectedCreateTableColumn *columns,
                                       size_t column_count) {
  MyliteParseResult result;
  MyliteAst *ast = NULL;
  MyliteParseStatus status = mylite_parse_sql_ast(sql, &ast, &result);
  if (status != MYLITE_PARSE_OK) {
    fprintf(stderr,
            "CREATE TABLE column parse failed: %s\nstatus=%s offset=%zu token=%d "
            "message=%s\n",
            sql, mylite_parse_status_name(status), result.offset, result.token,
            result.message);
    return 1;
  }

  int failed = 0;
  if (mylite_ast_statement_count(ast) != 1 ||
      mylite_ast_statement_kind(ast, 0) != MYLITE_STATEMENT_CREATE ||
      mylite_ast_create_table_column_count(ast, 0) != column_count) {
    fprintf(stderr,
            "CREATE TABLE column header failed: %s\nkind=%s column_count=%zu\n",
            sql, mylite_statement_kind_name(mylite_ast_statement_kind(ast, 0)),
            mylite_ast_create_table_column_count(ast, 0));
    failed = 1;
  }

  size_t actual_count = mylite_ast_create_table_column_count(ast, 0);
  for (size_t i = 0; i < column_count && i < actual_count; i++) {
    if (!span_matches(sql, mylite_ast_create_table_column_start(ast, 0, i),
                      mylite_ast_create_table_column_end(ast, 0, i),
                      columns[i].definition) ||
        !span_matches(sql, mylite_ast_create_table_column_name_start(ast, 0, i),
                      mylite_ast_create_table_column_name_end(ast, 0, i),
                      columns[i].name) ||
        !span_matches(sql, mylite_ast_create_table_column_type_start(ast, 0, i),
                      mylite_ast_create_table_column_type_end(ast, 0, i),
                      columns[i].type) ||
        !span_matches(sql, mylite_ast_create_table_column_options_start(ast, 0, i),
                      mylite_ast_create_table_column_options_end(ast, 0, i),
                      columns[i].options) ||
        mylite_ast_create_table_column_type_family(ast, 0, i) !=
            columns[i].type_family ||
        (columns[i].type_kind != MYLITE_CREATE_TABLE_COLUMN_TYPE_KIND_UNKNOWN &&
         mylite_ast_create_table_column_type_kind(ast, 0, i) !=
             columns[i].type_kind) ||
        (columns[i].storage_class != MYLITE_CREATE_TABLE_COLUMN_STORAGE_UNKNOWN &&
         mylite_ast_create_table_column_storage_class(ast, 0, i) !=
             columns[i].storage_class) ||
        !node_symbol_matches_when_expected(
            mylite_ast_create_table_column_type_node(ast, 0, i),
            columns[i].type_node_symbol) ||
        !node_symbol_matches_when_expected(
            mylite_ast_create_table_column_options_node(ast, 0, i),
            columns[i].options_node_symbol) ||
        !node_symbol_matches_when_expected(
            mylite_ast_create_table_column_default_node(ast, 0, i),
            columns[i].default_node_symbol) ||
        !node_symbol_matches_when_expected(
            mylite_ast_create_table_column_default_value_node(ast, 0, i),
            columns[i].default_value_node_symbol) ||
        !node_symbol_matches_when_expected(
            mylite_ast_create_table_column_on_update_value_node(ast, 0, i),
            columns[i].on_update_value_node_symbol) ||
        !node_symbol_matches_when_expected(
            mylite_ast_create_table_column_generated_expression_node(ast, 0, i),
            columns[i].generated_expression_node_symbol) ||
        !node_symbol_matches_when_expected(
            mylite_ast_create_table_column_generated_storage_node(ast, 0, i),
            columns[i].generated_storage_node_symbol) ||
        !node_symbol_matches_when_expected(
            mylite_ast_create_table_column_check_expression_node(ast, 0, i),
            columns[i].check_expression_node_symbol) ||
        !node_symbol_matches_when_expected(
            mylite_ast_create_table_column_check_enforcement_node(ast, 0, i),
            columns[i].check_enforcement_node_symbol) ||
        !node_symbol_matches_when_expected(
            mylite_ast_create_table_column_reference_node(ast, 0, i),
            columns[i].reference_node_symbol) ||
        mylite_ast_create_table_column_flags(ast, 0, i) != columns[i].flags ||
        !span_matches_when_expected(
            sql, mylite_ast_create_table_column_type_name_start(ast, 0, i),
            mylite_ast_create_table_column_type_name_end(ast, 0, i),
            columns[i].type_name) ||
        !span_matches_when_expected(
            sql, mylite_ast_create_table_column_type_parameters_start(ast, 0, i),
            mylite_ast_create_table_column_type_parameters_end(ast, 0, i),
            columns[i].type_parameters) ||
        (columns[i].type_numeric_parameter_count > 0 &&
         (mylite_ast_create_table_column_type_numeric_parameter_count(ast, 0, i) !=
              columns[i].type_numeric_parameter_count ||
          mylite_ast_create_table_column_type_numeric_parameter_at(ast, 0, i, 0) !=
              columns[i].type_numeric_parameters[0] ||
          mylite_ast_create_table_column_type_numeric_parameter_at(ast, 0, i, 1) !=
              columns[i].type_numeric_parameters[1])) ||
        (columns[i].type_element_count > 0 &&
         mylite_ast_create_table_column_type_element_count(ast, 0, i) !=
             columns[i].type_element_count) ||
        !span_matches_when_expected(
            sql, mylite_ast_create_table_column_type_attributes_start(ast, 0, i),
            mylite_ast_create_table_column_type_attributes_end(ast, 0, i),
            columns[i].type_attributes) ||
        !span_matches_when_expected(
            sql, mylite_ast_create_table_column_default_start(ast, 0, i),
            mylite_ast_create_table_column_default_end(ast, 0, i),
            columns[i].default_span) ||
        !span_matches_when_expected(
            sql, mylite_ast_create_table_column_default_value_start(ast, 0, i),
            mylite_ast_create_table_column_default_value_end(ast, 0, i),
            columns[i].default_value) ||
        !span_matches_when_expected(
            sql, mylite_ast_create_table_column_on_update_start(ast, 0, i),
            mylite_ast_create_table_column_on_update_end(ast, 0, i),
            columns[i].on_update) ||
        !span_matches_when_expected(
            sql, mylite_ast_create_table_column_on_update_value_start(ast, 0, i),
            mylite_ast_create_table_column_on_update_value_end(ast, 0, i),
            columns[i].on_update_value) ||
        !span_matches_when_expected(
            sql, mylite_ast_create_table_column_generated_start(ast, 0, i),
            mylite_ast_create_table_column_generated_end(ast, 0, i),
            columns[i].generated) ||
        !span_matches_when_expected(
            sql,
            mylite_ast_create_table_column_generated_expression_start(ast, 0, i),
            mylite_ast_create_table_column_generated_expression_end(ast, 0, i),
            columns[i].generated_expression) ||
        !span_matches_when_expected(
            sql,
            mylite_ast_create_table_column_generated_storage_start(ast, 0, i),
            mylite_ast_create_table_column_generated_storage_end(ast, 0, i),
            columns[i].generated_storage) ||
        !span_matches_when_expected(
            sql, mylite_ast_create_table_column_comment_start(ast, 0, i),
            mylite_ast_create_table_column_comment_end(ast, 0, i),
            columns[i].comment) ||
        !span_matches_when_expected(
            sql, mylite_ast_create_table_column_comment_value_start(ast, 0, i),
            mylite_ast_create_table_column_comment_value_end(ast, 0, i),
            columns[i].comment_value) ||
        !span_matches_when_expected(
            sql, mylite_ast_create_table_column_check_start(ast, 0, i),
            mylite_ast_create_table_column_check_end(ast, 0, i),
            columns[i].check_span) ||
        !span_matches_when_expected(
            sql, mylite_ast_create_table_column_check_expression_start(ast, 0, i),
            mylite_ast_create_table_column_check_expression_end(ast, 0, i),
            columns[i].check_expression) ||
        (columns[i].check_enforcement !=
             MYLITE_CREATE_TABLE_CHECK_ENFORCEMENT_UNSPECIFIED &&
         mylite_ast_create_table_column_check_enforcement(ast, 0, i) !=
             columns[i].check_enforcement) ||
        !span_matches_when_expected(
            sql,
            mylite_ast_create_table_column_check_enforcement_start(ast, 0, i),
            mylite_ast_create_table_column_check_enforcement_end(ast, 0, i),
            columns[i].check_enforcement_span) ||
        !span_matches_when_expected(
            sql, mylite_ast_create_table_column_reference_start(ast, 0, i),
            mylite_ast_create_table_column_reference_end(ast, 0, i),
            columns[i].reference)) {
      fprintf(stderr,
              "CREATE TABLE column[%zu] failed: %s\ndef=%zu..%zu name=%zu..%zu "
              "type=%zu..%zu options=%zu..%zu family=%s kind=%s storage=%s "
              "flags=0x%x\n"
              "type_name=%zu..%zu type_params=%zu..%zu "
              "type_numeric_params=%zu:%llu,%llu type_elements=%zu "
              "type_attrs=%zu..%zu "
              "default=%zu..%zu default_value=%zu..%zu on_update=%zu..%zu "
              "on_update_value=%zu..%zu generated=%zu..%zu "
              "generated_expr=%zu..%zu generated_storage=%zu..%zu "
              "comment=%zu..%zu comment_value=%zu..%zu check=%zu..%zu "
              "check_expr=%zu..%zu check_enforced=%s:%zu..%zu "
              "reference=%zu..%zu\n",
              i, sql, mylite_ast_create_table_column_start(ast, 0, i),
              mylite_ast_create_table_column_end(ast, 0, i),
              mylite_ast_create_table_column_name_start(ast, 0, i),
              mylite_ast_create_table_column_name_end(ast, 0, i),
              mylite_ast_create_table_column_type_start(ast, 0, i),
              mylite_ast_create_table_column_type_end(ast, 0, i),
              mylite_ast_create_table_column_options_start(ast, 0, i),
              mylite_ast_create_table_column_options_end(ast, 0, i),
              mylite_create_table_column_type_family_name(
                  mylite_ast_create_table_column_type_family(ast, 0, i)),
              mylite_create_table_column_type_kind_name(
                  mylite_ast_create_table_column_type_kind(ast, 0, i)),
              mylite_create_table_column_storage_class_name(
                  mylite_ast_create_table_column_storage_class(ast, 0, i)),
              mylite_ast_create_table_column_flags(ast, 0, i),
              mylite_ast_create_table_column_type_name_start(ast, 0, i),
              mylite_ast_create_table_column_type_name_end(ast, 0, i),
              mylite_ast_create_table_column_type_parameters_start(ast, 0, i),
              mylite_ast_create_table_column_type_parameters_end(ast, 0, i),
              mylite_ast_create_table_column_type_numeric_parameter_count(ast, 0,
                                                                          i),
              mylite_ast_create_table_column_type_numeric_parameter_at(ast, 0, i,
                                                                       0),
              mylite_ast_create_table_column_type_numeric_parameter_at(ast, 0, i,
                                                                       1),
              mylite_ast_create_table_column_type_element_count(ast, 0, i),
              mylite_ast_create_table_column_type_attributes_start(ast, 0, i),
              mylite_ast_create_table_column_type_attributes_end(ast, 0, i),
              mylite_ast_create_table_column_default_start(ast, 0, i),
              mylite_ast_create_table_column_default_end(ast, 0, i),
              mylite_ast_create_table_column_default_value_start(ast, 0, i),
              mylite_ast_create_table_column_default_value_end(ast, 0, i),
              mylite_ast_create_table_column_on_update_start(ast, 0, i),
              mylite_ast_create_table_column_on_update_end(ast, 0, i),
              mylite_ast_create_table_column_on_update_value_start(ast, 0, i),
              mylite_ast_create_table_column_on_update_value_end(ast, 0, i),
              mylite_ast_create_table_column_generated_start(ast, 0, i),
              mylite_ast_create_table_column_generated_end(ast, 0, i),
              mylite_ast_create_table_column_generated_expression_start(ast, 0, i),
              mylite_ast_create_table_column_generated_expression_end(ast, 0, i),
              mylite_ast_create_table_column_generated_storage_start(ast, 0, i),
              mylite_ast_create_table_column_generated_storage_end(ast, 0, i),
              mylite_ast_create_table_column_comment_start(ast, 0, i),
              mylite_ast_create_table_column_comment_end(ast, 0, i),
              mylite_ast_create_table_column_comment_value_start(ast, 0, i),
              mylite_ast_create_table_column_comment_value_end(ast, 0, i),
              mylite_ast_create_table_column_check_start(ast, 0, i),
              mylite_ast_create_table_column_check_end(ast, 0, i),
              mylite_ast_create_table_column_check_expression_start(ast, 0, i),
              mylite_ast_create_table_column_check_expression_end(ast, 0, i),
              mylite_create_table_check_enforcement_name(
                  mylite_ast_create_table_column_check_enforcement(ast, 0, i)),
              mylite_ast_create_table_column_check_enforcement_start(ast, 0, i),
              mylite_ast_create_table_column_check_enforcement_end(ast, 0, i),
              mylite_ast_create_table_column_reference_start(ast, 0, i),
              mylite_ast_create_table_column_reference_end(ast, 0, i));
      failed = 1;
    }
  }

  mylite_ast_free(ast);
  return failed;
}

static int expect_create_table_keys(const char *sql,
                                    const ExpectedCreateTableKey *keys,
                                    size_t key_count) {
  MyliteParseResult result;
  MyliteAst *ast = NULL;
  MyliteParseStatus status = mylite_parse_sql_ast(sql, &ast, &result);
  if (status != MYLITE_PARSE_OK) {
    fprintf(stderr,
            "CREATE TABLE key parse failed: %s\nstatus=%s offset=%zu token=%d "
            "message=%s\n",
            sql, mylite_parse_status_name(status), result.offset, result.token,
            result.message);
    return 1;
  }

  int failed = 0;
  if (mylite_ast_statement_count(ast) != 1 ||
      mylite_ast_statement_kind(ast, 0) != MYLITE_STATEMENT_CREATE ||
      mylite_ast_create_table_key_count(ast, 0) != key_count) {
    fprintf(stderr, "CREATE TABLE key header failed: %s\nkind=%s key_count=%zu\n",
            sql, mylite_statement_kind_name(mylite_ast_statement_kind(ast, 0)),
            mylite_ast_create_table_key_count(ast, 0));
    failed = 1;
  }

  size_t actual_count = mylite_ast_create_table_key_count(ast, 0);
  for (size_t i = 0; i < key_count && i < actual_count; i++) {
    if (mylite_ast_create_table_key_kind(ast, 0, i) != keys[i].kind ||
        !span_matches(sql, mylite_ast_create_table_key_start(ast, 0, i),
                      mylite_ast_create_table_key_end(ast, 0, i),
                      keys[i].definition) ||
        !span_matches(sql,
                      mylite_ast_create_table_key_constraint_name_start(ast, 0,
                                                                       i),
                      mylite_ast_create_table_key_constraint_name_end(ast, 0, i),
                      keys[i].constraint_name) ||
        !span_matches(sql, mylite_ast_create_table_key_name_start(ast, 0, i),
                      mylite_ast_create_table_key_name_end(ast, 0, i),
                      keys[i].name) ||
        (keys[i].index_type != NULL &&
         !span_matches(sql,
                       mylite_ast_create_table_key_index_type_start(ast, 0, i),
                       mylite_ast_create_table_key_index_type_end(ast, 0, i),
                       keys[i].index_type)) ||
        !span_matches(sql,
                      mylite_ast_create_table_key_referenced_table_start(ast, 0,
                                                                        i),
                      mylite_ast_create_table_key_referenced_table_end(ast, 0,
                                                                      i),
                      keys[i].referenced_table) ||
        !span_matches(sql,
                      mylite_ast_create_table_key_referenced_table_schema_start(
                          ast, 0, i),
                      mylite_ast_create_table_key_referenced_table_schema_end(
                          ast, 0, i),
                      keys[i].referenced_schema) ||
        !span_matches(sql,
                      mylite_ast_create_table_key_referenced_table_name_start(
                          ast, 0, i),
                      mylite_ast_create_table_key_referenced_table_name_end(
                          ast, 0, i),
                      keys[i].referenced_name) ||
        (keys[i].foreign_match !=
             MYLITE_CREATE_TABLE_FOREIGN_MATCH_UNSPECIFIED &&
         mylite_ast_create_table_key_foreign_match_kind(ast, 0, i) !=
             keys[i].foreign_match) ||
        (keys[i].foreign_match_span != NULL &&
         !span_matches(sql,
                       mylite_ast_create_table_key_foreign_match_start(ast, 0,
                                                                      i),
                       mylite_ast_create_table_key_foreign_match_end(ast, 0, i),
                       keys[i].foreign_match_span)) ||
        (keys[i].foreign_on_delete !=
             MYLITE_CREATE_TABLE_FOREIGN_ACTION_UNSPECIFIED &&
         mylite_ast_create_table_key_foreign_on_delete_action(ast, 0, i) !=
             keys[i].foreign_on_delete) ||
        (keys[i].foreign_on_delete_span != NULL &&
         !span_matches(sql,
                       mylite_ast_create_table_key_foreign_on_delete_start(
                           ast, 0, i),
                       mylite_ast_create_table_key_foreign_on_delete_end(ast, 0,
                                                                        i),
                       keys[i].foreign_on_delete_span)) ||
        (keys[i].foreign_on_update !=
             MYLITE_CREATE_TABLE_FOREIGN_ACTION_UNSPECIFIED &&
         mylite_ast_create_table_key_foreign_on_update_action(ast, 0, i) !=
             keys[i].foreign_on_update) ||
        (keys[i].foreign_on_update_span != NULL &&
         !span_matches(sql,
                       mylite_ast_create_table_key_foreign_on_update_start(
                           ast, 0, i),
                       mylite_ast_create_table_key_foreign_on_update_end(ast, 0,
                                                                        i),
                       keys[i].foreign_on_update_span)) ||
        (keys[i].check_expression != NULL &&
         !span_matches(sql,
                       mylite_ast_create_table_key_check_expression_start(ast, 0,
                                                                        i),
                       mylite_ast_create_table_key_check_expression_end(ast, 0,
                                                                      i),
                       keys[i].check_expression)) ||
        (keys[i].check_enforcement !=
             MYLITE_CREATE_TABLE_CHECK_ENFORCEMENT_UNSPECIFIED &&
         mylite_ast_create_table_key_check_enforcement(ast, 0, i) !=
             keys[i].check_enforcement) ||
        (keys[i].check_enforcement_span != NULL &&
         !span_matches(sql,
                       mylite_ast_create_table_key_check_enforcement_start(
                           ast, 0, i),
                       mylite_ast_create_table_key_check_enforcement_end(ast, 0,
                                                                        i),
                       keys[i].check_enforcement_span))) {
      fprintf(stderr,
              "CREATE TABLE key[%zu] failed: %s\nkind=%s span=%zu..%zu "
              "constraint=%zu..%zu name=%zu..%zu index_type=%zu..%zu "
              "ref_table=%zu..%zu ref_schema=%zu..%zu ref_name=%zu..%zu "
              "match=%s:%zu..%zu on_delete=%s:%zu..%zu "
              "on_update=%s:%zu..%zu check_expr=%zu..%zu "
              "check_enforcement=%s:%zu..%zu\n",
              i, sql,
              mylite_create_table_key_kind_name(
                  mylite_ast_create_table_key_kind(ast, 0, i)),
              mylite_ast_create_table_key_start(ast, 0, i),
              mylite_ast_create_table_key_end(ast, 0, i),
              mylite_ast_create_table_key_constraint_name_start(ast, 0, i),
              mylite_ast_create_table_key_constraint_name_end(ast, 0, i),
              mylite_ast_create_table_key_name_start(ast, 0, i),
              mylite_ast_create_table_key_name_end(ast, 0, i),
              mylite_ast_create_table_key_index_type_start(ast, 0, i),
              mylite_ast_create_table_key_index_type_end(ast, 0, i),
              mylite_ast_create_table_key_referenced_table_start(ast, 0, i),
              mylite_ast_create_table_key_referenced_table_end(ast, 0, i),
              mylite_ast_create_table_key_referenced_table_schema_start(ast, 0,
                                                                       i),
              mylite_ast_create_table_key_referenced_table_schema_end(ast, 0,
                                                                     i),
              mylite_ast_create_table_key_referenced_table_name_start(ast, 0, i),
              mylite_ast_create_table_key_referenced_table_name_end(ast, 0, i),
              mylite_create_table_foreign_match_kind_name(
                  mylite_ast_create_table_key_foreign_match_kind(ast, 0, i)),
              mylite_ast_create_table_key_foreign_match_start(ast, 0, i),
              mylite_ast_create_table_key_foreign_match_end(ast, 0, i),
              mylite_create_table_foreign_action_name(
                  mylite_ast_create_table_key_foreign_on_delete_action(ast, 0,
                                                                      i)),
              mylite_ast_create_table_key_foreign_on_delete_start(ast, 0, i),
              mylite_ast_create_table_key_foreign_on_delete_end(ast, 0, i),
              mylite_create_table_foreign_action_name(
                  mylite_ast_create_table_key_foreign_on_update_action(ast, 0,
                                                                      i)),
              mylite_ast_create_table_key_foreign_on_update_start(ast, 0, i),
              mylite_ast_create_table_key_foreign_on_update_end(ast, 0, i),
              mylite_ast_create_table_key_check_expression_start(ast, 0, i),
              mylite_ast_create_table_key_check_expression_end(ast, 0, i),
              mylite_create_table_check_enforcement_name(
                  mylite_ast_create_table_key_check_enforcement(ast, 0, i)),
              mylite_ast_create_table_key_check_enforcement_start(ast, 0, i),
              mylite_ast_create_table_key_check_enforcement_end(ast, 0, i));
      failed = 1;
    }

    failed += expect_create_table_key_parts(sql, ast, i, keys[i].columns,
                                            keys[i].column_count, 0);
    failed += expect_create_table_key_parts(
        sql, ast, i, keys[i].referenced_columns, keys[i].referenced_column_count,
        1);
    failed += expect_create_table_key_options(sql, ast, i, keys[i].options,
                                              keys[i].option_count);
  }

  mylite_ast_free(ast);
  return failed;
}

static int expect_create_table_key_parts(
    const char *sql, const MyliteAst *ast, size_t key_index,
    const ExpectedCreateTableKeyPart *parts, size_t part_count,
    int referenced) {
  size_t actual_count =
      referenced ? mylite_ast_create_table_key_referenced_column_count(
                       ast, 0, key_index)
                 : mylite_ast_create_table_key_column_count(ast, 0, key_index);
  if (actual_count != part_count) {
    fprintf(stderr,
            "CREATE TABLE key[%zu] %s count failed: %s\nexpected=%zu "
            "actual=%zu\n",
            key_index, referenced ? "referenced column" : "column", sql,
            part_count, actual_count);
    return 1;
  }

  int failed = 0;
  for (size_t i = 0; i < part_count; i++) {
    size_t start =
        referenced ? mylite_ast_create_table_key_referenced_column_start(
                         ast, 0, key_index, i)
                   : mylite_ast_create_table_key_column_start(ast, 0, key_index,
                                                              i);
    size_t end = referenced
                     ? mylite_ast_create_table_key_referenced_column_end(
                           ast, 0, key_index, i)
                     : mylite_ast_create_table_key_column_end(ast, 0, key_index,
                                                              i);
    size_t name_start =
        referenced ? mylite_ast_create_table_key_referenced_column_name_start(
                         ast, 0, key_index, i)
                   : mylite_ast_create_table_key_column_name_start(
                         ast, 0, key_index, i);
    size_t name_end =
        referenced ? mylite_ast_create_table_key_referenced_column_name_end(
                         ast, 0, key_index, i)
                   : mylite_ast_create_table_key_column_name_end(
                         ast, 0, key_index, i);
    MyliteCreateTableKeyPartKind kind =
        referenced ? mylite_ast_create_table_key_referenced_column_kind(
                         ast, 0, key_index, i)
                   : mylite_ast_create_table_key_column_kind(ast, 0, key_index,
                                                             i);
    size_t expression_start =
        referenced ? mylite_ast_create_table_key_referenced_column_expression_start(
                         ast, 0, key_index, i)
                   : mylite_ast_create_table_key_column_expression_start(
                         ast, 0, key_index, i);
    size_t expression_end =
        referenced ? mylite_ast_create_table_key_referenced_column_expression_end(
                         ast, 0, key_index, i)
                   : mylite_ast_create_table_key_column_expression_end(
                         ast, 0, key_index, i);
    MyliteCreateTableKeyPartOrder order =
        referenced ? mylite_ast_create_table_key_referenced_column_order(
                         ast, 0, key_index, i)
                   : mylite_ast_create_table_key_column_order(ast, 0, key_index,
                                                              i);
    size_t order_start =
        referenced ? 0
                   : mylite_ast_create_table_key_column_order_start(
                         ast, 0, key_index, i);
    size_t order_end =
        referenced ? 0
                   : mylite_ast_create_table_key_column_order_end(
                         ast, 0, key_index, i);
    if (!span_matches(sql, start, end, parts[i].definition) ||
        !span_matches(sql, name_start, name_end, parts[i].name) ||
        (parts[i].kind != MYLITE_CREATE_TABLE_KEY_PART_UNKNOWN &&
         kind != parts[i].kind) ||
        (parts[i].expression != NULL &&
         !span_matches(sql, expression_start, expression_end,
                       parts[i].expression)) ||
        (!referenced && parts[i].prefix != NULL &&
         !span_matches(sql,
                       mylite_ast_create_table_key_column_prefix_start(
                           ast, 0, key_index, i),
                       mylite_ast_create_table_key_column_prefix_end(
                           ast, 0, key_index, i),
                       parts[i].prefix)) ||
        (!referenced && parts[i].prefix_value != NULL &&
         !span_matches(sql,
                       mylite_ast_create_table_key_column_prefix_value_start(
                           ast, 0, key_index, i),
                       mylite_ast_create_table_key_column_prefix_value_end(
                           ast, 0, key_index, i),
                       parts[i].prefix_value)) ||
        (parts[i].order != MYLITE_CREATE_TABLE_KEY_PART_ORDER_UNSPECIFIED &&
         order != parts[i].order) ||
        (!referenced && parts[i].order_span != NULL &&
         !span_matches(sql, order_start, order_end, parts[i].order_span))) {
      fprintf(stderr,
              "CREATE TABLE key[%zu] %s[%zu] failed: %s\nspan=%zu..%zu "
              "name=%zu..%zu kind=%s expr=%zu..%zu order=%s:%zu..%zu\n",
              key_index, referenced ? "ref_column" : "column", i, sql, start,
              end, name_start, name_end,
              mylite_create_table_key_part_kind_name(kind), expression_start,
              expression_end, mylite_create_table_key_part_order_name(order),
              order_start, order_end);
      failed = 1;
    }
  }
  return failed;
}

static int expect_create_table_key_options(
    const char *sql, const MyliteAst *ast, size_t key_index,
    const ExpectedCreateTableKeyOption *options, size_t option_count) {
  if (options == NULL && option_count == 0) {
    return 0;
  }

  size_t actual_count =
      mylite_ast_create_table_key_option_count(ast, 0, key_index);
  if (actual_count != option_count) {
    fprintf(stderr,
            "CREATE TABLE key[%zu] option count failed: %s\nexpected=%zu "
            "actual=%zu\n",
            key_index, sql, option_count, actual_count);
    return 1;
  }

  int failed = 0;
  for (size_t i = 0; i < option_count; i++) {
    if (mylite_ast_create_table_key_option_kind(ast, 0, key_index, i) !=
            options[i].kind ||
        !span_matches(sql,
                      mylite_ast_create_table_key_option_start(ast, 0,
                                                              key_index, i),
                      mylite_ast_create_table_key_option_end(ast, 0, key_index,
                                                            i),
                      options[i].definition) ||
        !span_matches(sql,
                      mylite_ast_create_table_key_option_name_start(
                          ast, 0, key_index, i),
                      mylite_ast_create_table_key_option_name_end(
                          ast, 0, key_index, i),
                      options[i].name) ||
        !span_matches(sql,
                      mylite_ast_create_table_key_option_value_start(
                          ast, 0, key_index, i),
                      mylite_ast_create_table_key_option_value_end(
                          ast, 0, key_index, i),
                      options[i].value)) {
      fprintf(stderr,
              "CREATE TABLE key[%zu] option[%zu] failed: %s\nkind=%s "
              "span=%zu..%zu name=%zu..%zu value=%zu..%zu\n",
              key_index, i, sql,
              mylite_create_table_key_option_kind_name(
                  mylite_ast_create_table_key_option_kind(ast, 0, key_index,
                                                          i)),
              mylite_ast_create_table_key_option_start(ast, 0, key_index, i),
              mylite_ast_create_table_key_option_end(ast, 0, key_index, i),
              mylite_ast_create_table_key_option_name_start(ast, 0, key_index,
                                                            i),
              mylite_ast_create_table_key_option_name_end(ast, 0, key_index,
                                                          i),
              mylite_ast_create_table_key_option_value_start(ast, 0, key_index,
                                                             i),
              mylite_ast_create_table_key_option_value_end(ast, 0, key_index,
                                                           i));
      failed = 1;
    }
  }
  return failed;
}

static int expect_create_table_options(const char *sql,
                                       const ExpectedCreateTableOption *options,
                                       size_t option_count) {
  MyliteParseResult result;
  MyliteAst *ast = NULL;
  MyliteParseStatus status = mylite_parse_sql_ast(sql, &ast, &result);
  if (status != MYLITE_PARSE_OK) {
    fprintf(stderr,
            "CREATE TABLE option parse failed: %s\nstatus=%s offset=%zu "
            "token=%d message=%s\n",
            sql, mylite_parse_status_name(status), result.offset, result.token,
            result.message);
    return 1;
  }

  int failed = 0;
  if (mylite_ast_statement_count(ast) != 1 ||
      mylite_ast_statement_kind(ast, 0) != MYLITE_STATEMENT_CREATE ||
      mylite_ast_create_table_option_count(ast, 0) != option_count) {
    fprintf(stderr,
            "CREATE TABLE option header failed: %s\nkind=%s "
            "option_count=%zu\n",
            sql, mylite_statement_kind_name(mylite_ast_statement_kind(ast, 0)),
            mylite_ast_create_table_option_count(ast, 0));
    failed = 1;
  }

  size_t actual_count = mylite_ast_create_table_option_count(ast, 0);
  for (size_t i = 0; i < option_count && i < actual_count; i++) {
    if (mylite_ast_create_table_option_kind(ast, 0, i) != options[i].kind ||
        !span_matches(sql, mylite_ast_create_table_option_start(ast, 0, i),
                      mylite_ast_create_table_option_end(ast, 0, i),
                      options[i].definition) ||
        !span_matches(sql, mylite_ast_create_table_option_name_start(ast, 0, i),
                      mylite_ast_create_table_option_name_end(ast, 0, i),
                      options[i].name) ||
        !span_matches(sql, mylite_ast_create_table_option_value_start(ast, 0, i),
                      mylite_ast_create_table_option_value_end(ast, 0, i),
                      options[i].value)) {
      fprintf(stderr,
              "CREATE TABLE option[%zu] failed: %s\nkind=%s span=%zu..%zu "
              "name=%zu..%zu value=%zu..%zu\n",
              i, sql,
              mylite_create_table_option_kind_name(
                  mylite_ast_create_table_option_kind(ast, 0, i)),
              mylite_ast_create_table_option_start(ast, 0, i),
              mylite_ast_create_table_option_end(ast, 0, i),
              mylite_ast_create_table_option_name_start(ast, 0, i),
              mylite_ast_create_table_option_name_end(ast, 0, i),
              mylite_ast_create_table_option_value_start(ast, 0, i),
              mylite_ast_create_table_option_value_end(ast, 0, i));
      failed = 1;
    }
  }

  mylite_ast_free(ast);
  return failed;
}

static int span_matches(const char *sql, size_t start, size_t end,
                        const char *expected) {
  if (expected == NULL) {
    return start == 0 && end == 0;
  }
  size_t expected_length = strlen(expected);
  return end >= start && end - start == expected_length &&
         strncmp(sql + start, expected, expected_length) == 0;
}

static int span_matches_when_expected(const char *sql, size_t start, size_t end,
                                      const char *expected) {
  return expected == NULL || span_matches(sql, start, end, expected);
}

static int node_symbol_matches_when_expected(const MyliteAstNode *node,
                                             const char *expected) {
  if (expected == NULL) {
    return 1;
  }
  const char *actual = mylite_ast_node_symbol_name(node);
  return actual != NULL && strcmp(actual, expected) == 0;
}

static int expect_ast_statements(const char *sql, size_t count,
                                 const MyliteStatementKind *kinds) {
  MyliteParseResult result;
  MyliteAst *ast = NULL;
  MyliteParseStatus status = mylite_parse_sql_ast(sql, &ast, &result);
  if (status != MYLITE_PARSE_OK) {
    fprintf(stderr,
            "AST statement parse failed: %s\nstatus=%s offset=%zu token=%d message=%s\n",
            sql, mylite_parse_status_name(status), result.offset, result.token,
            result.message);
    return 1;
  }

  int failed = 0;
  if (mylite_ast_statement_count(ast) != count) {
    fprintf(stderr, "AST statement count failed: %s\nexpected=%zu actual=%zu\n", sql,
            count, mylite_ast_statement_count(ast));
    failed = 1;
  }
  for (size_t i = 0; i < count && i < mylite_ast_statement_count(ast); i++) {
    MyliteStatementKind actual = mylite_ast_statement_kind(ast, i);
    if (actual != kinds[i]) {
      fprintf(stderr,
              "AST statement kind failed: %s\nindex=%zu expected=%s actual=%s symbol=%s\n",
              sql, i, mylite_statement_kind_name(kinds[i]),
              mylite_statement_kind_name(actual),
              mylite_ast_statement_symbol_name(ast, i));
      failed = 1;
    }
  }

  mylite_ast_free(ast);
  return failed;
}
