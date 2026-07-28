# Bounded Growable Parser Stack

## Status

Specified; implementation and release qualification are pending.

## Summary

MyLite's generated Lemon parser currently embeds a fixed 512-entry stack in
every parser object. The present grammar makes each stack entry 344 bytes on
the qualified x86-64 Debug build, so even a trivial statement allocates a
parser object of about 176 KiB. Despite that fixed cost, valid scalar syntax
reaches the ceiling quickly: 507 nested expression parentheses parse, while
508 report `MYLITE_SQL_PARSE_STACK_OVERFLOW`; 84 nested `IF()` calls parse,
while 85 overflow.

MySQL 8.4.9 accepts substantially deeper valid expressions. On the pinned
runtime with a 1 MiB `thread_stack`, 16,384 nested expression parentheses and
1,732 direct nested `IF()` calls succeed. The next `IF()` depth fails only
because MySQL's recursive expression handling reaches the configured server
thread stack.

This feature replaces MyLite's large fixed parser stack with a small embedded
stack that grows geometrically on demand. Total storage reserved for the
embedded and growable Lemon stack is bounded by eight MiB. Allocation failure
and the configured ceiling remain distinct internal outcomes.

## Sources

- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- MyLite grammar and parser driver:
  `packages/libmylite/src/sql/mylite_parse.y` and
  `packages/libmylite/src/sql/mylite_parser.c`
- MyLite parser resource contracts:
  `packages/libmylite/src/sql/mylite_parser_resources.h`
- Existing nested-`IF()` corpus feature:
  `docs/specs/parser-corpus-nested-if-stack/specs.md`
- Bundled public-domain Lemon generator and template:
  `third_party/sqlite/upstream/tool/lemon.c` and
  `third_party/sqlite/upstream/tool/lempar.c`
- Follow-up review finding `SQL-05`:
  `docs/architecture/review-2026-07-followup-remediation-plan.md`
- MySQL 8.4 expressions:
  <https://dev.mysql.com/doc/refman/8.4/en/expressions.html>
- MySQL 8.4 flow-control functions:
  <https://dev.mysql.com/doc/refman/8.4/en/flow-control-functions.html>
- MySQL 8.4 `parser_max_mem_size`:
  <https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html>
- MySQL 8.4.9 observations pinned by
  `packages/libmylite/tests/mysql_bounded_growable_parser_stack_expectations.sh`.

The specification is independently authored from public documentation,
observed MySQL behavior, MyLite source, and the public-domain Lemon generator.
It does not use MySQL parser or server implementation source.

## Observed MySQL 8.4.9 Behavior

The pinned runtime reports:

```text
VERSION()             8.4.9
lc_messages           en_US
thread_stack          1048576
parser_max_mem_size   18446744073709551615
```

The default `parser_max_mem_size` places no configured parser-memory ceiling
on this 64-bit runtime. The documented variable is configurable and MySQL's
observable nesting boundary also depends on `thread_stack`; neither value
defines a portable SQL-language nesting limit.

For generated, valid, tableless `SELECT` expressions:

- 16,384 nested parentheses execute and prepare successfully;
- 32,768 nested parentheses return `3950` / `HY000` / `Out of memory`;
- 1,732 direct nested `IF(1, 1, ...)` calls execute successfully;
- depth 1,733 returns `1436` / `HY000` with a thread-stack-overrun message;
- a prepared nested `IF()` expression succeeds at depth 1,024;
- the connection remains usable after both resource failures.

The accepted/rejected boundary is an observation of the pinned runtime, not a
portable promise for every MySQL build or server configuration.

## Pre-Remediation MyLite Baseline

MyLite's grammar sets:

```lemon
%stack_size 512
```

The bundled Lemon generator therefore embeds 512 entries in every parser
object and disables its growable-stack path. The qualified x86-64 build has:

```text
sizeof(yyStackEntry) = 344
sizeof(yyParser)     = 176160
```

The exact valid-expression boundaries are:

| Shape | Highest accepted depth | First stack overflow |
| --- | ---: | ---: |
| Parenthesized integer | 507 | 508 |
| Nested `IF(1,1,...)` | 84 | 85 |
| Corpus-shaped `IF((ROUND(1,2)=1),2,...)` | 84 | 85 |

The stack-overflow callback clears the Lemon stack and records
`MYLITE_SQL_PARSE_STACK_OVERFLOW`. Public execution and prepare surfaces map
that internal status to a bounded `1064` / `42000` syntax diagnostic.

## Scope

This feature covers:

- the Lemon stack used by primary grammar parses and every retry grammar pass;
- valid nested expression parentheses;
- valid nested `IF()` calls, including the prior corpus shape;
- direct execution, streaming prepare, and buffered prepare;
- a small common-case embedded stack;
- geometric stack growth with a fixed byte ceiling;
- allocation-failure precedence and cleanup;
- deterministic ceiling behavior and connection reuse;
- stack-growth measurements in internal parse results.

This feature does not:

- change accepted grammar productions or expression semantics;
- change the separate 512-level retry parenthesis-index limit;
- make MySQL's configurable thread-stack boundary a fixed compatibility
  contract;
- expose parser limits through public ABI or a system variable;
- change the `.mylite` format, SQLite, dependencies, or generated files
  checked into source control;
- remove or reduce the retry recognizer (`ARCH-03`).

## Parser Stack Contract

The grammar uses the bundled Lemon growable-stack directives:

```lemon
%stack_size MYLITE_SQL_PARSER_STACK_INITIAL_ENTRY_COUNT
%stack_size_limit MYLITE_SQL_PARSER_STACK_SIZE_LIMIT
%realloc mylite_sql_parser_stack_reallocate
%free mylite_sql_parser_stack_free
%extra_context { struct mylite_sql_parser_state *state }
```

`MYLITE_SQL_PARSER_STACK_INITIAL_ENTRY_COUNT` is 64. This storage remains
embedded in the parser object, so ordinary statements require no separate
stack allocation.

The combined embedded and growable stack-storage ceiling is
`mylite_sql_parser_stack_byte_limit`, eight MiB. The generated size-limit
expression derives its entry ceiling from `sizeof(yyStackEntry)` and subtracts
the embedded entries. Grammar changes that alter the semantic-value union
therefore cannot silently raise the byte ceiling.

Lemon grows the active backing array geometrically. At most one growable
backing allocation is live for a parser, and it is released during parser
finalization. Retries are sequential, so their Lemon stacks do not accumulate.

## Allocation And Failure Semantics

The parser driver supplies MyLite-owned realloc/free callbacks with the parser
state as Lemon's extra context.

On successful growth, the parse result records:

- the number of successful stack growth operations;
- the largest growable backing allocation in bytes.

The recorded peak must not exceed the growable portion of the eight-MiB
ceiling.

If `realloc()` fails:

1. the callback records `MYLITE_SQL_PARSE_NOMEM`;
2. Lemon unwinds its existing stack;
3. the stack-overflow callback does not replace the fatal status;
4. parser and AST cleanup release every retained allocation;
5. public execution and prepare return `MYLITE_NOMEM` with `HY001`.

If Lemon reaches the configured entry ceiling without allocation failure, the
existing stack-overflow callback records
`MYLITE_SQL_PARSE_STACK_OVERFLOW`. Public surfaces continue to return
`MYLITE_ERROR` with a bounded `1064` / `42000` diagnostic. This is an explicit
MyLite resource limit rather than a claim that MySQL uses the same boundary or
diagnostic.

## Performance And Memory

The common parser object falls from 512 to 64 embedded entries. With the
current 344-byte entry, embedded stack storage falls from 176,128 bytes to
22,016 bytes. The exact sizes remain generated-layout details; the entry
count and byte ceiling are the stable MyLite policy.

Statements whose peak fits the embedded stack perform no stack reallocations.
Deeper statements pay only for the backing array sizes they reach. Growth is
geometric rather than per-token, preserving linear parser work while avoiding
the previous large fixed cost.

The eight-MiB stack ceiling is independent of the eight-MiB retry-workspace
ceiling. The former bounds one active Lemon parser; the latter bounds token
indexes and other retry-recognizer workspace.

## Tests

The MySQL 8.4.9 fixture pins:

- version, English locale, `thread_stack`, and `parser_max_mem_size`;
- direct success for 16,384 parentheses and 1,732 nested `IF()` calls;
- prepare/execute success for 16,384 parentheses and 1,024 nested `IF()`
  calls;
- `3950` / `HY000` resource failure at 32,768 parentheses;
- `1436` / `HY000` thread-stack failure at 1,733 direct `IF()` calls;
- connection reuse after every resource failure.

Native parser tests must cover:

- no stack growth for representative shallow SQL;
- the old boundaries at 507/508 parentheses and 84/85 nested `IF()` calls;
- successful parentheses at depths 1,024, 4,096, and 16,384;
- successful nested `IF()` at depths 512, 1,024, and 1,732;
- the corpus-shaped nested `IF()` expression beyond its old boundary;
- positive growth counts and bounded peak bytes;
- deterministic stack overflow beyond the eight-MiB ceiling;
- parser reuse after a ceiling failure.

Native runtime tests must cover direct execution, streaming prepare, buffered
prepare, exact result values, public diagnostics at the MyLite ceiling, and
connection reuse.

The deterministic allocator profile must sweep every allocation in a
multi-growth nested expression and require `MYLITE_SQL_PARSE_NOMEM` /
`MYLITE_NOMEM`, `HY001`, and leak-free cleanup at every injected stack-growth
failure.

Qualification must include Development, Debug-CI, Release, ASan/UBSan, and
deterministic allocator profiles; all parser and affected runtime suites; all
pinned parser MySQL fixtures; parser fuzzing; host Lemon generation;
formatting; full static analysis; ABI/install-consumer checks; and the
production size gate.
