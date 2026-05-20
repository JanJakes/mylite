# SQL query hints

Index and optimizer hint syntax, scope, acceptance, and diagnostics compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| Index hints | 🟡 | Limited `SELECT` table-source and single-table `UPDATE` target `USE`/`FORCE`/`IGNORE INDEX|KEY` acceptance and descriptor-name validation as planner no-ops; supports `FOR JOIN`, `FOR ORDER BY`, `FOR GROUP BY`, `PRIMARY`, unambiguous index-name prefixes, duplicate names, and `USE INDEX ()`, while ambiguous prefixes use MySQL's unknown-key diagnostic shape; no optimizer behavior, `DELETE` hints, partitions, or new-style optimizer hints. |
| Optimizer hints | ❌ | Comment-hint grammar and ignored/accepted hint diagnostics |

[Back to compatibility overview](../../COMPATIBILITY.md)
