# Parser Corpus DDL Key and Type Surfaces Tasks

- [x] Verify representative MySQL 8.4.9 syntax and metadata expectations.
- [x] Add focused parser tests for inline `KEY`, primary-prefix key parts,
  index-level `KEY_BLOCK_SIZE`, and `ZEROFILL` placeholder classification.
- [x] Add parser/AST support for the implemented key and index option grammar.
- [x] Add runtime support for inline `KEY`, ignored index `KEY_BLOCK_SIZE`, and
  primary-prefix key rejection before descriptor mutation.
- [x] Add runtime tests for metadata, diagnostics, and no-mutation behavior.
- [x] Update compatibility documentation.
- [x] Rerun focused tests and parser corpus benchmark.
- [x] Review, fix findings, commit, and push.

Parser corpus benchmark after implementation:

```text
parse.csv.mysql_server_tests,parse,1,69595,69595,67803,1792,0,5544381,3564.596,51.219,19523.953
# parse_status ok=67803 misuse=0 nomem=0 lexer_error=21 syntax_error=1770 stack_overflow=1
```
