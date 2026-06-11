# Parser Corpus Nonreserved Identifier Residuals Tasks

- [x] Verify representative residual behavior against MySQL 8.4.9.
- [x] Document the supported grammar and runtime scope.
- [x] Admit missing nonreserved keyword identifier tokens in the parser.
- [x] Keep `SESSION_USER` and `SYSTEM_USER` function calls while accepting
      their names in `CREATE TABLE name(...)` table-name position.
- [x] Extend parser retry identifier classification for plural
      table-maintenance targets.
- [x] Add parser, runtime, and MySQL expectation tests.
- [x] Re-run focused checks and parser-corpus benchmark.
