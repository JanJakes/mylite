# Parser Corpus Admin Transaction Surfaces Tasks

- [x] Verify transaction completion, plural rename, foreign-server DDL, and
  schema encryption/read-only behavior against MySQL 8.4.9.
- [x] Specify grammar, runtime semantics, and embedded-design gaps.
- [x] Add transaction completion grammar and AST marker for `AND CHAIN`.
- [x] Implement chained commit/rollback runtime behavior.
- [x] Accept `RENAME TABLES` as an alias of the existing rename path.
- [x] Classify foreign-server DDL as administrative no-op placeholders.
- [x] Classify schema encryption/read-only DDL as unsupported utility
  placeholders.
- [x] Add parser, runtime, and MySQL expectation tests.
- [x] Update compatibility documentation.
