# Baseline RAND Seed Coercion

## Goal

Extend the existing no-source seeded `RAND(seed)` baseline with the small seed
coercion subset that WordPress-style scalar tests exercise:

```sql
SELECT RAND(3.9), RAND('5'), RAND('3.9'), RAND(NULLIF(1, 1));
```

This phase intentionally does not add general expression evaluation for random
predicates or broad table-backed nonconstant seeds. A later table-backed RAND
slice admits warning-free integer descriptor `CAST()` / `CONVERT()` seed
expressions for WordPress-style queries, but warning-producing table-backed seed
coercion remains deferred until MyLite has a row-expression warning path that
can report per-row conversion warnings correctly.

## Sources

- Official MySQL 8.4 Reference Manual, mathematical functions:
  <https://dev.mysql.com/doc/refman/8.4/en/mathematical-functions.html>
- Observed MySQL 8.4.9 runtime behavior in the local `mysql:8.4.9` runtime.
- Existing MyLite `RAND()`, seeded `RAND(seed)`, scalar cast, scalar
  `NULLIF()`, and diagnostic staging code.

This specification is independently authored from official MySQL 8.4
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite code. It does not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or restrictively licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

MySQL documents `RAND([N])` as returning a double in `0 <= value < 1.0` and
using `N` as the optional seed. Runtime probes against MySQL 8.4.9 establish
the additional coercions used by this slice:

```sql
SELECT
  RAND(3.4), RAND(3.5), RAND(3.9),
  RAND(-3.4), RAND(-3.5), RAND(-3.9),
  RAND(+TRUE), RAND(-TRUE), RAND(+NULL), RAND(-NULL),
  RAND(NULLIF(1, 1)), RAND(NULLIF(1, 2));
```

returns:

```text
0.9057697559760601  0.15595286540310166  0.15595286540310166
0.40467110313910165 0.15448799371206015  0.15448799371206015
0.40540353712197724 0.9050373219931845   0.15522042769493574
0.15522042769493574 0.15522042769493574  0.40540353712197724
```

with zero warnings.

String seed probes:

```sql
SELECT RAND('5'), RAND('3.9'), RAND('abc'), RAND('  -2x');
SELECT @@warning_count;
```

return:

```text
0.40613597483014313 0.9057697559760601 0.15522042769493574 0.6548542125661431
3
```

The warnings are `1292 / 22007` truncated incorrect integer warnings for
`'3.9'`, `'abc'`, and `'  -2x'`.

## Supported Surface

MyLite supports extended seed coercion only for no-source scalar `SELECT`,
`SELECT ... FROM DUAL`, and `DO` calls:

- integer seed literals with optional unary sign, preserving the existing
  32-bit wrapping behavior;
- fixed decimal seed literals with optional unary sign, rounded half away from
  zero before the existing 32-bit wrapping step;
- approximate numeric seed literals, including exponent-token forms, with
  optional unary sign and MySQL's observed approximate half-to-even integer
  rounding before the existing 32-bit wrapping step;
- `TRUE`, `FALSE`, and `NULL`, with optional unary sign on these keyword
  literals matching MySQL's scalar numeric coercion;
- ordinary string literals decoded by MyLite, converted with MySQL-style
  integer-prefix scanning, warning on truncated or nonnumeric text, then wrapped
  to the seed domain;
- no-source `NULLIF(...)`, `CAST(... AS SIGNED)`, `CAST(... AS UNSIGNED)`,
  `CONVERT(..., SIGNED)`, and `CONVERT(..., UNSIGNED)` seed expressions when
  their arguments fit the existing literal-only scalar `NULLIF()` and integer
  cast/convert subsets;
- successful supported calls use the existing MyLite seeded `RAND` recurrence,
  return double text, preserve result labels from the source SQL, and do not
  mutate catalog descriptors, storage, the `.mylite` preamble, or SQLite schema
  generation.

Warnings produced while coercing the seed are staged on the `RAND()` result
cell, so `@@warning_count` projected in the same `SELECT` observes the previous
statement diagnostics like MySQL, while the final result object and later
`SHOW WARNINGS` expose the seed-conversion warnings.

## Deferred Surface

This phase does not support:

- table-backed nonconstant seeds beyond the later warning-free integer
  descriptor `CAST()` / `CONVERT()` subset;
- table-backed string, decimal, approximate, or warning-producing seed coercion,
  because those forms need statement-level warning staging in the row-scalar
  planner;
- broad random predicates, random DML assignments beyond the later
  WordPress-style nonbinary string-target `RAND()` / `RAND(seed)` value subset,
  random defaults, generated columns, constraints, indexes, joins, grouped
  queries, compound queries, or scalar subqueries;
- arithmetic seed expressions such as `RAND(1 + 0)`;
- hexadecimal or bit seed literals;
- broad expression metadata, optimizer, replication, or binlog warning
  semantics.

## Grammar

No new grammar is required. The existing independently authored grammar already
parses the admitted seed argument as a normal expression:

```lemon
expression(A) ::= RAND(T) LPAREN RPAREN(R).
expression(A) ::= RAND(T) LPAREN expression(B) RPAREN(R).
expression(A) ::= RAND(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R).
```

This feature changes the scalar analyzer/runtime interpretation of the
one-argument node only in no-source scalar execution.

## Runtime Design

The no-source scalar `RAND(seed)` path uses a MyLite-owned coercion helper:

1. Unwrap expression parentheses and unary `+` / `-`.
2. Convert admitted numeric literals to an integral seed. Fixed decimal numeric
   literals round half away from zero; approximate numeric literals use the
   observed MySQL approximate half-to-even behavior. Integer literals retain the
   existing wrapping behavior.
3. Convert admitted string literals with the existing scalar integer-cast
   scanner so warning text and complement behavior stay aligned with current
   `CAST(... AS SIGNED)` behavior.
4. Evaluate admitted `NULLIF` and integer `CAST` / `CONVERT` seed expressions
   directly through their existing literal-only helper paths, then convert the
   resulting scalar cell through the same seed conversion path. This avoids
   introducing a recursive dependency from `RAND()` back into the general scalar
   dispatcher.
5. Copy or transfer staged seed warnings onto the final `RAND()` output cell.
6. Initialize the existing MyLite seeded random state and format the resulting
   double through the existing scalar double formatter.

Table-backed constant `RAND(seed)` planning deliberately continues to use the
existing literal-only converter in this seed-coercion phase so it cannot silently
accept forms whose warnings would be lost in row-scalar execution.

## Diagnostics

- `RAND(1, 2)` and wider argument lists keep MySQL-compatible native function
  parameter-count diagnostics `1582 / 42000`.
- Unsupported no-source seed expressions report a deterministic MyLite
  capability diagnostic:
  `RAND(seed) does not support this seed expression`.
- Numeric seed values outside the admitted finite integral conversion envelope
  report:
  `RAND(seed) numeric seed is out of range`.
  MySQL 8.4.9 rejects extreme approximate literals such as `1e309` during
  parsing with `1367 / 22007`; MyLite reports the deterministic capability
  diagnostic above for the same admitted grammar shape.
- String literal decoding failures, embedded `NUL`, allocation failures, and
  public API misuse reuse existing scalar literal and public API diagnostics.

## Architecture Boundary

- Public API: unchanged; callers use `mylite_execute()` and existing result
  accessors.
- Parser/AST: unchanged; existing `RAND(seed)` AST nodes are reused.
- Analyzer/planner: no descriptor or row-source planning changes in this phase.
- Runtime/result builder: owns the new source-free seed coercion and warning
  staging.
- Catalog/storage/VFS: unchanged. No descriptors, physical rows, preamble bytes,
  file-format metadata, or schema generation counters change.
- SQLite: no new SQLite SQL, callback, extension API, or fork patch is needed.

## Tests

MySQL-runtime expectations cover exact values and warning counts for:

- fixed decimal positive and negative seed literals around the half boundary;
- exponent-form approximate seed literals and the out-of-range approximate
  literal behavior observed in MySQL 8.4.9;
- signed boolean and signed `NULL` seed literals;
- `NULLIF()` seed expressions returning `NULL` and non-`NULL` values;
- string seeds with exact integer text, decimal-prefix text, nonnumeric text,
  and whitespace/sign/trailing-garbage text;
- same-statement `@@warning_count`, `SELECT ... FROM DUAL`, and `DO` warning
  behavior for coercion warnings.

Fast C tests cover the same successful no-source behavior, result warning
counts, `SHOW WARNINGS` contents, existing file-safety invariants, and continued
rejection of unsupported arithmetic seed expressions plus table-backed extended
seed forms.

## Compatibility Documentation

Update `COMPATIBILITY.md` and
`docs/compatibility/functions-numeric-math.md` with limited wording. Do not
claim table-backed warning-producing seed conversion, broad random predicates,
broad random DML assignments, general expression seeds, hexadecimal/bit seeds,
or full expression metadata.
