# Parser Corpus Expression And DDL Tail Residuals Tasks

- [x] Verify target behavior against MySQL 8.4.9 for `NTH_VALUE FROM FIRST`,
  `FROM LAST`, repeated nullability, `CREATE TABLE ... START TRANSACTION`,
  interval-bitwise expressions, legacy replication aliases, and
  `LOCK TABLES ... LOW_PRIORITY WRITE`.
- [x] Add parser support for `NTH_VALUE(...) FROM FIRST OVER (...)`.
- [x] Normalize repeated nullability attributes so the last attribute determines
  descriptor metadata.
- [x] Extend unsupported utility placeholder coverage for interval value binary
  expressions and `CREATE TABLE ... START TRANSACTION`.
- [x] Add parser, runtime, MySQL expectation, compatibility, and benchmark
  coverage.
- [x] Run focused verification, full check workflow, review, commit, and push.
