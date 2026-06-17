# Baseline WHERE OR Predicates

## Status

This phase expands the descriptor-backed `WHERE` predicate subset from `AND`
conjunctions to limited boolean expressions with `OR` disjunctions.

The feature is not full MySQL expression support. It admits only the existing
descriptor-column comparison and `IS [NOT] NULL` predicate atoms, combined with
`AND`/`&&`, `OR`/`||`, and parentheses, on statement shapes that already support
baseline `WHERE`.

The design is based on independently authored MyLite behavior, official MySQL
8.4 documentation, and observed MySQL 8.4.9 runtime probes. Relevant MySQL
documentation:

- https://dev.mysql.com/doc/refman/8.4/en/select.html
- https://dev.mysql.com/doc/refman/8.4/en/delete.html
- https://dev.mysql.com/doc/refman/8.4/en/update.html
- https://dev.mysql.com/doc/refman/8.4/en/logical-operators.html
- https://dev.mysql.com/doc/refman/8.4/en/operator-precedence.html
- https://dev.mysql.com/doc/refman/8.4/en/built-in-function-reference.html

The MySQL 8.4.9 expectation script for this phase is:

- `packages/libmylite/tests/mysql_baseline_where_or_predicates_expectations.sh`

## Scope

Supported `WHERE` predicate shape:

```sql
predicate_atom
predicate AND predicate
predicate && predicate
predicate OR predicate
predicate || predicate
(predicate)
```

The grammar applies MySQL's default precedence for this slice:

```text
comparison / IS predicates
AND / &&
OR / ||
```

`AND` and `OR` groups are left-associative at their precedence level. Parentheses
may override the default grouping.

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
current descriptor range, optionally prefixed by unary `+` or `-`, or `TRUE` /
`FALSE`. Boolean literals convert to `1` and `0`.

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
context. Single-table `DELETE` and `UPDATE` now share the qualified descriptor-column policy documented in [baseline qualified predicate columns](../baseline-qualified-predicate-columns/specs.md).

`||` is accepted as MySQL accepts it under the default SQL mode, where it is a
logical `OR` synonym. It is deprecated in that mode. Each `||` token in an
executed supported statement records warning `1287` with the message:

```text
'|| as a synonym for OR' is deprecated and will be removed in a future release. Please use OR instead
```

`OR` itself records no warning. Existing `&&` warnings continue to use the
baseline `AND` warning text.

## Out Of Scope

This phase does not add:

- `XOR`, `NOT`, `!`, or boolean composition in `HAVING`;
- bare truth tests such as `WHERE TRUE`, `WHERE column`, or literal operands
  joined directly by `OR`;
- `IS TRUE`, `IS FALSE`, `IS UNKNOWN`, `BETWEEN`, `IN`, `LIKE`, `REGEXP`, row
  constructors, quantified comparisons, or subqueries;
- literal-left comparisons, column-to-column comparisons, expression
  comparisons, arithmetic, functions, casts, collations, parameters, variables,
  or arbitrary expression evaluation;
- `NULL` comparison literals such as `column = NULL` or `column <=> NULL`;
- string, decimal, float, hex, bit, temporal, JSON, or binary literals;
- `PIPES_AS_CONCAT`, mutable `sql_mode`, or `||` string concatenation;
- joins, multi-table DML, aliases for `DELETE` or `UPDATE`, full MySQL
  resolver rules, optimizer hints, locks, privileges, indexes, constraints,
  triggers, cascades, or SQLite fork patches.

MySQL accepts many broader forms. MyLite must reject them deterministically until
the expression planner owns their semantics.

## MySQL Behavior Verified

Runtime probes against MySQL 8.4.9 verify the following behavior for the
admitted surface:

- `OR` and `||` behave as logical disjunction over predicate results under the
  default SQL mode.
- `AND` binds more tightly than `OR`; parentheses override the default grouping.
- Comparisons and `IS [NOT] NULL` atoms keep SQL three-valued behavior. Rows
  pass only when the complete `WHERE` predicate evaluates true.
- With a `NULL` operand, `OR` returns true when the other operand is true,
  returns unknown when the other operand is false or unknown, and filters out
  unknown rows in `WHERE`.
- `||` records one warning 1287 per token under the default SQL mode. `OR`
  records no warning.
- `WHERE` filtering happens before grouping, aggregate calculation, DML row
  mutation, ordering, and limiting.
- `UPDATE` affected rows remain changed-row counts; `DELETE` affected rows
  remain deleted-row counts.
- Unknown predicate columns report MySQL error 1054 / SQLSTATE `42S22` with
  `Unknown column '<name>' in 'where clause'` in MySQL. MyLite keeps the
  current descriptor resolver diagnostics for unknown names.

MyLite intentionally keeps the current descriptor-range predicate conversion
policy even where MySQL's general expression engine would coerce or compare a
broader literal. Unsupported literals and out-of-range descriptor predicate
inputs remain deterministic MyLite diagnostics.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public call
  validation, result-handle ownership, public misuse behavior, and failure
  cleanup.
- Statement context owns diagnostics reset, warning count, affected rows, and
  previous-diagnostics snapshot behavior. `&&` and `||` warnings are statement
  diagnostics and must appear in public result warning counts and `SHOW
  WARNINGS`.
- Lexer/parser/AST own syntax admission, source spans, and whether a logical
  connective came from keyword or symbolic spelling. They stay independent of
  runtime, catalog, storage, and SQLite.
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

predicate ::= predicate_disjunction.

predicate_disjunction ::= predicate_conjunction.
predicate_disjunction ::= predicate_disjunction OR predicate_conjunction.
predicate_disjunction ::= predicate_disjunction LOGICAL_OR predicate_conjunction.

predicate_conjunction ::= predicate_primary.
predicate_conjunction ::= predicate_conjunction AND predicate_primary.
predicate_conjunction ::= predicate_conjunction LOGICAL_AND predicate_primary.

predicate_primary ::= predicate_atom.
predicate_primary ::= LPAREN predicate RPAREN.

predicate_atom ::= qualified_identifier comparison_operator predicate_integer_value.
predicate_atom ::= qualified_identifier IS NULL.
predicate_atom ::= qualified_identifier IS NOT NULL.
```

The parser adapter maps reserved keyword `OR` to a parser token and maps the
lexer `||` operator to a distinct parser token so runtime planning can record
deprecation warnings. Existing `AND` / `&&` mapping remains unchanged.

## Name Resolution

Every predicate atom resolves its column independently through the existing
descriptor resolver:

- table-backed `SELECT` and aggregate source filters may use the supported
  source-qualification forms for the single selected source table or alias;
- qualifier support for current single-table `DELETE` and `UPDATE` predicate columns is covered by [baseline qualified predicate columns](../baseline-qualified-predicate-columns/specs.md);
- invisible descriptor columns may be named explicitly, matching the existing
  predicate subset;
- unknown columns use the existing deterministic unknown-column diagnostics;
- reserved `_mylite_*` schema or table names are rejected before SQLite SQL is
  generated;
- descriptor lookup keeps the current catalog case-insensitive identifier
  comparison expectations.

The implementation must plan and convert predicate atoms from left to right so
diagnostics and warning order are deterministic when multiple terms contain
unsupported or deprecated forms. This is a planning-order rule, not a runtime
short-circuit promise.

## Value Conversion

Each comparison atom reuses the existing MyLite-owned predicate conversion:

- signed integer descriptor columns admit values in their signed type range;
- unsigned descriptor columns admit nonnegative values in their unsigned type
  range, limited by the current signed-64 SQLite physical storage envelope
  where applicable;
- `TRUE` and `FALSE` convert to `1` and `0`;
- out-of-range predicate literals produce the current MyLite predicate range
  diagnostic for the referenced descriptor column;
- `IS NULL` and `IS NOT NULL` require no bound literal.

No new literal or expression conversions are introduced.

## Execution And SQLite Handling

This feature is a MyLite wrapper/translation change over public SQLite APIs.
There are no SQLite fork changes.

The planner must preserve a boolean predicate tree rather than flattening all
terms into one conjunction. Generated SQLite SQL must:

- use the descriptor-resolved physical table name;
- quote every generated SQLite identifier;
- emit parentheses around each planned boolean node or term so MySQL precedence
  and explicit grouping remain visible in the generated shape;
- represent `AND` and `OR` with SQLite's standard boolean operators;
- bind every comparison literal as a prepared-statement parameter in
  left-to-right SQL emission order;
- continue binding `LIMIT` parameters after predicate parameters in the same
  statement-specific order used by the existing SELECT/DELETE/UPDATE paths.

SQLite's three-valued `AND` / `OR` behavior is close enough for this admitted
integer/`NULL` subset because the only operands are generated comparisons and
`IS [NOT] NULL` predicates over integer or NULL storage values. MyLite must not
materialize candidate rows into C just to evaluate the boolean expression.

For `DELETE` and `UPDATE` with `ORDER BY ... LIMIT`, the rowid subquery shape
continues to push the full predicate tree into SQLite before ordering and
limiting. This preserves the existing physical table invariant that MyLite user
tables are rowid tables for ordered/limited DML. The rowid detail remains
internal to generated physical SQL.

## Result And Diagnostics Behavior

Successful supported statements keep existing result conventions:

- table `SELECT` and aggregate statements return their ordinary result sets;
- `INSERT ... SELECT`, `REPLACE ... SELECT`, and `CREATE TABLE ... SELECT`
  report existing affected-row counts;
- `DELETE` reports deleted rows;
- `UPDATE` reports changed rows;
- successful non-symbolic `OR` statements report `warning_count == 0`;
- successful `||` statements report one warning per `||` token;
- no catalog rows, descriptor versions, descriptor caches, catalog generation,
  or SQLite schema generation values are mutated by predicate planning.

Diagnostics:

- syntax errors for unsupported boolean forms report the existing parse error;
- unknown schemas, missing default schema, unknown tables, reserved MyLite names,
  unsupported object kinds, unknown predicate columns, unsupported predicate
  shapes, unsupported literals, predicate integer range errors, SQLite
  failures, allocation failures, and public API misuse keep the existing
  diagnostics for their layer;
- `PIPES_AS_CONCAT` and mutable SQL-mode behavior remain unsupported because
  MyLite currently exposes a fixed SQL-mode surface.

## Tests

The implementation must add focused parser and runtime C tests plus a
MySQL-runtime expectation script.

Parser tests:

- `OR` and `||` parse in `WHERE`;
- `AND` binds tighter than `OR`;
- parentheses override grouping;
- chains are left-associative at each supported precedence level;
- unsupported `XOR`, `NOT`, bare truth tests, expression predicates, and
  `HAVING` boolean composition remain rejected.

Runtime tests:

- successful `SELECT` filters for `OR`, `||`, mixed `AND`/`OR`, parentheses,
  `IS NULL`, `IS NOT NULL`, equality, inequality, comparison, `<=>`, boolean
  right operands, signed and unsigned boundary values, and nullable columns;
- `||` warning count and `SHOW WARNINGS` message;
- `SELECT DISTINCT`, `COUNT`, column aggregates, grouped aggregate source
  filtering, `CREATE TABLE ... SELECT`, `INSERT ... SELECT`, and
  `REPLACE ... SELECT`;
- `UPDATE` and `DELETE`, including ordered/limited forms and affected rows;
- schema-qualified and alias-qualified SELECT predicates where currently
  admitted;
- missing default schema, unknown schema, unknown table, reserved target names,
  unknown first/second predicate columns, and deterministic out-of-range
  predicate literal diagnostics;
- persistence after close/reopen and `.mylite` preamble preservation;
- independent file-backed handles with independent DML effects;
- zero-initialized cleanup and failure cleanup for new predicate plan objects;
- no regression for lexer, parser, statement context, catalog, row values,
  select/order/limit, delete, update, insert/replace select, grouped
  aggregates, diagnostics, storage/VFS, and result metadata tests.

The MySQL expectation script must verify the MySQL-visible supported behavior
against MySQL 8.4.9 and version-gate itself to `8.4.9*`.

## Review Checklist

- MySQL 8.4.9 evidence exists for every newly supported user-visible behavior.
- Grammar snippets and implementation remain independently authored.
- `HAVING` boolean composition remains out of scope.
- Predicate planning preserves MySQL precedence and explicit parentheses.
- Name resolution and conversion remain descriptor-driven.
- Generated SQLite SQL uses physical names, quoted identifiers, and bound
  parameters.
- Predicate filtering stays in SQLite; no C-side candidate row materialization
  is introduced.
- `||` warning count and message match MySQL 8.4.9 under MyLite's fixed default
  SQL-mode surface.
- DML affected rows, warning counts, file-format invariants, cleanup paths,
  compatibility docs, and tests match the exact supported subset.
