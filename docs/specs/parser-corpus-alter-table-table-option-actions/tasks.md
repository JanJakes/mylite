# Parser Corpus ALTER TABLE Table-Option Actions Tasks

- [x] Classify residual ALTER/CREATE table-option parser corpus failures.
- [x] Verify representative MySQL 8.4.9 behavior for table-option-first ALTER and `UNION=()`.
- [x] Implement parser/runtime support for the scoped table-option action slice.
- [x] Add focused parser/runtime/MySQL expectation coverage and update compatibility docs.
- [x] Rerun focused tests, corpus benchmark, diff checks, and full workflow.
- [x] Release-gate review, fix findings, commit, push, and continue to the next bucket.

Parser corpus benchmark after implementation:

```text
parse.csv.mysql_server_tests: ok=69288, errors=307, total_ms=2682.028, avg_us=38.538
```
