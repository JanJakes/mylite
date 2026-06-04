# Baseline Joined UPDATE Lifecycle

## Status

Implemented first descriptor-driven joined `UPDATE` slice. The feature is
intentionally narrower than MySQL's full multiple-table update grammar, but the
admitted behavior is verified against MySQL 8.4.9 and is implemented through
MyLite descriptors rather than SQLite schema text.

## References

- MySQL 8.4 Reference Manual, `UPDATE` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/update.html>
- MySQL 8.4 Reference Manual, join syntax through the `SELECT` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/join.html>
- Existing MyLite specs:
  - `docs/specs/baseline-update-lifecycle/specs.md`
  - `docs/specs/baseline-inner-join-select/specs.md`
  - `docs/specs/baseline-left-join-select/specs.md`
  - `docs/specs/baseline-joined-delete-lifecycle/specs.md`

## MySQL 8.4.9 Runtime Observations

Observed against the local MySQL 8.4.9 runtime and captured by the expectation
script added for this feature:

- `UPDATE t JOIN u ON t.k = u.k SET t.v = 7` updates each matching `t` row
  once, even when duplicate `u` rows match the same `t` row.
- Repeating the same assignment after values already match reports
  `ROW_COUNT() = 0` and `@@warning_count = 0`.
- `WHERE` may reference joined source columns. `LEFT JOIN` exposes `NULL`
  right-side columns for unmatched rows.
- If the resolved update target is the right side of a `LEFT JOIN`, unmatched
  `NULL`-extended right-side rows are not target rows; MySQL reports
  `ROW_COUNT() = 0` and no warnings, and does not reject an otherwise invalid
  assignment value for those nonexistent target rows.
- Inner `JOIN` without `ON` is accepted and behaves as a Cartesian joined
  source filtered by `WHERE`.
- Assignment targets may be unqualified when the column name resolves to one
  joined source, or qualified by source alias, table name, or schema/table name.
- When a source has an alias, assignment through the underlying table name is
  rejected as `1054 / 42S22` in the `field list`.
- Bare source aliases such as `UPDATE t a JOIN u b ...` resolve the same way
  as explicit `AS` aliases for table and assignment references.
- A descriptor-backed right-side derived table may be joined by alias and may
  read the same physical target table, including an ignored `FOR UPDATE`
  locking clause inside the derived `SELECT`.
- Multiple constant assignments to columns of the same resolved target table
  update matched rows once and report changed-row affected counts.
- Ambiguous unqualified assignment columns report `1052 / 23000` in the
  `field list`.
- Schema-qualified sources and alias-qualified assignments can run without a
  selected default schema.
- Unknown `ON`, assignment, and `WHERE` columns report the same `on clause`,
  `field list`, and `where clause` diagnostics as MySQL.
- MySQL rejects `ORDER BY` and `LIMIT` in multiple-table `UPDATE` syntax with
  `1221 / HY000`, `Incorrect usage of UPDATE and ORDER BY` or
  `Incorrect usage of UPDATE and LIMIT`.

## Feature Scope

MyLite supports the smallest coherent joined update path:

- persistent base tables and shadowing session temporary base tables already
  admitted by readable/writable table resolution;
- exactly two joined table sources;
- the existing two-source join envelope: `JOIN`, `INNER JOIN`, `CROSS JOIN`,
  `LEFT JOIN`, and `LEFT OUTER JOIN`;
- source aliases in explicit `AS alias` and bare `alias` forms;
- a right-side derived-table source with a required alias when the derived
  `SELECT` is descriptor-backed, projects direct descriptor columns without
  output aliases, and uses the existing single-source `SELECT` predicate,
  order, limit, and no-op row-locking envelope;
- the existing joined `ON` equality subset;
- the existing joined `WHERE` predicate subset;
- one or more constant assignments to one target source table, with
  multi-assignment lists limited to distinct non-key, non-`AUTO_INCREMENT`
  target columns;
- assignment target resolution through the joined source context, including
  unqualified, alias-qualified, table-qualified, and schema-table-qualified
  descriptor columns;
- assignment value conversion through the existing single-table update constant
  value subset for the target descriptor, including `NULL`, `DEFAULT`, integer,
  decimal, approximate, canonical temporal, supported string, binary string,
  `BIT`, `ENUM`, `SET`, and JSON literals where the current single-table update
  path admits them;
- automatic `ON UPDATE CURRENT_TIMESTAMP` descriptor columns for the updated
  target table when the user assignment changes a row;
- exact changed-row affected counts after descriptor conversion and no-op
  filtering;
- direct non-recursive parent-side `ON UPDATE CASCADE` and `ON UPDATE SET NULL`
  actions already supported by single-table update, using the joined target-row
  selector;
- warning count `0` for supported in-range joined updates;
- result shape follows existing non-query statement result conventions.

The feature deliberately excludes:

- updating more than one table in one statement;
- multi-assignment lists that touch more than one joined source, duplicate
  targets, key columns, or `AUTO_INCREMENT` columns;
- derived-table sources on the update-target side, derived output aliases,
  derived non-column projections, derived wildcard projections beyond the
  current direct descriptor-column envelope, and multiple derived sources;
- `LOW_PRIORITY`, `IGNORE`, `PARTITION`, CTEs, subqueries in predicates,
  nested joins, parenthesized table references, comma joins,
  natural/right joins, `USING` join conditions, optimizer hints beyond existing
  no-op/index-hint validation on admitted table sources, and arbitrary SQLite
  SQL pass-through;
- joined-update `ORDER BY` and `LIMIT`;
- expression assignments outside the current descriptor constant assignment
  subset, same-column arithmetic assignments, scalar subquery assignments,
  column-to-column assignments, functions as assignment values outside the
  current admitted default/current temporal forms, parameters, variables, and
  general expression evaluation;
- trigger behavior, privileges, generated columns, recursive foreign-key
  actions, or SQLite fork patches.

## Grammar

Independently authored MyLite Lemon-syntax target:

```lemon
joined_update_statement ::=
    UPDATE joined_update_table_source join_operator joined_update_table_source
    SET update_assignment_list where_clause_opt joined_update_order_clause_opt
    joined_update_limit_clause_opt.

joined_update_statement ::=
    UPDATE joined_update_table_source join_operator joined_update_table_source
    ON join_condition SET update_assignment_list where_clause_opt joined_update_order_clause_opt
    joined_update_limit_clause_opt.

joined_update_table_source ::=
    table_name joined_update_table_alias_opt table_index_hints_opt.

joined_update_table_alias_opt ::= .
joined_update_table_alias_opt ::= AS identifier.
joined_update_table_alias_opt ::= bare_identifier.

joined_update_table_source ::=
    LPAREN select_statement RPAREN joined_update_derived_alias.

joined_update_derived_alias ::= bare_identifier.
joined_update_derived_alias ::= AS identifier.

joined_update_order_clause_opt ::= .
joined_update_order_clause_opt ::= order_clause.

joined_update_limit_clause_opt ::= .
joined_update_limit_clause_opt ::= update_limit_clause.
```

`ORDER BY` and `LIMIT` are parsed only so the planner can return MySQL-shaped
`1221 / HY000` diagnostics for multiple-table update. The semantic subset
requires both clauses to be absent on the joined `UPDATE` itself. Derived-table
sources require an alias in the grammar, which avoids the `SET` keyword
ambiguity after the closing parenthesis and matches the MySQL requirement for
derived table aliases.

## Architecture Boundaries

- Public API: no ABI or public header changes. Joined updates enter through
  `mylite_execute()` and return the existing non-row result object.
- Statement context: no new context state. Diagnostics, warning count, affected
  rows, and transaction completion follow the existing update statement path.
- Parser/AST: add a joined-update AST statement so planner code can distinguish
  single-table and joined update shapes without reinterpreting child positions.
- Analyzer/planner: resolve joined sources, assignment target, `ON`, and
  `WHERE` columns from MyLite descriptors. Assignment conversion remains
  MyLite-owned and happens before SQLite binding.
- Catalog: descriptors remain authoritative. Joined update does not mutate
  table descriptors, descriptor versions, catalog generation, descriptor
  caches, or `sqlite_schema_generation`.
- Result builder: successful joined updates report changed-row affected rows
  and zero warnings with no row result set.
- Storage/VFS: no `.mylite` preamble or VFS changes. Physical SQLite payload is
  updated through normal prepared SQLite statements.
- SQLite physical storage: MyLite generates standard SQLite over stable
  physical table names and rowid selectors. SQLite does not parse MySQL joined
  update syntax.

## Name and Column Resolution

Table sources use the existing selected/default schema policy:

- unqualified source tables require a selected schema;
- schema-qualified source tables resolve that schema explicitly;
- aliases are admitted as `AS alias` or bare `alias` and hide the underlying
  table name for qualified column references, matching the verified MySQL
  behavior for aliased sources;
- derived sources expose the direct descriptor columns projected by their
  nested `SELECT` through the required derived-table alias; the current slice
  rejects derived output aliases because MyLite's derived SQL renderer does not
  yet rewrite projected output names;
- `information_schema` write targets are rejected once the assignment target
  resolves to a source table in that schema;
- reserved `_mylite_*` source names are rejected before generated SQL is
  produced;
- unknown schemas and unknown source tables use existing diagnostics.

The assignment target determines the table being updated:

- a one-part assignment target resolves through the joined source context and
  must match exactly one descriptor column;
- a two-part target resolves to either a source alias or an unaliased source
  table name plus descriptor column;
- a three-part target resolves to an unaliased `schema.table.column` descriptor
  reference;
- unknown assignment columns report `1054 / 42S22` in the `field list`;
- ambiguous unqualified assignment columns report `1052 / 23000` in the
  `field list`;
- the resolved source table becomes the single update target; additional
  assignments in the same statement must resolve to that same source.

`ON` and `WHERE` resolution reuses the existing joined `SELECT` descriptors and
diagnostics. Current descriptor catalog matching remains ASCII
case-insensitive for schema, table, alias, and column names.

## Semantics

The joined update selects target rows from the joined source expression and
updates matching target rows. Duplicate join matches do not multiply
`affected_rows`; the count is the number of target rows whose stored values
changed.

`LEFT JOIN` follows the same row-preservation and `NULL` extension behavior as
the existing joined `SELECT` slice. For example,
`UPDATE t LEFT JOIN u ON t.k = u.k SET t.v = 8 WHERE u.id IS NULL` updates
unmatched `t` rows.

When the resolved target source is the nullable side of a `LEFT JOIN`, only rows
with a real target rowid are considered matched update rows. `NULL`-extended
placeholders do not trigger assignment conversion, foreign-key action planning,
or changed-row accounting.

`ORDER BY` and `LIMIT` are rejected for joined updates because MySQL 8.4.9
rejects those clauses in multiple-table `UPDATE` syntax. Single-table
`UPDATE ... ORDER BY ... LIMIT ...` remains covered by the previous slice.

## Physical SQLite Handling

MyLite lowers joined update to an update over the resolved target physical
table guarded by a rowid selector:

```sql
UPDATE "<target_physical>"
SET "<target_column>" = ?1[, ...]
WHERE rowid IN (
    SELECT <target_source_alias>.rowid
    FROM "<left_physical>" AS "_mylite_s0"
    JOIN "<right_physical>" AS "_mylite_s1" ON ...
    WHERE ...
)
AND ("<target_column>" IS NULL OR "<target_column>" <> ?N)
```

When the right source is a derived table, MyLite embeds a generated descriptor
`SELECT` in the rowid selector and continues numbering parameters from the
outer update assignment list:

```sql
FROM "<left_physical>" AS "_mylite_s0"
JOIN (
    SELECT "<column>" FROM "<source_physical>" WHERE ... ORDER BY ... LIMIT ...
) AS "_mylite_s1" ON ...
```

For `LEFT JOIN`, the generated join uses `LEFT JOIN`; for inner/cross/no-`ON`
forms, the existing inner join generation is used. Physical table names,
generated aliases, and physical column names are quoted. Assignment values,
automatic timestamp values, derived-source predicates, joined predicates, and
outer predicate literals are bound parameters in SQL order.

The selector requires an unshadowed SQLite rowid alias for the target table.
If the target descriptor has columns named `rowid`, `_rowid_`, and `oid`,
MyLite reports `1064 / 42000` with
`joined UPDATE requires an unshadowed SQLite rowid alias`. This is an internal
physical-table invariant and does not expose SQLite schema metadata as
authority.

Parent-side foreign-key actions reuse the same target-row selector before the
target rows are updated. Direct non-recursive `ON UPDATE CASCADE` and
`ON UPDATE SET NULL` behavior from the single-table update slice is preserved
for joined updates, and parent affected-row counts are not changed by child
actions.

## Diagnostics

| Case | Diagnostic |
| --- | --- |
| Syntax outside admitted grammar | `1064 / 42000` parse error |
| Joined `UPDATE ... ORDER BY ...` | `1221 / HY000`, `Incorrect usage of UPDATE and ORDER BY` |
| Joined `UPDATE ... LIMIT ...` | `1221 / HY000`, `Incorrect usage of UPDATE and LIMIT` |
| `information_schema` write target | existing `1044 / 42000` access denied diagnostic |
| No selected schema for unqualified source | `1046 / 3D000` |
| Unknown schema | existing `1049 / 42000` diagnostic |
| Unknown source table | existing `1146 / 42S02` diagnostic |
| Reserved schema/table/source name | existing reserved-name diagnostic |
| Duplicate source alias/reference | existing `1066 / 42000` diagnostic |
| Unknown `ON` column | existing `1054 / 42S22` `on clause` diagnostic |
| Unknown `WHERE` column | existing `1054 / 42S22` `where clause` diagnostic |
| Ambiguous `WHERE` column | existing `1052 / 23000` diagnostic |
| Unknown assignment column | `1054 / 42S22` `field list` diagnostic |
| Ambiguous assignment column | `1052 / 23000` `field list` diagnostic |
| Multiple assignment list spanning sources, keys, or duplicate targets | deterministic unsupported diagnostic |
| Unsupported assignment expression | deterministic unsupported diagnostic |
| Assignment conversion failure | existing single-table update diagnostic for the target descriptor |
| Rowid aliases all shadowed | `1064 / 42000`, `joined UPDATE requires an unshadowed SQLite rowid alias` |
| SQLite prepare/step failure | existing physical SQLite row error or mapped constraint diagnostic |
| Allocation failure | `MYLITE_NOMEM` with OOM diagnostic |

## Tests

Add a fast C runtime lifecycle test and MySQL 8.4.9 expectation script covering:

- basic joined update, duplicate join matches, no-op affected rows, and warning
  count;
- `WHERE` over right-source columns;
- inner join without `ON`;
- left join unmatched rows;
- explicit-alias, bare-alias, unqualified unique, right-source,
  schema-qualified, and no-default-schema update targets;
- right-side derived source with `ORDER BY`, `LIMIT`, no-op `FOR UPDATE`, and
  multiple constant assignments to the same target table;
- table rename, close/reopen persistence, `.mylite` preamble preservation, and
  independent file-backed handles;
- parent-side `ON UPDATE CASCADE` and `ON UPDATE SET NULL` preservation;
- unknown assignment, unknown `ON`, unknown `WHERE`, ambiguous assignment, and
  ambiguous `WHERE` diagnostics;
- duplicate source alias diagnostics and rowid-shadow diagnostics;
- `ORDER BY` and `LIMIT` wrong-usage diagnostics;
- parser coverage for admitted joined-update syntax and rejected/diagnosed
  unsupported variants.

## Compatibility Documentation

Update `COMPATIBILITY.md` and `docs/compatibility/sql-table-dml.md` to mark
multi-table / joined `UPDATE` as limited to one resolved physical target over
the current two-source joined-source envelope, including explicit and bare
aliases, the documented right-side derived-source slice, and multiple constant
assignments only when they target distinct non-key columns of that one table.
Do not claim full multi-table updates, multiple physical targets, modifiers,
partitioned updates, arbitrary joins, joined-update ordering or limiting,
triggers, privileges, recursive cascades, or general expression assignment.
