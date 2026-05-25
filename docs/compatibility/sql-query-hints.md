# SQL query hints

Index and optimizer hint syntax, scope, acceptance, and diagnostics compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| Index hints | 🟡 | Limited `SELECT` table-source and single-table `UPDATE` target `USE`/`FORCE`/`IGNORE INDEX|KEY` acceptance and descriptor-name validation as planner no-ops; supports `FOR JOIN`, `FOR ORDER BY`, `FOR GROUP BY`, `PRIMARY`, unambiguous index-name prefixes, duplicate names, and `USE INDEX ()`, while ambiguous prefixes use MySQL's unknown-key diagnostic shape; no optimizer behavior, `DELETE` hints, partitions, or new-style optimizer hints. |
| Optimizer hints | 🟡 | Limited valid `/*+ ... */` optimizer-hint comments are accepted as comments/no-ops for currently supported `SELECT`, `INSERT`, `REPLACE`, `UPDATE`, and `DELETE` statements, preserving result rows, affected rows, and zero warning counts for the verified valid-hint subset; hint-shaped comments outside recognized hint positions are ordinary comments. No hint payload parsing, optimizer behavior, `SET_VAR` effects, invalid-hint warnings, duplicate/conflicting hint diagnostics, table/index/function hint resolution, or statement support beyond the currently implemented SQL surface. See [baseline optimizer hints no-op](../specs/baseline-optimizer-hints-noop/specs.md). |

[Back to compatibility overview](../../COMPATIBILITY.md)
