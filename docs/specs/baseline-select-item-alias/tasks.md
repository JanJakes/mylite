# Baseline SELECT Item Alias Tasks

## Goal

Add projection aliases to currently supported `SELECT` forms while keeping
descriptor resolution and generated SQLite execution authoritative.

## Tasks

1. Design and expectations
   - [x] Create `docs/specs/baseline-select-item-alias/specs.md`.
   - [x] Record official MySQL 8.4 documentation sources and MySQL 8.4.9
     runtime observations.
   - [x] Specify parser grammar, AST storage, alias decoding, result labels,
     alias-aware order resolution, diagnostics, and unsupported forms.
   - [x] Add MySQL-runtime-verified expectation coverage.
   - [x] Update compatibility docs for the exact limited select-item alias
     subset.

2. Parser and AST
   - [ ] Add select-alias grammar for identifier, quoted identifier, and string
     literal aliases.
   - [ ] Store the alias as optional child `1` of `SELECT_ITEM`, preserving the
     expression as child `0`.
   - [ ] Preserve current default, `ALL`, `DISTINCT`, `DISTINCTROW`, wildcard,
     aggregate, `DUAL`, no-source, table-alias, and qualified-column parsing.
   - [ ] Add parser tests for `AS`, bare, quoted-identifier, string-literal,
     scalar, aggregate, distinct, table-backed, and unsupported star alias
     forms.

3. Runtime execution
   - [ ] Decode alias labels without mutating descriptors.
   - [ ] Use aliases as public result column labels for supported projections,
     aggregates, scalar/session functions, and system variables.
   - [ ] Resolve unqualified table-backed `ORDER BY` keys against projected
     aliases before descriptor columns.
   - [ ] Preserve descriptor authority for table sources, selected values,
     predicates, distinct values, aggregate arguments, fallback order columns,
     and limits.
   - [ ] Preserve generated SQLite identifier quoting and parameter binding.
   - [ ] Preserve result rows, affected rows, warning count, and following
     `ROW_COUNT()` behavior.
   - [ ] Avoid catalog, storage, VFS, or SQLite fork changes.

4. Tests
   - [ ] Add fast C tests under `packages/libmylite/tests/`.
   - [ ] Cover successful aliases across descriptor, `ALL`, `DISTINCT`,
     `DISTINCTROW`, aggregate, scalar/session, no-source, `DUAL`, table-alias,
     schema-qualified, filtered, ordered, and limited forms.
   - [ ] Cover alias labels, order alias shadowing, case-insensitive order
     matching, duplicate order alias ambiguity, `WHERE` ignoring aliases,
     unsupported forms, unknown names, reopen, rename/drop, independent
     handles, result conventions, and file-format preservation.
   - [ ] Keep tests deterministic and avoid a new framework.

5. Verification and review
   - [ ] Run `cmake --build --preset dev`.
   - [ ] Run relevant parser/select/aggregate/scalar lifecycle CTest entries.
   - [ ] Run
     `./packages/libmylite/tests/mysql_baseline_select_item_alias_expectations.sh`.
   - [ ] Run `cmake --workflow --preset check`.
   - [ ] Review final diff for scope control, grammar independence, descriptor
     authority, SQLite pushdown, alias semantics, compatibility docs, and test
     relevance.

## Out Of Scope

Aliases on `SELECT *`, qualified wildcards, alias resolution in `WHERE`, alias
resolution in unsupported grouping/having/window/set-operation clauses, ordinal
ordering, string-literal order expressions, expression order keys, multiple
order keys, general projection expressions, joins, derived tables, CTEs,
protocol metadata changes, storage changes, and SQLite fork patches.
