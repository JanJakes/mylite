# Baseline Identity Row-Scalar Contexts

## Status

This feature completes the embedded identity-function baseline inside MyLite's
current row-scalar SELECT envelope. The supported functions are statement-local
session constants:

- `USER()`
- `CURRENT_USER()` and bare `CURRENT_USER`
- no-whitespace `SESSION_USER()`
- no-whitespace `SYSTEM_USER()`
- `CURRENT_ROLE()`

They can be used in source-backed projection, `WHERE` comparisons, and
`ORDER BY` expressions for row-scalar SELECT statements. Existing account,
authentication, privilege, role-state, and stored-program gaps remain tracked
by the broader user/role compatibility rows.

No SQLite fork, SQLite UDF, catalog row, storage change, or public API change is
required. The existing AST nodes are admitted through the current row-scalar
predicate expression and comparison-value grammar paths, then routed through
the row-scalar session-value planner as bound constants.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline current user identity:
  `docs/specs/baseline-current-user-identity/specs.md`
- Baseline session/system user identity:
  `docs/specs/baseline-session-system-user-identity/specs.md`
- Baseline current role function:
  `docs/specs/baseline-current-role-function/specs.md`
- Baseline session value scalar projection:
  `docs/specs/baseline-session-value-scalar-projection/specs.md`
- MySQL 8.4 Reference Manual, information functions:
  https://dev.mysql.com/doc/refman/8.4/en/information-functions.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against a TCP MySQL 8.4.9 session with no active roles:

- With a table containing rows `(2), (1)`, `SELECT USER(), CURRENT_USER(),
  CURRENT_USER, SESSION_USER(), SYSTEM_USER(), CURRENT_ROLE(), id FROM t ...`
  returns one identity tuple per source row.
- `USER()`, `SESSION_USER()`, and `SYSTEM_USER()` report the client identity;
  `CURRENT_USER()` and bare `CURRENT_USER` report the authenticated account.
- `CURRENT_ROLE()` returns `NONE` when no roles are active.
- The unaliased result labels remain the source spellings, including
  `CURRENT_USER` for the bare form.
- The statement produces no warnings.
- A following `ROW_COUNT()` after the source-backed select returns `-1`.

MyLite's embedded identity uses `root@%` for both the client and current-user
values. That intentionally differs from TCP MySQL's transport-derived
`USER()` host component and is documented in the original identity specs.

## Scope

The implementation must add:

- source-backed row-scalar projection for the listed identity functions;
- the listed identity functions as row-scalar predicate expressions on either
  side of comparison predicates;
- the listed identity functions as row-scalar `ORDER BY` expressions;
- preservation of existing `SESSION_USER()` and `SYSTEM_USER()` no-whitespace
  parsing behavior;
- repeated result values for each physical source row without changing session,
  catalog, or storage state;
- MySQL-runtime-verified expectation coverage and fast C regression coverage.

The accepted source shapes are the existing row-scalar source envelope to the
extent those envelopes already support the surrounding SELECT shape.

## Parser And AST Handling

No new AST node kind is required. The existing identity-function expression
nodes are additionally accepted by the current row-scalar predicate expression
and predicate comparison-value grammar paths:

```lemon
predicate_row_scalar_expression ::= USER LPAREN RPAREN.
predicate_row_scalar_expression ::= SESSION_USER LPAREN RPAREN.
predicate_row_scalar_expression ::= SYSTEM_USER LPAREN RPAREN.
predicate_row_scalar_expression ::= CURRENT_USER LPAREN RPAREN.
predicate_row_scalar_expression ::= CURRENT_USER.
predicate_row_scalar_expression ::= CURRENT_ROLE LPAREN RPAREN.
predicate_comparison_value ::= USER LPAREN RPAREN.
predicate_comparison_value ::= SESSION_USER LPAREN RPAREN.
predicate_comparison_value ::= SYSTEM_USER LPAREN RPAREN.
predicate_comparison_value ::= CURRENT_USER LPAREN RPAREN.
predicate_comparison_value ::= CURRENT_USER.
predicate_comparison_value ::= CURRENT_ROLE LPAREN RPAREN.
```

`SESSION_USER()` and `SYSTEM_USER()` continue to use the no-space zero-argument
builder. Whitespace or comment-separated spellings such as `SESSION_USER ()`
and `SYSTEM_USER/**/()` are not native-function forms in the default MySQL
mode and remain outside this row-scalar slice.

Those row-scalar alternatives participate in comparison predicates and order
keys:

```lemon
predicate_atom ::= predicate_row_scalar_expression comparison_operator
                   predicate_comparison_value.
select_order_key ::= predicate_row_scalar_expression.
```

Bare `USER`, `SESSION_USER`, `SYSTEM_USER`, and `CURRENT_ROLE` remain ordinary
identifier expressions in unsupported scalar contexts. Bare `CURRENT_USER`
remains the native current-user function.

## Non-Goals

This feature does not implement:

- authentication, account matching, host matching, users, definers, roles, role
  grants, default roles, active-role state, or privilege enforcement;
- mutable role state from `SET ROLE` or `SET DEFAULT ROLE`;
- stored-program definer/invoker semantics;
- `IGNORE_SPACE` stored-function resolution;
- grouping, window clauses, locking clauses, or other SELECT clauses not
  already supported by row-scalar SELECT planning;
- general arithmetic or string conversion expressions such as
  `CURRENT_ROLE() + 1`;
- user variables, prepared-statement parameters, defaults, generated columns,
  check constraints, or broader DML expressions;
- a SQLite UDF or SQLite fork hook.

## Runtime Semantics

Each supported identity function is planned as a source-independent row-scalar
value. Planning reads the relevant MyLite session identity once for the
statement, creates a constant planned value, and binds that value into the
generated SQLite query. SQLite still performs source scanning, filtering,
ordering, and row production.

Successful source-backed reads return the same labels and text values as the
existing one-row scalar identity baselines, with zero statement warnings and
row-result affected-row semantics.

## SQLite And Storage Handling

The implementation is MyLite wrapper/planner logic using public SQLite
prepared statement parameters. It does not require a SQLite extension function,
virtual table, VFS change, or targeted SQLite fork patch. No catalog rows or
`.mylite` file header fields are read or modified.

## Tests

Fast C tests must cover:

- source-backed projection for identity functions and `CURRENT_ROLE()`;
- row-scalar `WHERE` comparisons between identity functions;
- row-scalar `ORDER BY` over identity functions;
- parser admission of the new predicate operands;
- preservation of existing unsupported argument and whitespace-sensitive
  diagnostics.

MySQL expectation scripts must cover:

- MySQL 8.4.9 labels and source-backed repeated values;
- zero warning count;
- following `ROW_COUNT() = -1`;
- no-active-role `CURRENT_ROLE()` value.
