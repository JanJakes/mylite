# Baseline LIKE Predicates

## Summary

This phase adds a narrow descriptor-backed `LIKE` / `NOT LIKE` predicate slice
for the existing single-table row envelopes. It targets common application
string filtering while preserving MyLite's current architecture: descriptors
resolve columns, MyLite decodes and validates pattern literals, and SQLite
executes the physical row scan or index-assisted plan.

The supported surface is intentionally limited to string descriptor columns on
the left and ordinary ASCII string pattern literals on the right. It does not
add general expression predicates, explicit `ESCAPE`, Unicode collation weight
matching, binary strings, regex, or arbitrary SQLite pass-through.

## Sources And Evidence

- MyLite architecture and standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Current predicate and string slices:
  - `docs/specs/baseline-string-equality-predicates/specs.md`
  - `docs/specs/baseline-varchar-type/specs.md`
  - `docs/specs/baseline-char-type/specs.md`
  - `docs/specs/baseline-text-type/specs.md`
  - `docs/specs/baseline-where-and-predicates/specs.md`
  - `docs/specs/baseline-sql-mode-session-state/specs.md`
- Compatibility docs:
  - `docs/compatibility/sql-query-expressions.md`
  - `docs/compatibility/sql-table-dml.md`
  - `docs/compatibility/operators.md`
  - `docs/compatibility/type-system-literals-conversion.md`
- Official MySQL 8.4 Reference Manual:
  - string comparison functions and operators:
    <https://dev.mysql.com/doc/refman/8.4/en/string-comparison-functions.html>
  - expression and operator behavior:
    <https://dev.mysql.com/doc/refman/8.4/en/expressions.html>
  - string literals:
    <https://dev.mysql.com/doc/refman/8.4/en/string-literals.html>
  - SQL modes:
    <https://dev.mysql.com/doc/refman/8.4/en/sql-mode.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_like_predicates_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## Runtime Observations

MySQL 8.4.9 behavior used for this slice:

- `CHAR`, `VARCHAR`, and `TEXT` columns can be filtered with
  `column LIKE 'pattern'` and `column NOT LIKE 'pattern'`.
- `%` matches zero or more characters and `_` matches one character.
- Under the default `utf8mb4_0900_ai_ci` collation, ASCII case differences do
  not make otherwise matching values distinct.
- `VARCHAR` and `TEXT` trailing spaces are significant for exact `LIKE`
  patterns. `v LIKE 'abc'` does not match a stored `VARCHAR` value `abc  `.
- `CHAR` comparisons observe MySQL's trimmed retrieval/comparison shape in the
  verified subset. A stored `CHAR(5)` value inserted as `abc  ` matches
  `c LIKE 'abc'`.
- In the default SQL mode, backslash escapes pattern wildcards. For example,
  `LIKE 'ab\_%'` matches strings beginning with literal `ab_`, and
  `LIKE 'ab\%%'` matches strings beginning with literal `ab%`.
- With `NO_BACKSLASH_ESCAPES`, the same `LIKE 'ab\_%'` pattern does not use
  backslash as a pattern escape in the verified MySQL 8.4.9 behavior.
- `NOT LIKE` and `NOT (column LIKE pattern)` do not match `NULL` column values.
- Successful supported statements report warning count `0`.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` and result ownership conventions do
  not change.
- Statement context: no new public statement state. Supported predicates share
  existing warning, affected-row, row-count, and diagnostics behavior.
- Lexer/parser/AST: adds explicit AST/operator coverage for `LIKE`. `NOT LIKE`
  is represented as the existing logical `NOT` predicate around a `LIKE`
  predicate, preserving the current predicate tree model.
- Analyzer/planner: extends the shared descriptor predicate planner. Left
  operands are resolved from MyLite descriptors; right operands are decoded
  through MyLite string-literal handling and validated before any SQLite SQL is
  prepared.
- Catalog: read-only descriptor authority. This phase does not mutate catalog
  rows, descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- SQLite physical execution: generated SQL uses stable MyLite physical table
  names, quoted descriptor column identifiers, and bound pattern parameters.
  MyLite may add SQLite's standard `ESCAPE '\'` clause for default-mode pattern
  escapes. It does not depend on SQLite fork patches.
- Storage/VFS: unchanged. Row storage, `.mylite` preamble, and shifted SQLite
  payload invariants are unaffected.

## Supported SQL

The existing descriptor predicate contexts admit:

```sql
string_like_predicate:
    string_column LIKE string_pattern_literal
  | string_column NOT LIKE string_pattern_literal
```

Supported contexts:

```sql
SELECT ... FROM table WHERE string_like_predicate
SELECT aggregate FROM table WHERE string_like_predicate
DELETE FROM table WHERE string_like_predicate
UPDATE table SET column = value WHERE string_like_predicate
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

`string_pattern_literal` is an ordinary single- or double-quoted MyLite string
literal after current SQL-mode decoding. The admitted compatibility subset is
ASCII text without embedded `NUL`.

### MyLite Lemon-Syntax Snippet

```lemon
predicate_atom(A) ::= qualified_identifier(C) LIKE(O) string_pattern_literal(P). {
    A = mylite_sql_parser_make_like_predicate(state, C, O, P);
}

predicate_atom(A) ::= qualified_identifier(C) NOT(N) LIKE(O) string_pattern_literal(P). {
    A = mylite_sql_parser_make_not_predicate(
        state, N, mylite_sql_parser_make_like_predicate(state, C, O, P));
}

string_pattern_literal(A) ::= STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
```

These snippets describe MyLite's supported subset, not MySQL's full grammar.

## Semantics

Planning:

1. Resolve the left operand through the existing descriptor-column resolver for
   the current statement context.
2. Require a string descriptor type.
3. Decode the right operand as an ordinary string literal using current MyLite
   string-literal and SQL-mode rules.
4. Reject embedded `NUL` and non-ASCII pattern bytes for this slice.
5. Bind the decoded pattern as a `TEXT` parameter.

Generated SQLite predicate SQL:

```sql
("column_name" LIKE ?N ESCAPE '\')
```

when `NO_BACKSLASH_ESCAPES` is disabled, and:

```sql
("column_name" LIKE ?N)
```

when `NO_BACKSLASH_ESCAPES` is enabled for the session.

The column identifier is always generated from the descriptor and quoted as a
SQLite identifier. The pattern is always bound as a prepared-statement
parameter.

Supported comparison semantics:

- ASCII case folding follows the verified MySQL default-collation behavior for
  the admitted ASCII subset. MyLite relies on SQLite's standard ASCII `LIKE`
  behavior for this slice.
- `%` and `_` have their MySQL wildcard meaning.
- In default SQL mode, decoded backslash-preserved `\%` and `\_` pattern bytes
  are interpreted as escaped literal wildcard characters.
- With `NO_BACKSLASH_ESCAPES`, MyLite omits an explicit pattern escape clause,
  matching the verified behavior that the backslash is not a pattern escape for
  this slice.
- `NULL` column behavior is inherited from SQL three-valued logic and verified
  against MySQL: `LIKE` does not match `NULL`; `NOT LIKE` does not match `NULL`.

## Unsupported

Deferred until later slices:

- explicit `ESCAPE` clauses;
- literal-left, expression-left, function, parameter, variable, column-to-column,
  subquery, row-constructor, or generated-column operands;
- pattern values from columns, functions, parameters, or subqueries;
- non-ASCII pattern or stored-value collation parity;
- binary strings and binary collations;
- numeric-to-string and string-to-numeric comparison conversion;
- `REGEXP`, `RLIKE`, `NOT REGEXP`, `NOT RLIKE`, and `STRCMP()`;
- table-backed expression projection or ordering using `LIKE`;
- optimizer or index-use parity with MySQL.

## Diagnostics

Supported diagnostics:

- parser syntax errors through existing parse diagnostics;
- unknown columns through existing descriptor-column diagnostics;
- non-string left operands with a deterministic unsupported diagnostic;
- non-string right operands with a deterministic unsupported diagnostic;
- explicit `ESCAPE` clauses through syntax rejection in this phase;
- non-ASCII or embedded-`NUL` pattern literals with a deterministic
  unsupported diagnostic;
- allocation failures through existing `MYLITE_NOMEM` and diagnostics;
- physical SQLite failures through existing physical-row diagnostics.

## Tests

Add MySQL-runtime expectation coverage for:

- `%` and `_` wildcards over `VARCHAR`;
- ASCII case folding over `CHAR`, `VARCHAR`, and `TEXT`;
- `VARCHAR` trailing-space significance;
- `CHAR` trimmed comparison shape;
- default backslash escaping of literal `_` and `%`;
- `NO_BACKSLASH_ESCAPES` disabling the default pattern escape behavior;
- `NOT LIKE` and wrapped `NOT (LIKE)` excluding `NULL`;
- aggregate, `UPDATE`, and `DELETE` use with affected rows and warning count.

Add MyLite C coverage for:

- successful `SELECT`, aggregate, `UPDATE`, and `DELETE` predicates;
- composition with existing boolean predicate operators;
- reopen persistence after an update driven by `LIKE`;
- file-backed preamble safety;
- unknown columns and unsupported non-string left operands;
- unsupported pattern operands, explicit `ESCAPE`, non-ASCII patterns, and
  embedded `NUL` patterns;
- parser AST shape and source spans for `LIKE` and `NOT LIKE`.

## Performance

This phase stays close to SQLite execution. MyLite resolves descriptors,
decodes and validates one bound pattern literal, and emits a standard SQLite
predicate. Rows are not materialized into MyLite memory for pattern matching.
SQLite remains responsible for row access and any usable physical optimization;
this phase does not claim optimizer parity.
