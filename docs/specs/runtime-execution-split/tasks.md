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
- [ ] Commit and push the third split.
