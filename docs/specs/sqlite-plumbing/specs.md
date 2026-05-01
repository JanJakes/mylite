# SQLite Plumbing Execution Slice

## Status

This feature is MyLite's first executable SQLite-backed pipeline slice. It
connects the MySQL lexer/parser scaffold to a narrow SQLite translator, prepares
translated SQL with the bundled SQLite engine, and exposes statement stepping
through the initial MyLite C API.

`SELECT 123` is the seed statement for proving the plumbing. This is not full
`SELECT` support, expression support, or a general constant-projection feature.

## Sources

- MySQL 8.4 Reference Manual, `SELECT` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- MySQL 8.4 Reference Manual, Numeric Literals:
  https://dev.mysql.com/doc/refman/8.4/en/number-literals.html
- SQLite C Interface, `sqlite3_prepare_v3()`:
  https://www.sqlite.org/c3ref/prepare.html
- SQLite C Interface, `sqlite3_step()`:
  https://www.sqlite.org/c3ref/step.html

## Scope

The initial SQLite plumbing must provide:

- an in-memory `mylite_db` backed by `sqlite3`
- a public prepare/step/finalize path for `mylite_stmt`
- handle-owned diagnostic text for parser, translator, SQLite, and allocation
  failures
- a MyLite-to-SQLite translator with one deliberately tiny supported AST shape

The seed SQL shape accepted by the runtime is a single signless integer literal
projection, with `SELECT 123` as the canonical smoke case:

```sql
SELECT 123
SELECT 123;
```

That seed path corresponds to one parsed statement with this AST shape:

```text
script
  select_statement
    select_list
      select_item
        integer literal
```

No `FROM`, aliases, unary signs, multiple select items, identifiers, strings,
hex literals, decimal literals, functions, variables, or clauses are supported
by this plumbing slice. Full MySQL integer range, overflow, unsigned-type, and
metadata semantics remain future analyzer/runtime work.

## Runtime Behavior

For supported statements, MyLite must:

1. open an in-memory SQLite database for `mylite_open_memory()`
2. lex and parse the SQL input with the MySQL parser scaffold
3. validate that the AST is the currently supported seed shape
4. translate it to SQLite SQL of the form `SELECT <literal>`
5. prepare the translated SQL with SQLite so SQLite produces executable bytecode
6. step the SQLite statement and expose the single row through MyLite's C API

The value must be returned as SQLite's integer value for the literal. For
`SELECT 123`, callers should observe one row, one column, and `123` through
`mylite_column_int64()`.

The result-column label for this seed path is the literal text, matching both
MySQL and SQLite for `SELECT 123`.

## Diagnostics

- Parser failures return `MYLITE_PARSE_ERROR`.
- Valid MyLite parser ASTs outside this feature scope return
  `MYLITE_UNSUPPORTED`.
- SQLite prepare or step failures return `MYLITE_SQLITE_ERROR`.
- Invalid public API usage returns `MYLITE_MISUSE`.
- Allocation failure returns `MYLITE_NOMEM`.

Diagnostics are initially stored as handle-owned text. MySQL-compatible numeric
error codes, SQLSTATE, warnings, and metadata are future work.

## MySQL 8.4.9 Runtime Verification

Checked on 2026-05-01 against the official `mysql:8.4.9` Docker image:

| SQL | Observed behavior |
| --- | --- |
| `SELECT 123;` | Returns one row with one column named `123` and value `123`. |

## Compatibility Decisions

- Full `SELECT` remains unsupported until it has its own feature specs,
  compatibility decisions, and MySQL-runtime comparison tests.
- The parser continues accepting more seed syntax than the runtime executes.
  Accepted-but-unimplemented AST shapes must fail with `MYLITE_UNSUPPORTED`
  until their own specs and tests exist.
- The parser layer must remain independent of SQLite. SQLite-specific lowering
  belongs in a translation/runtime layer after parsing.

## Test Plan

Fast C tests must cover:

- `SELECT 123` prepares, steps to one row, returns one column named `123`, and
  returns integer value `123`
- `SELECT 123;` works with a trailing semicolon
- unsupported parsed statements such as `SELECT 1 + 2` return
  `MYLITE_UNSUPPORTED`
- syntax errors such as `SELECT FROM DUAL` return `MYLITE_PARSE_ERROR`
- parser tests include the exact `SELECT 123` parse path
