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

## Round 20

- [x] Reinspect mutable catalog write-side helper clusters after the catalog
      read/materialization split.
- [x] Select column descriptor value handling as the next true module boundary.
- [x] Move column insert/replace bind indexes and bind helpers into
      `mylite_catalog_column_values`.
- [x] Move column default-kind storage predicates and default/generated-column
      validation into `mylite_catalog_column_values`.
- [x] Expose only the column value struct, three column-value private entry
      points, and two shared default-kind predicates through
      `mylite_catalog_internal.h`.
- [x] Register the new runtime source in CMake.
- [x] Run focused build and catalog column descriptor tests.
- [x] Review the split for behavior preservation, helper-surface width, and
      function ownership.
- [x] Fix review findings.
- [x] Run `git diff --check`, staged diff check, and full check workflow.
- [x] Commit and push the twentieth split.

## Round 21

- [x] Reinspect the remaining sys summary catalog descriptor monolith.
- [x] Select host, InnoDB, IO/latest-file, memory, and instrumentation families
      as the next true module boundaries.
- [x] Add a private sys summary family provider header.
- [x] Move sys host summary descriptors and `x$` variants into
      `mylite_execution_catalog_sys_summary_host_tables`.
- [x] Move sys InnoDB summary descriptors and `x$` variants into
      `mylite_execution_catalog_sys_summary_innodb_tables`.
- [x] Move sys IO/latest-file descriptors and `x$` variants into
      `mylite_execution_catalog_sys_summary_io_tables`.
- [x] Move sys memory summary descriptors and `x$` variants into
      `mylite_execution_catalog_sys_summary_memory_tables`.
- [x] Move `ps_check_lost_instrumentation` descriptor ownership into
      `mylite_execution_catalog_sys_summary_instrumentation_tables`.
- [x] Preserve normal and `x$` sys summary public order through the existing
      sys summary aggregator entry points.
- [x] Register the new runtime sources in CMake.
- [x] Run focused build and sys summary catalog tests.
- [x] Review the split for behavior preservation, descriptor order, and
      helper-surface width.
- [x] Fix review findings.
- [x] Run `git diff --check`, staged diff check, and full check workflow.
- [x] Commit and push the twenty-first split.

## Round 22

- [x] Reinspect the remaining non-InnoDB information-schema catalog metadata
      monolith.
- [x] Select extension, metadata, routine, access/constraint, and view metadata
      families as the next true module boundaries.
- [x] Move extension/prefix descriptors into
      `mylite_execution_catalog_information_schema_extension_tables`.
- [x] Move first post-InnoDB metadata descriptors into
      `mylite_execution_catalog_information_schema_metadata_tables`.
- [x] Move routine and operational descriptors into
      `mylite_execution_catalog_information_schema_routine_tables`.
- [x] Move role, privilege, constraint, statistic, spatial, resource, and
      referential descriptors into
      `mylite_execution_catalog_information_schema_access_tables`.
- [x] Move trigger, user, view, and view-usage descriptors into
      `mylite_execution_catalog_information_schema_view_tables`.
- [x] Preserve public `INFORMATION_SCHEMA` table-definition order through the
      existing aggregator entry points.
- [x] Register the new runtime sources in CMake.
- [x] Run focused build and information-schema catalog tests.
- [x] Review the split for behavior preservation, descriptor order, and
      helper-surface width.
- [x] Fix review findings.
- [x] Run `git diff --check`, staged diff check, and full check workflow.
- [x] Commit and push the twenty-second split.

## Round 23

- [x] Reinspect mutable catalog key and constraint mutation ownership.
- [x] Select index, index-column, foreign-key, foreign-key-column, and
      check-constraint mutation APIs as the next true module boundary.
- [x] Move key/constraint mutation bind indexes and helpers into
      `mylite_catalog_key_constraints`.
- [x] Move index and index-column mutation APIs into
      `mylite_catalog_key_constraints`.
- [x] Move foreign-key and foreign-key-column mutation APIs into
      `mylite_catalog_key_constraints`.
- [x] Move check-constraint mutation APIs into
      `mylite_catalog_key_constraints`.
- [x] Expose only table/schema key-constraint cleanup helpers through the
      private catalog internal header.
- [x] Register the new runtime source in CMake.
- [x] Run focused build and catalog key/constraint mutation tests.
- [x] Review the split for behavior preservation, helper-surface width, and
      ownership.
- [x] Fix review findings.
- [x] Run `git diff --check`, staged diff check, and full check workflow.
- [x] Commit and push the twenty-third split.

## Round 24

- [x] Reinspect the standalone JSON runtime monolith and clang-tidy timing.
- [x] Select public API, DOM/emitter, parser, validator, path, containment, and
      mutation families as true module boundaries.
- [x] Add a private JSON internal header for shared value, parser, writer, path,
      and mutation types.
- [x] Move DOM ownership, array/object storage, writer, copy, lookup, and parser
      primitive helpers into `mylite_json_dom`.
- [x] Move iterative document parsing into `mylite_json_parse`.
- [x] Move allocation-free document validation into `mylite_json_validate`.
- [x] Move path lookup and mutation path parsing into `mylite_json_path`.
- [x] Move containment traversal into `mylite_json_contains`.
- [x] Move JSON mutation application into `mylite_json_mutation`.
- [x] Keep public JSON API wrappers and SQL-value conversion in `mylite_json.c`.
- [x] Register the new runtime sources in CMake.
- [x] Run focused JSON build, tests, and tidy.
- [x] Review the split for behavior preservation, helper-surface width, and
      ownership.
- [x] Fix review findings.
- [x] Run `git diff --check`, staged diff check, and full check workflow.
- [x] Commit and push the twenty-fourth split.

## Round 25

- [x] Reinspect scalar JSON execution and choose a true-module boundary.
- [x] Add a private scalar JSON internal header for shared constructor, path,
      and JSON_EXTRACT finish helpers.
- [x] Move JSON_SET, JSON_INSERT, JSON_REPLACE, and JSON_REMOVE scalar
      execution into `mylite_execution_scalar_json_mutation.c`.
- [x] Keep JSON construction, extraction, introspection, quoting, validation,
      and containment scalar execution in `mylite_execution_scalar_json.c`.
- [x] Register the new runtime source in CMake.
- [x] Run focused scalar JSON build, tests, and tidy.
- [x] Review the split for behavior preservation, helper-surface width, and
      ownership.
- [x] Fix review findings.
- [x] Run `git diff --check`, staged diff check, and full check workflow.
- [x] Commit and push the twenty-fifth split.

## Round 26

- [x] Reinspect `mylite_execution_catalog_loading.inc` and identify that its
      loaded-descriptor head is a true-module boundary.
- [x] Add `mylite_execution_loaded_catalog.h/.c` for loaded table columns,
      primary-key, index, foreign-key, and CHECK constraint descriptors.
- [x] Move loaded descriptor structs, loader APIs, cleanup APIs, column-key
      classification, and metadata-presence rejection helpers out of
      `mylite_execution.c` and `mylite_execution_catalog_loading.inc`.
- [x] Keep column-reference resolution, INSERT planning, DML value conversion,
      literal decoding, UTF-8 validation, and row-scalar select-item planning in
      execution fragments.
- [x] Register the new runtime source in CMake.
- [x] Run focused metadata/DML build, tests, and tidy.
- [x] Review the split for behavior preservation, helper-surface width, and
      ownership.
- [x] Fix review findings.
- [x] Run `git diff --check`, staged diff check, and full check workflow.
- [x] Commit and push the twenty-sixth split.

## Round 27

- [x] Reinspect `mylite_execution_catalog_loading.inc` after loaded-catalog
      extraction and identify DML numeric string scanning as a true-module
      boundary.
- [x] Add `mylite_execution_dml_numeric.h/.c` for DML numeric parse-result
      types, scanned token shape, integer-string parsing, signed-integer
      parsing, and numeric token scanning.
- [x] Move reusable DML numeric scanner/parser helpers out of
      `mylite_execution.c` and `mylite_execution_catalog_loading.inc`.
- [x] Keep DML value conversion, DECIMAL metadata/canonicalization,
      approximate-number conversion, diagnostics, and warning policy in
      execution fragments.
- [x] Register the new runtime source in CMake.
- [x] Run focused DML/type build, tests, and tidy.
- [x] Review the split for behavior preservation, helper-surface width, and
      ownership.
- [x] Fix review findings.
- [x] Run `git diff --check`, staged diff check, and full check workflow.
- [x] Commit and push the twenty-seventh split.

## Round 28

- [x] Reinspect remaining monolith fragments and identify scalar
      base-conversion plus binary/encoding scalar execution as a true-module
      boundary.
- [x] Add `mylite_execution_scalar_binary.c` for `BIN()`, `OCT()`, `CONV()`,
      `BIT_COUNT()`, `CRC32()`, `HEX()`, `WEIGHT_STRING()`, `UNHEX()`,
      `TO_BASE64()`, `FROM_BASE64()`, UUID conversion, `CHAR()`, and shared
      hex-byte formatting.
- [x] Move conversion-specific argument decoding, byte ownership, warning
      staging, unsupported-error message formatting, UUID swap handling, CRC32,
      and base-conversion formatting out of
      `mylite_execution_scalar_numeric.inc`.
- [x] Keep scalar arithmetic/bitwise evaluation, temporal scalar values,
      system-variable scalar values, binary cast/convert, JSON scalar helpers,
      shared diagnostic sinks, and row-scalar unknown-column behavior owned by
      the execution runtime through narrow internal wrappers.
- [x] Register the new runtime source in CMake.
- [x] Run focused scalar/binary build, tests, and tidy.
- [x] Review the split for behavior preservation, helper-surface width, and
      ownership.
- [x] Fix review findings.
- [x] Run `git diff --check`, staged diff check, and full check workflow.
- [x] Commit and push the twenty-eighth split.

## Round 29

- [x] Reinspect scalar string core/extended fragments and identify scalar
      string, REGEXP, and charset/collation execution as a true-module
      boundary.
- [x] Add `mylite_execution_scalar_string.c` for core string functions,
      extended string functions, REGEXP scalar functions, and scalar
      charset/collation/coercibility metadata execution.
- [x] Move string-specific argument normalization, UTF-8 slice validation,
      signed slice argument parsing, REGEXP pattern validation, collation
      metadata classification, and function-family dispatch out of the
      string `.inc` fragments.
- [x] Keep row-scalar planning, predicate planning, scalar projection
      descriptor construction, generic session-scalar dispatch, table-option
      validation, shared diagnostics, and session state in execution fragments
      behind explicit internal scalar helper wrappers.
- [x] Register the new runtime source in CMake.
- [x] Run focused scalar string, REGEXP, charset/collation, JSON quote, and
      string predicate tests.
- [x] Review the split for behavior preservation, helper-surface width, and
      ownership.
- [x] Fix review findings.
- [x] Run `git diff --check`, staged diff check, and full check workflow.
- [x] Commit and push the twenty-ninth split.

## Round 30

- [x] Reinspect scalar temporal core and temporal format/date-add fragments,
      identifying scalar temporal core as the next true-module boundary.
- [x] Add `mylite_execution_scalar_temporal.c` for `UNIX_TIMESTAMP`,
      `TIMESTAMP`, `DATEDIFF`, `TIMESTAMPDIFF`, `TIMEDIFF`, temporal extract,
      `SEC_TO_TIME`, `FROM_UNIXTIME`, temporal constructor, period, and
      `CONVERT_TZ` scalar execution.
- [x] Move temporal-core argument decoding, unit parsing, integer-literal
      parsing, unsupported diagnostics, NULL propagation, and function-family
      dispatch out of `mylite_execution_scalar_temporal_core.inc`.
- [x] Keep row-scalar temporal planning, predicate planning, date-format
      execution, date-interval execution, time arithmetic, shared calendar
      arithmetic, generic scalar dispatch, diagnostics, and session state in
      execution fragments behind explicit internal scalar helper wrappers.
- [x] Register the new runtime source in CMake.
- [x] Run focused scalar temporal build, tests, and tidy.
- [x] Review the split for behavior preservation, helper-surface width, and
      ownership.
- [x] Fix review findings.
- [x] Run `git diff --check`, staged diff check, and full check workflow.
- [x] Commit and push the thirtieth split.

## Round 31

- [x] Reinspect scalar temporal format/date-interval execution and identify
      its shared calendar arithmetic dependency.
- [x] Add `mylite_temporal_arithmetic.h/.c` for canonical datetime parsing,
      checked temporal arithmetic, calendar-month application, and day/second
      conversion helpers.
- [x] Add `mylite_execution_scalar_temporal_format.c` for `DATE_FORMAT`,
      `GET_FORMAT`, `TIME_FORMAT`, `STR_TO_DATE`, `DATE_ADD`/`DATE_SUB`,
      `ADDDATE`/`SUBDATE`, `TIMESTAMPADD`, `ADDTIME`, and `SUBTIME` scalar
      execution.
- [x] Move DATE_FORMAT numeric-comparison execution and the small planner
      helper surface for STR_TO_DATE child classification and date-interval
      function shape parsing into prefixed internal scalar helpers.
- [x] Add a dedicated `mylite_execution_scalar_temporal_format.h` boundary and
      keep DATE_FORMAT numeric-comparison side detection inside the temporal
      format module instead of depending back on row-scalar planning internals.
- [x] Replace `mylite_execution_scalar_temporal_format.inc` with a tombstone
      include and register the new runtime sources in CMake.
- [x] Run focused scalar temporal format build, tests, and tidy.
- [x] Review the split for behavior preservation, helper-surface width, and
      ownership.
- [x] Fix review findings.
- [x] Run `git diff --check`, staged diff check, and full check workflow.
- [x] Commit and push the thirty-first split.

## Round 32

- [x] Reinspect `mylite_execution_scalar_string.c` and identify REGEXP and
      charset/collation/coercibility as independent scalar submodules.
- [x] Add `mylite_execution_scalar_regexp.h/.c` for `REGEXP_LIKE`,
      `REGEXP_INSTR`, `REGEXP_SUBSTR`, `REGEXP_REPLACE`, REGEXP argument
      classification, match-type decoding, and pattern validation helpers.
- [x] Add `mylite_execution_scalar_charset_collation.h/.c` for `CHARSET`,
      `COLLATION`, `COERCIBILITY`, scalar collation lookup, and scalar
      charset/collation metadata helpers.
- [x] Move REGEXP and charset/collation helper declarations out of the
      catch-all scalar header into dedicated internal headers included by the
      execution monolith boundary.
- [x] Register the new runtime sources in CMake.
- [x] Run focused scalar string, REGEXP, charset/collation, coercibility, and
      string predicate build/tests/tidy.
- [x] Review the split for behavior preservation, helper-surface width, and
      ownership.
- [x] Fix review findings.
- [x] Run `git diff --check`, staged diff check, and full check workflow.
- [x] Commit and push the thirty-second split.

## Round 33

- [x] Reinspect `mylite_execution_scalar_string.c` after the REGEXP and
      charset/collation extraction.
- [x] Add `mylite_execution_scalar_string_position.h/.c` for string slice,
      padding, bitmask, search, `FIND_IN_SET`, and `STRCMP` execution.
- [x] Add `mylite_execution_scalar_string_transform.h/.c` for `CONCAT_WS`,
      `REPLACE`, string `INSERT`, `REVERSE`, `SOUNDEX`, `QUOTE`, and
      `SUBSTRING_INDEX` execution.
- [x] Move position/search/padding/bitmask and transform declarations out of
      the catch-all scalar header into dedicated internal headers.
- [x] Keep core string length, codepoint, case, trim, and borrowed
      session-scalar text conversion in `mylite_execution_scalar_string.c`.
- [x] Register the new runtime sources in CMake.
- [x] Run focused scalar string build, tests, and tidy.
- [x] Review the split for behavior preservation, helper-surface width, and
      ownership.
- [x] Fix review findings.
- [x] Run `git diff --check`, staged diff check, and full check workflow.
- [x] Commit and push the thirty-third split.

## Round 34

- [x] Reinspect remaining scalar fragments after the scalar string-family split.
- [x] Identify numeric scalar execution as a true-module boundary with a narrow
      helper surface.
- [x] Add `mylite_execution_scalar_numeric.h/.c` for scalar division, bitwise
      scalar result formatting, `ABS`, `SIGN`, rounding, approximate math,
      trigonometry, logarithm/power, `FORMAT`, and `TRUNCATE` execution.
- [x] Move locale-stable double parsing/formatting into the numeric module and
      expose it through internal declarations for `RAND()` and catalog numeric
      normalization.
- [x] Remove `mylite_execution_scalar_numeric.inc` and stale numeric structs,
      prototypes, and static constants from `mylite_execution.c`.
- [x] Register the new runtime source in CMake.
- [x] Run focused scalar numeric and catalog-loading-adjacent tests.
- [x] Review the split for behavior preservation, helper-surface width, and
      ownership.
- [x] Fix review findings.
- [x] Run focused tidy.
- [x] Run `git diff --check`, staged diff check, and full check workflow.
- [x] Commit and push the thirty-fourth split.

## Round 35

- [x] Reinspect the remaining largest runtime fragments and identify diagnostics
      as the next cohesive true-module boundary.
- [x] Add `mylite_execution_diagnostics.h/.c` for MySQL-compatible runtime
      error, warning, note, and parser-status translation helpers.
- [x] Add `mylite_mysql_error_codes.h` so diagnostics and the remaining runtime
      fragments share the existing MySQL numeric diagnostic constants without
      duplicating enum entries.
- [x] Add `mylite_execution_plan_types.h` for small planning/diagnostic types
      that must cross the diagnostics module boundary while runtime fragments
      remain include-based.
- [x] Remove `mylite_execution_diagnostics.inc` and stale diagnostic prototypes
      from `mylite_execution.c`.
- [x] Register the new runtime source in CMake.
- [x] Run focused diagnostics, DDL/DML, scalar, and parser-adjacent build/tests.
- [x] Review the split for behavior preservation, helper-surface width, and
      ownership.
- [x] Fix review findings.
- [x] Run focused tidy.
- [x] Run `git diff --check`, staged diff check, and full check workflow.
- [x] Commit and push the thirty-fifth split.

## Round 36

- [x] Reinspect the remaining largest runtime fragments after diagnostics
      extraction.
- [x] Identify the misnamed catalog-loading bucket as a same-translation-unit
      DML/conversion split candidate rather than a clean true-module boundary.
- [x] Split descriptor helpers, INSERT row planning, INSERT value
      conversion, DML default materialization, integer conversion, ENUM/SET
      conversion, string/binary/bit conversion, decimal/approximate conversion,
      temporal default values, value helper, string validation, implicit-value,
      and row-scalar select-item code into named fragments.
- [x] Remove `mylite_execution_catalog_loading.inc` and replace it with the
      ordered fragment include sequence.
- [x] Run focused DML, type, scalar, and parser-adjacent build/tests.
- [x] Review the split for behavior preservation, fragment boundaries,
      misleading names, and future true-module readiness.
- [x] Fix review findings.
- [x] Run `git diff --check`, staged diff check, and full check workflow.
- [x] Commit and push the thirty-sixth split.
