# MySQL Bison Parser Prototype

## Goal

MyLite needs a parser that can accept the MySQL 8.4 SQL surface before the
analyzer and SQLite translation layers are complete. This prototype establishes
the Bison-based parser boundary, a MySQL-aware lexer, a public parse API, and a
corpus gate against the WordPress SQLite Database Integration MySQL query set.

## Sources

- MySQL 8.4.9 parser grammar: `sql/sql_yacc.yy`
- WordPress SQLite Database Integration query corpus:
  `packages/mysql-on-sqlite/tests/mysql/data/mysql-server-tests-queries.csv`

## Design

The current parser is a syntax-acceptance milestone, not the final semantic
grammar. Bison owns statement sequencing and balanced structure. The lexer
handles MySQL comments, executable version-comment bodies, quoted identifiers,
string literals, numeric literals, user/system variables, parameter markers,
statement-leading keywords, and stored program `END IF` / `END LOOP` style
compound endings.

The grammar records a statement kind for each parsed statement and validates
that grouping delimiters and `BEGIN ... END` blocks are balanced. The body of
each statement remains token-preserving and permissive so the prototype can
accept the broad MySQL statement inventory while detailed productions are added
statement by statement.

## Boundaries

- Produces a statement-kind AST shell only.
- Does not yet resolve identifiers, expression precedence, table references,
  metadata, warnings, or MySQL runtime errors.
- Skips ordinary comments. MySQL executable `/*! ... */` comments are tokenized
  as SQL because they can carry required syntax.
- Accepts unknown statement starts as `unknown`; later grammar work should
  reduce that surface as concrete productions land.

## Verification

- `make smoke` builds the parser and checks representative MySQL syntax plus
  negative tests for unmatched delimiters and unterminated strings.
- `make corpus` downloads the WordPress corpus into `build/corpus/` and parses
  every query through the same CLI in NUL-delimited batch mode. The current
  corpus gate covers 69,577 records.
