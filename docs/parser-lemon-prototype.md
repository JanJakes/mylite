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
  structurally, with numeric remote clone ports.
- Account-reference forms that permit `CURRENT_USER` or `CURRENT_USER()` now
  have explicit grammar productions.
- Stored function signatures require empty or comma-separated input parameter
  lists, and stored procedure signatures accept `IN`/`OUT`/`INOUT` modes while
  preserving nested type bodies.
- Loadable UDF declarations and plugin installs require string-literal
  `SONAME` values.
- `CREATE VIEW` and `ALTER VIEW` recognize algorithm, definer, SQL security,
  column lists, view body starts, and explicit check-option tails for
  non-`SELECT` body forms.
- `CALL` recognizes one- and two-part routine names plus comma-separated
  argument lists with nested expression bodies.
- `INSERT` and `REPLACE` recognize empty and comma-separated column lists before
  write payloads.
- `VALUES` recognizes comma-separated row contents while preserving nested
  expression bodies.
- `HANDLER` recognizes one- and two-part table names, aliases, key names, read
  directions, tuple reads, `WHERE`, and numeric `LIMIT` tails.
- `USE` recognizes one-part schema names using the shared identifier grammar.
- Account and role names use the shared unreserved identifier grammar across
  `CREATE`/`ALTER`/`DROP` account statements.
- `RENAME USER` reuses the shared account-reference grammar for source and
  destination account pairs.
- `CREATE USER` and `ALTER USER` recognize account lists, authentication
  clauses, TLS/resource/password/lock options, string-literal comments and
  attributes, and default-role clauses rather than generic token tails,
  including numeric resource limits and password policy counts, MFA,
  initial-authentication, and WebAuthn registration syntax.
- `GRANT` and `REVOKE` recognize grant/admin options, proxy forms, recipient
  authentication/resource clauses, `AS ... WITH ROLE`, and
  `IGNORE UNKNOWN USER`.
- `CREATE INDEX` recognizes non-empty functional/key-part lists and standalone
  index options including parser plugins, string-literal comments, visibility,
  attributes, numeric `KEY_BLOCK_SIZE`, `ALGORITHM`, and `LOCK`.
- `CREATE TABLE` table-definition bodies require non-empty comma-separated
  elements while preserving nested token bodies for column and constraint
  definitions; trailing table options must start with known MySQL table-option
  keywords, and post-definition CTAS forms are recognized explicitly.
- `CREATE LOGFILE GROUP` and `ALTER LOGFILE GROUP` recognize `ADD UNDOFILE`,
  string-literal file names, documented NDB logfile options with numeric
  size/nodegroup values, and required `ENGINE` clauses.
- `CREATE TABLESPACE`, `CREATE UNDO TABLESPACE`, `ALTER TABLESPACE`, and
  undo/drop tablespace tails recognize documented string-literal data files,
  numeric size, `'Y'`/`'N'` encryption, optional-equals engine, and attribute
  clauses.
- `CREATE SERVER` and `ALTER SERVER` recognize the documented foreign-server
  `OPTIONS` names, string-valued options, and numeric ports.
- `ALTER INSTANCE` recognizes redo-log enable/disable, InnoDB/binlog master-key
  rotation, TLS reload with channel/no-rollback options, and keyring reload.
- `ALTER TABLE` recognizes selected closed actions including `FORCE`,
  `ENABLE/DISABLE KEYS`, `ADD`/`CHANGE`/`MODIFY` heads, `RENAME` forms,
  comma-separated `ADD`/`CHANGE`/`MODIFY` bodies, `DROP` forms,
  `ALTER` subactions, charset/order changes, partition definition,
  maintenance/exchange, reorganize forms, and numeric coalesce counts,
  tablespace/storage/union changes,
  table option changes with numeric/boolean/default value domains,
  `ALGORITHM`/`LOCK` options, and tablespace discard/import forms.
  `DROP`/`EXCHANGE`/`REORGANIZE PARTITION` require concrete partition names;
  `REORGANIZE PARTITION` also requires a non-empty `INTO (...)` body.
- `CREATE DATABASE` and `ALTER DATABASE` recognize schema names, charset,
  collation, `'Y'`/`'N'` encryption, and alter-only `READ ONLY` option clauses
  with MySQL's `DEFAULT`/`0`/`1` value grammar.
- `CREATE EVENT` and `ALTER EVENT` recognize ordered event metadata clauses
  for schedules, completion policy, enablement state, comments, and event
  bodies; `ALTER EVENT` also recognizes renames.
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
  component file lists and optional scoped `SET` assignments for installs.
- `ANALYZE TABLE` recognizes table lists and histogram update/drop clauses using
  the shared identifier grammar for table and column names, with numeric
  histogram bucket counts and string-literal histogram data.
- `CHECK TABLE`, `CHECKSUM TABLE`, `OPTIMIZE TABLE`, and `REPAIR TABLE`
  recognize table lists and their documented parser-level option keywords.
- Resource group DDL and utility statements recognize MySQL 8.4 resource
  attributes, numeric VCPU ranges, numeric thread priorities, force modifiers,
  and numeric thread-id assignment lists.
- `START REPLICA` and legacy `START SLAVE` recognize thread, `UNTIL`,
  connection, and channel clauses with string-literal log/GTID/user option
  values and numeric log-position values.
- `RESET BINARY LOGS AND GTIDS` recognizes optional numeric `TO` index values.
- `BINLOG` requires a string-literal payload, and `PURGE BINARY LOGS ... TO`
  requires a string-literal log name.
- `KILL` recognizes optional `CONNECTION`/`QUERY` modes and literal,
  shared-identifier local-variable, or user-variable targets.
- `LOCK TABLES` recognizes table lists, aliases, and MySQL lock types using the
  shared identifier grammar for alias names.
- `LOCK INSTANCE FOR BACKUP`, `UNLOCK INSTANCE`, and `UNLOCK TABLES` have
  closed statement shapes.
- `LOAD DATA` and `LOAD XML` recognize file modifiers, duplicate handling,
  string-literal file names, partition or row-matching clauses, character sets,
  field/line options, numeric ignored-row counts, column/user-variable lists,
  and `SET` tails.
  `LOAD DATA` partition names use the shared identifier grammar.
- `LOAD INDEX INTO CACHE` recognizes table/key lists, partition lists, `ALL`,
  and `IGNORE LEAVES`.
- `IMPORT TABLE` recognizes comma-separated string-literal file lists.
- `EXPLAIN` and `DESCRIBE` recognize numeric `FOR CONNECTION` ids.
- `CHANGE REPLICATION FILTER` recognizes the MySQL 8.4 replication filter names,
  parenthesized rule lists, rewrite-db pairs, and optional channel clauses.
- `CHANGE REPLICATION SOURCE TO` recognizes documented MySQL 8.4 source option
  names, legacy `CHANGE MASTER TO` option names, typed numeric/boolean option
  values, generic string/list/enum values, and optional channel clauses.
- Replication channel clauses share one identifier grammar across
  `START`/`STOP`/`RESET`/`SHOW`/`FLUSH` and `CHANGE ... FOR CHANNEL`.
- `SHOW PARSE_TREE` recognizes SELECT and WITH SELECT inputs as a
  debug/development SHOW form.
- `SHOW ENGINE ... STATUS|LOGS|MUTEX` recognizes engine names using the shared
  identifier grammar.
- Shared `SHOW ... LIKE` filters require string-literal patterns, while
  `SHOW ... WHERE` keeps using the general expression tail.
- `SHOW BINLOG EVENTS` and `SHOW RELAYLOG EVENTS` recognize optional
  string-literal log names, numeric `FROM` positions, and numeric `LIMIT` tails.
- `SHOW PROFILE` recognizes profile type lists, numeric `FOR QUERY` ids, and
  numeric `LIMIT` tails.
- `SHOW WARNINGS` and `SHOW ERRORS` recognize numeric `LIMIT` tails.
- `SET ROLE` and `SET DEFAULT ROLE` recognize MySQL role specifiers and account
  lists rather than permissive token tails.
- `SET PASSWORD` recognizes MySQL 8.4 string-literal and random password
  assignment forms, including replacement and secondary-password clauses.
- `SET TRANSACTION` recognizes GLOBAL/SESSION/LOCAL scope, isolation levels, and
  read access modes without permissive tails.
- `SAVEPOINT`, `ROLLBACK TO SAVEPOINT`, and `RELEASE SAVEPOINT` recognize
  savepoint names using the shared identifier grammar.
- Stored-program label and cursor references use the shared identifier grammar.
- `DECLARE`, `FETCH ... INTO`, and named signal conditions use the shared
  identifier grammar for local names, with five-character SQLSTATE literals.
- Prepared-statement names, `PREPARE ... FROM` user-variable sources, and
  `EXECUTE ... USING` user-variable lists use the shared identifier grammar.
- XA statements recognize one-, two-, and three-part XIDs with numeric
  `formatID` values.
- `CACHE INDEX` and `RESET PERSIST` use the shared identifier grammar for
  key-cache and persisted-variable names.
- `GET DIAGNOSTICS` assignment targets use local/user-variable grammar and
  condition numbers may come from user variables.
- `SIGNAL` and `RESIGNAL` recognize five-character SQLSTATE literals and
  condition item assignments with numeric `MYSQL_ERRNO` values.
- `SET NAMES` and `SET CHARACTER SET` recognize charset/collation names using
  the shared identifier grammar, `BINARY`, `DEFAULT`, optional collation, and
  comma-following variable assignments.
- `SET` variable assignments recognize comma-separated assignment lists with
  optional per-assignment scopes and nested value expressions.
- `EXPLAIN FORMAT=JSON INTO @var` is recognized as a JSON-only EXPLAIN form
  with user-variable targets.
- `EXPLAIN ... FOR SCHEMA|DATABASE name` schema specifiers are recognized.
- `DESCRIBE` and `DESC` reuse the EXPLAIN syntax variants for execution-plan
  statements while preserving table and column description forms with shared
  identifier handling.
- `EXPLAIN ANALYZE` has explicit statement-start handling and rejects
  unsupported non-`TREE` format names.
- `ALTER TABLESPACE` recognizes `ADD DATAFILE` and `DROP DATAFILE` actions.
- `SHOW CREATE DATABASE` and `SHOW CREATE SCHEMA` recognize optional
  `IF NOT EXISTS`.
- `SHOW CREATE USER` reuses the shared account-reference grammar.
- `FLUSH` recognizes `LOCAL`/`NO_WRITE_TO_BINLOG` modifiers, simple option
  lists, table forms, log variants, and channel-qualified relay logs.
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
