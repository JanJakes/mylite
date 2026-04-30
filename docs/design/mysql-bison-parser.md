# MySQL Bison Parser Prototype

## Goal

MyLite needs a parser that can accept the MySQL 8.4 SQL surface before the
analyzer and SQLite translation layers are complete. This prototype establishes
the Bison-based parser boundary, a MySQL-aware lexer, a public parse API, and a
corpus gate against the WordPress SQLite Database Integration MySQL query set.

## Sources

- MySQL 8.4.9 parser grammar: `sql/sql_yacc.yy`
- MySQL 8.4 statement labels:
  `https://dev.mysql.com/doc/refman/8.4/en/statement-labels.html`
- MySQL 8.4 account-management SET statements:
  `https://dev.mysql.com/doc/refman/8.4/en/set-role.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/set-default-role.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/set-password.html`
- MySQL 8.4 account-introspection SHOW statements:
  `https://dev.mysql.com/doc/refman/8.4/en/show-create-user.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/show-grants.html`
- MySQL 8.4 component and plugin statements:
  `https://dev.mysql.com/doc/refman/8.4/en/install-component.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/uninstall-component.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/install-plugin.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/uninstall-plugin.html`
- WordPress SQLite Database Integration query corpus:
  `packages/mysql-on-sqlite/tests/mysql/data/mysql-server-tests-queries.csv`

## Design

The current parser is a syntax-acceptance milestone, not the final semantic
grammar. Bison owns statement sequencing and balanced structure. The lexer
handles MySQL comments, executable version-comment bodies, quoted identifiers,
string literals, numeric literals, user/system variables, parameter markers,
statement-leading keywords, and stored program `END IF` / `END LOOP` style
compound endings.
MySQL prefixed literals such as `_utf8mb4'text'`, `N'text'`, `X'ff'`, and
`B'1010'` are emitted as single literal tokens with source spans covering the
prefix and quoted body.

The parser records the full token stream, a statement kind, an optional target
object kind for DDL/admin statements and DML table operations, an optional first
target-name span, and source spans for each parsed statement. Spans include
token ordinals, byte offsets into the original SQL buffer, and line/column
endpoints for diagnostics and future AST nodes. Object-name spans preserve
exact source text, including backtick quoting and schema qualification. Balanced
structural tokens also carry bidirectional match references for `(...)`,
`[...]`, `{...}`, `BEGIN ... END`, and `CASE ... END`. The lexer classifies
statement-leading words, major clause words, common DDL/admin/transaction/load
words, boolean/null operators, and join/set operators as keywords so the future
analyzer does not need to rediscover them from identifier text. Keyword-like
nonreserved words that MySQL commonly permits as identifiers remain usable in
target object-name spans. The grammar validates that
grouping delimiters, `BEGIN ... END`, and `CASE ... END` blocks are balanced.
Known statement heads that require a body now reject a bare keyword, while
transaction statements that MySQL accepts as single-keyword statements remain
valid.

Statement bodies remain token-preserving and permissive so the prototype can
accept the broad MySQL statement inventory while detailed productions are added
statement by statement. `WITH` statements are post-classified by skipping
matched CTE subqueries and inspecting the outer DML verb, so `WITH ... UPDATE`,
`WITH ... DELETE`, and `WITH ... INSERT` do not collapse to `select`. DML
target-name classification uses the same matched-token data to skip CTE bodies
before locating the first affected table for `INSERT`, `REPLACE`, `UPDATE`, and
`DELETE`, including common priority, delayed, quick, and ignore modifiers.
Direct target metadata is also recorded for simple utility and table statements
where the target is syntactically unambiguous: `USE`, `TABLE`, `TRUNCATE`,
`HANDLER`, direct `DESCRIBE` / `EXPLAIN` table forms, `LOAD ... INTO TABLE`,
`LOCK TABLES`, `SHOW CREATE ...`, `SHOW COLUMNS` / `FIELDS`,
`SHOW INDEX` / `KEYS`, `SHOW TABLES FROM ...`, account targets in
`SHOW CREATE USER` and `SHOW GRANTS FOR`, and prepared-statement names in
`PREPARE`, `EXECUTE`, `DEALLOCATE PREPARE`, and `DROP PREPARE`. Component and
plugin targets are recorded for `INSTALL` and `UNINSTALL` administrative
statements. Principal targets are recorded for `GRANT ... TO` and
`REVOKE ... FROM`, including the first `user@host` span when present. Account
and role DDL target spans also
preserve `user@host` / `role@host` syntax for `CREATE`, `ALTER`, `DROP`, and
`RENAME` forms. Account-management `SET` metadata is recorded for explicit
`SET ROLE`, `SET DEFAULT ROLE`, and `SET PASSWORD FOR` role or account targets,
while session-variable `SET` statements remain objectless. Savepoint names are
recorded for `SAVEPOINT`, `RELEASE SAVEPOINT`, and `ROLLBACK TO SAVEPOINT`.
Statements that begin with parenthesized query expressions keep spans anchored
to the opening parenthesis and are classified as `SELECT`, `VALUES`, or `TABLE`
according to the innermost leading query token.
Stored-program statement heads such as `DECLARE`, cursor operations, `IF`,
`CASE`, loop forms, `LEAVE`, `ITERATE`, and `RETURN` have explicit statement
kinds. Compound-control tokens are structurally matched for `IF ... END IF`,
`LOOP ... END LOOP`, `REPEAT ... END REPEAT`, and `WHILE ... END WHILE` without
misclassifying `IF(...)` expressions or `IF [NOT] EXISTS` clauses as compound
block starts. Cursor names are recorded for `DECLARE ... CURSOR`, `OPEN`,
`FETCH`, and `CLOSE`. Jump target labels are recorded for `LEAVE` and
`ITERATE`. Label declarations are recorded when they prefix the MySQL-labeled
constructs: `BEGIN`, `LOOP`, `REPEAT`, and `WHILE`.

## Boundaries

- Produces a token stream and statement/object-kind/name-span AST shell only.
  Matching token pairs are structural metadata, not a full expression tree.
- Does not yet resolve identifiers, expression precedence, table references,
  metadata, warnings, or MySQL runtime errors.
- DML object metadata records the first syntactic target table span only. It
  does not yet resolve aliases, joined table references, partition clauses, or
  every affected table in multi-table statements.
- Utility object metadata records the first direct target only and does not yet
  expand multi-table maintenance or lock lists. `DESCRIBE` and `EXPLAIN`
  target metadata is deliberately conservative so query-plan forms such as
  `EXPLAIN SELECT` and `EXPLAIN FORMAT=... SELECT` remain objectless.
  `SHOW` metadata is similarly limited to forms with a clear table, view, or
  schema/account target. Prepared-statement metadata records the statement
  handle name, not the SQL text referenced by `PREPARE`. Component/plugin
  metadata records only the first target in multi-target statements.
- Account and principal metadata records the first syntactic account or role
  target only. It does not yet resolve roles, dynamic privileges, multiple
  accounts, proxy grants, account-name normalization, rename destinations, or
  implicit current-user/current-role targets in `SET` statements.
- Savepoint metadata records the named savepoint only. Bare `ROLLBACK` and
  non-savepoint `RELEASE` forms remain objectless.
- Parenthesized query-expression classification only identifies the leading
  query statement kind; it does not build the query-expression tree.
- Stored-program control matching records token pairs only; it does not yet
  validate label binding, declaration ordering, cursor scope, or control-flow
  semantics.
- Cursor metadata records the first cursor handle only. It does not yet validate
  declaration scope, result shape, fetch target lists, or cursor lifecycle.
- Label metadata records direct `LEAVE` / `ITERATE` targets and leading label
  declarations. It does not yet validate end labels, duplicate labels, the
  16-character label limit, or label binding.
- Skips ordinary comments. MySQL executable `/*! ... */` comments are tokenized
  as SQL because they can carry required syntax.
- Accepts unknown statement starts as `unknown`; later grammar work should
  reduce that surface as concrete productions land.
- Rejects bare known statement keywords such as `SELECT`, `CREATE`, and `SET`,
  but detailed clause-level syntax errors still require future productions.

## Verification

- `make smoke` builds the parser and checks representative MySQL syntax plus
  negative tests for unmatched delimiters and unterminated strings.
- `make corpus` downloads the WordPress corpus into `build/corpus/` and parses
  every query through the same CLI in NUL-delimited batch mode. The current
  corpus gate covers 69,577 records.
