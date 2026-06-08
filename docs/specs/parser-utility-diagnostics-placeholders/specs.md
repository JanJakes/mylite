# Parser utility and diagnostics placeholders

This slice broadens MyLite parser acceptance for MySQL 8.4.9 utility,
diagnostic, and server-managed statement families that appear frequently in the
MySQL server-test corpus. It keeps behavior explicit: statements whose effects
are meaningless in an embedded single-file runtime are accepted as no-ops with a
warning, while statements that would read or mutate user data, cursor state, or
transaction semantics are parsed and rejected with an unsupported diagnostic.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/analyze-table.html
- https://dev.mysql.com/doc/refman/8.4/en/xa-statements.html
- https://dev.mysql.com/doc/refman/8.4/en/handler.html
- https://dev.mysql.com/doc/refman/8.4/en/install-component.html
- https://dev.mysql.com/doc/refman/8.4/en/uninstall-component.html
- https://dev.mysql.com/doc/refman/8.4/en/install-plugin.html
- https://dev.mysql.com/doc/refman/8.4/en/uninstall-plugin.html
- https://dev.mysql.com/doc/refman/8.4/en/create-tablespace.html
- https://dev.mysql.com/doc/refman/8.4/en/drop-tablespace.html
- https://dev.mysql.com/doc/refman/8.4/en/get-diagnostics.html
- https://dev.mysql.com/doc/refman/8.4/en/show-profile.html
- https://dev.mysql.com/doc/refman/8.4/en/show-profiles.html
- https://dev.mysql.com/doc/refman/8.4/en/show-procedure-code.html
- https://dev.mysql.com/doc/refman/8.4/en/show-function-code.html
- https://dev.mysql.com/doc/refman/8.4/en/show-grants.html
- https://dev.mysql.com/doc/refman/8.4/en/set-variable.html
- https://dev.mysql.com/doc/refman/8.4/en/load-data.html

## Scope

### Embedded utility no-ops

MyLite accepts the following raw statement families after normal grammar parsing
fails:

- `ANALYZE TABLE ... UPDATE HISTOGRAM ...` and
  `ANALYZE TABLE ... DROP HISTOGRAM ...`;
- `INSTALL COMPONENT`, `UNINSTALL COMPONENT`, `INSTALL PLUGIN`, and
  `UNINSTALL PLUGIN`;
- `CREATE TABLESPACE`, `ALTER TABLESPACE`, and `DROP TABLESPACE`;
- unsupported server-global or parser-admitted system-variable `SET` variants
  such as broad `SET GLOBAL ...`, `SET @@GLOBAL...`, and unsupported
  persisted/session/global assignments not handled by the normal `SET` runtime.

The runtime behavior is:

- return success with no columns and no rows;
- report affected rows `0`;
- set `ROW_COUNT()` to `0`;
- append one warning, code `1105`, SQLSTATE `HY000`, message
  `MyLite accepted this utility statement as an embedded no-op`;
- leave catalogs, optimizer state, component/plugin registries, tablespace
  metadata, system variables, user data, and user transactions unchanged.

This intentionally differs from MySQL server side effects. MyLite has no
loadable component/plugin mechanism, physical tablespaces, histogram optimizer
statistics, mutable server-global variable store, binary logging, or shared
server lifecycle. Committing an active user transaction for these placeholders
would be more surprising than useful, so no implicit commit is performed.

Existing supported behavior remains separate:

- simple `ANALYZE TABLE` continues to use the table-maintenance implementation
  with MySQL-shaped result rows and existing implicit-commit behavior;
- supported `SET` statements still execute through the normal session-variable
  runtime;
- limited `LOAD DATA INFILE` and disabled `LOAD DATA LOCAL INFILE` behavior
  stays in the normal `LOAD DATA` parser/runtime path.

### Parse-and-error utility surface

MyLite accepts broad syntax for the following statement families but runtime
execution returns `1064 / 42000` with a clear unsupported message:

- `XA START`, `XA END`, `XA PREPARE`, `XA COMMIT`, `XA ROLLBACK`, `XA RECOVER`,
  and related XA forms;
- `HANDLER ... OPEN`, `HANDLER ... READ ...`, and `HANDLER ... CLOSE`;
- `GET DIAGNOSTICS`;
- `SHOW PROFILE` and `SHOW PROFILES`;
- `SHOW PROCEDURE CODE` and `SHOW FUNCTION CODE`;
- unsupported `LOAD DATA` forms outside the current implemented grammar, such
  as custom `FIELDS` / `LINES`, duplicate-handling `IGNORE`, `REPLACE`,
  charset clauses, user variables, and `SET` tails.

These are not no-ops because success would imply visible data access, explicit
XA transaction state, diagnostics-area reads, profiling rows, stored-program
debug output, or file-import behavior that MyLite does not yet implement.

### SHOW GRANTS named account

MyLite extends the existing `SHOW GRANTS` baseline to accept
`SHOW GRANTS FOR 'user'@'host'` for simple MySQL account names. The embedded
identity remains synthetic `root@%`:

- `SHOW GRANTS FOR 'root'@'%'` returns the same fixed global grant rows and
  column label as `SHOW GRANTS`;
- any other simple named account returns MySQL-shaped `1141 / 42000`
  diagnostics: `There is no such grant defined for user 'u' on host 'h'`;
- `USING`, role expansion, filters, account storage, and privilege enforcement
  remain unsupported.

The named-account behavior is based on the existing MySQL 8.4.9 expectation
script for `SHOW GRANTS`, which already records the `root@%` success and
missing-account error cases.

### SHOW PROCEDURE/FUNCTION STATUS WHERE

MyLite's current routine-status surface is an empty metadata result with
MySQL-shaped columns. This slice admits `WHERE` predicates on
`SHOW PROCEDURE STATUS` and `SHOW FUNCTION STATUS` and returns the same empty
result. Since MyLite currently has no persistent routine rows, the predicate
does not affect visible data. `LIKE` remains accepted as before.

## Parser approach

The normal Lemon grammar remains the authority for supported SQL. The existing
post-parse placeholder recognizer runs only after a syntax error, retokenizes
the single SQL input, and classifies broad leading-token families into raw AST
nodes:

- `admin_noop_statement`;
- `unsupported_stored_program_statement`;
- `utility_noop_statement`;
- `unsupported_utility_statement`.

The classifier accepts broad suffix syntax only for statement families whose
runtime behavior is placeholder-only. It still rejects multi-statement strings
for no-op placeholders. Parse-and-error placeholders can contain semicolons
inside stored-program-like bodies, but execution remains a single raw statement.

Normal grammar additions are limited to:

- simple named-account `SHOW GRANTS FOR 'user'@'host'`;
- `SHOW PROCEDURE STATUS WHERE ...` and `SHOW FUNCTION STATUS WHERE ...`.

## MyLite grammar snippets

These snippets describe the intended MyLite-owned grammar shape and fallback
classification. They are independently authored from official MySQL
documentation and observed runtime behavior.

```lemon
show_grants_statement ::= SHOW GRANTS.
show_grants_statement ::= SHOW GRANTS FOR CURRENT_USER.
show_grants_statement ::= SHOW GRANTS FOR CURRENT_USER LPAREN RPAREN.
show_grants_statement ::= SHOW GRANTS FOR STRING user_variable.

show_routine_status_statement ::= SHOW PROCEDURE STATUS show_catalog_filter_opt.
show_routine_status_statement ::= SHOW FUNCTION STATUS show_catalog_filter_opt.
show_catalog_filter_opt ::= .
show_catalog_filter_opt ::= LIKE STRING.
show_catalog_filter_opt ::= WHERE predicate.
```

Post-parse placeholder classifier:

```text
utility_noop_statement:
    ANALYZE TABLE ... UPDATE HISTOGRAM ...
  | ANALYZE TABLE ... DROP HISTOGRAM ...
  | INSTALL COMPONENT ...
  | UNINSTALL COMPONENT ...
  | INSTALL PLUGIN ...
  | UNINSTALL PLUGIN ...
  | CREATE TABLESPACE ...
  | ALTER TABLESPACE ...
  | DROP TABLESPACE ...
  | SET GLOBAL ...
  | SET @@GLOBAL...
  | SET SESSION ... unsupported-value-shape
  | SET @@SESSION... unsupported-value-shape

unsupported_utility_statement:
    XA ...
  | HANDLER ...
  | GET DIAGNOSTICS ...
  | SHOW PROFILE ...
  | SHOW PROFILES ...
  | SHOW PROCEDURE CODE ...
  | SHOW FUNCTION CODE ...
  | LOAD DATA ... unsupported-shape
```

## Tests

Coverage added:

- parser acceptance for histogram `ANALYZE TABLE`, component/plugin lifecycle,
  tablespace lifecycle, broad system-variable `SET` variants, XA, HANDLER,
  diagnostics/profile statements, unsupported `LOAD DATA`, and
  routine-code `SHOW` statements;
- runtime no-op result shape, warning, `ROW_COUNT()`, and transaction
  preservation for utility no-ops;
- runtime unsupported diagnostics for XA, HANDLER, `GET DIAGNOSTICS`,
  profiling, routine-code, and unsupported `LOAD DATA` placeholders;
- named-account `SHOW GRANTS` root success and missing-user `1141 / 42000`
  diagnostics;
- empty `SHOW PROCEDURE/FUNCTION STATUS WHERE` result shape.

## Non-goals

- no histogram statistics storage or optimizer integration;
- no component/plugin loading, unloading, registries, services, or loadable
  function lifecycle;
- no physical tablespaces, datafiles, undo tablespaces, or NDB tablespaces;
- no XA state machine, XA recovery records, two-phase commit, or distributed
  transaction integration;
- no HANDLER cursor state or direct index cursor access;
- no full diagnostics-area stack or `GET DIAGNOSTICS` variable assignment;
- no profiling collector, `SHOW PROFILE` result rows, or profiling system
  variables;
- no broad `LOAD DATA` option support beyond the existing implemented subset;
- no account storage, roles, grants, or privilege enforcement beyond the fixed
  embedded `root@%` identity.
