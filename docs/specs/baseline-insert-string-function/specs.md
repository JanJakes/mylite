# Baseline INSERT String Function

## Summary

This phase adds the MySQL string function:

```sql
INSERT(str, pos, len, newstr)
```

The goal is to cover the common substring-replacement helper without expanding
MyLite into general expression evaluation. The feature follows the existing
string slice and string replacement architecture: parse the MySQL function,
evaluate scalar no-source/`DUAL`/`DO` calls in MyLite, and lower row-scalar
table projections to an internal MyLite SQLite scalar function so SQLite still
performs table scanning and projection.

This phase does not add broader DML `INSERT` behavior, binary-string
substring replacement, string-to-integer conversion warnings for `pos`/`len`,
decimal/floating argument conversion, predicates, ordering expressions,
grouping expressions, generated columns, defaults, or arbitrary expression
support outside the documented row-scalar value and integer argument subsets.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
  - `third_party/sqlite/README.md`
- Existing string slices:
  - `docs/specs/baseline-substring-functions/specs.md`
  - `docs/specs/baseline-string-padding-functions/specs.md`
  - `docs/specs/baseline-replace-string-function/specs.md`
- Official MySQL 8.4 Reference Manual:
  - String functions and operators:
    <https://dev.mysql.com/doc/refman/8.4/en/string-functions.html>
  - Built-in functions and parsing notes:
    <https://dev.mysql.com/doc/refman/8.4/en/functions.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_insert_string_function_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Observed behavior shaping this slice:

- `INSERT(str, pos, len, newstr)` is the only accepted arity.
- Fewer or more arguments fail as syntax errors with `1064 / 42000`.
- Whitespace between `INSERT` and `(` is accepted.
- The function returns SQL `NULL` when any argument is SQL `NULL`.
- `pos` is 1-based. If `pos <= 0`, `pos` is greater than the character length
  of `str`, or `str` is empty, MySQL returns the original `str`.
- When `pos` is valid, the prefix before `pos` is kept, `newstr` is inserted,
  and the suffix begins after `len` characters.
- `len = 0` inserts `newstr` without removing any character.
- A negative `len`, or a positive `len` beyond the remaining string, replaces
  through the end of `str`.
- Position and length are character based for nonbinary UTF-8 strings.
- Literal integer, boolean, and numeric-looking string/decimal position and
  length arguments are coerced by MySQL. MyLite admits signed-64 integer,
  boolean, and `NULL` scalars plus documented integer descriptor and
  integer-expression arguments, deferring broader warning-producing numeric
  conversion.
- Successful supported invocations emit no warnings. Scalar `SELECT` follows
  existing result-set conventions; `DO` reports row count `0`.

## Supported Scope

Supported:

- no-source scalar `SELECT`, `SELECT ... FROM DUAL`, and `DO`;
- single-table row-scalar `SELECT` projections over persistent and temporary
  base tables already supported by the row-scalar expression framework;
- exactly four arguments: `INSERT(str, pos, len, newstr)`;
- `str` and `newstr` operands from:
  - SQL string literals without decoded `NUL` bytes;
  - SQL decimal integer literals and optional unary sign;
  - `TRUE`, `FALSE`, and SQL `NULL`;
  - supported session scalar values and system variables already admitted by
    current string functions;
  - descriptor-backed integer, exact `DECIMAL`, nonbinary string, baseline
    `TEXT`, `YEAR`, `DATE`, `TIME`, `DATETIME`, and `TIMESTAMP` columns in
    row-scalar projection;
- `pos` and `len` operands from:
  - SQL decimal integer literals with optional unary sign inside the current
    signed-64 range;
  - `TRUE`, `FALSE`, and SQL `NULL`;
  - direct supported integer-domain numeric and string-length scalar functions;
  - descriptor-backed integer columns in row-scalar projection;
  - supported table-backed integer row-scalar expressions, including integer
    arithmetic over admitted operands, supported numeric functions,
    string-length functions, `UNIX_TIMESTAMP()`, and numeric temporal
    extractors;
- UTF-8 character based replacement for admitted nonbinary strings;
- existing result labels, explicit aliases, result ownership, row-count state,
  and warning count conventions.

Deferred:

- use as DML `INSERT` syntax beyond existing table DML;
- binary string, `BIT`, approximate numeric, JSON, spatial, or BLOB-family
  arguments;
- warning-producing string, decimal, floating-point, hexadecimal, bit,
  parameter, user-variable, or unsupported expression-valued `pos` / `len`;
- nested `INSERT()` calls, arbitrary nested functions outside the supported
  row-scalar value and integer argument subsets, scalar subqueries inside the
  function, table-backed subqueries, predicates, grouping expressions, ordering
  expressions, DML assignments, defaults, generated columns, and functional
  indexes;
- MySQL's warning-producing string-to-integer conversion for `pos` / `len`;
- full protocol-grade expression metadata and binary result typing.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` owns public misuse behavior, result
  lifetime, and cleanup. Successful scalar and row-scalar calls expose text or
  SQL `NULL` through the existing result API.
- Statement context: owns diagnostics, warning count, affected-row state, row
  count state, and result finalization. Supported successful invocations report
  `warning_count == 0`.
- Lexer/parser/AST: admits `INSERT(str,pos,len,newstr)` in expression contexts
  while preserving existing table-DML `INSERT` statement grammar. It does not
  inspect descriptors or perform string conversion.
- Analyzer/planner: admits the function only in current scalar and row-scalar
  expression envelopes, resolves descriptor columns through MyLite descriptors,
  rejects unsupported argument shapes before generated SQLite SQL is executed,
  and keeps result metadata in the current string-function metadata envelope.
- String runtime module: owns UTF-8 validation, character-boundary discovery,
  MySQL-shaped position/length behavior, result construction, and the internal
  SQLite scalar callback. It does not depend on SQLite string semantics for the
  visible substring-replacement rules.
- SQLite physical execution: row-scalar projection is lowered to an internal
  MyLite SQLite scalar function registered through public SQLite APIs. SQLite
  scans and projects rows; MyLite does not materialize table rows in C for this
  feature.
- Catalog/storage/VFS: unchanged. No successful `INSERT()` function statement
  mutates catalog rows, descriptor versions, descriptor caches, catalog
  generation, `sqlite_schema_generation`, `.mylite` preamble bytes, or shifted
  SQLite payload invariants.

## Grammar

Supported SQL expression shape:

```sql
INSERT(str, pos, len, newstr)
```

MyLite Lemon-syntax snippet:

```lemon
expression(A) ::= INSERT(T) LPAREN expression(B) COMMA expression(C)
                  COMMA expression(D) COMMA expression(E) RPAREN(R). {
    A = mylite_sql_parser_make_four_argument_function(
        state, T, MYLITE_SQL_AST_INSERT_STRING_FUNCTION, B, C, D, E, R);
}
```

The implementation may use MyLite's list-argument helper internally, but the
accepted function arity remains exactly four. These snippets describe MyLite's
admitted subset, not MySQL's full grammar.

## Semantics

For supported non-`NULL` arguments:

1. Validate `str` and `newstr` as UTF-8 text.
2. Count characters in `str`.
3. If `pos <= 0`, `pos > character_count(str)`, or `str` has zero characters,
   return `str` unchanged.
4. Keep all characters before `pos`.
5. Append `newstr`.
6. If `len < 0` or `len` reaches beyond the remaining string, omit the suffix.
   Otherwise append the suffix beginning `len` characters after `pos`.

Any SQL `NULL` argument returns SQL `NULL`.

## Diagnostics

Supported diagnostics:

- wrong arity: syntax error `1064 / 42000`, matching observed MySQL parsing for
  this reserved-keyword function;
- unsupported `str` or `newstr` expression: deterministic MyLite unsupported
  diagnostic;
- unsupported `pos` or `len` expression: deterministic MyLite unsupported
  diagnostic requiring the supported integer argument envelope;
- signed-64 range overflow for `pos` or `len`: deterministic MyLite
  unsupported diagnostic;
- decoded `NUL` in scalar string literals: existing MyLite unsupported string
  literal diagnostic;
- invalid UTF-8 in runtime string data: deterministic MyLite runtime diagnostic;
- missing row-scalar column: existing descriptor-driven unknown-column
  diagnostic;
- allocation failure: `MYLITE_NOMEM` through current public API behavior;
- public API misuse: unchanged.

Supported successful invocations produce `warning_count == 0`.

## Physical SQLite Handling

MyLite lowers row-scalar `INSERT()` projections to:

```sql
_mylite_insert_string(str_sql, pos_sql, len_sql, newstr_sql)
```

Generated SQL is built from planned descriptor expressions, generated
expressions from the supported argument subsets, and bound scalar parameters.
Descriptor columns use quoted physical identifiers and stable physical table
aliases. Literal arguments are bound as parameters. The internal SQLite
function performs the same MyLite-owned UTF-8 character replacement as scalar
execution. No SQLite fork hook or SQLite JSON/string extension is required.

## Tests

Add fast plain C coverage in
`packages/libmylite/tests/runtime_insert_string_function_test.c` and MySQL
expectation coverage in
`packages/libmylite/tests/mysql_baseline_insert_string_function_expectations.sh`.

Required coverage:

- basic replacement, out-of-range positions, zero/negative/large lengths, empty
  string behavior, UTF-8 multibyte character boundaries, integer/boolean
  coercible text operands, and SQL `NULL` arguments;
- `SELECT ... FROM DUAL`, whitespace before `(`, aliases/labels, and `DO`
  status;
- row-scalar projection from supported descriptor column families and reopen
  persistence;
- row-scalar projection with `WHERE`, `ORDER BY`, and `LIMIT` using existing
  descriptor-driven row selection;
- unsupported/wrong arity syntax, supported integer `pos`/`len` expressions,
  unsupported `pos`/`len` operands, unsupported binary and approximate
  descriptor columns, missing row-scalar columns, and unsupported expression
  arguments;
- no result rows for `DO`, `ROW_COUNT() == 0`, scalar/select `ROW_COUNT() ==
  -1`, and `warning_count == 0` for successful supported statements;
- existing parser, string slice, padding, search, replacement, and runtime
  lifecycle tests continue to pass.

## Compatibility Updates

- `COMPATIBILITY.md`: move `INSERT()` string function from not implemented to
  limited supported.
- `docs/compatibility/functions-string.md`: document the exact
  no-source/`DUAL`/`DO` and row-scalar subset.
- `docs/compatibility/sql-query-expressions.md` and
  `docs/compatibility/type-system-literals-conversion.md`: update only if the
  scalar/row-scalar expression summary needs the new function named explicitly.
