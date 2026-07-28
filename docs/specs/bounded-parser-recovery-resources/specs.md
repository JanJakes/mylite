# Bounded Parser Recovery Resources

## Status

Specified; implementation and release qualification are pending.

## Summary

MyLite runs a compatibility retry layer after a recoverable Lemon syntax error
or an unsupported utility parse. The shared retry context currently copies
every token into a 64-byte token vector and, whenever the statement contains a
parenthesis, eagerly allocates two `size_t` indexes plus one flag byte for
every token. Up to eight retry callbacks can then inspect the complete token
sequence, and selected callbacks run one or two additional Lemon parses.

This work is linear after the earlier predicate-context correction, but it has
large constants and no statement-local ceiling. A one-megabyte shallow
malformed statement currently retains approximately 81 MiB at peak and
requests approximately 149 MiB cumulatively before returning a syntax error.

This feature gives compatibility recovery explicit token, parenthesis-depth,
lexer-pass, callback, and workspace budgets. It also constructs parenthesis
indexes only after a retry whose syntax shape needs them. Statements accepted
by the primary Lemon grammar do not enter these budgets.

## Sources

- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Initial parser ownership and status contract:
  `docs/specs/mysql-parser-scaffold/specs.md`
- Fatal retry status precedence:
  `docs/specs/parser-retry-fatal-status-propagation/specs.md`
- Follow-up review finding `SQL-02`:
  `docs/architecture/review-2026-07-followup-remediation-plan.md`
- MySQL 8.4 packet-size behavior:
  <https://dev.mysql.com/doc/refman/8.4/en/packet-too-large.html>
- MySQL 8.4 memory-use description:
  <https://dev.mysql.com/doc/refman/8.4/en/memory-use.html>
- MySQL 8.4 statement-digest memory limits:
  <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-statement-digests.html>
- MySQL 8.4 error reference:
  <https://dev.mysql.com/doc/mysql-errors/8.4/en/>
- MySQL 8.4.9 observations pinned by
  `packages/libmylite/tests/mysql_bounded_parser_recovery_resources_expectations.sh`.

The specification is independently authored from public behavior and MyLite
source. It does not copy MySQL parser or server implementation source.

## Observed MySQL 8.4.9 Behavior

The pinned runtime accepts statements up to its configured packet boundary;
the server default `max_allowed_packet` is 64 MiB and the protocol maximum is
1 GiB. The parser does not expose a configurable token or recovery-work limit.
MySQL documents bounded statement-digest storage separately from parsing.

Observed malformed statements containing 64 and 65,536 flat integer tokens,
and a one-megabyte statement padded by a comment, all return:

| Property | Value |
| --- | --- |
| Error | `1064` (`ER_PARSE_ERROR`) |
| SQLSTATE | `42000` |
| Message prefix | `You have an error in your SQL syntax` |
| Connection | Remains usable |

MySQL exposes neither retry counters nor deterministic parser allocation
instrumentation. MyLite therefore uses the runtime fixture to pin externally
observable syntax behavior and native instrumentation to qualify its internal
resource limits.

## Current Baseline

The following Release measurements were captured before implementation with
link-time allocation and lexer wrappers around one direct `mylite_sql_parse()`
call. Requested bytes count every successful `malloc`, `calloc`, and new
`realloc` size; peak bytes track simultaneously live requested sizes. Runtime
is a single monotonic-clock observation and is evidence, not an absolute gate.

| Shape | Input bytes | Lexer token reads | Lexer passes | Callbacks | Allocations | Requested bytes | Peak live bytes | Runtime |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Flat malformed | 131,081 | 65,539 | 2 | 8 | 17 | 17,083,428 | 8,454,146 | 35.8 ms |
| Depth-one malformed | 87,393 | 65,547 | 2 | 8 | 19 | 18,197,612 | 9,568,328 | 44.3 ms |
| Flat malformed | 1,048,577 | 524,291 | 2 | 8 | 19 | 68,332,572 | 34,078,718 | 354 ms |
| Depth-one malformed | 1,048,569 | 786,429 | 2 | 8 | 22 | 149,334,882 | 81,264,460 | 541 ms |
| One-megabyte comment padding | 1,048,576 | 11 | 2 | 8 | 4 | 177,190 | 176,160 | 1.8 ms |

The first Lemon pass stops near the leading syntax defect. The second lexer
pass still scans the entire input to populate recovery tokens. The shallow
shape then allocates parenthesis indexes even though no callback can handle
the statement.

## Scope

This feature covers:

- shared retry tokenization and its capacity growth;
- retry callback dispatch and nested Lemon retry parses;
- parenthesis matching, predecessor, flag, and depth indexes;
- callback-local predicate flags and retry descriptor vectors;
- deterministic counters for tokens, lexer passes, callbacks, workspace, and
  budget exhaustion;
- direct parse, execute, buffered execute, and prepare behavior when a retry
  budget is exhausted;
- scaling tests through at least 65,536 tokens and one MiB;
- parser fuzzing at and beyond each recovery boundary.

This feature does not:

- cap statements accepted by the primary Lemon grammar;
- replace the public `max_allowed_packet` compatibility surface;
- change the 512-entry Lemon parser stack contract (`SQL-05`);
- repair retry-produced payload span rebasing (`SQL-03`);
- change syntax-diagnostic truncation or wording (`SQL-04`);
- remove or reorganize retry recognizers (`ARCH-03`);
- introduce public ABI, serialized-format, dependency, or SQLite changes.

## Resource Model

Recovery uses the following fixed limits:

| Resource | Limit |
| --- | ---: |
| Stored retry tokens | 65,536 |
| Indexed parenthesis depth | 512 |
| Retry callbacks | 8 |
| Total lexer passes, including the primary pass | 4 |
| Absolute live retry workspace | 8 MiB |

The statement-local live workspace allowance is:

```text
min(8 MiB, max(256 KiB, 96 * input_bytes))
```

All multiplication and addition use checked `size_t` arithmetic. Overflow
saturates at the absolute limit rather than wrapping.

The workspace covers the shared token vector, parenthesis indexes and flags,
predicate-context flags, and retry descriptor vectors. It excludes the fixed
Lemon stack, the caller-owned input, and nodes retained in a successful output
AST. Those objects already have independent ownership and are not transient
recovery amplification.

The token limit admits the review requirement's 65,536-token boundary. Since a
stored token is at least one source byte, both token work and the workspace
allowance remain proportional to input. Statements with few large tokens or
comments remain byte-linear and use constant recovery workspace.

## Work Model

One primary Lemon pass is always permitted. A recoverable result may add:

1. one complete shared retry tokenization pass;
2. at most eight callback dispatches over the shared tokens;
3. at most two bounded prefix Lemon passes, for a total of four lexer passes
   including the primary pass.

Every callback scan must be monotone or use precomputed constant-time
parenthesis lookups. A callback may perform at most one full pass for each
declared scan phase; it must not restart a prefix scan per token. The callback
table is the authoritative eight-entry ratchet. Adding a callback, lexer pass,
or scan phase requires a spec change and updated scaling evidence.

The parser result records:

- stored retry token count;
- total lexer-pass count;
- callback and handled-callback counts;
- peak live retry-workspace bytes;
- whether a recovery budget was exhausted.

These are internal source-level observables and do not change ABI.

## Lazy Parenthesis Indexes

Shared retry initialization stores tokens but does not construct a
parenthesis index. A callback first checks its cheap statement prefix and
minimum-token shape. Only a callback that can inspect nested syntax requests
the index.

Index construction is single-shot:

- `unbuilt` means no index memory exists;
- `ready` means matching indexes, predecessor indexes, flags, and maximum
  depth are complete;
- `unavailable` means construction hit a token, depth, workspace, or
  allocation failure and no callback may inspect the partial index.

Allocation failure remains fatal under `SQL-01`. A deterministic resource
limit is recoverable and preserves the original syntax result. Deinitializing
any state releases all partial allocations exactly once.

## Budget Exhaustion And Diagnostics

Budget exhaustion is not allocation failure:

- direct parsing returns `MYLITE_SQL_PARSE_SYNTAX_ERROR`;
- execution and prepare return the existing MySQL-compatible parse error
  `1064` / `42000`;
- an initial syntax error retains its original error token and message;
- an initially successful unsupported-utility AST is replaced by a syntax
  error at the first token that could not be admitted;
- `retry_budget_exhausted` is true and later callbacks do not run;
- `MYLITE_NOMEM` / `HY001` remains reserved for actual allocation failure;
- the connection and statement cleanup contract remains unchanged.

This intentionally differs from MySQL only for syntax accepted exclusively by
MyLite's retry layer after a limit is exceeded. Primary-grammar syntax,
including large literals and comments within the packet surface, is not
subject to the recovery limits.

## Grammar

No Lemon grammar change is required. Resource checks and lazy indexes are
implemented around the existing primary grammar and compatibility callbacks.

## Tests

The MySQL 8.4.9 fixture pins:

- the default 64 MiB `max_allowed_packet` readback;
- `1064` / `42000` for 64-token and 65,536-token flat malformed statements;
- `1064` / `42000` for a one-megabyte comment-padded malformed statement;
- successful reuse of the same connection after each error.

Native tests must cover:

- primary-grammar success with no recovery allocation;
- retry success below and at each applicable token, workspace, depth, callback,
  and lexer-pass boundary;
- flat, depth-one, deeply nested, long-token, long-comment, version-comment,
  and prefix-reparse rejection below, at, and above every boundary;
- exactly 65,536 stored tokens and deterministic rejection of token 65,537;
- one-megabyte flat, shallow, and comment-padded inputs;
- stable original syntax tokens and public `1064` / `42000` diagnostics;
- actual allocator failure at every new allocation, still producing
  `MYLITE_NOMEM` / `HY001`;
- complete cleanup and successful subsequent statements;
- SQL modes and parameter-enabled parsing;
- 32-bit overflow-safe calculations and Windows builds.

The structured scaling gate must report input bytes, tokens, lexer passes,
callbacks, allocation count, cumulative requested bytes, peak live bytes,
budget exhaustion, status, and elapsed time at 64, 256, 1,024, 4,096, 16,384,
and 65,536 tokens plus one MiB. It must assert:

- no more than 65,536 stored retry tokens;
- no more than four lexer passes or eight callbacks;
- rejected-input peak live recovery workspace at or below its computed
  statement allowance and 8 MiB absolute cap;
- linear counter growth, using counts rather than hosted-runner time as the
  release gate;
- no generated corpus artifacts in the source tree.

Qualification must include the focused native and structured scaling tests in
Development, Debug-CI, Release, ASan/UBSan, and deterministic allocator
profiles; all parser native suites; the pinned parser MySQL fixtures; at least
10,000 seeded parser-fuzzer executions with inputs above one MiB; formatting,
static analysis, ABI/install-consumer checks, and the production size gate.
