# Reduced Parser Retry Recognizer

## Status

Implemented and release-qualified.

## Summary

MyLite first parses SQL with its generated Lemon grammar. On a syntax error, or
after Lemon produces an unsupported-utility marker, the parser may tokenize
the input again and run a bounded sequence of compatibility recognizers. Those
recognizers preserve useful MySQL surfaces that do not yet fit the primary
grammar, but they also form a second syntax-recognition layer.

Before this remediation, the parser driver names eight retry callbacks and may
attempt all eight after one syntax error. Two callbacks now duplicate syntax
that Lemon can recognize:

- tableless `SELECT ... LIMIT` is accepted directly by the current grammar;
- `CREATE INDEX ... TYPE ...` already has Lemon productions, but `TYPE` is not
  mapped to its parser terminal during the primary pass.

This feature removes those retries, gives the remaining retry strategies a
typed internal interface, lowers the callback ceiling from eight to six, and
adds zero-growth ratchets for both the strategy count and retry-layer source
bytes. SQL results, AST ownership, fatal-status precedence, diagnostics, and
the supported compatibility surface remain unchanged.

## Sources

- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- MyLite parser driver and grammar:
  `packages/libmylite/src/sql/mylite_parser.c` and
  `packages/libmylite/src/sql/mylite_parse.y`
- MyLite retry layer:
  `packages/libmylite/src/sql/mylite_parser_placeholders.c`,
  `packages/libmylite/src/sql/mylite_parser_placeholders_retry.inc`, and
  `packages/libmylite/src/sql/mylite_parser_placeholders.h`
- MyLite retry resource contract:
  `packages/libmylite/src/sql/mylite_parser_resources.h`
- Existing retry specifications:
  `docs/specs/parser-retry-fatal-status-propagation/specs.md`,
  `docs/specs/bounded-parser-recovery-resources/specs.md`, and
  `docs/specs/retry-ast-payload-span-integrity/specs.md`
- Follow-up review finding `ARCH-03`:
  `docs/architecture/review-2026-07-followup-remediation-plan.md`
- MySQL 8.4 `SELECT` syntax:
  <https://dev.mysql.com/doc/refman/8.4/en/select.html>
- MySQL 8.4 `CREATE INDEX` syntax:
  <https://dev.mysql.com/doc/refman/8.4/en/create-index.html>
- MySQL 8.4 locking reads:
  <https://dev.mysql.com/doc/refman/8.4/en/innodb-locking-reads.html>
- MySQL 8.4 row subqueries:
  <https://dev.mysql.com/doc/refman/8.4/en/row-subqueries.html>
- MySQL 8.4.9 observations pinned by
  `packages/libmylite/tests/mysql_reduced_parser_retry_recognizer_expectations.sh`.

The specification is independently authored from public documentation,
observed MySQL behavior, and MyLite source. It does not use MySQL parser or
server implementation source.

## Observed MySQL 8.4.9 Behavior

The pinned runtime verifies:

- tableless `SELECT` accepts `LIMIT row_count`, `LIMIT offset, row_count`, and
  `LIMIT row_count OFFSET offset`;
- result options before `DISTINCT`, including
  `SELECT SQL_BIG_RESULT DISTINCT ...`, are accepted;
- `ROW(1, 2) = ROW(1, 2)` and `(1, 2) = (1, 2)` have equivalent row-comparison
  behavior for the covered values;
- one query block may carry separate locking clauses for separate tables;
- `TYPE type_name` is accepted as an index-type synonym for
  `USING type_name` before `ON` and after the key-part list;
- when index types are supplied in both positions, the final one applies;
- `TYPE` remains usable as a nonreserved table or column identifier.

The fixture pins MySQL version `8.4.9` and exact representative results.

## Pre-Remediation Baseline

The retry-layer files measure:

| File | Lines | Bytes |
| --- | ---: | ---: |
| `mylite_parser_placeholders.c` | 1,973 | 72,847 |
| `mylite_parser_placeholders_retry.inc` | 6,889 | 235,040 |
| `mylite_parser_placeholders.h` | 95 | 3,767 |
| **Retry-layer total** | **8,957** | **311,654** |

The parser driver contributes another 30,225 bytes, including direct knowledge
of every callback and the callback accounting loop.

Representative full-parser measurements are:

| SQL shape | Primary Lemon | Retry callbacks | Handled |
| --- | --- | ---: | ---: |
| `SELECT 1` | accepted | 0 | 0 |
| `SELECT 1 LIMIT 1` | accepted | 0 | 0 |
| `SELECT SQL_SMALL_RESULT DISTINCT 1` | syntax error | 2 | 1 |
| `SELECT (1,2)` | syntax error | 3 | 1 |
| `SELECT (1,2) = (1,2)` | syntax error | 3 | 1 |
| `SELECT 1 FOR UPDATE FOR SHARE` | syntax error | 6 | 1 |
| `CREATE INDEX i TYPE BTREE ON t(c)` | syntax error | 7 | 1 |
| `SHOW EXTENDED COLUMNS FROM t` | syntax error | 8 | 1 |

The tableless-limit callback is no longer reached by the supported forms it
was created for. The legacy-`TYPE` retry reparses the entire statement while
overriding only the `TYPE` token to the existing `USING` terminal.

## Retry Inventory

Every retry path belongs to exactly one of the following architectural
categories. A callback may contain several explicitly inventoried residual
families, but no path may be added without selecting a category and updating
the ratchets.

| Retry path | Trigger | Category | Disposition |
| --- | --- | --- | --- |
| Row-constructor predicate repair | syntax error or unsupported query placeholder | Grammar transform | Retain behind typed dispatch; placeholder targeted row predicates, reparse, then restore the row AST. |
| Result-option-before-duplicate repair | syntax error | Token transform | Retain behind typed dispatch; reorder only the leading modifier tokens into the canonical Lemon order. |
| Parenthesized row-constructor repair | syntax error | Token transform | Retain behind typed dispatch; inject synthetic `ROW` terminals only for indexed tuple parentheses. |
| Parenthesized row/scalar arithmetic repair | syntax error or unsupported query placeholder | Grammar transform | Retain behind typed dispatch; replace targeted subjects, reparse, validate, and graft independently parsed expressions. |
| Tableless `SELECT ... LIMIT` repair | syntax error | Grammar transform | Remove; current Lemon productions already accept the supported forms. |
| Repeated locking-clause repair | syntax error | Grammar transform | Retain behind typed dispatch; parse the supported prefix and validate the remaining locking-clause sequence. |
| Legacy `CREATE INDEX ... TYPE` repair | syntax error | Token transform | Remove; map `TYPE` to the existing Lemon terminal while preserving it as an identifier fallback. |
| `ALTER TABLE` algorithm/lock tail | syntax error | Grammar transform | Retain within the residual dispatcher; parse the supported prefix and apply validated tail options. |
| `CREATE TABLE` partition tail | syntax error | Grammar transform | Retain within the residual dispatcher; parse and rebase the supported prefix while preserving the documented embedded partition placeholder. |
| Plural maintenance, filtered describe/explain, extended metadata, and system-variable `:=` scanners | syntax error | Grammar transform | Retain within the residual dispatcher until equivalent primary productions are implemented and measured. |
| Admin and utility no-op statements | syntax error | Utility placeholder | Retain; accept only the documented bounded shapes and build explicit no-op AST kinds. |
| Explain wrappers | syntax error | Utility placeholder | Retain; preserve the explicit explain AST/diagnostic contract. |
| Stored-program, schema-security, unsupported utility, query-expression, table-reference, expression, CTE, and DML residual classifiers | syntax error | Unsupported compatibility fallback | Retain; recognize only complete targeted shapes and build explicit unsupported AST kinds rather than executing them. |

The generic residual dispatcher is not permission to add an uncatalogued
recognizer. New supported syntax belongs in Lemon unless a feature
specification records a measured reason for a bounded transform.

## Typed Retry Interface

The parser driver must not name or store function pointers for individual
retry implementations. The internal retry header exposes:

```c
enum mylite_sql_parser_retry_category {
    MYLITE_SQL_PARSER_RETRY_CATEGORY_GRAMMAR_TRANSFORM = 1U << 0U,
    MYLITE_SQL_PARSER_RETRY_CATEGORY_TOKEN_TRANSFORM = 1U << 1U,
    MYLITE_SQL_PARSER_RETRY_CATEGORY_UTILITY_PLACEHOLDER = 1U << 2U,
    MYLITE_SQL_PARSER_RETRY_CATEGORY_UNSUPPORTED_FALLBACK = 1U << 3U
};

enum mylite_sql_parser_retry_kind {
    MYLITE_SQL_PARSER_RETRY_ROW_CONSTRUCTOR_PREDICATE,
    MYLITE_SQL_PARSER_RETRY_SELECT_RESULT_OPTION_REORDER,
    MYLITE_SQL_PARSER_RETRY_PARENTHESIZED_ROW_CONSTRUCTOR,
    MYLITE_SQL_PARSER_RETRY_ROW_ARITHMETIC_PREDICATE,
    MYLITE_SQL_PARSER_RETRY_REPEATED_SELECT_LOCKING,
    MYLITE_SQL_PARSER_RETRY_PLACEHOLDER,
    MYLITE_SQL_PARSER_RETRY_KIND_COUNT
};

unsigned int mylite_sql_parser_retry_category_mask(
    enum mylite_sql_parser_retry_kind kind
);

enum mylite_sql_parse_status mylite_sql_parser_try_retry(
    enum mylite_sql_parser_retry_kind kind,
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    struct mylite_sql_parser_retry_context *retry_context,
    bool *out_handled
);
```

The implementation dispatches the enum to private functions and exposes its
auditable classification as a category mask. The residual placeholder kind
has grammar-transform, utility-placeholder, and unsupported-fallback bits
because it contains separately inventoried families in all three categories.
Invalid enum values return `MYLITE_SQL_PARSE_MISUSE` without touching AST
ownership.

The parser driver keeps two ordered typed plans:

1. unsupported-utility probes for row arithmetic and row-constructor
   predicates;
2. syntax-error probes for row constructors, modifier order, parenthesized
   rows, row arithmetic, repeated locking, and residual placeholders.

Ordering is compatibility behavior. The first handled retry wins. Fatal
callback or retry-context status still wins over recoverable syntax failure.

## Primary-Grammar Corrections

### Tableless `LIMIT`

No grammar change is required. The existing independently authored production
already includes the limit tail:

```lemon
select_statement ::= SELECT select_modifiers select_item_list
    where_clause_opt window_clause_opt select_order_clause_opt
    limit_clause_opt select_locking_clause_opt.
```

The obsolete scanner, prefix parse, AST rebase, and limit-node synthesis are
deleted. Supported tableless limit forms must report zero retry tokenizations,
callbacks, and handled retries.

### Legacy Index `TYPE`

`TYPE` is a nonreserved MySQL keyword. The primary token mapper maps its text
to a dedicated Lemon terminal, and Lemon falls that terminal back to
`IDENTIFIER` in states that do not accept an index type:

```lemon
%fallback IDENTIFIER TYPE.
index_type_option ::= TYPE identifier.
```

This admits the already-declared prefix and suffix index-option productions
without a second parse. It also preserves unquoted `type` identifiers outside
that grammar context. Both legacy index positions must report zero retries.

## Resource, Ownership, And Diagnostic Contract

The syntax retry ceiling becomes six callbacks, equal to the complete ordered
syntax plan. The existing limits remain:

- one retry tokenization;
- 65,536 retry tokens;
- 512 parenthesis-index levels;
- four total lexer passes;
- proportional live retry workspace capped at eight MiB.

Removing callbacks must not change:

- parse-result deinitialization ownership;
- replacement of an original AST only after a successful handled retry;
- fatal `NOMEM`, misuse, lexer, or stack outcomes;
- preservation of the original syntax token for an unhandled retry;
- payload-span validation and atomic source-length rebasing;
- public MySQL-compatible diagnostics for supported or unsupported SQL.

Directly parsed forms avoid retry allocation and callback work entirely.

## Zero-Growth Ratchets

A repository test records two ceilings:

1. `MYLITE_SQL_PARSER_RETRY_KIND_COUNT` and the syntax callback budget may not
   exceed six;
2. the combined byte size of
   `mylite_parser_placeholders.c`,
   `mylite_parser_placeholders_retry.inc`, and
   `mylite_parser_placeholders.h` may not exceed the exact post-remediation
   baseline.

The test reads the enumerated source files explicitly, so generated parser
size and unrelated parser-driver growth do not cause false positives.
Relocating retry logic to an uncounted file is not a valid way to satisfy the
ratchet; the file list must be updated if retry implementation is split.

The post-remediation source ceiling is:

| File | Lines | Bytes |
| --- | ---: | ---: |
| `mylite_parser_placeholders.c` | 1,971 | 73,024 |
| `mylite_parser_placeholders_retry.inc` | 6,593 | 224,957 |
| `mylite_parser_placeholders.h` | 72 | 2,746 |
| **Retry-layer ceiling** | **8,636** | **300,727** |

This is 321 lines and 10,927 bytes below the pre-remediation layer.

An intentional increase requires a new feature specification, updated
inventory, measured grammar alternative, and an explicit ratchet change in
the same review.

## Compatibility And Storage

This is an internal parser-architecture change. It does not alter:

- supported SQL value semantics;
- AST kinds or public result metadata;
- runtime warnings, errors, or no-op behavior;
- `.mylite` file format, VFS behavior, or catalog schema;
- SQLite integration or fork patches;
- public ABI or dependencies.

The compatibility matrix records the reduced retry architecture as its own
qualified parser guarantee. Broader MySQL grammar and execution gaps remain
tracked by their existing feature rows.

## Tests

The MySQL 8.4.9 fixture covers tableless limit forms, modifier ordering, row
constructors, repeated locking clauses, both legacy index-type positions,
last-option precedence, and `TYPE` identifier use.

Native tests must cover:

- exact inventory order and category for all six retry kinds;
- invalid retry kind misuse without AST damage;
- zero retry metrics for tableless `LIMIT`, legacy index `TYPE`, and ordinary
  `type` identifiers;
- unchanged AST shape and runtime behavior for both removed retries;
- exact callback positions for every retained retry;
- the six-callback ceiling on unhandled syntax;
- fatal-status and allocation-failure propagation through typed dispatch;
- AST payload spans and source rebasing for retained prefix retries;
- the strategy-count and exact source-byte ratchets.

Qualification must include Development, Debug-CI, Release, ASan/UBSan, and
deterministic allocator profiles; all parser and affected runtime suites; all
pinned parser MySQL fixtures; parser fuzzing; host Lemon generation;
formatting; full static analysis; ABI/install-consumer checks; compatibility
validation; and the production size gate.

## Qualification

Release qualification completed on 2026-07-28 against MySQL 8.4.9 and the
supported native build profiles:

- all 52 parser-labeled suites and all 31 affected runtime parser, index, and
  limit suites pass in Development, assertion-enabled Debug-CI, Release, and
  ASan/UBSan with leak detection;
- retry architecture, fatal-status, and source-ratchet suites pass in the
  deterministic allocator profile;
- all 42 pinned parser-related MySQL 8.4.9 fixtures pass, including the
  dedicated tableless-limit, legacy-index-`TYPE`, row-constructor, modifier,
  and repeated-locking expectations;
- 10,000 seeded parser-fuzzer executions pass under ASan/UBSan;
- host Lemon generation, repository formatting, and full LLVM 19 static
  analysis across 930 translation units pass;
- the shared-library ABI snapshot, installed CMake and pkg-config consumers,
  multiarch pkg-config relocation, and compatibility ledger pass;
- the production archive is 12,381,120 bytes against the 15,000,000-byte
  ceiling.

The final ratchets require exactly six typed retry strategies and at most
300,727 bytes across the enumerated retry-layer sources. Tableless `LIMIT`,
both legacy index `TYPE` positions, and ordinary `type` identifiers produce
zero retry tokenizations, callbacks, and handled retries. Review of the typed
plans and retry result publication confirms finite forward progress, bounded
workspace and lexer work, fatal-status precedence, AST replacement only after
a successful handled retry, and preservation of the original MySQL-compatible
syntax diagnostic when no retry handles the statement.
