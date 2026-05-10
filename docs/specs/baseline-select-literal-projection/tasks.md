# Baseline SELECT Literal Projection Tasks

## Goal

Add the next narrow scalar query slice: `SELECT` projection of supported
decimal integer, `NULL`, `TRUE`, and `FALSE` literals with no table source or
with `FROM DUAL`, preserving existing result, diagnostics, alias, and
statement-context conventions.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-select-literal-projection/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify grammar, no-source/`DUAL` semantics, literal normalization,
     diagnostics, storage/runtime implications, unsupported behavior, and
     performance boundaries.
   - Update `COMPATIBILITY.md` and relevant detailed compatibility docs only
     for the exact partial subset.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script for supported literal
     projection and MySQL-accepted wider forms that remain deferred.
   - Verify result labels, values, warning counts, following `ROW_COUNT()`,
     leading-zero/sign behavior, `ALL`, `DUAL`, aliases, and large integer
     boundary behavior.
   - Treat a missing MySQL 8.4.9 runtime as a blocker for changing
     compatibility expectations.

3. Parser and AST
   - Add signed decimal integer literal expression grammar for `+ integer` and
     `- integer`.
   - Reuse existing literal, unary-expression, select-list, and alias AST nodes.
   - Keep parser code independent from runtime, catalog, storage, and SQLite.
   - Add parser tests for supported signed literal projection syntax and
     rejected unsupported forms where syntax is intentionally not admitted.

4. Runtime execution
   - Extend no-source/`DUAL` scalar select recognition to include admitted
     literal projection expressions.
   - Convert supported literal values to public result cells without using
     SQLite.
   - Normalize decimal integer values, including leading zeros, unary signs,
     signed zero, 81-significant-digit acceptance, and 82-significant-digit
     rejection.
   - Preserve existing scalar/session function, system-variable, aggregate,
     table-backed select, `SELECT ALL`, and alias behavior.
   - Return one row, zero affected rows, warning count zero, and following
     `ROW_COUNT() = -1` for supported statements.

5. Tests
   - Add a fast C test under `packages/libmylite/tests/` and register it with a
     dotted CTest name.
   - Cover no-source and `FROM DUAL`, `ALL`, aliases, values and labels,
     warning/affected/row-count behavior, large integer boundaries, unsupported
     wider projection forms, file-backed preamble preservation, catalog/storage
     nonmutation, and independent handles if the new test creates persistent
     handles.
   - Keep tests deterministic and avoid adding a new test framework.

6. Build integration
   - Add the new test target to `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

7. Verification and review
   - Run `cmake --build --preset dev`.
   - Run the new CTest entry and affected parser/scalar/select entries.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.
   - Review the final diff for architecture boundaries, public ABI stability,
     independently authored grammar/spec text, MySQL 8.4.9 evidence, exact
     supported scope, literal normalization correctness, diagnostics, no
     table-row materialization, file-format safety, compatibility-matrix
     accuracy, and test relevance.

## Out Of Scope

- General expression projection, table-backed literal projection,
  parenthesized literal projection, string/decimal/float/hex/bit values,
  arithmetic, parameters, casts, variables, functions outside existing scalar
  slices, subqueries, predicates, ordering, limits, grouping, joins, CTEs, set
  operations, exact expression metadata, warning/truncation behavior for
  >81-significant-digit integers, and SQLite fork patches.
