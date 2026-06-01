# Parser test split tasks

- [x] Identify parser test as the next clean monolith split target.
- [x] Specify category boundaries and shared support ownership.
- [x] Extract shared parser test helpers into `parser_test_support`.
- [x] Split parser test functions into category executables.
- [x] Register split parser tests in CMake under `libmylite.parser.*`.
- [x] Run focused parser build, CTest, and clang-tidy checks.
- [x] Review the split for coverage preservation and maintenance risk.
- [x] Fix review findings.
- [x] Run `git diff --check`, staged diff check, and full check workflow.
- [x] Commit and push the parser test split.
