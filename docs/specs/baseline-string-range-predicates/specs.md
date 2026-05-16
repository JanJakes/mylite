# Baseline String Range Predicates

## Summary

This phase extends descriptor-backed string predicates beyond equality and
pattern matching. The immediate application-facing target is text-stored
timestamp filters such as:

```sql
SELECT id FROM options
WHERE option_value BETWEEN '2016-01-15T00:00:00Z' AND '2016-01-15T23:59:59Z'
ORDER BY option_value
```

The slice admits ASCII string literals on the right of range and membership
predicates for existing `CHAR`, `VARCHAR`, and baseline `TEXT` family
descriptor columns. It does not add general expression evaluation, non-ASCII
collation parity, binary strings, casts, parameters, subqueries, or arbitrary
SQLite pass-through.

## Sources And Evidence

- MyLite architecture and standards:
  - `README.md`
  - `AGENTS.md`
  - `docs/architecture/engineering-standards.md`
  - `third_party/sqlite/README.md`
- Current predicate and string slices:
  - `docs/specs/baseline-string-equality-predicates/specs.md`
  - `docs/specs/baseline-like-predicates/specs.md`
  - `docs/specs/baseline-regexp-rlike-predicates/specs.md`
  - `docs/specs/baseline-string-order-lifecycle/specs.md`
  - `docs/specs/baseline-where-and-predicates/specs.md`
- Compatibility docs:
  - `COMPATIBILITY.md`
  - `docs/compatibility/sql-query-expressions.md`
  - `docs/compatibility/sql-table-dml.md`
  - `docs/compatibility/operators.md`
  - `docs/compatibility/type-system-literals-conversion.md`
- Official MySQL 8.4 Reference Manual:
  - comparison functions and operators:
    <https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html>
  - string comparison functions and operators:
    <https://dev.mysql.com/doc/refman/8.4/en/string-comparison-functions.html>
  - sorting rows:
    <https://dev.mysql.com/doc/refman/8.4/en/sorting-rows.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_string_range_predicates_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## Runtime Observations

MySQL 8.4.9 behavior used for this slice:

- `CHAR`, `VARCHAR`, and `TEXT` columns can be compared to string literals with
  `<`, `<=`, `>`, and `>=`.
- `CHAR`, `VARCHAR`, and `TEXT` columns can be filtered with `BETWEEN`,
  `NOT BETWEEN`, `IN`, and `NOT IN` when bounds or list values are compatible
  string constants. `IN` and `NOT IN` also participate in MySQL's normal
  three-valued `NULL` list semantics.
- Under the default `utf8mb4_0900_ai_ci` collation, ASCII case differences do
  not make otherwise equal values distinct. Runtime probes show `v >= 'abc'`
  matching both `abc` and `ABC`.
- `VARCHAR` and `TEXT` trailing spaces are significant for range and
  membership predicates under the verified default collation surface. A stored
  `VARCHAR` value `abc  ` compares greater than the literal `abc`, is not
  between `ab` and `abc`, and does not belong to `IN ('abc', ...)`.
- `CHAR` predicates observe the current canonical trimmed storage/readback
  shape. A value inserted as `abc  ` into `CHAR(5)` participates as `abc`.
- Ordinary comparison and range predicates do not match `NULL` column values.
  `NOT (v > 'abc')` excludes `NULL`.
- `v IN ('abc', NULL)` matches equal non-`NULL` values. `v NOT IN ('abc',
  NULL)` matches no rows when every non-equal result is `NULL`.
- ISO-like timestamp text in `VARCHAR` columns is compared as ordinary string
  data. MySQL does not apply temporal conversion merely because the literal
  text looks like `2016-01-15T00:00:00Z`.
- Successful supported statements report warning count `0`.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` and result ownership conventions do
  not change.
- Statement context: no new public statement state. Supported predicates share
  existing diagnostics, warning-count, affected-row, and result behavior.
- Lexer/parser/AST: unchanged. Existing comparison, `BETWEEN`, `IN`, logical
  `NOT`, and predicate-list nodes already represent the SQL shapes.
- Analyzer/planner: extends the shared descriptor predicate planner. Left
  operands resolve through MyLite descriptors; right operands decode through
  MyLite string-literal handling and are validated before preparing SQLite SQL.
- Catalog: read-only descriptor authority. The feature does not mutate catalog
  rows, descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- SQLite physical execution: generated SQL uses stable MyLite physical table
  names, quoted descriptor identifiers, MyLite's registered
  `utf8mb4_0900_ai_ci` ASCII collation, and bound parameters. It does not
  require a SQLite fork patch.
- Storage/VFS: unchanged. Row storage, `.mylite` preamble, and shifted SQLite
  payload invariants are unaffected.

## Supported SQL

The existing descriptor predicate contexts admit:

```sql
string_range_predicate:
    string_column <  string_literal
  | string_column <= string_literal
  | string_column >  string_literal
  | string_column >= string_literal
  | string_column BETWEEN string_literal AND string_literal
  | string_column NOT BETWEEN string_literal AND string_literal
  | string_column IN (string_or_null_literal[, ...])
  | string_column NOT IN (string_or_null_literal[, ...])
```

Supported contexts:

```sql
SELECT ... FROM table WHERE string_range_predicate
SELECT aggregate FROM table WHERE string_range_predicate
DELETE FROM table WHERE string_range_predicate
UPDATE table SET column = value WHERE string_range_predicate
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
text without embedded `NUL`. `IN` lists may additionally contain `NULL`.

### MyLite Lemon-Syntax Snippet

No new parser production is needed. Existing predicate grammar already admits
the shapes:

```lemon
predicate(A) ::= expression(B) LESS expression(C).
predicate(A) ::= expression(B) LESS_EQUAL expression(C).
predicate(A) ::= expression(B) GREATER expression(C).
predicate(A) ::= expression(B) GREATER_EQUAL expression(C).
predicate(A) ::= expression(B) BETWEEN expression(C) AND expression(D).
predicate(A) ::= expression(B) NOT BETWEEN expression(C) AND expression(D).
predicate(A) ::= expression(B) IN LPAREN predicate_value_list(C) RPAREN.
predicate(A) ::= expression(B) NOT IN LPAREN predicate_value_list(C) RPAREN.
```

Analyzer acceptance for this phase:

```lemon
string_range_predicate(A) ::= descriptor_string_column(B) string_range_operator(C)
                              string_literal(D).
string_range_predicate(A) ::= descriptor_string_column(B) BETWEEN string_literal(C)
                              AND string_literal(D).
string_range_predicate(A) ::= descriptor_string_column(B) IN LPAREN
                              string_or_null_literal_list(C) RPAREN.

string_range_operator(A) ::= LESS.
string_range_operator(A) ::= LESS_EQUAL.
string_range_operator(A) ::= GREATER.
string_range_operator(A) ::= GREATER_EQUAL.

string_or_null_literal(A) ::= string_literal(B).
string_or_null_literal(A) ::= NULL(B).
```

These snippets describe MyLite's supported subset, not MySQL's full grammar.

## Semantics

Planning:

1. Resolve the left operand through the existing descriptor-column resolver for
   the current statement context.
2. Require a supported nonbinary string descriptor type.
3. Decode each non-`NULL` right operand as an ordinary string literal using
   current MyLite string-literal and SQL-mode rules.
4. Reject embedded `NUL` and non-ASCII literal bytes for this slice.
5. Bind each decoded string as a `TEXT` parameter and each admitted `NULL` list
   value as a `NULL` parameter.

Generated SQLite predicate SQL keeps the existing descriptor-value shape:

```sql
("column_name" COLLATE "utf8mb4_0900_ai_ci" > ?N)
("column_name" COLLATE "utf8mb4_0900_ai_ci" BETWEEN ?N AND ?N)
("column_name" COLLATE "utf8mb4_0900_ai_ci" IN (?N, ?N, ...))
```

The column identifier is always generated from the descriptor and quoted as a
SQLite identifier. Literal values are always bound as prepared-statement
parameters.

Supported comparison semantics:

- ASCII case folding follows MyLite's registered `utf8mb4_0900_ai_ci` ASCII
  collation.
- The current slice claims MySQL parity only when stored compared values and
  predicate literals are in the ASCII subset. Full Unicode UCA 9.0 weights,
  accent-insensitive comparison, contractions, expansions, locale-tailored
  weights, and non-ASCII behavior remain deferred.
- `CHAR` storage and comparison observe the existing canonicalized `CHAR`
  storage/readback policy.
- `VARCHAR` and `TEXT` trailing spaces remain significant for this slice.
- `NULL` column behavior follows SQL three-valued logic: range and membership
  predicates do not match `NULL`, and `NOT` over a `NULL` predicate remains
  unmatched in `WHERE`.
- `IN` lists with `NULL` values use MySQL-compatible three-valued semantics
  for the admitted list shapes.
- ISO-like datetime strings in string descriptor columns are not converted to
  temporal values. They are ordinary string operands.

## Unsupported

Deferred until later slices:

- non-ASCII string comparison parity;
- binary strings and binary collations;
- `ENUM` / `SET` range and membership predicates;
- literal-left, expression-left, function, parameter, variable,
  column-to-column, subquery, row-constructor, or generated-column operands;
- numeric-to-string and string-to-numeric comparison conversion;
- `BETWEEN` or comparison bounds that are `NULL`, numeric, temporal, boolean,
  hex, bit, or other non-string literals for string descriptor targets;
- empty `IN` lists, `IN` subqueries, row constructors, or expression list
  values outside ordinary string literals and `NULL`;
- optimizer or index-use parity with MySQL.

## Diagnostics

Supported diagnostics:

- parser syntax errors through existing parse diagnostics;
- unknown columns through existing descriptor-column diagnostics;
- unsupported non-string left operands through existing type gates for the
  target predicate family;
- unsupported non-string right operands with deterministic unsupported
  diagnostics;
- non-ASCII or embedded-`NUL` predicate literals with deterministic
  unsupported diagnostics;
- allocation failures through existing `MYLITE_NOMEM` and diagnostics;
- physical SQLite failures through existing physical-row diagnostics.

## Tests

Add MySQL-runtime expectation coverage for:

- `<`, `<=`, `>`, and `>=` over `VARCHAR` strings with ASCII case folding;
- `VARCHAR` and `TEXT` trailing-space range behavior;
- `CHAR` canonicalized range behavior;
- `BETWEEN`, `NOT BETWEEN`, `IN`, and `NOT IN`;
- `IN` / `NOT IN` with a `NULL` list value;
- ISO-like datetime text in `VARCHAR` range predicates and string ordering;
- aggregate-source, `UPDATE`, and `DELETE` use with affected rows and warning
  count.

Add MyLite C coverage for:

- successful `SELECT`, aggregate, `UPDATE`, and `DELETE` predicates;
- composition with existing boolean predicate operators;
- `IN` list `NULL` semantics;
- reopen persistence after an update driven by a string range predicate;
- file-backed preamble safety;
- unknown columns and unsupported non-string left operands;
- unsupported right operands, non-ASCII literals, and embedded `NUL` literals.

## Performance

This feature stays close to SQLite execution. MyLite resolves descriptors,
decodes and validates literal parameters once during planning, and emits normal
SQLite predicate SQL using the existing registered collation. Rows are not
materialized into MyLite memory for comparison. SQLite remains responsible for
row access and any usable physical optimization; this phase does not claim
optimizer parity.
