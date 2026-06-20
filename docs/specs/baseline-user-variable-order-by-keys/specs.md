# Baseline User-Variable ORDER BY Keys

## Status

This slice accepts user-variable reads as `SELECT ORDER BY` keys in supported
SELECT statements and treats them as row-constant no-op order terms:

```sql
SELECT column FROM source ORDER BY @rank;
SELECT column FROM source ORDER BY @rank DESC, real_order_key;
```

It builds on the existing user-variable parser/runtime surface and the
constant-order-key planner behavior.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- MySQL 8.4 Reference Manual, `SELECT` syntax:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- MySQL 8.4 Reference Manual, user variables:
  https://dev.mysql.com/doc/refman/8.4/en/user-variables.html
- MySQL 8.4.9 runtime probes recorded in
  `packages/libmylite/tests/mysql_parser_corpus_select_clause_residuals_expectations.sh`

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes verify:

- `ORDER BY @rank` is accepted when `@rank` is uninitialized.
- `ORDER BY @rank DESC` is accepted after `@rank` has been assigned.
- Because the variable read is row-constant, the key does not sort by any
  descriptor column.
- When followed by a real descriptor key, the user-variable key has no effect
  and the real key determines ordering.

## Supported Surface

Supported key shape:

```sql
ORDER BY @user_variable [ASC | DESC]
```

This key is supported in the same SELECT envelopes that already accept
descriptor, alias, ordinal, documented expression, or constant no-op order keys.
A user-variable key may appear alone or before/after other already-supported
order keys.

The MyLite Lemon grammar for this slice is:

```lemon
select_order_key ::= user_variable.
```

## Runtime Semantics

The order planner recognizes `MYLITE_SQL_AST_USER_VARIABLE` order keys as
row-constant no-op keys. It does not append them to the planned order list and
does not materialize rows in MyLite. If all order keys are no-op keys, MyLite
emits no SQLite `ORDER BY`. If a clause mixes no-op keys with real supported
keys, MyLite emits only the real keys in their original relative order.

Reading a user variable has no side effects, and this slice does not evaluate
or coerce the user-variable value. Existing user-variable assignment/readback
semantics remain unchanged for projection, `SET`, and scalar contexts.

No SQLite fork hook is needed.

## Non-Goals

This slice does not add:

- user-variable assignment expressions in `ORDER BY`;
- parameter, system-variable, function, or arbitrary expression order keys;
- side-effecting order-key evaluation;
- deterministic ordering when all order keys are no-op keys; or
- new catalog, file-format, public ABI, VFS, or SQLite fork changes.

Unsupported forms continue to use deterministic MyLite diagnostics.

## Validation

Coverage includes:

- MySQL 8.4.9 expectation rows for parser-corpus `ORDER BY @rank` probes;
- fast C parser/runtime movement for the parser-corpus probe; and
- focused runtime coverage that user-variable keys are skipped while later
  descriptor keys still sort.
