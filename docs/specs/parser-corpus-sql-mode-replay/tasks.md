# Parser Corpus SQL Mode Replay Tasks

- [x] Inspect remaining corpus failures and confirm SQL-mode state artifacts.
- [x] Specify benchmark-only SQL-mode replay semantics.
- [x] Implement a benchmark-local mode annotator for decoded CSV queries.
- [x] Add focused C coverage for direct sets, user-variable restore, list
  mutations, global no-ops, and ANSI-quoted parsing.
- [x] Make SQL-mode replay opt-in because the CSV is a flattened corpus without
  reliable session boundaries.
- [x] Rerun focused tests and the corpus benchmark, review remaining failures,
  commit, and push.
