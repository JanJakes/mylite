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
- [ ] Run `git diff --check`, staged diff check, and full check workflow.
- [ ] Commit and push the fourth split.
