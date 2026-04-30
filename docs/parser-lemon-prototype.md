# Lemon parser prototype

This branch introduces the first MyLite-owned C parser prototype using
SQLite's Lemon parser generator.

## Scope

The current parser is intentionally syntax-light:

- A MySQL-aware lexer recognizes strings, quoted identifiers, comments,
  numbers, identifiers, delimiters, and operator-like tokens.
- A Lemon grammar accepts the resulting token stream.
- The parser is recoverable for corpus rows that come from MySQL negative
  tests, including unmatched delimiters and unterminated quoted text.
- The CLI and corpus script verify that the WordPress SQLite Database
  Integration MySQL query corpus parses end to end.

This is not yet a full MySQL grammar. It is a lean parser foundation and corpus
harness for growing statement-level grammar coverage incrementally.

## Commands

Build the parser CLI:

```sh
make all
```

Regenerate Lemon output:

```sh
make regen-parser
```

Run the corpus test:

```sh
make test-parser
```

The corpus is downloaded to `tests/parser/.cache/` and is not committed.

## Next steps

1. Split the Lemon grammar from generic token-stream acceptance into
   statement-level productions.
2. Keep the permissive recovery mode for MySQL negative-test corpora, but add a
   strict mode for MyLite execution.
3. Add AST construction through arena-allocated C nodes.
4. Expand differential tests against MySQL 8.4.9 for accepted syntax, parse
   errors, SQL modes, comments, and quoted text behavior.

