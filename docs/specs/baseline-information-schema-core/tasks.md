# Baseline INFORMATION_SCHEMA core tasks

1. Research and expectations
   - [x] Verify MySQL 8.4.9 `SCHEMATA`, `TABLES`, and `COLUMNS` column shapes.
   - [x] Verify stable metadata values for current MyLite-supported table
     features.
   - [x] Add MySQL-runtime expectation script.

2. Parser and AST
   - [ ] Admit unquoted `TABLES` as an identifier where needed.
   - [ ] Admit string-literal and current-database function metadata predicate
     values.
   - [ ] Add parser tests for supported and deliberately unsupported
     `INFORMATION_SCHEMA` query shapes.

3. Runtime planning and execution
   - [ ] Detect schema-qualified `INFORMATION_SCHEMA.SCHEMATA`,
     `INFORMATION_SCHEMA.TABLES`, and `INFORMATION_SCHEMA.COLUMNS` sources
     before normal descriptor table resolution.
   - [ ] Resolve projection, predicate, and ordering columns against
     `INFORMATION_SCHEMA` metadata definitions.
   - [ ] Build synthetic rows from MyLite descriptors and fixed system-view
     metadata.
   - [ ] Implement supported filtering, ordering, limiting, and `COUNT(*)`.
   - [ ] Preserve normal descriptor-backed user table execution paths.

4. Metadata mapping
   - [ ] Map MyLite schemas to `SCHEMATA` rows.
   - [ ] Map MyLite base-table descriptors to `TABLES` rows.
   - [ ] Map MyLite column descriptors, primary-key descriptors, visibility,
     defaults, and auto-increment attributes to `COLUMNS` rows.
   - [ ] Add synthetic rows for the supported `information_schema` system
     views.

5. Tests and docs
   - [ ] Add `runtime_information_schema_core` C tests and register them in
     CMake with a dotted CTest name.
   - [ ] Update `COMPATIBILITY.md`.
   - [ ] Update `docs/compatibility/metadata-information-schema.md`.
   - [ ] Update related detail docs only for this exact limited surface.

6. Verification and review
   - [ ] `cmake --build --preset dev`
   - [ ] New targeted CTest entry.
   - [ ] `./packages/libmylite/tests/mysql_baseline_information_schema_core_expectations.sh`
   - [ ] Existing parser/runtime lifecycle entries affected by predicate
     grammar widening.
   - [ ] `cmake --workflow --preset check`
   - [ ] Subagent review, fix findings, amend implementation commit, and push
     `main`.
