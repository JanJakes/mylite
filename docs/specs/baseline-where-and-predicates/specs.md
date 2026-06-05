# Baseline WHERE AND Predicates

## Status

This phase expands the existing descriptor-backed `WHERE` predicate subset from
one predicate atom to an `AND` conjunction of supported predicate atoms.

The feature is intentionally not full MySQL expression support. It admits only
the existing descriptor-column comparison and `IS [NOT] NULL` atoms, joined by
`AND` or MySQL's deprecated `&&` synonym, on statement shapes that already
support baseline `WHERE`.

The design is based on independently authored MyLite behavior, official MySQL
8.4 documentation, and observed MySQL 8.4.9 runtime probes. Relevant MySQL
documentation:

- https://dev.mysql.com/doc/refman/8.4/en/select.html
- https://dev.mysql.com/doc/refman/8.4/en/delete.html
- https://dev.mysql.com/doc/refman/8.4/en/update.html
- https://dev.mysql.com/doc/refman/8.4/en/logical-operators.html
- https://dev.mysql.com/doc/refman/8.4/en/operator-precedence.html

The MySQL 8.4.9 expectation script for this phase is:

- `packages/libmylite/tests/mysql_baseline_where_and_predicates_expectations.sh`

## Scope

Supported `WHERE` predicate shape:

```sql
predicate_atom
predicate_atom AND predicate_atom [AND predicate_atom ...]
predicate_atom && predicate_atom [&& predicate_atom ...]
(predicate)
```

Each `predicate_atom` is one of the existing baseline forms:

```sql
column_reference comparison_operator integer_or_boolean_value
column_reference IS NULL
column_reference IS NOT NULL
```

`comparison_operator` is one of:

```sql
=  <=>  <>  !=  <  <=  >  >=
```

`integer_or_boolean_value` is an unsigned decimal integer literal in the
current descriptor range, optionally prefixed by unary `+` or `-`, an exact
quoted signed integer string with optional leading and trailing ASCII
whitespace, an empty or ASCII-whitespace-only quoted string that MySQL treats as
integer zero for descriptor integer comparisons, or `TRUE` / `FALSE`. Boolean
literals convert to `1` and `0`.

The feature applies to the existing statement families that already call the
shared descriptor-backed `WHERE` planner:

- table-backed `SELECT`;
- `SELECT DISTINCT column`;
- table-backed `COUNT()` and single-column aggregate `SELECT`;
- grouped aggregate source filtering before `GROUP BY`;
- source `SELECT` paths reused by `INSERT ... SELECT`, `REPLACE ... SELECT`,
  and `CREATE TABLE ... SELECT`;
- single-table `DELETE`;
- single-table `UPDATE`.

`SELECT` source predicates keep the existing source-qualification policy:
unqualified references, source table references, schema-table references, and
supported table aliases are resolved through the selected single-source
context. `DELETE` and `UPDATE` remain unqualified descriptor-column forms.

`&&` is accepted as MySQL accepts it, but it is deprecated. Each `&&` token in
an executed supported statement records warning `1287` with the message:

```text
'&&' is deprecated and will be removed in a future release. Please use AND instead
```

`AND` itself records no warning.

## Out Of Scope

This phase does not add:

- `OR`, `||`, `XOR`, `NOT`, `!`, or mixed boolean-expression parsing;
- bare truth tests such as `WHERE TRUE` or `WHERE column`;
- `IS TRUE`, `IS FALSE`, `IS UNKNOWN`, `BETWEEN`, `IN`, `LIKE`, `REGEXP`, row
  constructors, quantified comparisons, or subqueries;
- literal-left comparisons, column-to-column comparisons, expression
  comparisons, arithmetic, functions, casts, collations, parameters, variables,
  or arbitrary expression evaluation;
- `NULL` comparison literals such as `column = NULL` or `column <=> NULL`;
- noninteger string, decimal, float, hex, bit, temporal, JSON, or binary
  literals;
- boolean composition in `HAVING`;
- joins, multi-table DML, aliases for `DELETE` or `UPDATE`, full MySQL
  resolver rules, optimizer hints, locks, privileges, indexes, constraints,
  triggers, cascades, or SQLite fork patches.

MySQL accepts many of these broader forms. MyLite must reject them
deterministically until the expression planner owns their semantics.

## MySQL Behavior Verified

Runtime probes against MySQL 8.4.9 verify the following behavior for the
admitted surface:

- `AND` and `&&` behave as logical conjunction over predicate results.
- Parentheses may group admitted conjunctions without changing their result.
- Comparisons and `IS [NOT] NULL` atoms keep the existing three-valued SQL
  behavior; rows pass only when the complete `WHERE` predicate evaluates true.
- `AND` has lower precedence than comparisons and higher precedence than `XOR`
  and `OR`; MyLite avoids mixed boolean expressions in this phase.
- `&&` records one warning 1287 per token. `AND` records no warning.
- `WHERE` filtering happens before grouping, aggregate calculation, DML row
  mutation, ordering, and limiting.
- `UPDATE` affected rows remain changed-row counts; `DELETE` affected rows
  remain deleted-row counts.
- Unknown predicate columns report MySQL error 1054 / SQLSTATE `42S22` with
  `Unknown column '<name>' in 'where clause'` in MySQL. MyLite keeps the
  current descriptor resolver diagnostics for unknown names.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public call
  validation, result-handle ownership, public misuse behavior, and failure
  cleanup.
- Statement context owns diagnostics reset, warning count, affected rows, and
  previous-diagnostics snapshot behavior. `&&` warnings are statement
  diagnostics and must appear in public result warning counts and `SHOW
  WARNINGS`.
- Lexer/parser/AST own syntax admission, source spans, and whether a logical
  connective came from `AND` or `&&`. They stay independent of runtime,
  catalog, storage, and SQLite.
- Analyzer/planner owns descriptor name resolution, unsupported-shape
  rejection, integer/boolean literal conversion, deprecated-operator warning
  recording, and generation of a physical predicate plan.
- Catalog descriptors remain authoritative for schema, table, and column
  resolution. SQLite schema text is not consulted for MySQL-visible names.
- Result building remains unchanged. `SELECT` statements materialize only the
  final result rows; DML statements return through existing non-row result
  conventions.
- SQLite owns physical row scanning, filtering, grouping, ordering, limiting,
  updating, and deleting for generated SQL.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload. This
  feature does not change file format, VFS behavior, catalog generation,
  descriptor versions, or SQLite schema generation.

## Supported Grammar

The feature extends only the reusable `predicate` grammar used by `WHERE`.
`HAVING` keeps its current single-atom grammar.

MyLite Lemon-syntax snippet:

```lemon
where_clause_opt ::= .
where_clause_opt ::= WHERE predicate.

predicate ::= predicate_conjunction.

predicate_conjunction ::= predicate_primary.
predicate_conjunction ::= predicate_conjunction AND predicate_primary.
predicate_conjunction ::= predicate_conjunction LOGICAL_AND predicate_primary.

predicate_primary ::= predicate_atom.
predicate_primary ::= LPAREN predicate RPAREN.

predicate_atom ::= qualified_identifier comparison_operator predicate_integer_value.
predicate_atom ::= qualified_identifier IS NULL.
predicate_atom ::= qualified_identifier IS NOT NULL.
```

The grammar is left-associative for admitted conjunctions. Since every admitted
operator has the same truth-function and no side effects, left association has
no visible difference for this subset. Parenthesized conjunctions are preserved
in the AST but may be flattened by the planner.

The parser adapter maps reserved keyword `AND` to a parser token and maps the
lexer `&&` operator to a distinct parser token so runtime planning can record
deprecation warnings.

## Name Resolution

Every predicate atom resolves its column independently through the existing
descriptor resolver:

- table-backed `SELECT` and aggregate source filters may use the supported
  source-qualification forms for the single selected source table or alias;
- `DELETE` and `UPDATE` predicates remain unqualified descriptor-column forms;
- invisible descriptor columns may be named explicitly, matching the existing
  predicate subset;
- unknown columns use the existing deterministic unknown-column diagnostics;
- reserved `_mylite_*` schema or table names are rejected before SQLite SQL is
  generated;
- descriptor lookup keeps the current catalog case-insensitive identifier
  comparison expectations.

The implementation must plan and convert atoms from left to right so
diagnostics are deterministic when multiple atoms contain unsupported or
unknown names.

## Value Conversion

Each comparison atom reuses the existing MyLite-owned predicate conversion:

- signed integer descriptor columns admit values in their signed type range;
- unsigned descriptor columns admit nonnegative values in their unsigned type
  range, limited by the current signed-64 SQLite physical storage envelope
  where applicable;
- `TRUE` and `FALSE` convert to `1` and `0`;
- empty or ASCII-whitespace-only quoted strings convert to `0` without a
  warning in descriptor integer predicates, matching MySQL's observed behavior;
- out-of-range predicate literals produce the current MyLite predicate range
  diagnostic for the referenced descriptor column;
- `IS NULL` and `IS NOT NULL` require no bound literal.

No new literal or expression conversions are introduced.

## Execution And SQLite Handling

This feature is a MyLite wrapper/translation change over public SQLite APIs.
It does not require SQLite fork patches.

The planner lowers the conjunction to an internal list or tree of descriptor
predicate atoms. SQL generation emits one SQLite `WHERE` clause:

```sql
WHERE ("col_a" <op> ?)
  AND ("col_b" IS NOT NULL)
  AND ("col_c" <op> ?)
```

Every generated SQLite identifier is quoted. Every comparison literal is bound
through `sqlite3_bind_int64()`. User literals are never interpolated into
generated SQLite SQL.

For limited `DELETE` and `UPDATE`, the existing rowid subquery strategy remains
unchanged. The predicate conjunction is emitted inside the rowid subquery when
`LIMIT` is present so SQLite still performs filtering, ordering, and limiting:

```sql
DELETE FROM "physical"
WHERE rowid IN (
    SELECT rowid FROM "physical"
    WHERE ("a" = ?) AND ("b" IS NULL)
    ORDER BY "a" ASC
    LIMIT ?
)
```

The unlimited `UPDATE` path still appends the existing changed-row condition
after the user predicate with another SQLite `AND`, preserving MySQL changed-row
affected semantics.

## Result And Diagnostics

Successful supported statements use existing result conventions:

- `SELECT` returns descriptor-backed result rows and columns;
- aggregate and grouped aggregate statements keep their existing metadata and
  result formatting;
- `INSERT ... SELECT`, `REPLACE ... SELECT`, and `CREATE TABLE ... SELECT`
  keep their existing affected-row behavior;
- `DELETE` and `UPDATE` keep their existing affected-row behavior;
- `AND` contributes no warnings;
- each `&&` contributes one warning 1287;
- supported in-range `AND` statements without `&&` have `warning_count == 0`.

Unsupported syntax generally fails in the parser. Unsupported AST shapes that
the parser admits fail during planning with deterministic MyLite unsupported
diagnostics.

## Diagnostics

The implementation must preserve existing diagnostics for:

- syntax errors and unsupported grammar;
- missing default schema, unknown schema, unknown table, and reserved names;
- unsupported object kind;
- unknown predicate column;
- unsupported predicate shape;
- unsupported literal or expression;
- integer out-of-range predicate literal;
- physical SQLite failure;
- allocation failure;
- public API misuse.

New diagnostic behavior:

- each admitted `&&` token records warning 1287 with MySQL-compatible text.

## Tests

Add MySQL-runtime-verified expectations and C tests for:

- `AND` over comparison atoms, `IS NULL`, `IS NOT NULL`, `<=>`, boolean
  literals, signed literals, and unsigned boundary literals;
- `&&` behavior and one warning per token;
- parenthesized atoms and parenthesized conjunctions;
- false and unknown predicate outcomes filtering rows out;
- source-qualified `SELECT` and table-alias predicates;
- `SELECT DISTINCT`, `COUNT`, column aggregates, grouped aggregate source
  `WHERE`, `INSERT ... SELECT`, `REPLACE ... SELECT`, and `CREATE TABLE ...
  SELECT` reuse;
- `DELETE` and `UPDATE`, including affected rows, `ORDER BY`, and `LIMIT`;
- unknown first and later predicate columns;
- out-of-range first and later predicate literals;
- unsupported `OR`, `XOR`, `NOT`, bare truth tests, expression predicates,
  column-to-column predicates, literal-left predicates, string/decimal/float/
  hex/bit values, parameters, functions, subqueries, and `HAVING` boolean
  composition;
- persistence after reopen, rename/drop interactions where existing lifecycle
  tests cover them, independent file-backed handles, preamble preservation, and
  zero-initialized cleanup for any new planner state.
