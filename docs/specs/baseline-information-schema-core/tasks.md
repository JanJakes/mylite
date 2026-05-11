# Baseline INFORMATION_SCHEMA core tasks

1. Research and expectations
   - [x] Verify MySQL 8.4.9 `SCHEMATA`, `TABLES`, and `COLUMNS` column shapes.
   - [x] Verify stable metadata values for current MyLite-supported table
     features.
   - [x] Add MySQL-runtime expectation script.

2. Parser and AST
   - [x] Admit unquoted `TABLES` as an identifier where needed.
   - [x] Admit string-literal and current-database function metadata predicate
     values.
   - [x] Add parser tests for supported and deliberately unsupported
     `INFORMATION_SCHEMA` query shapes.

3. Runtime planning and execution
   - [x] Detect schema-qualified `INFORMATION_SCHEMA.SCHEMATA`,
     `INFORMATION_SCHEMA.TABLES`, and `INFORMATION_SCHEMA.COLUMNS` sources
     before normal descriptor table resolution.
   - [x] Resolve projection, predicate, and ordering columns against
     `INFORMATION_SCHEMA` metadata definitions.
   - [x] Build synthetic rows from MyLite descriptors and fixed system-view
     metadata.
   - [x] Implement supported filtering, ordering, limiting, and `COUNT(*)`.
   - [x] Preserve normal descriptor-backed user table execution paths.

4. Metadata mapping
   - [x] Map MyLite schemas to `SCHEMATA` rows.
   - [x] Map MyLite base-table descriptors to `TABLES` rows.
   - [x] Map MyLite column descriptors, primary-key descriptors, visibility,
     defaults, and auto-increment attributes to `COLUMNS` rows.
   - [x] Add synthetic rows for the supported `information_schema` system
     views.

5. Tests and docs
   - [x] Add `runtime_information_schema_core` C tests and register them in
     CMake with a dotted CTest name.
   - [x] Update `COMPATIBILITY.md`.
   - [x] Update `docs/compatibility/metadata-information-schema.md`.
   - [x] Update related detail docs only for this exact limited surface.

6. Verification and review
   - [x] `cmake --build --preset dev`
   - [x] New targeted CTest entry.
   - [x] `./packages/libmylite/tests/mysql_baseline_information_schema_core_expectations.sh`
   - [x] Existing parser/runtime lifecycle entries affected by predicate
     grammar widening.
   - [x] `cmake --workflow --preset check`
   - [x] Subagent review, fix findings, amend implementation commit, and push
     `main`.
