# Baseline Qualified Predicate Columns Tasks

- [x] Specify MySQL-compatible single-source qualifier scope.
- [x] Verify MySQL 8.4.9 behavior for qualified `DELETE` predicate side effects.
- [x] Reuse existing qualified-identifier grammar without adding a new parser
      surface.
- [x] Pass single-table `DELETE` target context into descriptor predicate
      planning.
- [x] Add MyLite runtime coverage for table-qualified and schema-qualified
      single-table `DELETE` predicates.
- [x] Add MySQL expectation coverage for qualified predicate side effects.
- [x] Update compatibility documentation for the narrower remaining expression
      gaps.
