# Parser Corpus Benchmark CSV Loader Tasks

- [x] Inspect current failure dump and identify row-decoding artifacts.
- [x] Specify corpus quote handling for doubled quotes and MySQL backslash
  quotes inside quoted multiline rows.
- [x] Extract the benchmark CSV loader into a small benchmark-local module.
- [x] Add C coverage for doubled quotes, backslash quotes, multiline rows,
  plain rows, and blank rows.
- [x] Run focused build/tests, rerun the corpus parse benchmark, review, commit,
  and push.
