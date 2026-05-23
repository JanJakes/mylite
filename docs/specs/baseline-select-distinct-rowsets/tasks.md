# Baseline SELECT DISTINCT Rowsets Tasks

- [x] Read project architecture, current descriptor `SELECT`, ordering,
  limiting, alias, row-value, and compatibility docs.
- [x] Verify MySQL 8.4.9 `DISTINCT` / `DISTINCTROW` rowset behavior for
  duplicate rows, `NULL`, ASCII string collation, aliases, wildcard projection,
  `ORDER BY`, `LIMIT`, and deferred broader forms.
- [x] Specify the independent MyLite grammar/runtime subset and architecture
  ownership boundary.
- [x] Add MySQL-runtime expectation script for this phase.
- [x] Update compatibility docs for the planned limited rowset distinct scope.
- [x] Replace the one-column integer-only distinct planner with the specified
  descriptor rowset planner validation.
- [x] Generate `SELECT DISTINCT` projection SQL with MyLite string collation for
  supported nonbinary string descriptor columns.
- [x] Add focused C runtime coverage and update old rejection tests.
- [x] Run the MySQL expectation script, focused CTest entries, and full
  workflow.
- [x] Review the final diff for scope, descriptor authority, SQLite SQL shape,
  collation correctness, memory behavior, docs, and tests.
