# Parser Retry Fatal Status Propagation

## Status

Implemented and release-qualified.

## Summary

MyLite may run a bounded sequence of compatibility retries after the Lemon
parser reports a syntax error or produces an unsupported utility AST. The
retry pipeline currently ignores failure while creating its shared token and
parenthesis context. Its callback dispatcher also restores the original parse
status whenever a callback reports `out_handled == false`, even when that
callback returned `MYLITE_SQL_PARSE_NOMEM`. Several retry callbacks separately
turn a fatal failure from a nested Lemon parse back into an ordinary rejected
retry.

These paths can report a syntax or unsupported-statement error after an
allocation actually failed. The public runtime then returns `MYLITE_ERROR`
instead of `MYLITE_NOMEM` and publishes the wrong diagnostic.

This feature makes fatal parse infrastructure status authoritative throughout
retry initialization, dispatch, nested retry parsing, cleanup, and public
runtime translation.

## Sources

- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Initial parser status and ownership contract:
  `docs/specs/mysql-parser-scaffold/specs.md`
- Follow-up review finding `SQL-01`:
  `docs/architecture/review-2026-07-followup-remediation-plan.md`
- MySQL 8.4 error information interfaces:
  <https://dev.mysql.com/doc/refman/8.4/en/error-interfaces.html>
- MySQL 8.4 error message reference:
  <https://dev.mysql.com/doc/mysql-errors/8.4/en/>
- Existing MySQL 8.4.9 parser fixtures under
  `packages/libmylite/tests/mysql_parser_corpus_*_expectations.sh`.

MySQL does not expose a deterministic statement-local allocator failpoint, so
its runtime cannot provide a reproducible equivalent of this internal failure
matrix. Existing pinned fixtures establish that ordinary accepted and rejected
syntax is unchanged. MyLite's deterministic allocator profile establishes the
failure-only behavior.

This specification is independently authored from public behavior and MyLite
source. It does not copy MySQL parser or server implementation source.

## Scope

This feature covers:

- shared retry tokenization and parenthesis-index initialization;
- all compatibility-retry callback dispatch;
- nested Lemon parses used by retry callbacks;
- AST allocations performed while scanning or constructing a retry result;
- cleanup of the original result, retry-local results, shared context, token
  vectors, predicate flags, and retry descriptor vectors;
- translation from `MYLITE_SQL_PARSE_NOMEM` to public `MYLITE_NOMEM`;
- stable connection diagnostics for public execution and prepare paths;
- retry profiling counters on both recoverable and fatal attempts.

This feature does not:

- add or remove accepted SQL syntax;
- change MySQL syntax-error wording;
- bound retry work or memory beyond existing allocation overflow checks
  (`SQL-02`);
- change source-span rebasing (`SQL-03`);
- change large-diagnostic formatting (`SQL-04`);
- change Lemon stack limits (`SQL-05`);
- reduce or reorganize the compatibility recognizer (`ARCH-03`);
- change public ABI, serialized data, dependencies, or SQLite.

## Status Precedence

The parser uses these status classes:

| Class | Statuses | Retry behavior |
| --- | --- | --- |
| Success | `MYLITE_SQL_PARSE_OK` | Accept a handled retry or continue after an unhandled probe. |
| Recoverable rejection | `MYLITE_SQL_PARSE_SYNTAX_ERROR` | Preserve the original recoverable result unless a later retry succeeds. |
| Fatal infrastructure | `MYLITE_SQL_PARSE_NOMEM`, `MYLITE_SQL_PARSE_MISUSE`, `MYLITE_SQL_PARSE_LEXER_ERROR`, `MYLITE_SQL_PARSE_STACK_OVERFLOW` | Replace the earlier status immediately and stop all later retries. |

The precedence rule is:

```text
fatal retry failure > accepted handled retry > original recoverable result
```

`out_handled` describes whether a callback recognized and published a
compatibility result. It must never decide whether a fatal return value is
propagated.

The shared retry context is initialized only after an initial recoverable
result. If initialization fails, its exact status becomes both the function
return and `out_result->status`; no callback runs. Context deinitialization
still releases every allocation that succeeded before the failure.

## Callback Contract

Every retry callback must follow one contract:

- initialize `*out_handled` to `false`;
- return `OK` with `out_handled == false` when its syntax shape does not apply;
- return `OK` with `out_handled == true` only after replacing the original
  result with a complete owned retry result;
- translate a retry-local syntax error to an unhandled recoverable rejection
  without overwriting the original diagnostic;
- return every other retry-local status unchanged, regardless of
  `out_handled`;
- deinitialize every retry-local parse result and temporary vector before
  returning failure.

Nested callback chains use the same rule. A helper returning a fatal status
must terminate its caller even if it did not mark the statement handled.
Prefix parses used by tableless `LIMIT`, repeated locking clauses, legacy
index syntax, `ALTER TABLE` option tails, and partition placeholders may
discard only `SYNTAX_ERROR`; they must not discard allocation, lexer, misuse,
or stack failures.

## Result And Counter Semantics

On fatal retry failure:

- `mylite_sql_parse()` returns the fatal status;
- `result.status` equals the returned status;
- the result remains valid for exactly one
  `mylite_sql_parse_result_deinit()` call;
- no partial retry AST is published as successful;
- `retry_tokenization_count` records an attempted shared tokenization;
- `retry_callback_count` includes the callback that failed;
- `retry_handled_count` increases only for an actually handled callback, not
  merely to force fatal propagation.

The original syntax token may remain in the failed result for debugging, but
it is not surfaced as the public error because fatal status has precedence.

## Public Runtime Diagnostics

The existing runtime mapping remains authoritative:

| Parser result | Public result | SQLSTATE | Message |
| --- | --- | --- | --- |
| `MYLITE_SQL_PARSE_NOMEM` | `MYLITE_NOMEM` | `HY001` | `out of memory` |
| recoverable syntax/stack/lexer failure | `MYLITE_ERROR` | existing condition-specific value | existing parse diagnostic |

Execution, buffered execution, lazy prepared parsing, and statement digest
parsing must not replace `MYLITE_NOMEM` with a syntax diagnostic. The
connection remains usable after the one-shot allocator failpoint is cleared.

## Grammar

No Lemon grammar change is required. This feature changes only failure
propagation around existing grammar and compatibility retries.

## Tests

A dedicated allocator-profile native test must sweep every allocation until a
successful no-failure parse is reached for representative paths:

- retry-context token growth without parentheses;
- retry-context token growth plus both parenthesis-index allocations;
- result-option-before-duplicate retry;
- parenthesized row-constructor retry;
- row-constructor predicate retry;
- parenthesized row-arithmetic predicate retry;
- tableless `LIMIT` prefix retry;
- repeated-locking prefix retry;
- legacy `CREATE INDEX ... TYPE` retry;
- generic and specialized placeholder retries, including `ALTER TABLE`,
  partition, and scanned-statement AST construction.

For each injected allocation:

- direct parsing returns `MYLITE_SQL_PARSE_NOMEM`;
- `result.status` is `MYLITE_SQL_PARSE_NOMEM`;
- result deinitialization is leak-free;
- retry counters remain internally consistent.

An end-to-end execution sweep over normally accepted retry syntax must reject
any failure that is downgraded to `MYLITE_ERROR`. Every fatal parser allocation
returns `MYLITE_NOMEM`, publishes `HY001` / `out of memory`, and permits a
successful subsequent statement after the failpoint is cleared.

Qualification must include the focused failure test in the deterministic
allocator profile, all parser native tests in Debug, Release, and ASan/UBSan
profiles, the pinned parser MySQL fixtures, formatting, static analysis, ABI
and install-consumer checks, parser fuzzing, and the production size gate.

## Qualification

Release qualification completed on 2026-07-28 against MySQL 8.4.9 and the
supported native build profiles:

- all 45 ordinary parser suites pass in Development, assertion-enabled
  Debug-CI, Release, and ASan/UBSan with leak detection;
- the deterministic fault-injection profile passes all 46 parser suites,
  including the allocation sweep for every retry path, plus the runtime
  allocator-failpoint suite;
- all 37 pinned parser MySQL 8.4.9 fixtures pass, establishing unchanged
  accepted results and rejection diagnostics across the parser corpus;
- 10,000 seeded parser-fuzzer executions pass under ASan/UBSan;
- focused LLVM 19 static analysis, repository formatting, shared-library ABI
  snapshots, installed CMake/pkg-config consumers, and production size gates
  pass;
- the production archive is 12,397,476 bytes against the 15,000,000-byte
  ceiling.

The focused failure suite is registered in the cross-platform deterministic
allocator configuration, including Windows CI. The local sanitizer, fuzz,
fault, and production qualification was performed on Linux.
