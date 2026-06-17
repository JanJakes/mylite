# Baseline WHERE NOT Predicates

## Status

This phase expands the descriptor-backed `WHERE` predicate subset with keyword
`NOT` over the existing predicate atoms and boolean groups.

The feature is not full MySQL expression support. It admits only the existing
descriptor-column comparison and `IS [NOT] NULL` predicate atoms, combined with
`AND`/`&&`, `OR`/`||`, keyword `NOT`, and parentheses, on statement shapes that
already support baseline `WHERE`.

The design is based on independently authored MyLite behavior, official MySQL
8.4 documentation, and observed MySQL 8.4.9 runtime probes. Relevant MySQL
documentation:

- https://dev.mysql.com/doc/refman/8.4/en/select.html
- https://dev.mysql.com/doc/refman/8.4/en/delete.html
- https://dev.mysql.com/doc/refman/8.4/en/update.html
- https://dev.mysql.com/doc/refman/8.4/en/logical-operators.html
- https://dev.mysql.com/doc/refman/8.4/en/operator-precedence.html
- https://dev.mysql.com/doc/refman/8.4/en/expressions.html

The MySQL 8.4.9 expectation script for this phase is:

- `packages/libmylite/tests/mysql_baseline_where_not_predicates_expectations.sh`

## Scope

Supported `WHERE` predicate shape:

```sql
predicate_atom
predicate AND predicate
predicate && predicate
predicate OR predicate
predicate || predicate
NOT predicate
(predicate)
```

The grammar applies MySQL's default precedence for this slice:

```text
comparison / IS predicates
NOT
AND / &&
OR / ||
```

`NOT` is right-associative for repeated negation. `AND` and `OR` groups keep
their existing left-associative behavior at their precedence levels.
Parentheses may override the default grouping.

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

Keyword `NOT` records no warning. Existing `&&` and `||` deprecation warnings
continue unchanged.

## Out Of Scope

This phase does not add:

- symbolic `!`;
- `XOR` or boolean composition in `HAVING`;
- bare truth tests such as `WHERE TRUE`, `WHERE column`, or literal operands
  joined directly by boolean operators;
- `IS TRUE`, `IS FALSE`, `IS UNKNOWN`, `BETWEEN`, `IN`, `LIKE`, `REGEXP`, row
  constructors, quantified comparisons, or subqueries;
- literal-left comparisons, column-to-column comparisons, expression
  comparisons, arithmetic, functions, casts, collations, parameters, variables,
  or arbitrary expression evaluation;
- `NULL` comparison literals such as `column = NULL` or `column <=> NULL`;
- string, decimal, float, hex, bit, temporal, JSON, or binary literals;
- `HIGH_NOT_PRECEDENCE`, mutable `sql_mode`, or `!` expression semantics;
- joins, multi-table DML, aliases for `DELETE` or `UPDATE`, full MySQL
  resolver rules, optimizer hints, locks, privileges, indexes, constraints,
  triggers, cascades, or SQLite fork patches.

MySQL accepts many broader forms. MyLite must reject them deterministically until
the expression planner owns their semantics.

The symbolic `!` operator is intentionally deferred. MySQL 8.4.9 accepts `!`
with warning 1287, but its default precedence is higher than comparison
operators. For example, `! id = 1` is evaluated like `(!id) = 1`, while
`NOT id = 1` is evaluated like `NOT (id = 1)`. Accepting `!` as a simple
predicate-tree synonym for `NOT` would therefore admit wrong behavior.

## MySQL Behavior Verified

Runtime probes against MySQL 8.4.9 verify the following behavior for the
admitted surface:

- `NOT predicate_atom` negates comparison and `IS [NOT] NULL` predicate atoms.
- `NOT (predicate)` negates the full parenthesized boolean result.
- `NOT` binds more tightly than `AND` / `&&` and more loosely than comparison
  and `IS` predicates.
- Repeated `NOT` applies right-to-left for the admitted predicate subset.
- SQL three-valued behavior is preserved: negating unknown yields unknown, and
  unknown rows do not pass `WHERE`.
- `<=>` remains a two-valued NULL-safe comparison before negation. For example,
  `NOT nullable_col <=> 9` accepts rows where `nullable_col` is `NULL`.
- `NOT` records no warning.
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
  previous-diagnostics snapshot behavior. `NOT` does not add warnings, while
  existing `&&` and `||` warnings remain statement diagnostics.
- Lexer/parser/AST own syntax admission, source spans, and a distinct AST node
  for keyword `NOT`. They stay independent of runtime, catalog, storage, and
  SQLite.
- Analyzer/planner owns descriptor name resolution, unsupported-shape
  rejection, integer/boolean literal conversion, deprecated-operator warning
  recording for existing symbolic boolean operators, and generation of a
  physical predicate plan.
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

predicate_conjunction ::= predicate_negation.
predicate_conjunction ::= predicate_conjunction AND predicate_negation.
predicate_conjunction ::= predicate_conjunction LOGICAL_AND predicate_negation.

predicate_negation ::= predicate_primary.
predicate_negation ::= NOT predicate_negation.

predicate_primary ::= predicate_atom.
predicate_primary ::= LPAREN predicate RPAREN.

predicate_atom ::= qualified_identifier comparison_operator predicate_integer_value.
predicate_atom ::= qualified_identifier IS NULL.
predicate_atom ::= qualified_identifier IS NOT NULL.
```

The parser adapter maps reserved keyword `NOT` to a parser token in the new
predicate-negation position. The lexer `!` operator remains unmapped for this
grammar and therefore stays unsupported in MyLite for this phase.

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
unsupported or deprecated forms. `NOT` does not change the order in which child
predicate atoms are planned.

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

The planner must preserve a boolean predicate tree. Generated SQLite SQL must:

- use the descriptor-resolved physical table name;
- quote every generated SQLite identifier;
- emit parentheses around each planned boolean node or term so MySQL precedence
  and explicit grouping remain visible in the generated shape;
- represent keyword `NOT` with SQLite's standard unary `NOT` operator over a
  generated child predicate expression;
- represent `AND` and `OR` with SQLite's standard boolean operators;
- represent `<=>` with a descriptor-built NULL-safe physical comparison so
  composing it under `NOT` keeps MySQL's two-valued behavior;
- bind every comparison literal as a prepared-statement parameter in
  left-to-right SQL emission order;
- continue binding `LIMIT` parameters after predicate parameters in the same
  statement-specific order used by the existing SELECT/DELETE/UPDATE paths.

`DELETE` and `UPDATE` ordered/limited paths continue to use descriptor-built
rowid subqueries and must not rely on optional SQLite `ORDER BY` / `LIMIT` DML
syntax. `UPDATE` changed-row conditions must remain outside the planned `WHERE`
predicate and must be joined with `AND` after the complete negated predicate is
parenthesized.

The physical design stays close to SQLite's optimal path for the current
baseline: MyLite validates and translates names and values, then lets SQLite
scan, filter, sort, limit, update, and delete rows. MyLite does not materialize
candidate row sets to evaluate `NOT`.

## Result Behavior

Successful `SELECT` statements return rows through the existing result API.
Successful `DELETE` and `UPDATE` statements return no row result set and use the
existing non-query result conventions.

`affected_rows` remains:

- selected row count for result sets;
- changed-row count for `UPDATE`;
- deleted-row count for `DELETE`;
- inserted/replaced/copied row count for statement families that reuse the
  source `SELECT`.

`warning_count` is `0` for supported keyword-`NOT` statements unless another
already-supported construct in the statement records warnings, such as `&&` or
`||`.

## Diagnostics

The feature preserves existing diagnostics for:

- syntax errors and unsupported grammar;
- missing default schema, unknown schema, unknown table, and reserved
  `_mylite_*` names;
- unsupported object kind;
- unknown predicate columns;
- unsupported predicate shape;
- unsupported literals and predicate integer out-of-range;
- physical SQLite failures;
- allocation failures;
- public API misuse.

Unsupported symbolic `!` must be rejected deterministically. It must not be
accepted as a synonym for `NOT` until MyLite supports MySQL's `!` precedence and
warning behavior for the admitted expression surface.

## Tests

Add parser and fast C runtime tests under `packages/libmylite/tests/`, reusing
the existing predicate lifecycle binary unless a new binary makes the suite
clearer. Add a MySQL-runtime expectation script for the feature.

Coverage must include:

- parser AST shape for `NOT`, repeated `NOT`, precedence with `AND`/`OR`, and
  parentheses;
- successful filtered `SELECT` over comparisons, `<=>`, `IS NULL`, and
  `IS NOT NULL`;
- SQL three-valued behavior for negated comparisons over `NULL` values;
- interaction with existing `AND`/`&&`, `OR`/`||`, and warning counts;
- source reuse in distinct, aggregate, grouped aggregate, `CREATE TABLE ...
  SELECT`, `INSERT ... SELECT`, and `REPLACE ... SELECT`;
- `UPDATE` and `DELETE`, including ordered/limited forms and affected rows;
- persistence after close/reopen, file preamble preservation, and independent
  file-backed handles;
- schema-qualified and unqualified target behavior inherited from the existing
  source resolvers;
- unknown columns and out-of-range predicate literals through a negated
  predicate;
- deterministic rejection for unsupported `!`, `XOR`, bare truth tests,
  expression predicates, column-to-column comparisons, unsupported literals,
  subqueries, functions, parameters, and unsupported DML shapes;
- existing parser/runtime lifecycle suites still passing.

Verification before marking done:

1. `cmake --build --preset dev`
2. Focused CTest entries for parser and runtime predicate/DML/source-reuse
   lifecycles.
3. `packages/libmylite/tests/mysql_baseline_where_not_predicates_expectations.sh`
4. `cmake --workflow --preset check`

## Compatibility Docs

Update `COMPATIBILITY.md`, `docs/compatibility/operators.md`,
`docs/compatibility/sql-query-expressions.md`, and
`docs/compatibility/sql-table-dml.md` only for the exact keyword-`NOT` subset.
Do not overclaim symbolic `!`, full expression negation, mutable SQL mode,
boolean `HAVING`, or general expression evaluation.
