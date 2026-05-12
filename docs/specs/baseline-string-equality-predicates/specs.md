# Baseline String Equality Predicates

## Summary

This phase adds the first descriptor-backed string comparison predicate slice.
It is a prerequisite for table-backed scalar subqueries such as:

```sql
UPDATE _dates
SET option_value = (
    SELECT option_value FROM _options WHERE option_name = 'User 0000019'
)
```

The supported surface is intentionally narrow: `CHAR`, `VARCHAR`, and baseline
`TEXT` family descriptor columns may be compared with ordinary ASCII string
literals using equality, null-safe equality, and inequality operators in the
existing descriptor predicate contexts.

## Sources And Evidence

- MyLite architecture and standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Current string, predicate, and DML slices:
  - `docs/compatibility/sql-query-expressions.md`
  - `docs/compatibility/sql-table-dml.md`
  - `docs/compatibility/type-system-literals-conversion.md`
  - `docs/specs/baseline-varchar-type/specs.md`
  - `docs/specs/baseline-text-type/specs.md`
  - `docs/specs/baseline-select-where-lifecycle/specs.md`
  - `docs/specs/baseline-update-lifecycle/specs.md`
- Official MySQL 8.4 Reference Manual:
  - comparison operators:
    <https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html>
  - string comparison functions and operators:
    <https://dev.mysql.com/doc/refman/8.4/en/string-comparison-functions.html>
  - character sets and collations:
    <https://dev.mysql.com/doc/refman/8.4/en/charset.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_string_equality_predicates_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## Runtime Observations

MySQL 8.4.9 behavior used for this slice:

- `CHAR`, `VARCHAR`, and `TEXT` columns can be compared to string literals with
  `=`, `<=>`, `<>`, and `!=`.
- Under the default `utf8mb4_0900_ai_ci` collation, ASCII case differences do
  not make otherwise equal values distinct.
- `CHAR` storage/readback uses the existing MyLite policy of canonical trimmed
  values, so `CHAR` comparisons in this slice observe that canonical stored
  shape.
- `VARCHAR` and `TEXT` values preserve trailing spaces, and MySQL 8.4.9 does
  not match a stored `VARCHAR` or `TEXT` value with trailing spaces to a shorter
  string literal in the runtime probes used for this slice.
- `column = 'literal'` and `column <> 'literal'` do not match `NULL` column
  values. `column <=> 'literal'` matches only non-`NULL` equal values.
- `NOT (column <=> 'literal')` includes `NULL` rows.
- Successful supported statements report warning count `0`.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` continues to own result handles,
  diagnostics, and public misuse behavior.
- Statement context: no new statement state. Supported predicates participate
  in the same warning and affected-row reporting as existing `SELECT`,
  `DELETE`, and `UPDATE` statements.
- Lexer/parser/AST: unchanged. Existing comparison predicate AST nodes already
  carry descriptor-column operands, string literal operands, and operators.
- Analyzer/planner: extends the shared descriptor predicate planner. String
  predicate columns are resolved through MyLite descriptors. String literals
  are decoded through MyLite string-literal handling before any SQLite SQL is
  prepared.
- Catalog: read-only descriptor authority. This phase does not mutate catalog
  rows, descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- SQLite physical execution: generated SQL uses stable MyLite physical table
  names, quoted identifiers, bound literal parameters, and MyLite's registered
  `utf8mb4_0900_ai_ci` ASCII collation. MyLite remains responsible for the
  compatibility subset; SQLite is only the row-storage executor.
- Storage/VFS: unchanged. The `.mylite` preamble and shifted SQLite payload
  invariants are not affected by predicate evaluation.

## Supported SQL

The existing descriptor predicate contexts admit this new atom:

```sql
string_predicate:
    string_column = string_literal
  | string_column <=> string_literal
  | string_column <> string_literal
  | string_column != string_literal
```

Supported contexts:

```sql
SELECT ... FROM table WHERE string_predicate
DELETE FROM table WHERE string_predicate
UPDATE table SET column = value WHERE string_predicate
SELECT aggregate FROM table WHERE string_predicate
```

The predicate may be combined with existing `NOT`, `AND`/`&&`, `XOR`,
`OR`/`||`, and parenthesized predicate support.

`string_column` must resolve to one descriptor column whose logical type is:

- `CHAR`;
- `VARCHAR`;
- `TINYTEXT`;
- `TEXT`;
- `MEDIUMTEXT`;
- `LONGTEXT`.

`string_literal` is an ordinary single- or double-quoted MyLite string literal
after current SQL-mode decoding. The admitted compatibility subset is ASCII
text without embedded `NUL`.

### MyLite Lemon-Syntax Snippet

No new parser production is needed. Existing predicate grammar already admits
the shape:

```lemon
predicate(A) ::= expression(B) EQUAL expression(C).
predicate(A) ::= expression(B) NULL_SAFE_EQUAL expression(C).
predicate(A) ::= expression(B) NOT_EQUAL expression(C).
predicate(A) ::= expression(B) BANG_EQUAL expression(C).
```

Analyzer acceptance for this phase:

```lemon
string_predicate(A) ::= descriptor_string_column(B) string_eq_operator(C) string_literal(D).

string_eq_operator(A) ::= EQUAL.
string_eq_operator(A) ::= NULL_SAFE_EQUAL.
string_eq_operator(A) ::= NOT_EQUAL.
string_eq_operator(A) ::= BANG_EQUAL.
```

These snippets describe MyLite's supported subset, not MySQL's full grammar.

## Semantics

Planning:

1. Resolve the left operand using the existing descriptor-column resolver for
   the current statement context.
2. Require a string descriptor type.
3. Require one of `=`, `<=>`, `<>`, or `!=`.
4. Decode the right operand as an ordinary string literal using current
   MyLite string literal and SQL-mode rules.
5. Reject embedded `NUL` and non-ASCII literal bytes for this slice.
6. Store the decoded literal as a bound `TEXT` parameter.

Generated SQLite predicate SQL:

```sql
("column_name" COLLATE "utf8mb4_0900_ai_ci" <operator> ?N)
```

where `<operator>` is `=`, `IS`, or `<>` for MySQL `=`, `<=>`, and
`<>`/`!=` respectively. The column identifier is always generated from the
descriptor and quoted as a SQLite identifier. The literal is always bound as a
prepared-statement parameter.

Supported comparison semantics:

- ASCII case folding follows MyLite's registered `utf8mb4_0900_ai_ci` ASCII
  collation.
- The current slice claims MySQL parity only when stored compared string values
  and predicate literals are in the ASCII subset. Full Unicode
  `utf8mb4_0900_ai_ci` weights, accent-insensitive comparison, contractions,
  expansions, locale-tailored weights, and other non-ASCII collation behavior
  remain deferred.
- `NULL` column behavior is inherited from SQLite's generated operator shape
  and is verified against MySQL for this subset: ordinary equality and
  inequality do not match `NULL`; null-safe equality with a non-`NULL` literal
  matches only equal non-`NULL` rows; `NOT (column <=> literal)` includes
  `NULL` rows.

## Unsupported

Deferred until later slices:

- `LIKE`, `NOT LIKE`, `REGEXP`, `STRCMP()`, `BINARY`, `COLLATE`, `CAST()`, and
  explicit character-set introducers;
- string `<`, `<=`, `>`, `>=`, `BETWEEN`, `IN`, `NOT IN`, range ordering, and
  full string sort/group/distinct semantics;
- non-ASCII string comparison semantics;
- literal-left, expression-left, function, parameter, variable, column-to-column,
  subquery, row-constructor, or generated-column operands;
- numeric-to-string and string-to-numeric comparison conversion;
- binary strings, `BINARY`/`VARBINARY`/`BLOB`, and `ENUM`/`SET` descriptors;
- mutable connection collation effects beyond the fixed registered baseline
  collation.

## Diagnostics

Supported diagnostics:

- parser syntax errors through existing parse diagnostics;
- unknown columns through existing descriptor-column diagnostics;
- unsupported string operators with a deterministic unsupported diagnostic;
- unsupported non-string right operands with a deterministic unsupported
  diagnostic;
- non-ASCII or embedded-`NUL` predicate literals with a deterministic
  unsupported diagnostic;
- allocation failures through existing `MYLITE_NOMEM` and diagnostics;
- physical SQLite failures through existing physical-row diagnostics.

## Tests

Add MySQL-runtime expectation coverage for:

- `CHAR`, `VARCHAR`, and `TEXT` equality with ASCII case folding;
- `VARCHAR` and `TEXT` trailing-space behavior used by this slice;
- `<>`, `!=`, and `<=>` with non-`NULL` string literals;
- `NOT (column <=> literal)` including `NULL`;
- string predicates in `SELECT`, `UPDATE`, and `DELETE`;
- affected rows and warning count for DML statements;
- unsupported non-string right operands, unsupported string range operators,
  unsupported `IN`, unsupported `LIKE`, and non-ASCII literals in MyLite C
  tests.

## Performance

This phase stays close to SQLite execution. MyLite resolves descriptors,
decodes and validates one bound literal, and emits a normal SQLite predicate.
Rows are not materialized into MyLite memory for comparison. Index use remains
SQLite's responsibility; this phase does not claim optimizer parity or add new
indexing behavior.
