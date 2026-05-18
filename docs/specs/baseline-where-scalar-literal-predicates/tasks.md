# Baseline WHERE Scalar Literal Predicates Tasks

- [x] Read current compatibility, predicate, parser, runtime, catalog, result,
  storage, and test context.
- [x] Research official MySQL 8.4 expression, comparison, logical operator, and
  precedence documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for scalar literal truth predicates,
  scalar literal comparisons, literal-left descriptor comparisons, descriptor
  `NULL` comparisons, scalar literal `IS` tests, DML effects, diagnostics, and
  warnings.
- [x] Write the independent feature spec with MyLite grammar snippets,
  ownership boundaries, physical SQLite handling, diagnostics, and tests.
- [x] Add MySQL-runtime expectation script for this feature.
- [ ] Extend parser support for scalar literal predicate atoms and comparisons.
- [ ] Extend descriptor predicate planning for scalar literal truth,
  scalar-literal comparisons, literal-left descriptor comparisons, and
  descriptor `NULL` comparison literals.
- [ ] Add parser and runtime lifecycle tests, including SELECT, aggregates,
  DML, persistence, preamble preservation, independent handles, and
  deterministic unsupported-shape diagnostics.
- [ ] Update compatibility documentation for the exact limited subset.
- [ ] Run focused build/tests and the MySQL expectation script.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the final diff for architecture boundaries, descriptor authority,
  performance, cleanup, compatibility wording, and test relevance.
- [ ] Commit, push `main`, run a review subagent, amend if needed, push again,
  and continue to the next baseline slice.
