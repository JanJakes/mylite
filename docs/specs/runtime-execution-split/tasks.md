# Runtime execution split tasks

- [x] Identify low-risk logical split boundaries in `mylite_execution.c`.
- [x] Specify the behavior-preserving split and module ownership.
- [x] Extract dynamic string helper into its own internal module.
- [x] Extract immutable execution catalog metadata into its own internal module.
- [x] Replace direct references to moved static arrays with accessors.
- [x] Register new runtime sources in CMake.
- [x] Run focused build and runtime tests.
- [x] Run `git diff --check`, staged diff check, and full check workflow.
- [x] Review the split for architecture, behavior preservation, and maintenance
      risk.
- [x] Fix review findings.
- [x] Commit and push the refactor.

## Round 2

- [x] Reinspect split candidates after the first metadata extraction.
- [x] Extract system variable descriptors and SHOW STATUS descriptors into
      `mylite_execution_system_variables`.
- [x] Extract SQL mode descriptor parsing and canonical formatting helpers.
- [x] Extract pure system variable scope, mutability, and read-only
      classification helpers.
- [x] Register the new runtime source in CMake.
- [x] Run focused build and runtime tests.
- [x] Run `git diff --check`, staged diff check, and full check workflow.
- [x] Review the second split for behavior preservation and module boundaries.
- [x] Fix review findings.
- [x] Commit and push the second split.

## Round 3

- [x] Reinspect large boundaries after the system-variable extraction.
- [x] Specify a larger same-translation-unit implementation-fragment split.
- [x] Split the remaining execution implementation into coarse logical
      fragments.
- [x] Preserve the existing private helper surface without adding a broad
      exported bridge.
- [x] Run focused build and runtime tests.
- [x] Run `git diff --check`, staged diff check, and full check workflow.
- [x] Review the third split for behavior preservation, fragment boundaries,
      and readiness for later true-module extraction.
- [x] Fix review findings.
- [x] Commit and push the third split.

## Round 4

- [x] Reinspect true-module candidates after the fragment split.
- [x] Select immutable execution catalog data as the next narrow dependency
      boundary.
- [x] Split character set, collation, and scalar collation metadata into
      `mylite_execution_catalog_charsets`.
- [x] Split `INFORMATION_SCHEMA` keywords, table definitions, and column
      definitions into `mylite_execution_catalog_information_schema`.
- [x] Split built-in schema descriptors, table directories, and placeholder
      rows into `mylite_execution_catalog_builtin`.
- [x] Keep `mysql` and `sys` system table metadata in a focused catalog system
      tables module until their combined accessor surface is redesigned.
- [x] Register the catalog-family sources in CMake.
- [x] Run focused build and runtime tests for catalog, SHOW, and metadata
      surfaces.
- [x] Review the split for behavior preservation, module ownership, and
      maintenance risk.
- [x] Fix review findings.
- [x] Run `git diff --check`, staged diff check, and full check workflow.
- [x] Commit and push the fourth split.

## Round 5

- [x] Reinspect true-module candidates after the parser test split.
- [x] Select JSON session-scalar functions as a narrow first scalar extraction.
- [x] Add an internal scalar execution header for scalar cells, JSON mutation
      kinds, JSON scalar entry points, and helper wrappers.
- [x] Move JSON session-scalar function implementations into
      `mylite_execution_scalar_json`.
- [x] Keep non-JSON scalar dispatch and row-scalar planning in the existing
      scalar fragment.
- [x] Register the JSON scalar source in CMake.
- [x] Run focused build and JSON/runtime tests.
- [x] Review the split for helper-surface width, behavior preservation, and
      compile/tidy impact.
- [x] Fix review findings.
- [x] Run `git diff --check`, staged diff check, and full check workflow.
- [x] Commit and push the fifth split.

## Round 6

- [x] Reinspect the remaining oversized scalar fragment after JSON extraction.
- [x] Select coarse scalar-family fragments and misplaced planning tail
      fragments as the next behavior-preserving split.
- [x] Split basic string, temporal, extended string, misc, numeric, conversion,
      temporal-format, expression-evaluator, control-flow, scalar-projection,
      DELETE planning, and column-planning code into named fragments.
- [x] Preserve same-translation-unit static linkage and original function
      ordering.
- [x] Run focused build and runtime tests.
- [x] Review the split for missing includes, ordering drift, behavior changes,
      and fragment naming.
- [x] Fix review findings.
- [x] Run `git diff --check`, staged diff check, and full check workflow.
- [x] Commit and push the sixth split.

## Round 7

- [x] Reinspect the remaining oversized query-planning fragment after the
      scalar split.
- [x] Select coarse row-scalar and SELECT/UPDATE planning fragments as the next
      behavior-preserving split.
- [x] Split row-scalar string, JSON, value, temporal, and misc planning into
      named fragments.
- [x] Split SELECT column, predicate, order/limit, and UPDATE helper planning
      into named fragments.
- [x] Preserve same-translation-unit static linkage and original function
      ordering.
- [x] Run focused build and runtime tests.
- [x] Review the split for missing includes, ordering drift, behavior changes,
      and fragment naming.
- [x] Fix review findings.
- [x] Run `git diff --check`, staged diff check, and full check workflow.
- [x] Commit and push the seventh split.

## Round 8

- [x] Reinspect the remaining oversized metadata-query fragment after the query
      planning split.
- [x] Select coarse metadata execution, mysql/sys, INFORMATION_SCHEMA, table
      maintenance, SHOW, and result-completion fragments.
- [x] Split mysql/sys virtual rows, INFORMATION_SCHEMA rows, INFORMATION_SCHEMA
      filtering, table maintenance, general SHOW, SHOW columns/indexes,
      SHOW CREATE, and result-completion helpers into named fragments.
- [x] Preserve same-translation-unit static linkage and original function
      ordering.
- [x] Run focused build and runtime tests.
- [x] Review the split for missing includes, ordering drift, behavior changes,
      and fragment naming.
- [x] Fix review findings.
- [x] Run `git diff --check`, staged diff check, and full check workflow.
- [x] Commit and push the eighth split.

## Round 9

- [x] Reinspect the remaining oversized DML planning fragment after the
      metadata-query split.
- [x] Select coarse INSERT, INSERT SELECT, UPDATE, SELECT, aggregate, VALUES,
      scalar projection, and warning-helper fragments.
- [x] Split INSERT execution, INSERT SELECT, UPDATE planning/execution, SELECT
      planning/execution, aggregate planning/execution, scalar projection
      queries, and related warning helpers into named fragments.
- [x] Preserve same-translation-unit static linkage and original function
      ordering.
- [x] Run focused build and runtime tests.
- [x] Review the split for missing includes, ordering drift, behavior changes,
      and fragment naming.
- [x] Fix review findings.
- [x] Run `git diff --check`, staged diff check, and full check workflow.
- [x] Commit and push the ninth split.

## Round 10

- [x] Reinspect the remaining oversized DDL planning fragment after the DML
      planning split.
- [x] Select coarse CREATE TABLE, table option, schema/table admin, ALTER
      TABLE, and LOAD DATA planning fragments.
- [x] Split CREATE TABLE constraints/options/execution, schema/table admin,
      ALTER TABLE add-column, add-index, foreign-key/index, check-constraint,
      drop/rename-column, modify/options, and LOAD DATA helpers into named
      fragments.
- [x] Preserve same-translation-unit static linkage and original function
      ordering.
- [x] Run focused build and runtime tests.
- [x] Review the split for missing includes, ordering drift, behavior changes,
      and fragment naming.
- [x] Fix review findings.
- [x] Run `git diff --check`, staged diff check, and full check workflow.
- [x] Commit and push the tenth split.

## Round 11

- [x] Reinspect the remaining oversized SQL builder fragment after the DDL
      planning split.
- [x] Select coarse DDL/admin, INSERT, SELECT, row-scalar, aggregate/predicate,
      DML, SQLite write-helper, parameter-binding, and result-extraction
      fragments.
- [x] Split SQL rendering, write helpers, parameter binding, selected-row
      extraction, and parser helper tails into named fragments.
- [x] Preserve same-translation-unit static linkage and original function
      ordering.
- [x] Run focused build and runtime tests.
- [x] Review the split for missing includes, ordering drift, behavior changes,
      and fragment naming.
- [x] Fix review findings.
- [x] Run `git diff --check`, staged diff check, and full check workflow.
- [x] Commit and push the eleventh split.

## Round 12

- [x] Reinspect the remaining catalog system-table monolith after the SQL
      builder split.
- [x] Select immutable sys built-in view definitions as the next true module
      boundary.
- [x] Move sys view SQL definitions, SHOW CREATE text, and lookup/count
      accessors into `mylite_execution_catalog_sys_views`.
- [x] Keep the unified mysql/sys system-table descriptor array in
      `mylite_execution_catalog_system_tables`.
- [x] Register the new runtime source in CMake.
- [x] Run focused build and catalog/sys metadata tests.
- [x] Review the split for behavior preservation, order, and module ownership.
- [x] Fix review findings.
- [x] Run `git diff --check`, staged diff check, and full check workflow.
- [x] Commit and push the twelfth split.

## Round 13

- [x] Reinspect the remaining system-table descriptor monolith after the sys
      view split.
- [x] Select mysql/sys system-table descriptor families as the next true module
      boundary.
- [x] Add a private internal provider header for system-table descriptor
      owners.
- [x] Move mysql auth, service, replication, and remaining mysql descriptor
      families into owner modules.
- [x] Move sys core, summary, and schema descriptor families into owner
      modules.
- [x] Replace the combined descriptor array with a public-order-preserving
      provider aggregator.
- [x] Register the new runtime sources in CMake.
- [x] Run focused build and catalog/sys/mysql metadata tests.
- [x] Review the split for behavior preservation, descriptor order, and module
      ownership.
- [x] Fix review findings.
- [x] Run `git diff --check`, staged diff check, and full check workflow.
- [x] Commit and push the thirteenth split.

## Round 14

- [x] Reinspect the remaining largest runtime/catalog monoliths after the
      system-table descriptor split.
- [x] Select catalog schema migrations as the next true module boundary.
- [x] Add a private catalog internal header for migration entry points.
- [x] Move catalog schema one-step migrations into
      `mylite_catalog_migrations`.
- [x] Register the new runtime source in CMake.
- [x] Run focused build and catalog migration/runtime tests.
- [x] Review the split for behavior preservation, helper ownership, and
      catalog schema compatibility.
- [x] Fix review findings.
- [x] Run `git diff --check`, staged diff check, and full check workflow.
- [x] Commit and push the fourteenth split.

## Round 15

- [x] Reinspect the mutable catalog helper clusters after the migration split.
- [x] Select shared SQLite statement helpers as the next true module boundary.
- [x] Move catalog SQLite execution, prepare/bind/finalize, changed-row,
      checked-column, and integer range helpers into `mylite_catalog_sqlite`.
- [x] Expose only the private helper surface through
      `mylite_catalog_internal.h`.
- [x] Register the new runtime source in CMake.
- [x] Run focused build and catalog helper/runtime tests.
- [x] Review the split for behavior preservation, helper ownership, and
      symbol-surface width.
- [x] Fix review findings.
- [x] Run `git diff --check`, staged diff check, and full check workflow.
- [x] Commit and push the fifteenth split.

## Round 16

- [x] Reinspect catalog validation helpers after the SQLite helper split.
- [x] Select reusable validation primitives as the next true module boundary.
- [x] Move database readiness, name, enum/domain, id/ordinal, generation,
      callback, and boolean extraction validators into
      `mylite_catalog_validation`.
- [x] Keep private column-default validation in `mylite_catalog.c` until its
      private value structs have a dedicated ownership boundary.
- [x] Register the new runtime source in CMake.
- [x] Run focused build and catalog validation/runtime tests.
- [x] Review the split for behavior preservation, private struct leakage, and
      helper-surface width.
- [x] Fix review findings.
- [x] Run `git diff --check`, staged diff check, and full check workflow.
- [x] Commit and push the sixteenth split.

## Round 17

- [x] Reinspect catalog state/bootstrap ownership after the validation split.
- [x] Select catalog lifecycle, schema bootstrap/load, descriptor cache reset,
      mutation transaction lifecycle, and generation transactions as the next
      true module boundary.
- [x] Move catalog state helpers into `mylite_catalog_state`.
- [x] Expose only the private generation-change transaction helpers through
      `mylite_catalog_internal.h`.
- [x] Register the new runtime source in CMake.
- [x] Run focused build and catalog state/runtime tests.
- [x] Review the split for behavior preservation, transaction ownership, and
      helper-surface width.
- [x] Fix review findings.
- [x] Run `git diff --check`, staged diff check, and full check workflow.
- [x] Commit and push the seventeenth split.

## Round 18

- [x] Reinspect immutable `INFORMATION_SCHEMA` catalog metadata after the
      catalog state split.
- [x] Select keyword rows and InnoDB metadata as the next true module
      boundaries.
- [x] Add a private information-schema catalog provider header.
- [x] Move keyword rows and accessors into
      `mylite_execution_catalog_information_schema_keywords`.
- [x] Move InnoDB column definitions and table definitions into
      `mylite_execution_catalog_information_schema_innodb`.
- [x] Preserve the existing public table-definition order through an
      aggregator provider list.
- [x] Register the new runtime sources in CMake.
- [x] Run focused build and information-schema catalog tests.
- [x] Review the split for behavior preservation, descriptor order, and
      helper-surface width.
- [x] Fix review findings.
- [x] Run `git diff --check`, staged diff check, and full check workflow.
- [x] Commit and push the eighteenth split.

## Round 19

- [x] Reinspect mutable catalog monolith split candidates after the
      information-schema metadata split.
- [x] Select catalog read, iteration, next-id, post-insert read, and
      descriptor materialization helpers as the next true module boundary.
- [x] Move catalog SELECT column indexes and descriptor materializers into
      `mylite_catalog_read`.
- [x] Move public catalog read/iteration APIs and private SQLite-level read
      helpers into `mylite_catalog_read`.
- [x] Keep mutation/write bind helpers and write-side catalog value validation
      in `mylite_catalog`.
- [x] Expose only the shared table-descriptor validator, default-kind helpers,
      and SQLite-level read helpers through `mylite_catalog_internal.h`.
- [x] Register the new runtime source in CMake.
- [x] Run focused build and catalog read/materialization tests.
- [x] Review the split for behavior preservation, helper-surface width, and
      function ownership.
- [x] Fix review findings.
- [x] Run `git diff --check`, staged diff check, and full check workflow.
- [x] Commit and push the nineteenth split.
