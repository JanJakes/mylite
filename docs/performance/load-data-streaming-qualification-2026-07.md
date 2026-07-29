# LOAD DATA Streaming Qualification, July 2026

## Status

Implementation evidence is complete, but the performance qualification did
not pass. The streaming reader meets its allocation, memory, correctness, and
indexed absolute-time gates. The zero-index improvement is 8.2% at 100K rows
and 9.0% at 1M rows, below the specified 15% 100K requirement. The 100K
zero-index timing matrix is also too noisy for qualification.

The implementation remains a measured improvement and removes row-linear
allocation growth. `PERF-03` remains open pending either another demonstrated
optimization or a new specification justified independently of this result.

## Revisions and Method

The paired timing comparison used:

- baseline revision `ce436c103527ac2f7d6ef33ee49798217445d83e`;
- candidate revision `fcfe305f7` with the streaming implementation from
  `0915111dc`;
- profiling and RSS revision
  `473c73c43fcb8a8bdd90fc9f03319dd5e5d2a76f`;
- profiling-disabled Release clients for timing and a separate
  profiling-enabled Release client for counters;
- Linux 6.12.96 on an 18-vCPU AMD EPYC KVM guest;
- logical CPU 0 affinity;
- five measured samples after one warmup;
- fresh rollback state and the same generated input for MyLite and bundled
  SQLite;
- ABBA revision order for every row-count/index-count pair.

The timing harness validates affected rows and an identical result hash for
MyLite and SQLite before emitting a sample. All 160 measured engine samples
passed.

Each reported revision value below is the median of its forward and reverse
run medians. The normalized comparison divides each MyLite run median by its
paired SQLite run median before combining the two revision runs.

## Paired Release Timing

| Rows | Indexes | Baseline MyLite | Candidate MyLite | Absolute change | Baseline ratio | Candidate ratio | Normalized change |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 100K | 0 | 262.007 ms | 240.626 ms | 8.160% faster | 2.319x | 2.099x | 9.487% better |
| 1M | 0 | 2.648 s | 2.410 s | 9.011% faster | 2.307x | 2.064x | 10.535% better |
| 100K | 5 | 1.460 s | 1.372 s | 5.997% faster | 1.266x | 1.119x | 11.624% better |
| 1M | 5 | 37.096 s | 36.723 s | 1.004% faster | 1.041x | 1.116x | 7.155% worse |

Neither indexed MyLite workload regressed in absolute wall time. The 1M
indexed normalized result disagrees because the candidate-period SQLite median
was 7.6% faster than the baseline-period SQLite median while MyLite was nearly
flat. The report retains both views rather than selecting the favorable one.

The individual run medians were:

| Rows | Indexes | Run | MyLite | SQLite | Ratio |
| ---: | ---: | --- | ---: | ---: | ---: |
| 100K | 0 | Baseline A | 234.726 ms | 104.149 ms | 2.254x |
| 100K | 0 | Candidate A | 247.317 ms | 111.615 ms | 2.216x |
| 100K | 0 | Candidate B | 233.935 ms | 118.064 ms | 1.981x |
| 100K | 0 | Baseline B | 289.287 ms | 121.375 ms | 2.383x |
| 1M | 0 | Baseline A | 2.548 s | 1.111 s | 2.293x |
| 1M | 0 | Candidate A | 2.323 s | 1.211 s | 1.918x |
| 1M | 0 | Candidate B | 2.496 s | 1.130 s | 2.209x |
| 1M | 0 | Baseline B | 2.749 s | 1.184 s | 2.321x |
| 100K | 5 | Baseline A | 1.586 s | 1.179 s | 1.345x |
| 100K | 5 | Candidate A | 1.371 s | 1.237 s | 1.109x |
| 100K | 5 | Candidate B | 1.373 s | 1.216 s | 1.129x |
| 100K | 5 | Baseline B | 1.333 s | 1.123 s | 1.187x |
| 1M | 5 | Baseline A | 37.951 s | 35.685 s | 1.064x |
| 1M | 5 | Candidate A | 37.466 s | 33.342 s | 1.124x |
| 1M | 5 | Candidate B | 35.981 s | 32.492 s | 1.107x |
| 1M | 5 | Baseline B | 36.240 s | 35.578 s | 1.019x |

The maximum within-run median absolute deviation was 12.116% for baseline
MyLite and 11.060% for candidate MyLite in the 100K zero-index matrix. Those
values exceed the project's 10% timing-noise ceiling. All other matrices had a
maximum engine MAD of 8.220% or less.

## Allocation and Shape Evidence

The focused profiling client uses a file-backed database, validates four exact
aggregates after every import, rolls the transaction back, and verifies that
the table is empty. Its 100K-row medians were:

| Shape | Indexes | Input | Time | Throughput | Allocations | Allocation bytes | SQLite steps | Peak RSS |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Three integers | 0 | 1,566,785 B | 232.660 ms | 6.734 MB/s | 12 | 33,016 B | 100,013 | 10.95 MiB |
| Three integers | 5 | 1,566,785 B | 1.445 s | 1.085 MB/s | 20 | 143,992 B | 100,032 | 11.68 MiB |
| 1,024-byte text | 0 | 103,088,895 B | 800.869 ms | 128.721 MB/s | 13 | 38,840 B | 100,012 | 11.63 MiB |
| Sixteen integers | 0 | 9,422,245 B | 648.461 ms | 14.530 MB/s | 30 | 92,984 B | 100,026 | 11.18 MiB |
| Escape-dense text | 0 | 16,688,895 B | 413.433 ms | 40.367 MB/s | 13 | 33,464 B | 100,012 | 11.63 MiB |

Counter timings include instrumentation overhead and are not substituted for
the Release timing table. Every shape is below 64 allocations and 1 MiB of
allocation requests. The narrow zero-index statement is far below both gates.

Cold single-sample narrow imports demonstrate row-count independence:

| Rows | Allocations | Allocation bytes | SQLite steps | Process peak RSS |
| ---: | ---: | ---: | ---: | ---: |
| 100K | 23 | 43,662 B | 100,015 | 10,844 KiB |
| 1M | 23 | 43,662 B | 1,000,015 | 10,964 KiB |

Increasing the input tenfold adds exactly zero MyLite allocations, zero
allocation-request bytes, and 120 KiB of process peak RSS. The 103 MB wide
input also remains near 12 MiB RSS. Input storage is therefore bounded by the
largest retained row shape rather than total file size.

## Verification

The final tree passed:

- 707 of 707 Developer CTest tests;
- 707 of 707 ASan/UBSan CTest tests;
- the profiling Release LOAD DATA runtime, allocation, and benchmark smoke
  tests;
- the MySQL 8.4.9 LOAD DATA runtime expectation fixture;
- compatibility validation for 716 green claims backed by 810 MySQL fixtures;
- the first-party formatting check and changed-source `clang-tidy` checks in
  profiling-disabled and profiling-enabled configurations.

## Gate Decision

| Gate | Result |
| --- | --- |
| Fewer than 64 allocations | Pass |
| Fewer than 1 MiB allocation requests | Pass |
| Allocation count independent of rows | Pass |
| One user SQLite step per row plus fixed metadata | Pass |
| Bounded RSS as input size grows | Pass |
| Exact affected rows and aggregate checksums | Pass |
| No indexed absolute regression above 5% | Pass |
| At least 15% faster at 100K with zero indexes | **Fail: 8.160%** |
| Timing noise at or below 10% MAD | **Fail for 100K zero-index** |

This is a failed qualification, not a reason to weaken the gate after seeing
the result. Further work should profile the remaining uninstrumented parser
and conversion cost on a host that supports sampled call graphs, then optimize
only a demonstrated dominant path.
