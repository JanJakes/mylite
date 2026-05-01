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
- Closed keyword subgrammars for routine characteristics, table options,
  diagnostics items, replication options, profile types, UDF return types, and
  other statement modifiers are represented directly in Lemon rather than
  validated by C string checks.
- MySQL 8.4 administration forms such as `SHOW BINARY LOG STATUS` and
  `CLONE INSTANCE ... [DATA DIRECTORY ...] [REQUIRE [NO] SSL]` are recognized
  structurally.
- Account-reference forms that permit `CURRENT_USER` or `CURRENT_USER()` now
  have explicit grammar productions.
- `CREATE USER` and `ALTER USER` recognize account lists, authentication
  clauses, TLS/resource/password/lock options, comments, attributes, and
  default-role clauses rather than generic token tails, including MFA,
  initial-authentication, and WebAuthn registration syntax.
- `GRANT` and `REVOKE` recognize grant/admin options, proxy forms, recipient
  authentication/resource clauses, `AS ... WITH ROLE`, and
  `IGNORE UNKNOWN USER`.
- `CREATE INDEX` recognizes functional/key-part lists and standalone index
  options including parser plugins, visibility, attributes, `ALGORITHM`, and
  `LOCK`.
- `CREATE LOGFILE GROUP` and `ALTER LOGFILE GROUP` recognize `ADD UNDOFILE`,
  documented NDB logfile options, and required `ENGINE` clauses.
- `CREATE TABLESPACE`, `CREATE UNDO TABLESPACE`, and `ALTER TABLESPACE`
  recognize documented data-file, size, encryption, engine, and attribute
  clauses.
- `CREATE SERVER` and `ALTER SERVER` recognize the documented foreign-server
  `OPTIONS` names and value requirements.
- `ALTER TABLE` recognizes selected closed actions including `FORCE`,
  `ENABLE/DISABLE KEYS`, `RENAME` forms, `ALGORITHM`/`LOCK` options, and
  tablespace discard/import forms.
- `CREATE DATABASE` and `ALTER DATABASE` recognize charset, collation,
  encryption, and alter-only `READ ONLY` option clauses.
- `CREATE EVENT` and `ALTER EVENT` recognize ordered event metadata clauses
  for schedules, completion policy, enablement state, comments, and event
  bodies; `ALTER EVENT` also recognizes renames.
- `ALTER FUNCTION` and `ALTER PROCEDURE` recognize routine characteristics:
  comments, `LANGUAGE SQL`, SQL data access, and SQL security.
- Spatial reference system DDL recognizes the MySQL 8.4 `IF [NOT] EXISTS`,
  `OR REPLACE`, and documented attribute forms.
- `DROP INDEX` recognizes MySQL's `ALGORITHM` and `LOCK` option tails.
- Resource group DDL and utility statements recognize MySQL 8.4 resource
  attributes, VCPU ranges, force modifiers, and thread-id assignment lists.
- `LOAD DATA` and `LOAD XML` recognize file modifiers, duplicate handling,
  partition or row-matching clauses, character sets, field/line options,
  ignored rows, column lists, and `SET` tails.
- `CHANGE REPLICATION FILTER` recognizes the MySQL 8.4 replication filter names,
  parenthesized rule lists, rewrite-db pairs, and optional channel clauses.
- `SHOW PARSE_TREE` recognizes SELECT and WITH SELECT inputs as a
  debug/development SHOW form.
- `SET ROLE` and `SET DEFAULT ROLE` recognize MySQL role specifiers and account
  lists rather than permissive token tails.
- `SET PASSWORD` recognizes MySQL 8.4 literal and random password assignment
  forms, including replacement and secondary-password clauses.
- `SET TRANSACTION` recognizes GLOBAL/SESSION/LOCAL scope, isolation levels, and
  read access modes without permissive tails.
- `SET NAMES` and `SET CHARACTER SET` recognize their charset/default forms,
  optional collation, and comma-following variable assignments.
- `EXPLAIN FORMAT=JSON INTO @var` is recognized as a JSON-only EXPLAIN form.
- `EXPLAIN ... FOR SCHEMA|DATABASE name` schema specifiers are recognized.
- `DESCRIBE` and `DESC` reuse the EXPLAIN syntax variants for execution-plan
  statements while preserving table and column description forms.
- `EXPLAIN ANALYZE` has explicit statement-start handling and rejects
  unsupported non-`TREE` format names.
- `ALTER TABLESPACE` recognizes `ADD DATAFILE` and `DROP DATAFILE` actions.
- `SHOW CREATE DATABASE` and `SHOW CREATE SCHEMA` recognize optional
  `IF NOT EXISTS`.
- `FLUSH` recognizes engine, general, slow, and channel-qualified relay log
  variants.
- A permissive mode accepts extracted corpus fragments that are not standalone
  MySQL statements.
- The lexer is recoverable for corpus rows that come from MySQL negative tests,
  including unterminated quoted text.
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
