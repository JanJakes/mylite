# Lemon parser prototype

This branch introduces the first MyLite-owned C parser prototype using
SQLite's Lemon parser generator.

## Scope

The current parser is intentionally syntax-light, but it is no longer a pure
token sink:

- A MySQL-aware lexer recognizes strings, quoted identifiers, executable
  version comments, regular comments, numbers, identifiers, delimiters,
  operator-like tokens, and the current statement-start keywords.
- `*` is no longer a generic identifier fallback; wildcard and expression-body
  uses are explicit so object-name grammar rejects MySQL-invalid wildcard names.
- Operator-like fallback tokens are rejected in shared object-name productions
  while remaining available in expression and permissive statement bodies.
- Numeric and single-quoted literals are rejected in strict object-name
  productions while numeric-leading identifiers, double-quoted identifiers, and
  quoted account/charset strings remain valid. Standalone `CASCADE` and
  `RESTRICT` object, column, index, constraint, prepared-statement, account
  principal, and `USE` names are rejected where MySQL treats them as reserved
  syntax, while quoted and dotted references such as `` `restrict` `` and
  `db.restrict` remain valid.
- A Lemon grammar recognizes MySQL statement families for DDL, DML,
  transactions, utility statements, administration statements, replication/XA,
  and stored-program statement starts.
- The generated parser uses Lemon's growable stack path so long DDL action
  lists do not fail at the default fixed stack depth.
- Closed keyword subgrammars for routine characteristics, table options,
  diagnostics items, replication options, profile types, UDF return types, and
  other statement modifiers are represented directly in Lemon rather than
  validated by C string checks.
- MySQL 8.4 administration forms such as `SHOW BINARY LOG STATUS` and
  `CLONE INSTANCE ... [DATA DIRECTORY ...] [REQUIRE [NO] SSL]` are recognized
  structurally, with unsigned decimal plus lowercase-hex remote clone ports and
  text-string clone passwords and data directories that reject quoted hex/bit
  literals.
- Account-reference forms that permit `CURRENT_USER` or `CURRENT_USER()` now
  have explicit grammar productions.
- Stored function signatures require empty or comma-separated input parameter
  lists, and stored procedure signatures accept `IN`/`OUT`/`INOUT` modes while
  preserving nested type bodies. Stored routine bodies validate direct and
  compound `RETURN`, `DO`, `SET`, local-variable `DECLARE`, and control-flow
  condition expressions through the existing reusable validators; direct
  procedure DML, `VALUES`, and `SELECT` bodies reuse the DML/VALUES/query
  validators.
- Loadable UDF declarations and plugin installs require string-literal
  `SONAME` values.
- `CREATE VIEW` and `ALTER VIEW` recognize algorithm, definer, SQL security,
  column lists, view body starts, and explicit check-option tails for
  non-plain-`SELECT` body forms, including parenthesized query expressions and
  `VALUES` set operands, while validating parenthesized query `ORDER BY` and
  `LIMIT` suffixes, parenthesized query-body `SELECT`/`WITH`/`TABLE`/`VALUES`
  expression tails, direct body `SELECT` list tails, malformed `SELECT`
  operands after set operators before `WITH CHECK OPTION`, and `VALUES ROW(...)`
  row-expression tails.
- `CALL` recognizes one- and two-part routine names plus comma-separated
  argument lists with nested expression bodies, while rejecting malformed
  argument adjacent operands, dangling operators, and invalid plain
  parenthesized groups at top level and in stored-program body sinks.
- `DO` recognizes comma-separated expression lists, including MySQL's
  select-list wildcard forms for the first bare `*` item and qualified
  `table.*` items, while preserving `DEFAULT(column)` expressions and rejecting
  dangling separators, dangling operators, malformed nested expression groups,
  invalid or bare `DEFAULT` values, adjacent top-level operands, misplaced
  later bare wildcards, wildcard aliases/arithmetic, and top-level query-clause
  tails.
- `INSERT` and `REPLACE` recognize empty and comma-separated column lists before
  write payloads, MySQL's `INSERT` priority/`IGNORE` modifier order, and
  MySQL's single `REPLACE LOW_PRIORITY`/`DELAYED` modifier slot, and validate
  `SET` assignment lists including repeated-`SET`
  continuations, malformed top-level assignment value adjacent operands and
  dangling operators, adjacent operands, dangling operators, and trailing
  separators inside plain parenthesized assignment value groups, explicit
  `VALUE(S)` row-list tails, `VALUES ROW(...)` versus singular `VALUE (...)`,
  mixed `ROW(...)`/plain tuple rejection, `INSERT`
  `VALUE(S)` row expression lists with empty rows plus adjacent-operand,
  dangling-operator, and trailing-separator checks, `INSERT` plain
  `VALUE(S)`/`SET` row aliases,
  direct query payload `SELECT` list tails, parenthesized query payload
  `SELECT`/`WITH`/`TABLE`/`VALUES` expression tails plus outer `ORDER BY` and
  MySQL-shaped `LIMIT` suffixes, malformed `SELECT` operands after set
  operators, and
  `ON DUPLICATE KEY UPDATE` assignment tails including whole-value `DEFAULT`,
  malformed post-value continuations, and stray top-level `SELECT`/`FROM`
  suffixes after assignment values.
- Single-table `UPDATE` validates `SET` assignment lists, malformed top-level
  assignment value adjacent operands and dangling operators, adjacent operands,
  dangling operators, and trailing separators inside plain parenthesized
  assignment/`WHERE`/`ORDER BY` expression groups, plus `WHERE`, `ORDER BY`, and
  MySQL-shaped `LIMIT` tails, and rejects `AS` without a following table alias
  before `SET`.
- Single-table `DELETE` recognizes table aliases before optional partition
  lists, plus `WHERE`, `ORDER BY`, and MySQL-shaped `LIMIT` tails, rejecting
  incomplete DML clause tails, invalid top-level `ORDER BY` direction
  sequences, malformed `WHERE`/`ORDER BY` adjacent operands and dangling
  operators including inside plain parenthesized expression groups, trailing
  separators inside those groups, and out-of-order top-level DML clauses.
- `VALUES` recognizes comma-separated row contents, whole-value `DEFAULT`, and
  `DEFAULT(column)` while preserving nested expression bodies, rejects adjacent
  operands and dangling operators in row expression lists, applies the same
  checks in CTAS, view, and DML
  set-operation `VALUES` bodies, preserves set operators, validates
  parenthesized `ORDER BY` expressions plus `ORDER BY` and MySQL-shaped
  `LIMIT` tails, supports top-level and CTE-body `INTO` variable lists plus
  `OUTFILE`/`DUMPFILE` targets, and malformed `SELECT` operands after set
  operators are rejected.
- `TABLE` recognizes table references, set operators, `ORDER BY`,
  MySQL-shaped `LIMIT` forms, `INTO` variable lists, and file output targets
  including `OUTFILE` charset, field, and line option tails. Malformed `SELECT`
  operands after set operators are rejected. Embedded `TABLE` statements reuse
  a C-side validator for required targets plus dangling `ORDER BY`, `LIMIT`,
  and `INTO` tails.
- Expression-tail validation rejects trailing separators in nested plain
  parenthesized expression groups and empty `EXISTS()` predicates while
  preserving empty ordinary function calls and window `OVER()` clauses. It also
  rejects `BETWEEN` expressions that omit the lower bound before `AND`.
- `WITH` CTE wrappers dispatch their main query/body forms through the same
  validators used by top-level `SELECT`, `TABLE`, `VALUES`, parenthesized query,
  and MySQL-supported `UPDATE`/`DELETE` bodies. Top-level `WITH ... INSERT` and
  `WITH ... REPLACE` are rejected, while `INSERT`/`REPLACE ... WITH ... SELECT`
  payloads remain valid. CTE bodies must begin with a query expression
  (`SELECT`, `TABLE`, `VALUES`, nested `WITH`, or parenthesized forms), and DML
  bodies inside a CTE are rejected.
- Top-level `SELECT` recognizes `SQL_NO_CACHE` as a deprecated MySQL 8.4
  modifier token.
- Top-level `SELECT` rejects incompatible `ALL` and `DISTINCT` modifiers.
- Top-level and set-operand `SELECT` rejects empty or dangling select-list
  expressions before query clause and statement boundaries.
- Top-level `SELECT` rejects missing operands for major clause starts such as
  `FROM`, `WHERE`, `GROUP BY`, `HAVING`, `ORDER BY`, `LIMIT`, and `INTO`.
- Top-level `SELECT` rejects the removed MySQL 8.4 `PROCEDURE ANALYSE`
  clause shape.
- Top-level `SELECT` rejects trailing top-level comma separators in select
  lists and major clauses.
- Top-level `SELECT` rejects missing operands for top-level `JOIN`, `ON`, and
  `USING` join clauses.
- Top-level `SELECT` rejects `ON` and `USING` clauses that are not attached to
  a preceding joined table, including nested outer-join condition chains.
- Top-level `SELECT` validates `GROUP BY ... WITH ROLLUP` tails.
- Top-level `SELECT` validates `ORDER BY` `ASC`/`DESC` direction tails.
- Top-level `SELECT` validates `LIMIT` comma, `OFFSET`, adjacent-operand
  forms, and MySQL's integer-or-identifier option domain.
- Top-level `SELECT` rejects duplicate major clauses within one query block,
  including real `FROM` clauses while preserving `NTH_VALUE(... FROM
  FIRST|LAST ... OVER ...)` window-function modifiers.
- Top-level `SELECT` rejects out-of-order major clauses across `WHERE`,
  `GROUP BY`, `HAVING`, `WINDOW`, `QUALIFY`, `ORDER BY`, `LIMIT`, and locking
  tails.
- Top-level `SELECT` validates named `WINDOW name AS (...)` clause lists.
- Top-level and query-body `SELECT` list tails preserve window-function
  `RESPECT NULLS` and `IGNORE NULLS` continuations for `LEAD`, `LAG`,
  `FIRST_VALUE`, `LAST_VALUE`, and `NTH_VALUE` before `OVER`.
- Top-level `SELECT` rejects missing operands after `QUALIFY`.
- Top-level `SELECT` requires complete `LOCK IN SHARE MODE` locking tails.
- Top-level `SELECT` validates `FOR UPDATE`/`FOR SHARE` locking tails including
  optional `OF` table lists, `NOWAIT`, and `SKIP LOCKED`, while preserving
  index-hint `FOR JOIN`/`FOR ORDER BY`/`FOR GROUP BY`.
- Top-level `SELECT` validates `USE`/`FORCE`/`IGNORE INDEX|KEY` hint tails,
  including scoped `FOR JOIN`/`FOR ORDER BY`/`FOR GROUP BY` forms.
- Top-level `SELECT` validates table `PARTITION (...)` selection lists.
- Top-level `SELECT` validates table `TABLESAMPLE SYSTEM|BERNOULLI (...)`
  suffixes.
- Top-level `SELECT` rejects adjacent bare operands and dangling operators in
  `WHERE`, `HAVING`, and `QUALIFY` expression tails.
- Top-level `SELECT` rejects dangling operators and malformed `ASC`/`DESC`
  directions in `GROUP BY` expression lists.
- Top-level `SELECT` rejects adjacent bare operands and dangling operators in
  `ORDER BY` expression lists.
- Top-level and query-body `SELECT` list tails reject incomplete `AS` aliases
  and stray tokens after explicit or implicit aliases.
- Top-level and query-body `SELECT` list tails preserve MySQL's implicit alias
  tokens while rejecting pure numeric aliases, numeric-literal prefixes such as
  `2e3abc`, quoted hex/bit literals, charset-introduced string literals, and
  removed `SQL_CACHE` modifier forms.
- Top-level and query-body `SELECT` list tails allow bare `*` only as the
  first select item while preserving qualified `table.*` items later in the
  list.
- Ordinary expression validators preserve `DEFAULT(column)` and dotted
  `default` identifier paths while rejecting bare `DEFAULT` outside
  default-capable assignment and row-value contexts.
- `SELECT ... INTO` variable lists reject missing user-variable names, dangling
  commas, and adjacent variable targets.
- Top-level `SELECT` requires `INTO OUTFILE` and `INTO DUMPFILE` to include
  the mandatory text-string file name, rejecting quoted hex/bit literals.
- Top-level `SELECT ... INTO OUTFILE` validates the basic `CHARACTER SET`,
  including quoted charset names but not quoted hex/bit literals,
  `FIELDS`/`COLUMNS`, and `LINES` option tails, and file-output targets reject
  stray suffixes that are not valid following `SELECT` clauses.
- Top-level `SELECT` also rejects incomplete `UNION`/`INTERSECT`/`EXCEPT`
  set operations and repeated set-operation option tokens.
- Top-level parenthesized query expressions reject stray suffixes after the
  outer `)`, validate `ORDER BY` expression-list tails plus `LIMIT`/`INTO`
  continuations, validate inner `SELECT`/`WITH`/`TABLE`/`VALUES` expression
  tails, validate `INTO` variable-list and file-output tails, and require
  set-operation tails to start with a query operand whose `SELECT` clause tail
  is well formed.
- `HANDLER` recognizes one- and two-part table names, aliases, key names,
  MySQL's table-scan and indexed-read direction sets, equality/range tuple
  reads, `WHERE`, and integer-or-identifier `LIMIT` tails, while rejecting
  malformed tuple expressions, malformed `WHERE` adjacent operands, dangling
  operators, non-integer numeric limit values, and `LIMIT` suffixes after
  `READ`.
- `USE` recognizes one-part unquoted schema names using the shared identifier
  grammar and unreserved keyword names while rejecting string, numeric,
  variable, operator, and reserved-keyword targets including `CASCADE` and
  `RESTRICT`.
- Account and role names use the shared unreserved identifier grammar across
  `CREATE`/`ALTER`/`DROP` account statements, rejecting standalone
  `CASCADE`/`RESTRICT` principals while preserving quoted principals and hosts.
- `RENAME USER` reuses the shared account-reference grammar for source and
  destination account pairs.
- `CREATE USER` and `ALTER USER` recognize account lists, authentication
  clauses, repeatable TLS/resource/password/lock options, string-literal
  comments and attributes, string-literal authentication/TLS values with
  quoted hex/bit rejection in text-string positions, and
  default-role clauses rather than generic token tails, including unsigned
  decimal/lowercase-hex resource limits, unsigned integer/lowercase-hex
  password policy counts, string-literal and numeric-start identifier
  authentication plugin names, unsigned decimal MFA factor numbers,
  initial-authentication, and WebAuthn registration syntax.
- `GRANT` and `REVOKE` recognize grant/admin options, proxy forms, recipient
  authentication/resource clauses with unsigned decimal/lowercase-hex resource
  limits, `AS ... WITH ROLE`, and
  `IGNORE UNKNOWN USER`.
- `CREATE INDEX` recognizes non-empty functional/key-part lists and standalone
  index options including parser plugins, string-literal comments, visibility,
  string-literal attributes, unsigned decimal/lowercase-hex/quoted-hex
  `KEY_BLOCK_SIZE`, quoted hex/bit rejection in text-string options, closed
  `USING`/`TYPE` values, and
  identifier-valued `ALGORITHM`/`LOCK` with duplicate option rejection,
  while validating standalone key-part prefix lengths and `ASC`/`DESC` tails.
- `CREATE TABLE` table-definition bodies require non-empty comma-separated
  elements while preserving nested token bodies for column and constraint
  definitions, and validate table-level index key-part prefix lengths plus
  `ASC`/`DESC` tails, closed `USING`/`TYPE` values, and standalone reserved
  index/constraint names, plus table-level foreign-key child lists, referenced
  table names, optional parent column-list envelopes, and referential-action
  tails. Column and table `CHECK` constraints validate parenthesized expression
  bodies for dangling operators and scalar comma lists, and `CHECK` constraints
  validate `ENFORCED`/`NOT ENFORCED` tails while allowing later column
  attributes where MySQL permits them. Column definitions require a known MySQL
  type-family start, validate common numeric and `ENUM`/`SET` type-parameter
  lists, accept hex literals in `ENUM`/`SET` values, preserve MySQL's
  `DOUBLE PRECISION` modifier without accepting `PRECISION` after other types,
  require lengths for `VARCHAR`, `VARBINARY`, `NVARCHAR`, and `CHAR VARYING`
  forms while preserving `LONG VARCHAR`/`LONG VARBINARY`, and accept common
  attribute starts and selected closed attribute values, validate generated
  column heads (`AS (...)` or `GENERATED ALWAYS AS (...)`), require
  generated-column storage modifiers to follow generated expressions, reject
  repeated generated-column storage modifiers and repeated column `COLLATE`
  clauses, accept repeated column `COMMENT` attributes with MySQL string-token
  variants while rejecting quoted hex/bit literals in text-string positions,
  restrict column-level `CONSTRAINT` prefixes to `CHECK`, reject `DEFAULT` as
  a column charset/collation value, validate parenthesized default/generated
  expression bodies for dangling operators and scalar comma lists, validate
  temporal precision lists for `DEFAULT`/`ON UPDATE` functions, reject stray attribute parentheses, and
  column-level `REFERENCES` clauses validate optional referenced column-list
  envelopes plus referential-action tails, duplicate `MATCH`/`ON UPDATE`/`ON
  DELETE` rejection, and MySQL's `MATCH`-before-actions order. Trailing table options
  must start with known MySQL table-option keywords and validate selected value
  domains, including unsigned decimal table numbers, MySQL boolean/default
  number domains, string-literal options, closed `INSERT_METHOD`, closed
  `ROW_FORMAT`, decimal-versus-size number domains with quoted-hex size values,
  bounded numeric and quoted-hex `STATS_SAMPLE_PAGES`, storage values without equality signs,
  `UNION` table-name lists, and `START TRANSACTION`. `CREATE TABLE`
  partition tails validate `HASH`/`LINEAR HASH` expressions, reject
  `LINEAR RANGE`/`LINEAR LIST`, validate `RANGE`/`LIST`
  and `RANGE COLUMNS`/`LIST COLUMNS` expression-list envelopes, `KEY`/`LINEAR
  KEY` column lists including MySQL's syntax-valid empty list, key
  `ALGORITHM=1/2` values, partition/subpartition count literals,
  `HASH`/`KEY`-only subpartition methods, non-empty `VALUES LESS THAN (...)`
  and `VALUES IN (...)` lists, required values for explicit `RANGE`/`LIST` partitions,
  `VALUES LESS THAN MAXVALUE`, and malformed/trailing partition definition
  commas, plus explicit partition options for `ENGINE`/`STORAGE ENGINE`,
  `COMMENT`, `DATA DIRECTORY`, `INDEX DIRECTORY`, `MAX_ROWS`, `MIN_ROWS`,
  `TABLESPACE`, and `NODEGROUP` with MySQL-shaped value tokens, including
  double-quoted tablespace names for SQL-mode/corpus coverage. Explicit
  nested subpartition lists require comma-separated `SUBPARTITION` names and
  reuse the same option value checks. No-definition and
  post-definition CTAS forms are recognized explicitly with table/partition options, including
  `IGNORE`/`REPLACE` duplicate-handling modifiers, and no-definition
  table-option forms must include a query body. Direct CTAS query bodies validate
  `SELECT` list tails, and parenthesized CTAS query bodies validate their inner
  `SELECT`/`WITH`/`TABLE`/`VALUES` expression tails plus their outer `ORDER BY`
  and `LIMIT` suffixes.
- `CREATE LOGFILE GROUP` and `ALTER LOGFILE GROUP` recognize `ADD UNDOFILE`,
  string-literal file names, documented NDB logfile options with unsigned size
  values including suffixes and numeric/quoted hex, unsigned
  integer/lowercase-hex nodegroups, `WAIT`, and optional
  `ENGINE`/`STORAGE ENGINE` clauses, while rejecting quoted hex/bit literals in
  text-string file/comment positions.
- `CREATE TABLESPACE` and `ALTER TABLESPACE` recognize documented
  string-literal data files, unsigned size values including suffixes and
  numeric/quoted hex, unsigned integer/lowercase-hex nodegroups, `WAIT`,
  string-literal encryption, optional-equals engine/storage-engine options, and
  string-literal attribute clauses, including repeated simple
  `ALTER TABLESPACE` option lists, while rejecting quoted hex/bit literals in
  text-string positions. UNDO tablespaces use MySQL's narrower option list:
  required string-literal create data files plus optional
  `ENGINE`/`STORAGE ENGINE`.
  Drop tablespace/logfile tails recognize MySQL option lists including `ENGINE`,
  `STORAGE ENGINE`,
  `WAIT`, and `NO_WAIT`.
- `CREATE SERVER` and `ALTER SERVER` recognize the documented foreign-server
  `OPTIONS` names, text-string option values with quoted hex/bit rejection, and
  unsigned decimal plus lowercase-hex ports.
- `CREATE EVENT` recognizes event schedules with `AT` timestamps and `EVERY`
  intervals, validates interval units plus `STARTS`/`ENDS` schedule tails, and
  recognizes scheduler status tails including `DISABLE ON REPLICA` and
  deprecated `DISABLE ON SLAVE` immediately after the schedule. Recognized
  single-statement event bodies are routed through the existing statement
  validators, including direct `SELECT` list-tail checks.
- `CREATE TRIGGER` recognizes timing, event, target table, optional ordering,
  and stored-program statement starts for single-statement trigger bodies, and
  recognized body starts including flow-control conditions reuse existing
  statement validators, including direct `SELECT` list-tail and `RETURN`
  expression-tail checks.
- `ALTER INSTANCE` recognizes redo-log enable/disable, InnoDB/binlog master-key
  rotation, TLS reload with channel/no-rollback options, and keyring reload.
- `ALTER TABLE` recognizes selected closed actions including `FORCE`,
  `ENABLE/DISABLE KEYS`, `ADD`/`CHANGE`/`MODIFY` heads, `RENAME` forms,
  comma-separated `ADD`/`CHANGE`/`MODIFY` bodies, `DROP` forms, column-drop
  `RESTRICT`/`CASCADE` tails, `ALTER` subactions, charset/order changes,
  validated `ALTER ... SET DEFAULT` expression tails, validated `ORDER BY`
  expression-list tails, partition definition, maintenance/exchange,
  reorganize forms, validated partition-method expression lists, validated
  `PARTITION BY ... (...)`, `ADD PARTITION (...)`, and
  `REORGANIZE PARTITION ... INTO (...)` partition-definition envelopes, maintenance
  binlog modifiers, CHECK/REPAIR maintenance options, strict `PARTITION BY`
  method modifiers and count literals, and unsigned integer/lowercase-hex
  coalesce/add-partition counts,
  tablespace/storage/union changes with closed storage values,
  secondary-engine load/unload actions with optional concrete partition lists,
  table option changes with charset/collation continuations,
  unsigned decimal, boolean, and default-boolean value domains, closed `INSERT_METHOD` and
  `ROW_FORMAT` values, string-literal data/index
  directories, string-literal compression/password/connection/engine
  attributes, `AUTOEXTEND_SIZE`, `TABLE_CHECKSUM`, `START TRANSACTION`,
  decimal-versus-size number domains, numeric and quoted-hex
  `STATS_SAMPLE_PAGES` bounds, storage
  option equality rejection, identifier-valued `ALGORITHM`/`LOCK` options,
  invalid table-option continuation rejection,
  tablespace discard/import forms, and validated `ADD`/`CHANGE`/`MODIFY`
  column type starts plus
  MySQL's `DOUBLE PRECISION` modifier placement, column-definition attribute
  starts and selected closed attribute values including generated-column head
  validation, repeated `COLLATE` rejection, repeated column `COMMENT`
  attributes, quoted hex/bit literal rejection for text options,
  column-level `CONSTRAINT CHECK` restriction, `DEFAULT` charset/collation
  value rejection, storage modifier context and repetition checks, standalone
  reserved index/constraint names, index key-part prefix lengths,
  `ASC`/`DESC` tails, and index option values including unsigned
  decimal/lowercase-hex/quoted-hex `KEY_BLOCK_SIZE` values and closed index
  `USING`/`TYPE` values. `ADD FOREIGN KEY` child lists, referenced table names,
  optional parent column-list envelopes, and referential-action tails are also
  validated, including duplicate `MATCH`/`ON UPDATE`/`ON DELETE` rejection and MySQL's
  `MATCH`-before-actions order, and `ADD CHECK`
  requires a non-empty parenthesized expression body plus a valid
  `ENFORCED`/`NOT ENFORCED` tail when present.
- `CREATE TABLE ... LIKE` recognizes one- and two-part target/source table
  names, including `TEMPORARY` and `IF NOT EXISTS`, and rejects trailing table
  options or tokens after the source table.
- `CREATE TEMPORARY TABLE` shares the definition, `LIKE`, and CTAS parser
  paths with regular `CREATE TABLE`.
- `DROP EVENT`, `DROP PROCEDURE`, `DROP SERVER`, and `DROP TRIGGER` recognize
  optional `IF EXISTS` and validated object-name shapes.
- `RENAME TABLE` recognizes `TABLE`/`TABLES` plus comma-separated source/target
  table pairs.
  `DROP`/`EXCHANGE`/`REORGANIZE PARTITION` and secondary-engine partition
  lists require concrete partition names; `DISCARD`/`IMPORT PARTITION`
  accept MySQL's `ALL` form, and `REORGANIZE PARTITION` also requires a
  non-empty `INTO (...)` body.
  Table-level `ENGINE_ATTRIBUTE` and `SECONDARY_ENGINE_ATTRIBUTE` changes
  require string literals.
- `CREATE DATABASE` and `ALTER DATABASE` recognize schema names, charset,
  collation, string-literal encryption, and alter-only `READ ONLY` option clauses
  with MySQL-shaped `DEFAULT` or default-boolean numeric values.
- `CREATE EVENT` and `ALTER EVENT` recognize ordered event metadata clauses
  for schedules, completion policy, enablement state, comments with text-string
  validation, and event bodies, and reject malformed event schedule tails;
  `ALTER EVENT` also recognizes renames.
- `CREATE FUNCTION`, `CREATE PROCEDURE`, `ALTER FUNCTION`, and
  `ALTER PROCEDURE` recognize routine characteristics: comments with
  text-string validation, `LANGUAGE SQL`, SQL data access, and SQL security.
- Loadable `CREATE FUNCTION` requires text-string `SONAME` values.
- Spatial reference system DDL recognizes the MySQL 8.4 `IF [NOT] EXISTS`,
  `OR REPLACE`, bare unsigned integer/lowercase-hex SRS ids, documented
  text-string attribute forms with quoted hex/bit rejection, and unsigned
  integer/lowercase-hex organization authority codes.
- `DROP INDEX` recognizes MySQL's identifier-valued `ALGORITHM` and `LOCK`
  option tails, rejects duplicate options, and rejects standalone reserved
  `CASCADE`/`RESTRICT` index names.
- `TRUNCATE TABLE` recognizes optional `TABLE` and one- or two-part table
  references using the shared identifier grammar, with MySQL's standalone
  reserved-name rejection preserved for `CASCADE` and `RESTRICT`.
- `INSTALL PLUGIN` and `UNINSTALL PLUGIN` recognize plugin names using the
  shared identifier grammar; plugin `SONAME` values reject quoted hex/bit
  literals.
- `INSTALL COMPONENT` and `UNINSTALL COMPONENT` recognize text-string component
  file lists and optional scoped `SET` assignments for installs, while rejecting
  malformed component assignment values and quoted hex/bit component URNs.
- `ANALYZE TABLE` recognizes table lists and histogram update/drop clauses using
  the shared identifier grammar for table and column names, with unsigned-integer
  histogram bucket counts, MySQL 8.4 automatic/manual histogram update modes,
  histogram clauses after table lists, and text-string histogram data with
  quoted hex/bit rejection.
- `CHECK TABLE`, `CHECKSUM TABLE`/`TABLES`, `OPTIMIZE TABLE`, and `REPAIR TABLE`
  recognize table lists and their documented parser-level option keywords.
- Resource group DDL and utility statements recognize MySQL 8.4 resource
  attributes, unsigned-integer VCPU ranges, optional-negative-integer thread
  priorities, force modifiers, and unsigned integer/lowercase-hex thread-id
  assignment lists. VCPU range and `SET RESOURCE GROUP ... FOR` thread-id lists
  support MySQL's optional comma separators.
- `START REPLICA` recognizes `IO_THREAD`/`RELAY_THREAD` and `SQL_THREAD`,
  `UNTIL`, connection, and channel clauses with text-string log/GTID/user
  option values that reject quoted hex/bit literals. Source/relay log
  coordinate option lists are accepted in MySQL parser order, with unsigned
  decimal source log positions and MySQL's unsigned decimal plus lowercase-hex
  relay log-position forms. Replication channel names use MySQL's text-string
  channel grammar.
  Removed `START SLAVE` and
  `STOP SLAVE` syntax is
  permissive-corpus-only.
- `SHOW REPLICA STATUS` and `SHOW REPLICAS` recognize the current MySQL 8.4
  spellings. Removed `SHOW SLAVE STATUS` and `SHOW SLAVE HOSTS` syntax is
  permissive-corpus-only.
- `RESET` recognizes comma-separated MySQL 8.4 reset options, including
  `RESET BINARY LOGS AND GTIDS` with optional unsigned integer-or-hex `TO`
  index values and `RESET REPLICA` text-string channel clauses. Removed
  `RESET MASTER` and `RESET SLAVE` syntax is permissive-corpus-only.
- `BINLOG` requires a text-string payload, `PURGE BINARY LOGS ... TO` requires a
  text-string log name, and both reject quoted hex/bit literals.
  `PURGE BINARY LOGS ... BEFORE` validates expression tails. Removed
  `PURGE MASTER LOGS` syntax is permissive-corpus-only.
- `KILL` recognizes optional `CONNECTION`/`QUERY` modes, expression-style
  targets including function-call tails, and exact user-variable targets, while
  rejecting extra top-level tokens after the target.
- `LOCK TABLES` recognizes table lists, aliases, and MySQL 8.4 `READ [LOCAL]`
  and `WRITE` lock types using the shared identifier grammar for alias names.
  Removed `LOW_PRIORITY WRITE` syntax is permissive-corpus-only.
- `LOCK INSTANCE FOR BACKUP`, `UNLOCK INSTANCE`, and `UNLOCK TABLES` have
  closed statement shapes.
- `LOAD DATA` and `LOAD XML` recognize file modifiers, duplicate handling,
  optional `FROM`, `INFILE`/`URL`/`S3` sources, text-string source names,
  source counts and primary-key-order hints, partition or row-matching clauses,
  character sets, compression, field/line options with non-empty option bodies,
  unsigned-integer ignored-row counts, column/user-variable lists, validated
  comma-separated `SET` assignment tails, and bulk-load parallel, memory, and
  algorithm options where they are unambiguous with `SET` assignment tails.
  Source and compression strings reject quoted hex/bit literals while field and
  line option strings still allow MySQL's accepted string-literal forms.
  `LOAD DATA` partition clauses require concrete partition names and reject
  the `ALL` form that MySQL reserves for key-cache partition clauses.
- `LOAD INDEX INTO CACHE` recognizes MySQL's single-table partition form,
  `ALL`, optional empty or named key/index lists, comma-separated non-partition
  table lists, and `IGNORE LEAVES`.
- `IMPORT TABLE` recognizes comma-separated text-string file lists with quoted
  hex/bit rejection.
- `EXPLAIN` and `DESCRIBE` recognize table-description forms, explainable
  statement starts including `TABLE` with validated query/DML tails, and
  unsigned integer-or-hex `FOR CONNECTION` ids with optional `FORMAT` clauses.
- `CHANGE REPLICATION FILTER` recognizes the MySQL 8.4 replication filter names,
  parenthesized rule lists, rewrite-db pairs, and optional channel clauses.
- `CHANGE REPLICATION SOURCE TO` recognizes documented MySQL 8.4 source option
  names, typed numeric and boolean-like option values, including decimal-only heartbeat
  periods and decimal/lowercase-hex/quoted-hex retry, delay, port,
  compression-level, source SSL/public-key/auto-position, `REQUIRE_ROW_FORMAT`,
  and `IGNORE_SERVER_IDS` values, strict integer-or-hex `GTID_ONLY` and
  `SOURCE_CONNECTION_AUTO_FAILOVER` booleans, string-or-`NULL` values,
  privilege-check users, fixed primary-key-check enums, GTID assignment values,
  unsigned decimal source log positions, MySQL's unsigned decimal plus
  lowercase-hex relay log-position forms, and optional channel clauses,
  rejecting quoted hex/bit literals in text-string option values. Removed
  `CHANGE MASTER TO` syntax is permissive-corpus-only.
- Replication channel clauses share one text-string grammar across
  `START`/`STOP`/`RESET`/`SHOW`/`FLUSH` and `CHANGE ... FOR CHANNEL`, with
  quoted hex/bit rejection.
- `STOP GROUP_REPLICATION` recognizes the standalone statement and rejects
  trailing options.
- `SHOW PARSE_TREE` is accepted only in permissive corpus mode because normal
  MySQL 8.4 builds reject that conditional debug/development SHOW form.
- `SHOW CREATE` recognizes database/schema, event, function, procedure, table,
  trigger, view, and user targets with validated object/account names.
- `SHOW ENGINE ... STATUS|LOGS|MUTEX` recognizes engine names using the shared
  identifier grammar plus MySQL's `ALL` engine selector.
- Shared `SHOW ... LIKE` filters require text-string patterns, while
  `SHOW ... WHERE` rejects adjacent bare operands and dangling operators while
  preserving common MySQL expression forms.
- `SHOW INDEX`/`INDEXES`/`KEYS` uses MySQL's narrower parser shape with
  optional `EXTENDED` and `WHERE`, excluding `FULL` and `LIKE`.
- `SHOW BINLOG EVENTS` and `SHOW RELAYLOG EVENTS` recognize optional
  text-string log names, unsigned decimal/exponent `FROM` positions, and
  integer-or-identifier `LIMIT` tails; relay-log events also recognize MySQL
  channel clauses.
- `SHOW BINARY LOG STATUS` is the strict MySQL 8.4 binary-log status form;
  removed `SHOW MASTER STATUS` syntax is permissive-corpus-only.
- `SHOW BINARY LOGS` is the strict MySQL 8.4 binary-log listing form; removed
  `SHOW MASTER LOGS` syntax is permissive-corpus-only.
- `SHOW PROFILE` recognizes profile type lists, unsigned-integer `FOR QUERY`
  ids, and integer-or-identifier `LIMIT` tails.
- `SHOW WARNINGS` and `SHOW ERRORS` recognize integer-or-identifier `LIMIT`
  tails while rejecting non-integer numeric literals.
- `SET ROLE` and `SET DEFAULT ROLE` recognize MySQL role specifiers and account
  lists rather than permissive token tails.
- `SET PASSWORD` recognizes MySQL 8.4 text-string and random password
  assignment forms, including replacement and secondary-password clauses, with
  quoted hex/bit rejection.
- `SET TRANSACTION` recognizes GLOBAL/SESSION/LOCAL scope, isolation levels, and
  read access modes in MySQL's one-isolation plus one-access shape.
- `START TRANSACTION` recognizes comma-separated characteristics and rejects
  conflicting `READ ONLY`/`READ WRITE` access modes.
- `COMMIT` and `ROLLBACK` recognize `WORK`, `AND [NO] CHAIN`, and
  `[NO] RELEASE` tails while rejecting MySQL's invalid `AND CHAIN RELEASE`
  combination.
- `SAVEPOINT`, `ROLLBACK TO SAVEPOINT`, and `RELEASE SAVEPOINT` recognize
  savepoint names using the shared identifier grammar.
- XA statements recognize string, hex, and binary XID parts while rejecting
  bare decimal XID literals.
- Stored-program label declarations accept unquoted `label:` and quoted
  identifier-plus-colon forms, including in trigger and handler bodies. Label
  and cursor references use the shared identifier grammar.
- Compound `BEGIN ... END` blocks and `LOOP ... END LOOP` bodies are recognized
  with optional labels and preserved nested statement tails.
- Embedded stored-program `LEAVE` and `ITERATE` statements validate their
  required label arity.
- Embedded stored-program cursor statements validate `OPEN`, `FETCH`, and
  `CLOSE` arity, including `FETCH [[NEXT] FROM] cursor INTO target [, ...]`.
- `DECLARE`, `FETCH [[NEXT] FROM] ... INTO`, and named signal conditions use
  the shared identifier grammar for local names, with unsigned decimal,
  lowercase-hex, and quoted-hex MySQL error codes plus five-character
  `SQLSTATE [VALUE]` literals for condition declarations and handler
  conditions. Handler bodies recognize compound blocks and stored program
  starts for flow-control, cursor, DML, diagnostics, and return statements.
- Local-variable `DECLARE` statements validate `DEFAULT` expression tails.
- Cursor `DECLARE` statements validate `SELECT`, `TABLE`, `VALUES`, `WITH`,
  and parenthesized query bodies, including CTE wrappers before the outer query
  body.
- Stored-program `IF`/`ELSEIF`, `WHILE`, `REPEAT ... UNTIL`, `WHEN`, and
  `RETURN` validate expression tails at their statement boundaries.
- Prepared-statement names and `EXECUTE ... USING` user-variable lists use the
  shared identifier grammar; `PREPARE ... FROM` accepts text-string and
  user-variable sources, rejecting quoted hex/bit statement text literals.
- XA statements recognize one-, two-, and three-part string or hex XIDs with
  unsigned decimal, lowercase-hex, or quoted-hex `formatID` values.
- `XA RECOVER` recognizes the bare statement and `CONVERT XID`.
- `CACHE INDEX` recognizes table, single-table partition, optional empty or
  named key/index, and key-cache names using the shared identifier grammar;
  `RESET PERSIST` uses the shared grammar for persisted-variable names while
  rejecting wildcard targets.
- `GET DIAGNOSTICS` recognizes optional `CURRENT`/`STACKED` diagnostics-area
  selectors; assignment targets use local/user-variable grammar and condition
  numbers may come from literal/simple, user-variable, system-variable, and
  dotted identifier values. Embedded stored-program bodies validate assignment
  separators and diagnostics item names.
- `SIGNAL` and `RESIGNAL` recognize five-character `SQLSTATE [VALUE]` literals
  and condition item assignments with MySQL-shaped simple `MYSQL_ERRNO`
  values, including unsigned numeric/string/hex/bit literals, local/user/system
  variables, and dotted identifiers while rejecting signed, `DEFAULT`,
  parenthesized, and operator expressions; embedded stored-program bodies reuse
  `SET` assignment expression validation.
- `SET NAMES` and `SET CHARACTER SET` recognize charset/collation names using
  the shared identifier grammar, `BINARY`, `DEFAULT`, optional collation, and
  comma-following variable assignments.
- `SET` variable assignments recognize comma-separated assignment lists with
  optional per-assignment scopes, whole-value `DEFAULT`, and nested value
  expressions, reject user-variable-only misuse of system-variable literals such
  as bare `DEFAULT`, `ON`, and `ALL`, reject malformed non-`@@` assignment
  values with adjacent operands, dangling operators, and invalid plain
  parenthesized groups, preserve `@@` system-variable values as token tails, and
  reject repeated top-level `SET` continuations after
  completed assignment values.
- `EXPLAIN FORMAT=JSON INTO @var` is recognized as a JSON-only EXPLAIN form
  with user-variable targets, while `INTO` is still rejected for
  `FOR CONNECTION` plans.
- `EXPLAIN ... FOR SCHEMA|DATABASE name` schema specifiers are recognized.
- `DESCRIBE` and `DESC` reuse the EXPLAIN syntax variants for execution-plan
  statements while preserving table and column description forms with shared
  identifier handling.
- `EXPLAIN ANALYZE` has explicit statement-start handling and rejects
  unsupported non-`TREE` format names, empty explainable statements, and
  malformed query/DML bodies.
- `ALTER TABLESPACE` recognizes `ADD DATAFILE` and `DROP DATAFILE` actions.
- `SHOW CREATE DATABASE` and `SHOW CREATE SCHEMA` recognize optional
  `IF NOT EXISTS`.
- `SHOW CREATE USER` reuses the shared account-reference grammar.
- Core `SHOW` metadata statements recognize MySQL-supported `LIKE`/`WHERE`
  filters where the server grammar allows them (`DATABASES`, `TABLES`,
  `TABLE STATUS`, `OPEN TABLES`, `COLUMNS`/`FIELDS`, routine status,
  `CHARACTER SET`, `COLLATION`, `STATUS`, and `VARIABLES`) and reject filters
  on standalone forms such as `SHOW ENGINES`, `PLUGINS`, `PRIVILEGES`,
  `PROCESSLIST`, `PROFILES`, and `REPLICAS`.
- `FLUSH` recognizes `LOCAL`/`NO_WRITE_TO_BINLOG` modifiers, simple option
  lists, table forms, log variants, and channel-qualified relay logs. Removed
  `FLUSH HOSTS` syntax is permissive-corpus-only.
- `RESTART`, `SHUTDOWN`, and `HELP` recognize their closed parser-level
  statement shapes, with `HELP` requiring MySQL-compatible text-string,
  identifier, or unreserved keyword topics.
- A permissive mode accepts extracted corpus fragments that are not standalone
  MySQL statements.
- The lexer is recoverable for corpus rows that come from MySQL negative tests,
  including unterminated quoted text, and the parser preserves targeted
  extracted `SET` assignment fragments with missing closing parentheses.
- The CLI and corpus script verify that the WordPress SQLite Database
  Integration MySQL query corpus parses end to end.

This is not yet a full MySQL grammar. Most statement bodies currently use a
permissive tail rather than full clause-level productions. It is a lean parser
foundation and corpus harness for growing statement-level grammar coverage
incrementally.

## Commands

Build the parser CLI:

```sh
make all
```

Regenerate Lemon output:

```sh
make regen-parser
```

Run the corpus test:

```sh
make test-parser
```

The corpus is downloaded to `tests/parser/.cache/` and is not committed.
The corpus runner uses permissive mode because the CSV includes extracted
mysqltest fragments and negative-test inputs that are not standalone MySQL
statements. Strict mode currently rejects those fragment starts while accepting
the recognized MySQL statement families. `make test-parser` also runs the
strict corpus check and verifies the current known fragment-failure list.

## Next steps

1. Split the Lemon grammar from generic token-stream acceptance into
   statement-level productions.
2. Replace permissive statement tails with clause-level productions.
3. Add AST construction through arena-allocated C nodes.
4. Expand differential tests against MySQL 8.4.9 for accepted syntax, parse
   errors, SQL modes, comments, and quoted text behavior.
