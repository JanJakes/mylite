# Lemon parser prototype

This branch introduces the first MyLite-owned C parser prototype using
SQLite's Lemon parser generator.

## Scope

The current parser is intentionally syntax-light, but it is no longer a pure
token sink:

- A MySQL-aware lexer recognizes strings, quoted identifiers, executable
  version comments, regular comments, numbers, identifiers, delimiters,
  operator-like tokens, and the current statement-start keywords.
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
  structurally, with numeric remote clone ports and string-literal clone
  passwords and data directories.
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
  `LIMIT` suffixes, malformed `SELECT` operands after set operators before
  `WITH CHECK OPTION`, and `VALUES ROW(...)` row-expression tails.
- `CALL` recognizes one- and two-part routine names plus comma-separated
  argument lists with nested expression bodies, while rejecting malformed
  argument adjacent operands, dangling operators, and invalid plain
  parenthesized groups at top level and in stored-program body sinks.
- `DO` recognizes comma-separated expression lists while rejecting dangling
  separators, dangling operators, malformed nested expression groups, adjacent
  top-level operands, and top-level query-clause tails.
- `INSERT` and `REPLACE` recognize empty and comma-separated column lists before
  write payloads, and validate `SET` assignment lists including repeated-`SET`
  continuations, malformed top-level assignment value adjacent operands and
  dangling operators, adjacent operands, dangling operators, and trailing
  separators inside plain parenthesized assignment value groups, explicit
  `VALUE(S)` row-list tails, `ROW(...)` constructors, `INSERT`
  `VALUE(S)` row expression lists with empty rows plus adjacent-operand,
  dangling-operator, and trailing-separator checks, `INSERT` `VALUE(S)`/`SET`
  row aliases,
  parenthesized query payload `ORDER BY` and `LIMIT` suffixes, malformed
  `SELECT` operands after set operators, and `ON DUPLICATE KEY UPDATE`
  assignment tails including malformed post-value continuations and stray
  top-level `SELECT`/`FROM` suffixes after assignment values.
- Single-table `UPDATE` validates `SET` assignment lists, malformed top-level
  assignment value adjacent operands and dangling operators, adjacent operands,
  dangling operators, and trailing separators inside plain parenthesized
  assignment/`WHERE`/`ORDER BY` expression groups, plus `WHERE`, `ORDER BY`, and
  `LIMIT` tails.
- Single-table `DELETE` recognizes table aliases before optional partition
  lists, plus `WHERE`, `ORDER BY`, and `LIMIT` tails, rejecting incomplete
  DML clause tails, invalid top-level `ORDER BY` direction sequences, malformed
  `WHERE`/`ORDER BY` adjacent operands and dangling operators including inside
  plain parenthesized expression groups, trailing separators inside those
  groups, and out-of-order top-level DML clauses.
- `VALUES` recognizes comma-separated row contents while preserving nested
  expression bodies, rejects adjacent operands and dangling operators in row
  expression lists, applies the same checks in CTAS, view, and DML
  set-operation `VALUES` bodies, preserves set operators, validates
  parenthesized `ORDER BY` expressions plus `ORDER BY`/`LIMIT` tails, and
  malformed `SELECT` operands after set operators are rejected.
- `TABLE` recognizes table references, set operators, `ORDER BY`, `LIMIT`
  forms, `INTO` variable lists, and file output targets, and malformed `SELECT`
  operands after set operators are rejected. Embedded `TABLE` statements reuse
  a C-side validator for required targets plus dangling `ORDER BY`, `LIMIT`, and
  `INTO` tails.
- Expression-tail validation rejects trailing separators in nested plain
  parenthesized expression groups and empty `EXISTS()` predicates while
  preserving empty ordinary function calls and window `OVER()` clauses. It also
  rejects `BETWEEN` expressions that omit the lower bound before `AND`.
- `WITH` CTE wrappers dispatch their main query/body forms through the same
  validators used by top-level `SELECT`, `TABLE`, `VALUES`, parenthesized query,
  and DML statements, and validate CTE-body `SELECT` list tails.
- Top-level `SELECT` recognizes `SQL_NO_CACHE` as a deprecated MySQL 8.4
  modifier token.
- Top-level `SELECT` rejects incompatible `ALL` and `DISTINCT` modifiers.
- Top-level and set-operand `SELECT` rejects empty or dangling select-list
  expressions before query clause and statement boundaries.
- Top-level `SELECT` rejects missing operands for major clause starts such as
  `FROM`, `WHERE`, `GROUP BY`, `HAVING`, `ORDER BY`, `LIMIT`, and `INTO`.
- Top-level `SELECT` rejects trailing top-level comma separators in select
  lists and major clauses.
- Top-level `SELECT` rejects missing operands for top-level `JOIN`, `ON`, and
  `USING` join clauses.
- Top-level `SELECT` rejects `ON` and `USING` clauses that are not attached to
  a preceding joined table, including nested outer-join condition chains.
- Top-level `SELECT` validates `GROUP BY ... WITH ROLLUP` tails.
- Top-level `SELECT` validates `ORDER BY` `ASC`/`DESC` direction tails.
- Top-level `SELECT` validates `LIMIT` comma, `OFFSET`, and adjacent-operand
  forms.
- Top-level `SELECT` rejects duplicate major clauses within one query block,
  including real `FROM` clauses while preserving `NTH_VALUE(... FROM
  FIRST|LAST ... OVER ...)` window-function modifiers.
- Top-level `SELECT` rejects out-of-order major clauses across `WHERE`,
  `GROUP BY`, `HAVING`, `WINDOW`, `QUALIFY`, `ORDER BY`, `LIMIT`, and locking
  tails.
- Top-level `SELECT` validates named `WINDOW name AS (...)` clause lists.
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
- Top-level `SELECT` requires `INTO OUTFILE` and `INTO DUMPFILE` to include
  the mandatory file-name string.
- Top-level `SELECT ... INTO OUTFILE` validates the basic `CHARACTER SET`,
  `FIELDS`/`COLUMNS`, and `LINES` option tails, and file-output targets reject
  stray suffixes that are not valid following `SELECT` clauses.
- Top-level `SELECT` also rejects incomplete `UNION`/`INTERSECT`/`EXCEPT`
  set operations and repeated set-operation option tokens.
- Top-level parenthesized query expressions reject stray suffixes after the
  outer `)`, validate `ORDER BY` expression-list tails plus `LIMIT`/`INTO`
  continuations, validate `INTO` variable-list tails, and require set-operation
  tails to start with a query operand whose `SELECT` clause tail is well formed.
- `HANDLER` recognizes one- and two-part table names, aliases, key names,
  MySQL's table-scan and indexed-read direction sets, equality/range tuple
  reads, `WHERE`, and numeric or identifier `LIMIT` tails, while rejecting
  malformed tuple expressions, malformed `WHERE` adjacent operands, dangling
  operators, and `LIMIT` suffixes after `READ`.
- `USE` recognizes one-part schema names using the shared identifier grammar.
- Account and role names use the shared unreserved identifier grammar across
  `CREATE`/`ALTER`/`DROP` account statements.
- `RENAME USER` reuses the shared account-reference grammar for source and
  destination account pairs.
- `CREATE USER` and `ALTER USER` recognize account lists, authentication
  clauses, repeatable TLS/resource/password/lock options, string-literal
  comments and attributes, string-literal authentication/TLS values, and
  default-role clauses rather than generic token tails, including numeric
  resource limits and password policy counts, MFA, initial-authentication, and
  WebAuthn registration syntax.
- `GRANT` and `REVOKE` recognize grant/admin options, proxy forms, recipient
  authentication/resource clauses, `AS ... WITH ROLE`, and
  `IGNORE UNKNOWN USER`.
- `CREATE INDEX` recognizes non-empty functional/key-part lists and standalone
  index options including parser plugins, string-literal comments, visibility,
  string-literal attributes, numeric `KEY_BLOCK_SIZE`, closed `USING`/`TYPE`
  values, `ALGORITHM`, and `LOCK`,
  while validating standalone key-part prefix lengths and `ASC`/`DESC` tails.
- `CREATE TABLE` table-definition bodies require non-empty comma-separated
  elements while preserving nested token bodies for column and constraint
  definitions, and validate table-level index key-part prefix lengths plus
  `ASC`/`DESC` tails and closed `USING`/`TYPE` values plus table-level
  foreign-key child/reference column-list envelopes and referential-action
  tails. Column and table `CHECK` constraints
  require non-empty parenthesized expression bodies, and `CHECK` constraints
  validate `ENFORCED`/`NOT ENFORCED` tails while allowing later column
  attributes where MySQL permits them. Column definitions require a known MySQL
  type-family start plus common attribute starts and selected closed attribute
  values, require generated-column storage modifiers to follow generated
  expressions, reject repeated generated-column storage modifiers, and
  column-level `REFERENCES` clauses validate the referenced column-list envelope
  plus referential-action tails. Trailing table options
  must start with known MySQL table-option keywords and validate selected value
  domains, including numeric options, string-literal options, closed
  `INSERT_METHOD`, closed `ROW_FORMAT`, storage values, `UNION` lists, and
  `START TRANSACTION`. No-definition and post-definition CTAS forms are
  recognized explicitly with table/partition options, including
  `IGNORE`/`REPLACE` duplicate-handling modifiers, and no-definition
  table-option forms must include a query body. Parenthesized CTAS query bodies
  validate their outer `ORDER BY` and `LIMIT` suffixes.
- `CREATE LOGFILE GROUP` and `ALTER LOGFILE GROUP` recognize `ADD UNDOFILE`,
  string-literal file names, documented NDB logfile options with numeric
  size/nodegroup values, `WAIT`, and optional `ENGINE`/`STORAGE ENGINE`
  clauses.
- `CREATE TABLESPACE` and `ALTER TABLESPACE` recognize documented
  string-literal data files, numeric size, `WAIT`, `'Y'`/`'N'` encryption,
  optional-equals engine/storage-engine options, and string-literal attribute
  clauses. UNDO tablespaces use MySQL's narrower option list: required
  string-literal create data files plus optional `ENGINE`/`STORAGE ENGINE`.
  Drop tablespace/logfile tails recognize MySQL option lists including `ENGINE`,
  `STORAGE ENGINE`,
  `WAIT`, and `NO_WAIT`.
- `CREATE SERVER` and `ALTER SERVER` recognize the documented foreign-server
  `OPTIONS` names, string-valued options, and numeric ports.
- `CREATE EVENT` recognizes event schedules with `AT` timestamps and `EVERY`
  intervals, validates interval units plus `STARTS`/`ENDS` schedule tails, and
  recognizes scheduler status tails including `DISABLE ON REPLICA` and
  deprecated `DISABLE ON SLAVE` immediately after the schedule. Recognized
  single-statement event bodies are routed through the existing statement
  validators, including direct `SELECT` list-tail checks.
- `CREATE TRIGGER` recognizes timing, event, target table, optional ordering,
  and stored-program statement starts for single-statement trigger bodies, and
  recognized body starts including flow-control conditions reuse existing
  statement validators, including direct `SELECT` list-tail checks.
- `ALTER INSTANCE` recognizes redo-log enable/disable, InnoDB/binlog master-key
  rotation, TLS reload with channel/no-rollback options, and keyring reload.
- `ALTER TABLE` recognizes selected closed actions including `FORCE`,
  `ENABLE/DISABLE KEYS`, `ADD`/`CHANGE`/`MODIFY` heads, `RENAME` forms,
  comma-separated `ADD`/`CHANGE`/`MODIFY` bodies, `DROP` forms, column-drop
  `RESTRICT`/`CASCADE` tails, `ALTER` subactions, charset/order changes,
  validated `ALTER ... SET DEFAULT` expression tails, validated `ORDER BY`
  expression-list tails, partition definition, maintenance/exchange,
  reorganize forms, validated partition-method expression lists, maintenance
  binlog modifiers, CHECK/REPAIR maintenance options, and numeric coalesce counts,
  tablespace/storage/union changes with closed storage values,
  secondary-engine load/unload actions,
  table option changes with numeric/boolean/default value domains, closed
  `INSERT_METHOD` and `ROW_FORMAT` values, string-literal data/index
  directories, string-literal compression/password/connection/engine
  attributes, `AUTOEXTEND_SIZE`, `TABLE_CHECKSUM`, `START TRANSACTION`,
  `ALGORITHM`/`LOCK` options, tablespace discard/import forms, and validated
  `ADD`/`CHANGE`/`MODIFY` column type starts plus column-definition attribute
  starts and selected closed attribute values including generated-column storage
  modifier context and repetition checks, index key-part prefix lengths,
  `ASC`/`DESC` tails, and index option values including closed index
  `USING`/`TYPE` values. `ADD FOREIGN KEY` child/reference column-list
  envelopes and referential-action tails are also validated, and `ADD CHECK`
  requires a non-empty parenthesized expression body plus a valid
  `ENFORCED`/`NOT ENFORCED` tail when present.
  `DROP`/`EXCHANGE`/`REORGANIZE PARTITION` require concrete partition names;
  `REORGANIZE PARTITION` also requires a non-empty `INTO (...)` body.
  Table-level `ENGINE_ATTRIBUTE` and `SECONDARY_ENGINE_ATTRIBUTE` changes
  require string literals.
- `CREATE DATABASE` and `ALTER DATABASE` recognize schema names, charset,
  collation, `'Y'`/`'N'` encryption, and alter-only `READ ONLY` option clauses
  with MySQL's `DEFAULT`/`0`/`1` value grammar.
- `CREATE EVENT` and `ALTER EVENT` recognize ordered event metadata clauses
  for schedules, completion policy, enablement state, comments, and event
  bodies, and reject malformed event schedule tails; `ALTER EVENT` also
  recognizes renames.
- `ALTER FUNCTION` and `ALTER PROCEDURE` recognize routine characteristics:
  comments, `LANGUAGE SQL`, SQL data access, and SQL security.
- Spatial reference system DDL recognizes the MySQL 8.4 `IF [NOT] EXISTS`,
  `OR REPLACE`, numeric SRS ids, documented string-literal attribute forms, and
  numeric organization authority codes.
- `DROP INDEX` recognizes MySQL's `ALGORITHM` and `LOCK` option tails.
- `TRUNCATE TABLE` recognizes optional `TABLE` and one- or two-part table
  references using the shared identifier grammar.
- `INSTALL PLUGIN` and `UNINSTALL PLUGIN` recognize plugin names using the
  shared identifier grammar.
- `INSTALL COMPONENT` and `UNINSTALL COMPONENT` recognize string-literal
  component file lists and optional scoped `SET` assignments for installs,
  while rejecting malformed component assignment values.
- `ANALYZE TABLE` recognizes table lists and histogram update/drop clauses using
  the shared identifier grammar for table and column names, with numeric
  histogram bucket counts, MySQL 8.4 automatic/manual histogram update modes,
  single-table histogram clauses, and single-column string-literal histogram
  data.
- `CHECK TABLE`, `CHECKSUM TABLE`, `OPTIMIZE TABLE`, and `REPAIR TABLE`
  recognize table lists and their documented parser-level option keywords.
- Resource group DDL and utility statements recognize MySQL 8.4 resource
  attributes, numeric VCPU ranges, numeric thread priorities, force modifiers,
  and numeric thread-id assignment lists.
- `START REPLICA` recognizes `IO_THREAD`/`RELAY_THREAD` and `SQL_THREAD`,
  `UNTIL`, connection, and channel clauses with string-literal log/GTID/user
  option values and numeric log-position values. Source/relay log coordinate
  pairs are accepted in either MySQL parser order, and replication channel names
  use MySQL's string-literal channel grammar. Removed `START SLAVE` and
  `STOP SLAVE` syntax is
  permissive-corpus-only.
- `SHOW REPLICA STATUS` and `SHOW REPLICAS` recognize the current MySQL 8.4
  spellings. Removed `SHOW SLAVE STATUS` and `SHOW SLAVE HOSTS` syntax is
  permissive-corpus-only.
- `RESET` recognizes comma-separated MySQL 8.4 reset options, including
  `RESET BINARY LOGS AND GTIDS` with optional numeric `TO` index values and
  `RESET REPLICA` channel clauses. Removed `RESET MASTER` and `RESET SLAVE`
  syntax is permissive-corpus-only.
- `BINLOG` requires a string-literal payload, `PURGE BINARY LOGS ... TO`
  requires a string-literal log name, and `PURGE BINARY LOGS ... BEFORE`
  validates expression tails. Removed `PURGE MASTER LOGS` syntax is
  permissive-corpus-only.
- `KILL` recognizes optional `CONNECTION`/`QUERY` modes, expression-style
  targets including function-call tails, and exact user-variable targets, while
  rejecting extra top-level tokens after the target.
- `LOCK TABLES` recognizes table lists, aliases, and MySQL 8.4 `READ [LOCAL]`
  and `WRITE` lock types using the shared identifier grammar for alias names.
  Removed `LOW_PRIORITY WRITE` syntax is permissive-corpus-only.
- `LOCK INSTANCE FOR BACKUP`, `UNLOCK INSTANCE`, and `UNLOCK TABLES` have
  closed statement shapes.
- `LOAD DATA` and `LOAD XML` recognize file modifiers, duplicate handling,
  optional `FROM`, `INFILE`/`URL`/`S3` sources, string-literal file names,
  source counts and primary-key-order hints, partition or row-matching clauses,
  character sets, compression, field/line options, numeric ignored-row counts,
  column/user-variable lists, validated comma-separated `SET` assignment tails,
  and bulk-load parallel, memory, and algorithm options where they are
  unambiguous with `SET` assignment tails.
  `LOAD DATA` partition names use the shared identifier grammar.
- `LOAD INDEX INTO CACHE` recognizes MySQL's single-table partition form,
  `ALL`, optional empty or named key/index lists, comma-separated non-partition
  table lists, and `IGNORE LEAVES`.
- `IMPORT TABLE` recognizes comma-separated string-literal file lists.
- `EXPLAIN` and `DESCRIBE` recognize table-description forms, explainable
  statement starts including `TABLE` with validated query/DML tails, and
  numeric `FOR CONNECTION` ids with optional `FORMAT` clauses.
- `CHANGE REPLICATION FILTER` recognizes the MySQL 8.4 replication filter names,
  parenthesized rule lists, rewrite-db pairs, and optional channel clauses.
- `CHANGE REPLICATION SOURCE TO` recognizes documented MySQL 8.4 source option
  names, typed numeric/boolean option values, string-or-`NULL` option values,
  `IGNORE_SERVER_IDS` lists,
  privilege-check users, fixed primary-key-check enums, GTID assignment values,
  and optional channel clauses. Removed `CHANGE MASTER TO` syntax is
  permissive-corpus-only.
- Replication channel clauses share one identifier grammar across
  `START`/`STOP`/`RESET`/`SHOW`/`FLUSH` and `CHANGE ... FOR CHANNEL`.
- `SHOW PARSE_TREE` recognizes SELECT and WITH SELECT inputs as a
  debug/development SHOW form.
- `SHOW ENGINE ... STATUS|LOGS|MUTEX` recognizes engine names using the shared
  identifier grammar plus MySQL's `ALL` engine selector.
- Shared `SHOW ... LIKE` filters require string-literal patterns, while
  `SHOW ... WHERE` rejects adjacent bare operands and dangling operators while
  preserving common MySQL expression forms.
- `SHOW INDEX`/`INDEXES`/`KEYS` uses MySQL's narrower parser shape with
  optional `EXTENDED` and `WHERE`, excluding `FULL` and `LIKE`.
- `SHOW BINLOG EVENTS` and `SHOW RELAYLOG EVENTS` recognize optional
  string-literal log names, numeric `FROM` positions, and numeric or identifier
  `LIMIT` tails; relay-log events also recognize MySQL channel clauses.
- `SHOW BINARY LOG STATUS` is the strict MySQL 8.4 binary-log status form;
  removed `SHOW MASTER STATUS` syntax is permissive-corpus-only.
- `SHOW BINARY LOGS` is the strict MySQL 8.4 binary-log listing form; removed
  `SHOW MASTER LOGS` syntax is permissive-corpus-only.
- `SHOW PROFILE` recognizes profile type lists, numeric `FOR QUERY` ids, and
  numeric or identifier `LIMIT` tails.
- `SHOW WARNINGS` and `SHOW ERRORS` recognize numeric or identifier `LIMIT`
  tails.
- `SET ROLE` and `SET DEFAULT ROLE` recognize MySQL role specifiers and account
  lists rather than permissive token tails.
- `SET PASSWORD` recognizes MySQL 8.4 string-literal and random password
  assignment forms, including replacement and secondary-password clauses.
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
- Stored-program label and cursor references use the shared identifier grammar.
- Embedded stored-program `LEAVE` and `ITERATE` statements validate their
  required label arity.
- Embedded stored-program cursor statements validate `OPEN`, `FETCH`, and
  `CLOSE` arity, including `FETCH [[NEXT] FROM] cursor INTO target [, ...]`.
- `DECLARE`, `FETCH [[NEXT] FROM] ... INTO`, and named signal conditions use
  the shared identifier grammar for local names, with numeric MySQL error codes
  and five-character `SQLSTATE [VALUE]` literals for condition declarations and
  handler conditions. Handler bodies recognize compound blocks and stored
  program starts for flow-control, cursor, DML, diagnostics, and return
  statements.
- Local-variable `DECLARE` statements validate `DEFAULT` expression tails.
- Cursor `DECLARE` statements validate `SELECT`, `TABLE`, `VALUES`, `WITH`,
  and parenthesized query bodies, including CTE wrappers before the outer query
  body.
- Stored-program `IF`/`ELSEIF`, `WHILE`, `REPEAT ... UNTIL`, `WHEN`, and
  `RETURN` validate expression tails at their statement boundaries.
- Prepared-statement names and `EXECUTE ... USING` user-variable lists use the
  shared identifier grammar; `PREPARE ... FROM` accepts string-literal and
  user-variable sources.
- XA statements recognize one-, two-, and three-part string or hex XIDs with
  numeric `formatID` values.
- `CACHE INDEX` recognizes table, single-table partition, optional empty or
  named key/index, and key-cache names using the shared identifier grammar;
  `RESET PERSIST` uses the shared grammar for persisted-variable names.
- `GET DIAGNOSTICS` recognizes optional `CURRENT`/`STACKED` diagnostics-area
  selectors; assignment targets use local/user-variable grammar and condition
  numbers may come from literal/simple, user-variable, system-variable, and
  dotted identifier values. Embedded stored-program bodies validate assignment
  separators and diagnostics item names.
- `SIGNAL` and `RESIGNAL` recognize five-character `SQLSTATE [VALUE]` literals
  and condition item assignments with numeric `MYSQL_ERRNO`, literal/simple,
  user-variable, system-variable, and dotted local identifier values; embedded
  stored-program bodies reuse `SET` assignment expression validation.
- `SET NAMES` and `SET CHARACTER SET` recognize charset/collation names using
  the shared identifier grammar, `BINARY`, `DEFAULT`, optional collation, and
  comma-following variable assignments.
- `SET` variable assignments recognize comma-separated assignment lists with
  optional per-assignment scopes and nested value expressions, reject malformed
  non-`@@` assignment values with adjacent operands, dangling operators, and
  invalid plain parenthesized groups, preserve `@@` system-variable values as
  token tails, and reject repeated top-level `SET` continuations after
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
- `FLUSH` recognizes `LOCAL`/`NO_WRITE_TO_BINLOG` modifiers, simple option
  lists, table forms, log variants, and channel-qualified relay logs. Removed
  `FLUSH HOSTS` syntax is permissive-corpus-only.
- `RESTART`, `SHUTDOWN`, and `HELP` recognize their closed parser-level
  statement shapes.
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
