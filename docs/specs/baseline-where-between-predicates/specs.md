# Baseline WHERE BETWEEN Predicates

## Status

This phase expands MyLite's descriptor-backed `WHERE` predicate subset with
integer `BETWEEN` range predicates. It builds on `mylite_execute()`, statement
context, the MyLite parser and AST scaffold, durable table descriptors, integer
and `NULL` row values, descriptor-driven `SELECT ... WHERE`, descriptor-driven
`SELECT ... ORDER BY ... LIMIT`, single-table `DELETE`, single-table `UPDATE`,
and the existing `NOT` / `AND` / `OR` predicate tree.

The implementation is intentionally narrow. It supports one descriptor column on
the left side and two supported integer or boolean bound literals. It does not
introduce general expression evaluation.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Baseline select where lifecycle:
  `docs/specs/baseline-select-where-lifecycle/specs.md`
- Baseline WHERE AND predicates:
  `docs/specs/baseline-where-and-predicates/specs.md`
- Baseline WHERE OR predicates:
  `docs/specs/baseline-where-or-predicates/specs.md`
- Baseline WHERE NOT predicates:
  `docs/specs/baseline-where-not-predicates/specs.md`
- Baseline delete lifecycle:
  `docs/specs/baseline-delete-lifecycle/specs.md`
- Baseline update lifecycle:
  `docs/specs/baseline-update-lifecycle/specs.md`
- MySQL 8.4 Reference Manual, comparison functions and operators:
  https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html
- MySQL 8.4 Reference Manual, operator precedence:
  https://dev.mysql.com/doc/refman/8.4/en/operator-precedence.html
- MySQL 8.4 Reference Manual, expressions:
  https://dev.mysql.com/doc/refman/8.4/en/expressions.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## Scope

Supported `WHERE` predicate additions:

```sql
column_name BETWEEN lower_value AND upper_value
column_name NOT BETWEEN lower_value AND upper_value
NOT column_name BETWEEN lower_value AND upper_value
```

`lower_value` and `upper_value` are each limited to the existing predicate
right-operand value subset:

```sql
integer_literal
+ integer_literal
- integer_literal
TRUE
FALSE
```

The range predicate is available wherever the shared descriptor-backed `WHERE`
planner is already used:

- table `SELECT`;
- aggregate source filters;
- grouped aggregate source filters;
- `SELECT` sources reused by `INSERT ... SELECT`, `REPLACE ... SELECT`, and
  `CREATE TABLE ... SELECT`;
- single-table `DELETE`;
- single-table `UPDATE`.

`SELECT` source predicates keep the existing source-qualification policy:
unqualified references, source table references, schema-table references, and
supported source aliases resolve through the selected single-source context.
`DELETE` and `UPDATE` predicates remain unqualified descriptor-column forms.

## Out Of Scope

This phase does not add:

- arbitrary expression operands on the left side or either bound;
- literal-left range tests;
- `NULL` bounds;
- string, decimal, float, hex, bit, temporal, JSON, parameter, variable,
  function, cast, collation, subquery, row-constructor, or arithmetic bounds;
- column-to-column range tests;
- bare truth tests such as `WHERE column BETWEEN ...` outside the descriptor
  range form described here;
- `IS TRUE`, `IS FALSE`, `IS UNKNOWN`, `IN`, `LIKE`, `REGEXP`, quantified
  comparisons, row predicates, or subqueries;
- symbolic `!`;
- `XOR` or boolean composition in `HAVING`;
- joins, multi-table DML, aliases for `DELETE` or `UPDATE`, full MySQL resolver
  rules, optimizer hints, locks, privileges, indexes, constraints, triggers,
  cascades, or SQLite fork patches.

MySQL accepts many broader forms. MyLite must reject them deterministically until
the expression planner owns their semantics.

## MySQL Behavior Verified

Runtime probes against MySQL 8.4.9 verify the following behavior for the
admitted surface:

- `expr BETWEEN min AND max` is inclusive at both ends.
- If the lower bound is greater than the upper bound, the predicate is false
  for non-`NULL` tested values.
- A `NULL` tested value produces unknown. Unknown rows do not pass `WHERE`.
- `expr NOT BETWEEN min AND max` behaves as `NOT (expr BETWEEN min AND max)`;
  for `NULL` tested values it remains unknown.
- Prefix `NOT expr BETWEEN min AND max` uses MySQL's normal precedence and is
  equivalent to `NOT (expr BETWEEN min AND max)` for this admitted shape.
- `BETWEEN` binds more tightly than `NOT`, `AND` / `&&`, and `OR` / `||`, and
  less tightly than comparison operators. The first `AND` after `BETWEEN` is
  part of the range predicate; later `AND` tokens compose predicates.
- `TRUE` and `FALSE` bounds behave as integer `1` and `0` for this integer
  descriptor subset.
- Supported `BETWEEN` statements record no warnings.
- `WHERE` filtering happens before grouping, aggregate calculation, DML row
  mutation, ordering, and limiting.
- `UPDATE` affected rows remain changed-row counts; `DELETE` affected rows
  remain deleted-row counts.

The feature's MySQL expectation script is:

```text
packages/libmylite/tests/mysql_baseline_where_between_predicates_expectations.sh
```

## Ownership Boundary

- Public API remains unchanged. `mylite_execute()` owns public call validation,
  result-handle ownership, public misuse behavior, and cleanup on failure.
- Statement context owns the statement boundary: diagnostics reset, warning
  count, affected rows, and backend execution status.
- Lexer/parser/AST own syntax admission and source spans. They represent the
  descriptor range predicate independently of runtime, catalog, storage, and
  SQLite.
- Analyzer/planner code resolves the range column against MyLite catalog
  descriptors, converts both supported bounds through MyLite-owned integer
  predicate conversion, rejects unsupported shapes, and builds the physical
  SQLite predicate.
- The catalog module remains the metadata authority. Range predicates must not
  mutate catalog rows, descriptor versions, descriptor caches, catalog
  generation, or `sqlite_schema_generation`.
- The result builder owns descriptor-driven row results and non-query result
  conventions. `BETWEEN` adds no public result metadata surface.
- SQLite owns physical row storage and executes the generated scan/filter/update
  or delete. SQLite schema text and `PRAGMA` output are not metadata authority.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  Range predicates must not write through byte range `[0, 4096)` except through
  existing DML payload behavior.

## Supported Grammar

The feature extends only the reusable `predicate` grammar used by `WHERE`.

```sql
where_clause:
    WHERE predicate

predicate:
    predicate OR predicate
  | predicate || predicate
  | predicate AND predicate
  | predicate && predicate
  | NOT predicate
  | predicate_atom
  | ( predicate )

predicate_atom:
    column_name comparison_operator predicate_value
  | column_name IS NULL
  | column_name IS NOT NULL
  | column_name BETWEEN predicate_value AND predicate_value
  | column_name NOT BETWEEN predicate_value AND predicate_value
```

`predicate_value` is the existing integer/boolean predicate literal subset.

### MyLite Lemon-Syntax Snippet

This snippet describes MyLite's intended grammar extension, not MySQL's full
grammar:

```lemon
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
predicate_atom ::= qualified_identifier BETWEEN predicate_integer_value AND predicate_integer_value.
predicate_atom ::= qualified_identifier NOT BETWEEN predicate_integer_value AND predicate_integer_value.
```

The `qualified_identifier` token remains syntactically admitted so the analyzer
can apply the existing source-qualification policy. Unsupported qualified forms
still fail before SQLite SQL is generated.

## Name Resolution

Every range predicate resolves its left operand through the existing descriptor
resolver:

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

`BETWEEN` does not introduce bound-name resolution. Bounds are literals only in
this phase.

## Value Conversion

Both bounds reuse the existing MyLite-owned predicate conversion:

- signed integer descriptor columns admit values in their signed type range;
- unsigned descriptor columns admit nonnegative values in their unsigned type
  range, limited by the current signed-64 SQLite physical storage envelope where
  applicable;
- `TRUE` and `FALSE` convert to `1` and `0`;
- out-of-range bound literals produce the current MyLite predicate range
  diagnostic for the referenced descriptor column;
- `NULL` bounds are not admitted in this phase.

This intentionally keeps MyLite's current descriptor-range policy for predicate
literals. Some broader MySQL comparisons coerce out-of-descriptor-range
literals instead of erroring; those expression-conversion semantics remain out
of scope until MyLite owns full expression typing.

## Execution And SQLite Handling

This feature is a MyLite wrapper/translation change over public SQLite APIs.
There are no SQLite fork changes.

The planner must preserve a boolean predicate tree. Generated SQLite SQL must:

- use descriptor-resolved physical table names;
- quote every generated SQLite identifier;
- emit parentheses around each planned boolean node or term so MySQL precedence
  and explicit grouping remain visible in the generated shape;
- represent `BETWEEN` with SQLite's standard range predicate over two bound
  parameters;
- represent `NOT BETWEEN` as a planned `NOT` node around a planned `BETWEEN`
  node, reusing the existing keyword-`NOT` runtime behavior;
- bind the lower bound before the upper bound;
- continue binding later predicate, `LIMIT`, and `UPDATE` changed-row
  parameters in statement-specific order.

`DELETE` and `UPDATE` ordered/limited paths continue to use descriptor-built
rowid subqueries and must not rely on optional SQLite `ORDER BY` / `LIMIT` DML
syntax. `UPDATE` changed-row conditions must remain outside the planned `WHERE`
predicate and must be joined with `AND` after the complete range predicate is
parenthesized.

The physical design stays close to SQLite's optimal path for the current
baseline: MyLite validates names and values, then lets SQLite scan, filter,
sort, limit, update, and delete rows. MyLite does not materialize candidate row
sets to evaluate `BETWEEN`.

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

`warning_count` is `0` for supported `BETWEEN` statements unless another
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
- unsupported literal and expression operands;
- predicate integer out-of-range;
- physical SQLite failures;
- allocation failures;
- public API misuse.

Unsupported bound expressions such as `col BETWEEN other_col AND 9`, string
bounds, decimal bounds, parameter bounds, and `NULL` bounds must be rejected
deterministically. They must not be passed through to SQLite.

## Tests

Add or extend fast plain C tests under `packages/libmylite/tests/`. Reuse the
existing `runtime_where_and_predicates` binary unless a separate test binary
becomes clearer during implementation.

Coverage must include:

- parser AST shape for `BETWEEN`, `NOT BETWEEN`, prefix `NOT ... BETWEEN`,
  precedence with `AND`/`OR`, and parentheses;
- successful `SELECT` over inclusive bounds, reversed bounds, nullable tested
  values, boolean bounds, signed bounds, `INT`, `INTEGER`, `BIGINT`, and
  unsigned integer families within the current supported physical range;
- `NOT BETWEEN` and prefix `NOT ... BETWEEN`, including `NULL` tested values;
- interaction with `AND`, `OR`, and keyword `NOT`;
- aggregate source filters, grouped aggregate source filters, `COUNT`, `MIN` /
  `MAX`, and `SELECT DISTINCT` source reuse;
- `CREATE TABLE ... SELECT`, `INSERT ... SELECT`, and `REPLACE ... SELECT`
  source reuse;
- `UPDATE`, `UPDATE ... ORDER BY ... LIMIT`, `DELETE`, and
  `DELETE ... ORDER BY ... LIMIT`;
- warning counts and deprecated `&&` / `||` warnings when combined with
  `BETWEEN`;
- unknown predicate column and out-of-range bound diagnostics;
- unsupported syntax: expression bounds, column bounds, table-qualified DML
  predicate columns, literal-left ranges, `NULL` bounds, string/decimal/float/
  hex/bit bounds, parameters, subqueries, functions, row constructors, and
  symbolic `!`;
- persistence, preamble preservation, independent file-backed handles, and
  cleanup of zero-initialized predicate structures.

Verification commands:

```sh
cmake --build --preset dev
ctest --preset dev --output-on-failure -R 'libmylite\.(parser|runtime\.where_and_predicates|runtime\.select_where_lifecycle|runtime\.select_order_limit_lifecycle|runtime\.delete_lifecycle|runtime\.update_lifecycle|runtime\.row_values_lifecycle|runtime\.table_rename_lifecycle)'
./packages/libmylite/tests/mysql_baseline_where_between_predicates_expectations.sh
cmake --workflow --preset check
```

## Compatibility Documentation

Update only the exact supported subset:

- `COMPATIBILITY.md`;
- `docs/compatibility/operators.md`;
- `docs/compatibility/sql-query-expressions.md`;
- `docs/compatibility/sql-table-dml.md` if wording needs to mention the widened
  shared `WHERE` predicate subset;
- `docs/compatibility/type-system-literals-conversion.md` only if the
  documented predicate literal surface changes beyond the existing
  integer/boolean predicate right-operand wording.

Do not claim full `BETWEEN`, full `NOT BETWEEN`, arbitrary expression operands,
string ranges, temporal ranges, row ranges, expression conversion, collation
semantics, index/range optimization, or general expression metadata.
