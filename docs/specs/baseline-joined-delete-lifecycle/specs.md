# Baseline Joined DELETE Lifecycle

## Status

Design for the first descriptor-driven joined `DELETE` slice. The
implementation must stay independently authored and must not copy MySQL or
SQLite implementation text.

## References

- MySQL 8.4 Reference Manual, `DELETE` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/delete.html>
- MySQL 8.4 Reference Manual, join syntax through the `SELECT` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/select.html>
- Existing MyLite specs:
  - `docs/specs/baseline-delete-lifecycle/specs.md`
  - `docs/specs/baseline-inner-join-select/specs.md`
  - `docs/specs/baseline-left-join-select/specs.md`

## MySQL 8.4.9 Runtime Observations

Observed against the local MySQL 8.4.9 runtime and captured by the expectation
script added for this feature:

- `DELETE t FROM t JOIN u ON t.k = u.k` deletes matching rows from `t` and
  reports each deleted target row once even when the join has duplicate matches.
- `DELETE FROM t USING t JOIN u ON t.k = u.k WHERE u.v > 900` is accepted.
- Target aliases are accepted when declared in `table_references`, for example
  `DELETE a FROM t AS a JOIN u AS b ON a.k = b.k`.
- `DELETE a, b FROM t AS a, t AS b WHERE ...` is accepted when all delete
  targets resolve to aliases of the same physical table; each physical row is
  deleted once even if more than one target alias or join match selects it.
- When a source alias is declared, the delete target must use that alias.
  `DELETE t FROM t AS a JOIN u AS b ...` reports `1109 / 42S02` with
  `Unknown table 't' in MULTI DELETE`.
- An unqualified delete target, including an alias target, requires a selected
  default schema even when the source tables are schema-qualified. A
  schema-qualified target can run without a selected default schema.
- Unknown source tables report `1146 / 42S02`.
- Unknown `ON` columns report `1054 / 42S22` in the `on clause`.
- Ambiguous unqualified `WHERE` columns in joined sources report
  `1052 / 23000`.
- MySQL rejects `ORDER BY` and `LIMIT` in multiple-table `DELETE` syntax with
  `1064 / 42000`.
- WordPress-style same-table transient cleanup can compare a `LONGTEXT`
  timeout value to an integer literal, for example `b.option_value < 100`, and
  MySQL coerces the string operand numerically for that direct comparison.

## Feature Scope

MyLite supports the smallest coherent joined delete path:

- persistent base tables and shadowing session temporary base tables already
  admitted by readable table resolution;
- exactly one delete target, or multiple delete targets when every target
  resolves to an alias of the same physical table;
- first multiple-table syntax:
  `DELETE target[, target] FROM table_source join_operator table_source [ON equality] [WHERE predicate]`;
- first syntax with two comma table sources:
  `DELETE target[, target] FROM table_source, table_source [WHERE predicate]`;
- second multiple-table syntax:
  `DELETE FROM target USING table_source join_operator table_source [ON equality] [WHERE predicate]`;
- target may be the unqualified source table name, schema-qualified source table
  name, or declared source alias according to MySQL target resolution rules;
- the existing two-source join envelope: `JOIN`, `INNER JOIN`, `CROSS JOIN`,
  `LEFT JOIN`, and `LEFT OUTER JOIN`;
- the existing joined `ON` equality subset;
- the existing joined `WHERE` predicate subset, plus row-scalar RHS expressions
  admitted by the current row-scalar planner for string comparisons and
  signed integer RHS literals for direct string-family comparisons;
- exact affected-row counts for target rows deleted once;
- warning count `0` for supported in-range deletes;
- result shape follows existing non-query statement result conventions.

The feature deliberately excludes:

- deleting from more than one physical target table;
- `target.*` compatibility spelling;
- more than two comma table references, nested joins, derived tables,
  subqueries, CTEs, and parenthesized table references;
- `ORDER BY` and `LIMIT` on joined deletes;
- `LOW_PRIORITY`, `QUICK`, `IGNORE`, `PARTITION`, and optimizer hints beyond
  the existing no-op/index-hint validation on admitted table sources;
- table privileges and privilege metadata;
- trigger behavior;
- broader recursive foreign-key action behavior than the current direct
  descriptor-backed action subset.

## Grammar

Independently authored MyLite Lemon-syntax target:

```lemon
delete_statement ::=
    DELETE FROM table_name where_clause_opt order_clause_opt delete_limit_clause_opt.

joined_delete_statement ::=
    DELETE delete_target_list FROM table_source join_operator table_source
    join_condition_opt where_clause_opt.

joined_delete_statement ::=
    DELETE delete_target_list FROM table_source COMMA table_source
    where_clause_opt.

joined_delete_statement ::=
    DELETE FROM delete_target USING table_source join_operator table_source
    join_condition_opt where_clause_opt.

delete_target_list ::= delete_target.
delete_target_list ::= delete_target_list COMMA delete_target.
delete_target ::= table_name.
```

`DELETE target.* ...`, multi-physical-table target lists, more-than-two-source
comma references, `ORDER BY`, and `LIMIT` in `joined_delete_statement` are
intentionally outside the grammar for this slice.

## Architecture Boundaries

- Public API: no ABI or public header changes. Joined deletes enter through
  `mylite_execute()` and return the existing non-row result object.
- Statement context: no new context state. SQL mode, default schema, and
  diagnostics are read through existing connection/session state.
- Parser/AST: add a joined-delete AST statement so planner code can distinguish
  single-table and joined delete shapes without reinterpreting child positions.
- Analyzer/planner: resolve joined table sources and columns from MyLite
  descriptors, not SQLite schema text. Reuse existing joined `SELECT` source,
  alias, duplicate-reference, `ON`, and `WHERE` planning helpers.
- Catalog: descriptors remain authoritative. The feature does not mutate table
  descriptors, catalog generation, descriptor versions, or
  `sqlite_schema_generation`.
- Result builder: successful joined deletes report affected rows and zero
  warnings with no row result set.
- Storage/VFS: no `.mylite` preamble or VFS changes. Physical SQLite payload is
  updated through normal prepared SQLite statements.
- SQLite physical storage: MyLite generates standard SQLite using physical table
  names. It does not rely on SQLite supporting MySQL joined delete syntax.

## Name Resolution

Table sources use the existing selected/default schema policy:

- unqualified source tables require a selected schema;
- schema-qualified source tables resolve that schema explicitly;
- `information_schema` write targets are rejected with the existing access
  denied diagnostic before joined sources are planned;
- reserved `_mylite_*` source/target names are rejected before generated SQL is
  produced;
- unknown schemas and unknown source tables use existing MySQL-compatible
  diagnostics.

Delete-target resolution is separate from source resolution:

- a one-part target first requires a selected default schema, matching MySQL's
  observed behavior for unqualified joined-delete targets;
- if a source has an alias, a one-part target must match the alias;
- if a source has no alias, a one-part target must match the source table name
  and selected schema;
- a two-part target must match an unaliased source by schema and table name;
- a target that does not match a joined source reports
  `1109 / 42S02`, `Unknown table '<target>' in MULTI DELETE`;
- duplicate joined source references retain the existing
  `1066 / 42000` not-unique alias diagnostic.

Column resolution for `ON` and `WHERE` stays descriptor-driven:

- `ON` supports the existing same-family descriptor equality subset;
- `WHERE` supports the current joined predicate subset and diagnostics;
- unknown and ambiguous columns use the existing `on clause` and
  `where clause` diagnostics.

Current descriptor catalog matching remains ASCII case-insensitive for schema,
table, alias, and column names.

## Semantics

The joined delete selects target rows from the joined source expression and
deletes matching target rows. Duplicate join matches do not multiply
`affected_rows`; the count is the number of target rows physically deleted.

`LEFT JOIN` follows the same row-preservation and `NULL` extension behavior as
the existing joined `SELECT` slice. For example, `DELETE t FROM t LEFT JOIN u
ON t.k = u.k WHERE u.id IS NULL` deletes unmatched `t` rows.

`ORDER BY` and `LIMIT` are not admitted because MySQL 8.4.9 rejects those
clauses in multiple-table `DELETE` syntax. Single-table `DELETE ... ORDER BY
... LIMIT ...` remains covered by the previous slice.

## Physical SQLite Handling

MyLite lowers joined delete to a rowid selector:

```sql
DELETE FROM "<target_physical>"
WHERE rowid IN (
    SELECT <target_source_alias>.rowid
    FROM "<left_physical>" AS "_mylite_s0"
    JOIN "<right_physical>" AS "_mylite_s1" ON ...
    WHERE ...
)
```

For `LEFT JOIN`, the generated join uses `LEFT JOIN`; for inner/cross/no-`ON`
forms, the existing inner join generation is used. All physical table names and
generated aliases are quoted. Literal values from predicates are bound as
parameters through the existing predicate binder.

The selector requires an unshadowed SQLite rowid alias for the target table.
If the target descriptor has columns named `rowid`, `_rowid_`, and `oid`,
MyLite reports `1064 / 42000` with
`joined DELETE requires an unshadowed SQLite rowid alias`. This is an internal
physical-table invariant and does not expose SQLite schema metadata as
authority.

Foreign-key parent-side actions reuse the same target-row selector before the
target rows are deleted. The direct non-recursive `ON DELETE CASCADE` and
`ON DELETE SET NULL` behavior from the single-table delete slice is preserved
for joined deletes, and parent affected-row counts are not changed by child
actions.

## Diagnostics

| Case | Diagnostic |
| --- | --- |
| Syntax outside admitted grammar | `1064 / 42000` parse error |
| `information_schema` write target | existing `1044 / 42000` access denied diagnostic |
| No selected schema for unqualified target/source | `1046 / 3D000` |
| Unknown schema | existing `1049 / 42000` diagnostic |
| Unknown source table | existing `1146 / 42S02` diagnostic |
| Reserved schema/table/target name | existing reserved-name diagnostic |
| Target not present in joined sources | `1109 / 42S02`, `Unknown table '<target>' in MULTI DELETE` |
| Duplicate source alias/reference | existing `1066 / 42000` diagnostic |
| Unsupported source kind or join condition | existing joined `SELECT` unsupported diagnostics |
| Unknown `ON` column | existing `1054 / 42S22` `on clause` diagnostic |
| Unknown `WHERE` column | existing `1054 / 42S22` `where clause` diagnostic |
| Ambiguous `WHERE` column | existing `1052 / 23000` diagnostic |
| Rowid aliases all shadowed | `1064 / 42000`, `joined DELETE requires an unshadowed SQLite rowid alias` |
| SQLite prepare/step failure | existing physical SQLite row error |
| Allocation failure | `MYLITE_NOMEM` with OOM diagnostic |

## Tests

Add a fast C runtime lifecycle test and MySQL 8.4.9 expectation script covering:

- `DELETE target FROM ... JOIN ...` and `DELETE FROM target USING ...`;
- duplicate join matches counting target rows once;
- alias target success and table-name target failure when aliases are declared;
- schema-qualified target/source resolution without a selected schema;
- explicit and selected-schema `information_schema` target access denial;
- missing default schema for unqualified targets;
- unknown target, source table, `ON` column, and `WHERE` column diagnostics;
- ambiguous joined `WHERE` columns;
- inner, cross/no-`ON`, and left-join unmatched deletes;
- same-physical-table multi-target comma-source deletion for the WordPress
  transient cleanup shape;
- persistence after close/reopen and table rename;
- direct FK cascade/set-null preservation for joined-delete target rows;
- rowid shadow diagnostic;
- file preamble preservation and independent file-backed handles;
- parser coverage for both admitted joined-delete syntax forms and rejected
  `ORDER BY` / `LIMIT` joined-delete forms.

## Compatibility Documentation

Update `COMPATIBILITY.md` and `docs/compatibility/sql-table-dml.md` to mark
multi-table `DELETE` as limited to one target, or multiple aliases of the same
physical table, over the current two-source joined-source envelope. Do not
claim full multi-physical-table target deletes, `target.*`, partitioned
deletes, modifiers, arbitrary joins, joined-delete ordering, joined-delete
limits, triggers, privileges, or recursive cascades.
