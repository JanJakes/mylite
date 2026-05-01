# MySQL Parser Scaffold

## Status

This feature specifies MyLite's first SQL parser scaffold. It connects the
custom MySQL lexer to a Lemon-generated parser and produces an internal AST for
a deliberately tiny seed grammar. It is not statement execution support and it
does not mark `SELECT`, `USE`, expressions, transactions, or any other SQL
statement as MyLite-supported behavior.

The purpose of this scaffold is to establish the parser architecture that later
grammar work will extend safely:

- generated Lemon parser code built from checked-in MyLite grammar input
- lexer-to-parser token adaptation with MySQL keyword context rules
- internal AST ownership and source spans
- parse diagnostics that distinguish lexer errors, syntax errors, allocation
  failure, and parser stack overflow
- focused tests for end-to-end parsing of simple statements

## Sources

- MySQL 8.4 Reference Manual, SQL Statements:
  https://dev.mysql.com/doc/refman/8.4/en/sql-statements.html
- MySQL 8.4 Reference Manual, `SELECT` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- MySQL 8.4 Reference Manual, Expressions:
  https://dev.mysql.com/doc/refman/8.4/en/expressions.html
- MySQL 8.4 Reference Manual, Keywords and Reserved Words:
  https://dev.mysql.com/doc/refman/8.4/en/keywords.html
- MySQL 8.4 Reference Manual, Identifier Qualifiers:
  https://dev.mysql.com/doc/refman/8.4/en/identifier-qualifiers.html
- MySQL 8.4 Reference Manual, `USE` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/use.html
- SQLite Lemon parser generator documentation:
  https://www.sqlite.org/lemon.html

## Scope

The first implementation must parse only this seed surface:

- empty input and trailing semicolon after a parsed statement
- `USE schema_name`
- `SELECT` with one or more simple select expressions
- optional `FROM DUAL` on seed `SELECT`
- integer, decimal, float, string, hexadecimal, bit, `TRUE`, `FALSE`, and
  `NULL` literal expression nodes
- unquoted and quoted identifiers
- qualified identifiers, including reserved words after a period
- unqualified `*` as the whole select list
- parenthesized expressions
- unary `+` and `-`
- binary `+`, `-`, `*`, and `/` with normal arithmetic precedence
- ordinary comments skipped between tokens

Everything outside this seed grammar must remain unsupported. Unsupported
syntax should fail with a parse diagnostic rather than being accepted as a
placeholder AST. Later feature specs will add statement families, expression
operators, functions, clauses, DDL, DML, metadata, and runtime behavior.

## Non-Goals

The scaffold must not attempt to parse the full MySQL grammar. In particular,
the first implementation must not add:

- `WHERE`, joins, table references other than `DUAL`, grouping, ordering, set
  operations, CTEs, subqueries, locking clauses, or `SELECT ... INTO`
- function calls, aggregate syntax, collations, casts, interval expressions, or
  full operator precedence
- DDL, DML other than the seed `SELECT`, account statements, maintenance
  statements, prepared statements, compound statements, or stored programs
- semantic name resolution, type inference, value decoding, warning generation,
  optimizer hints, executable comment expansion, or SQLite lowering

## Parser Architecture

The parser is an internal `libmylite` component. It should be layered as:

1. The existing lexer tokenizes borrowed SQL text and records source spans.
2. A parser driver skips ordinary comments and maps lexer tokens to
   Lemon-terminal IDs.
3. The Lemon parser recognizes the seed grammar and calls small AST builder
   helpers.
4. The parse result owns all AST nodes and diagnostics. AST node spans borrow
   the input SQL text.
5. Later analyzer/runtime layers consume the AST; parser code must not depend
   on SQLite.

Generated parser output belongs in the CMake build directory. The checked-in
source of truth is the Lemon grammar file. This keeps generated code
reproducible and avoids reviewing large generated diffs while the grammar is
still changing rapidly.

## Lexer-To-Parser Token Rules

The lexer intentionally returns generic keyword tokens with keyword flags.
The parser driver is responsible for assigning keyword tokens to grammar
terminals:

- known seed syntax words become dedicated parser tokens when they are not
  immediately after a period: `SELECT`, `FROM`, `USE`, `TRUE`, `FALSE`,
  `NULL`, and `DUAL`
- nonreserved keywords may fall back to identifier tokens in identifier
  positions
- reserved keywords normally remain syntax tokens or reserved-keyword tokens
  that the seed grammar rejects
- any keyword following a period in a qualified name is treated as an
  identifier token, matching MySQL's qualified-name rule
- unknown operators and punctuation should produce a syntax error unless the
  seed grammar has a terminal for them

Executable version comments and optimizer hints are lexically distinct, but the
first parser scaffold skips all comment token kinds. Future parser work must
decide whether to expand executable comment bodies into nested token streams and
where optimizer hints attach in the AST.

## AST Design

The initial AST should be deliberately small but structurally extensible:

- a script root node owns ordered statement children
- each statement node has statement-specific children
- expression nodes use ordered children for operands
- identifiers preserve exact source spans and qualified-name structure
- literal nodes preserve token spans and literal categories without decoding
  values
- operator nodes store a parser-level operator enum, not raw lexer token kinds
- kind-specific node payloads are accessed through AST helper functions rather
  than by downstream phases reading generic fields by convention

AST nodes are owned by a parse-result arena/list. Individual child links are
non-owning. Deinitializing the parse result releases all AST nodes at once.
This keeps Lemon error cleanup simple while leaving room for a later arena
allocator if parser throughput requires it.

## Diagnostics

The parser must return structured status rather than printing or aborting:

- success
- misuse for invalid API arguments
- out of memory
- lexer error with the offending lexer token
- syntax error with the offending parser token and source span
- parser stack overflow

Syntax errors in the first scaffold should stop parsing immediately. Error
recovery can be designed later when MyLite has a statement-level recovery
policy for multi-statement scripts and tooling diagnostics.

## SQL Modes

The parser accepts the same lexical mode flags as the lexer and passes them
through unchanged. The initial parser does not add semantic SQL-mode behavior.
Mode-dependent grammar behavior, such as future `PIPES_AS_CONCAT` handling, must
be specified when those modes are implemented.

## MyLite Lemon Grammar Snippet

This snippet describes the intended seed grammar shape, not the complete MySQL
grammar:

```lemon
input ::= statement_list.

statement_list ::= .
statement_list ::= statements.

statements ::= statement.
statements ::= statements SEMICOLON.
statements ::= statements SEMICOLON statement.

statement ::= select_statement.
statement ::= use_statement.

use_statement ::= USE identifier.

select_statement ::= SELECT select_item_list.
select_statement ::= SELECT select_item_list FROM DUAL.
select_statement ::= SELECT STAR.
select_statement ::= SELECT STAR FROM DUAL.

select_item_list ::= select_item.
select_item_list ::= select_item_list COMMA select_item.

select_item ::= expression.

expression ::= literal.
expression ::= qualified_identifier.
expression ::= LPAREN expression RPAREN.
expression ::= PLUS expression.
expression ::= MINUS expression.
expression ::= expression PLUS expression.
expression ::= expression MINUS expression.
expression ::= expression STAR expression.
expression ::= expression SLASH expression.

literal ::= INTEGER.
literal ::= DECIMAL.
literal ::= FLOAT.
literal ::= STRING.
literal ::= NATIONAL_STRING.
literal ::= HEX_LITERAL.
literal ::= BIT_LITERAL.
literal ::= TRUE.
literal ::= FALSE.
literal ::= NULL.

qualified_identifier ::= identifier.
qualified_identifier ::= qualified_identifier DOT identifier.

identifier ::= IDENTIFIER.
identifier ::= QUOTED_IDENTIFIER.
```

The lexer-to-parser adapter maps a keyword token immediately after `.` to
`IDENTIFIER`. This keeps the grammar compact while preserving MySQL's
qualified-name behavior, where reserved words may appear as identifier parts
after dots.

Arithmetic precedence is:

1. unary `+` and `-`
2. binary `*` and `/`
3. binary `+` and `-`

The future full expression grammar should be added in small, verified slices.
Each slice should keep precedence decisions explicit in Lemon declarations and
tests.

## MySQL 8.4.9 Runtime Verification

These seed expectations were checked on 2026-05-01 against the official
`mysql:8.4.9` Docker image using a MySQL client connection as `root`.

Accepted by MySQL:

| SQL | Observed behavior |
| --- | --- |
| `SELECT VERSION();` | Returns `8.4.9`. |
| `SELECT 1;` | Returns one row with `1`. |
| `SELECT 1 + 2 * 3, 'text', TRUE, FALSE, NULL;` | Returns `7`, `text`, `1`, `0`, `NULL`. |
| `SELECT 1 FROM DUAL;` | Returns one row with `1`. |
| ``CREATE DATABASE IF NOT EXISTS `select`; USE `select`; SELECT 1;`` | Quoted reserved database name is accepted, then `SELECT 1` returns `1`. |
| `SELECT db.select;` | Syntax is accepted; execution fails with unknown-table diagnostic because semantic resolution is later than parsing. |

Rejected by MySQL with syntax error `1064` / SQLSTATE `42000`:

| SQL | Error location summary |
| --- | --- |
| `SELECT FROM DUAL;` | Near `FROM DUAL`. |
| `SELECT 1 +;` | Near end of input. |
| `SELECT INTERVAL;` | Near end of input. |

MyLite parser tests should mirror only the parse-level facts from this table.
Runtime result values remain future analyzer/executor responsibilities.

## Compatibility Decisions

- Statement rows such as `SELECT` and `USE` remain unsupported until analyzer,
  runtime behavior, metadata, warnings, and MySQL-runtime comparison tests are
  implemented for those statements.
- Parser acceptance of the seed grammar is a plumbing milestone, not user-facing
  SQL compatibility.
- The scaffold may reject valid MySQL syntax outside the seed grammar. Those
  rejections are known gaps, not deliberate long-term incompatibilities.

## Test Plan

Fast C tests must cover:

- successful parse of empty input and trailing semicolon
- successful parse of `USE` with quoted and unquoted identifiers
- successful parse of simple `SELECT` lists
- `FROM DUAL`
- expression precedence and parentheses
- `TRUE`, `FALSE`, `NULL`, string, numeric, hex, and bit literal nodes
- qualified identifier structure, including a reserved keyword after `.`
- comment skipping
- lexer error propagation
- syntax error propagation for malformed seed statements
- rejection of valid but unsupported MySQL syntax such as `WHERE`

Generated parser code must be built by CMake from the checked-in Lemon grammar.
The build must run with strict warnings, format checks, and clang-tidy on
first-party code.
