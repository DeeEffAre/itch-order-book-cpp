# ITCH Order Book Engine (C++)

A single-threaded C++23 order book engine that builds and maintains book state
from a Nasdaq ITCH 5.0 message feed. It processes `Add`/`Cancel`/`Delete`/
`Replace`/`Execute`/`System Event` messages, maintains one book per stock locate,
tracks best bid/ask, and rejects orders outside the active price window. It is a
book builder, not a matcher: there is no crossing or trade generation.

This repository is a C++ port of the original Rust implementation
([`itch-order-book-rs`](https://github.com/DeeEffAre/itch-order-book-rs)).
Both versions implement the same algorithms and data structures and are
benchmarked against each other on the same machine; the few porting differences
are listed in [Notes](#notes).

## Performance

Google Benchmark, Release (`-O3` + LTO, clang), single-threaded, same machine as
the Rust original (Criterion). Warmup 3s + measurement 5s to match Criterion's
methodology. Run-to-run variance in this environment is roughly ±10%, so treat
ratios as the signal rather than absolute numbers. Reproduce with:

```bash
./build/release/bin/matching_engine_bench --benchmark_min_warmup_time=3 --benchmark_min_time=5s
```

Synthetic feeds (`mt19937_64`):

| Benchmark                        | Messages  | C++ (mean) | C++ ns/msg | Rust ns/msg | Result      |
|----------------------------------|-----------|------------|------------|-------------|-------------|
| `mixed/parse_and_dispatch`       | 1,000,000 | 72.5 ms    | ~73        | ~73         | ~same       |
| `parser/parse_and_dispatch_noop` | 1,000,000 | 464.6 µs   | ~0.46      | ~0.72       | 1.6x faster |
| `single_type/add`                | 200,000   | 6.46 ms    | ~32        | ~18         | 1.8x slower |
| `single_type/cancel`             | 200,000   | 9.58 ms    | ~48        | ~76         | 1.6x faster |
| `single_type/delete`             | 200,000   | 29.58 ms   | ~148       | ~89         | 1.7x slower |
| `single_type/replace`            | 200,000   | 42.21 ms   | ~211       | ~177        | 1.2x slower |
| `single_type/execute`            | 200,000   | 9.31 ms    | ~47        | ~81         | 1.7x faster |
| `depth/shallow`                  | 1,000,000 | 70.3 ms    | ~70        | ~69         | ~same       |
| `depth/medium`                   | 1,000,000 | 73.6 ms    | ~74        | ~71         | ~same       |
| `depth/deep`                     | 1,000,000 | 78.6 ms    | ~79        | ~74         | ~same       |

Real ITCH replay (2,535,085 messages, 1,064 locates):

| Benchmark                 | Messages  | C++ (mean) | C++ ns/msg | Rust ns/msg | Result |
|---------------------------|-----------|------------|------------|-------------|--------|
| `replay/book_full_stream` | 2,535,085 | 317.1 ms   | ~125       | ~123        | ~same  |
| `replay/parser_only_noop` | 2,535,085 | 5.97 ms    | ~2.4       | ~2.0        | ~same  |

Takeaways: the two implementations are at parity on the real ITCH replay and on
the mixed/depth workloads. C++ leads on the parser and on `cancel`/`execute`;
it trails on `add`/`delete`/`replace` (~1.2-1.8x), which have not had
profile-guided work yet.

## Design

- **Fixed price-level arrays + two-level bitmap.** Each book holds two arrays of
  `MAX_PRICE` (100k) `PriceLevel` entries. A 100k-bit L1 bitmap plus a 64-bit L2
  summary gives near-O(1) best bid/ask and next-level search without pointer
  chasing; the same bitmap enumerates non-empty levels during a price rebase, so
  rebases never scan the full array. Cost: a fixed ~3.2 MB per book plus ~12.7 KB
  of bitmaps per side.
- **Price offset sliding instead of shifting.** Levels are indexed by
  `tick = price / TICK_SIZE` rebased against a sliding offset; a new low resets the
  levels and re-places active orders (O(active orders + non-empty levels)).
  Rejected: `std::map` / B-tree keyed by price, because hot-path traversal costs
  more than the occasional rebase.
- **Slab storage + intrusive doubly-linked lists.** Orders live in a single
  `std::vector<Order>` slab with a free list; each price level is an intrusive
  list over slab indices. No per-order allocation and no separate container per
  level. `Order` is 32-byte aligned and holds only id, price, shares, side, prev
  and next.
- **Direct-indexed order map and per-locate books.** `order_ref -> (locate, slab
  index)` lives in a flat vector for O(1) cancel/delete/replace/execute lookups;
  books are a vector of `optional<OrderBook>` indexed by stock locate, and system
  events skip locates that have no book yet.
- **Zero-copy parser.** Wire structs are `#pragma pack(1)` views over the raw
  buffer (`Message<Derived>::new_msg` bounds-checks and `reinterpret_cast`s);
  big-endian fields are byte-swapped lazily via `U16Be`/`U32Be`/`U48Be`/`U64Be`.
  Dispatch is a compile-time `ItchParser<Handler>` switch with no virtual calls.
- **Single-threaded by design.** No synchronization on the hot path. A
  feed-handler to strategy hand-off would be a separate, explicitly measured
  component (lock-free SPSC, thread pinning, latency percentiles), not an
  implicit addition.

## Layout

```
include/matching_engine/
  itch_parser/   # packed wire messages + zero-copy dispatch
  order_book/    # book, price bitmap, slab, types
  engine/        # per-locate books + global order map
src/             # engine, book and bitset implementations
tests/           # Catch2 unit tests
benchmark/       # Google Benchmark: synthetic feeds + real ITCH replay
```

## Build & Run

Requires CMake >= 3.25, Ninja, clang, and vcpkg (`VCPKG_ROOT` set; dependencies: `catch2`, `benchmark`).

```bash
cmake --preset release
cmake --build --preset release --target matching_engine_bench
./build/release/bin/matching_engine_bench --benchmark_min_warmup_time=3 --benchmark_min_time=5s
```

The replay benchmark reads a length-prefixed Nasdaq ITCH 5.0 binary file. Point
`ITCH_FILE` at one, or place it at the default path
`$HOME/data/itch/itch_subset.bin`.

## Tests

Catch2 unit tests cover the price bitmap and the order book (add/cancel/delete/
replace/execute, best-price tracking, price-offset rebasing, out-of-band
rejection, system events).

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

## Scope & Limitations

- Nasdaq ITCH 5.0 subset only (`A/F/X/D/U/E/C/S`); no MoldUDP64 framing, sequence
  numbers, or gap recovery.
- Book state only: no matching/crossing, no strategy or risk layer, no FIX/OUCH
  gateway.
- Single-threaded; no lock-free hand-off.
- Fixed 100k-tick price window and fixed per-book arrays: replaying 1,064 locates
  peaks at ~3.3 GB RSS.
- `shift_prices_down` resets both level arrays per rebase; in the ITCH subset this
  happens ~1,650 times (sparse real-data books).
- Synthetic feeds use `mt19937_64` while the Rust original uses ChaCha12:
  statistically equivalent, not bit-identical workloads.

## Notes

- **Porting.** The wire layout, data structures and benchmark generator follow the
  Rust implementation closely. Known differences:
  - The C++ build uses `-O3` + LTO; the Rust bench profile does not enable LTO.
  - `Order` is 32 bytes in both; C++ additionally forces 32-byte alignment
    (`alignas(32)`), while Rust uses its natural 8-byte alignment.
  - Synthetic feeds use `mt19937_64` (plus a separate prefill RNG) instead of the
    Rust `StdRng` (ChaCha12): same distributions, different exact messages.
  - Rust `GlobalMap` bounds-checks lookups (`Vec::get`) and only registers orders
    the book accepted; the C++ port indexes directly and registers
    unconditionally. Equivalent on well-formed feeds, but less defensive.
  - During a price rebase, Rust collects active orders by scanning the slab with a
    free-list mask; C++ enumerates non-empty levels through the price bitmap. Both
    avoid touching empty levels; the C++ output also preserves exact queue order.
- **Warmup matters.** Without `--benchmark_min_warmup_time`, Google Benchmark
  reported up to ~1.8x slower numbers for depth/mixed, and two identical configs
  differed by ~12% run-to-run; with a Criterion-like warmup the gap shrinks.
- **Measurement variance** is roughly ±10% in this environment; compare
  back-to-back runs rather than historic numbers, and prefer ratios.
- **Next targets:** profile `add`/`delete` with `perf` (suspects: cache behavior
  around the 32-byte `Order`, per-add global-map growth).
