# Benchmarks

Results through the Lookup section were collected on 2026-09-25 on an Apple
M5 Max:

- 40-core Apple GPU, Metal 4
- 18 CPU cores (12 performance, 6 efficiency)
- macOS 26.6
- Apple Clang 21.0
- Release build

The AVX-512 section records a separate host. Commands are shown so the same
workload can be repeated on CUDA hardware. Times are medians of five runs
unless stated otherwise. Compression includes validation, allocation, GPU
submission, synchronization, and canonical blob construction.
`device_seconds` is Metal command-buffer GPU time.

## Correctness

```sh
ctest --test-dir build --output-on-failure
./build/benes-tests --extended
```

Both passed. Coverage includes all permutations through size 8; identity,
reverse, structured, and seeded-random inputs around powers of two; forward and
inverse lookup; 1/2/4/8/12-thread canonical CPU output; byte equality between
portable CPU and Metal; golden blobs; malformed data; file round trips; mmap;
cached explicit file reads; custom middle group sizes, cluster compositions,
and middle placements; byte-identical 32-bit/64-bit source encodings; checked
layout arithmetic around `2^32`; sampled lookups in a sparse encoded file with
`2^33` entries; and large non-power-of-two inputs. The final version-1
suite also passed ThreadSanitizer and combined
AddressSanitizer/UndefinedBehaviorSanitizer.

## Compression size

The variable header is excluded from `payload`. `packed` is a tightly packed
array using `ceil(log2(n))` bits per entry. `minimum` is
`ceil(log2(n!)) / 8`.

| n | network | payload | uint32 | packed | minimum |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 31 | 32 | 15 | 124 | 20 | 15 |
| 32 | 32 | 15 | 128 | 20 | 15 |
| 33 | 64 | 16 | 132 | 25 | 16 |
| 63 | 64 | 37 | 252 | 48 | 37 |
| 64 | 64 | 38 | 256 | 48 | 37 |
| 65 | 128 | 39 | 260 | 57 | 38 |
| 96 | 128 | 64 | 384 | 84 | 63 |
| 127 | 128 | 91 | 508 | 112 | 89 |
| 128 | 128 | 91 | 512 | 112 | 90 |
| 129 | 256 | 92 | 516 | 129 | 91 |
| 1,048,575 | 1,048,576 | 2,449,406 | 4,194,300 | 2,621,438 | 2,432,342 |
| 1,048,576 | 1,048,576 | 2,449,408 | 4,194,304 | 2,621,440 | 2,432,345 |

Version 1 stores exactly `ceil(log2(c!))` bits for each populated middle group
of `c` entries and rounds only the three complete logical streams. It reports the
logical power-of-two network width, but unreachable padded subtrees and odd
unmatched switches occupy no payload bytes. Crossing a power-of-two boundary
therefore has no padding cliff.

Reproduce:

```sh
for n in 31 32 33 63 64 65 96 127 128 129 1048575 1048576; do
  ./build/benes-benchmark cpu "$n" 18 3
done
```

## Portable CPU thread scaling

One seeded random permutation of 1,048,575 entries using the compact recursive
encoding:

| threads | seconds | million entries/s | speedup |
| ---: | ---: | ---: | ---: |
| 1 | 0.2575 | 4.07 | 1.00x |
| 2 | 0.1672 | 6.27 | 1.54x |
| 4 | 0.1026 | 10.22 | 2.51x |
| 8 | 0.0781 | 13.43 | 3.30x |
| 18 | 0.0672 | 15.60 | 3.83x |

The portable encoder recursively divides independent child networks among
threads. It continues improving through all 18 cores. Root routing work and
memory bandwidth limit ideal linear scaling, but this deliberately simple
backend provides the canonical byte-for-byte reference for accelerated paths.

```sh
for t in 1 2 4 8 18; do
  ./build/benes-benchmark cpu 1048575 "$t" 5
done
```

### 64-bit source overhead

The optimized 32-bit route remains separate from the true 64-bit route. A
nine-run median comparison at 1,048,575 entries and 18 threads measured:

| build/path | seconds | million entries/s | relative to current 32-bit |
| --- | ---: | ---: | ---: |
| prior root commit, 32-bit | 0.074232 | 14.13 | 1.00x |
| current, 32-bit | 0.074113 | 14.15 | 1.00x |
| current, 64-bit | 0.102672 | 10.21 | 0.72x |

The common 32-bit path showed no measurable regression. The 64-bit path was
38.5% slower on this bounded workload, principally because its permutation,
inverse, routing-neighbor, and child arrays use twice the memory bandwidth.
It emits the same 2,449,430-byte blob.

```sh
./build/benes-benchmark cpu 1048575 18 9
./build/benes-benchmark cpu64 1048575 18 9
```

## Metal scaling

The recursive Metal path batches all nodes at each depth and performs
inverse construction, parity union-find, odd-tail orientation, switch packing,
routing, and exact-width middle-permutation coding on the GPU. Median results
from five runs:

| n | backend | wall seconds | device seconds | million entries/s |
| ---: | --- | ---: | ---: | ---: |
| 43,690 | Metal | 0.003750 | 0.002740 | 11.65 |
| 1,048,575 | portable CPU, 18 threads | 0.067202 | — | 15.60 |
| 1,048,575 | Metal | 0.012948 | 0.005558 | 80.99 |
| 1,048,576 | Metal | 0.014568 | 0.005486 | 71.98 |

At 1,048,575 entries Metal is 5.19x faster by wall time than all 18 CPU cores
and 19.9x faster than one CPU thread while producing byte-identical output.
The neighboring power-of-two case has essentially identical device time, so
non-power-of-two routing does not lose GPU parallelism or pay for padded
entries.

```sh
for n in 43690 1048575 1048576; do
  ./build/benes-benchmark cpu "$n" 18 5
  ./build/benes-benchmark metal "$n" 18 5
done
```

GPU generation uses 96 bytes of routing metadata per internal node and final
group.
At one million entries that flattened metadata is about 6 MiB. A measured
64-byte alternative saved about 2 MiB but increased device time from 5.5 ms to
7.1 ms, so the precomputed packet offsets were retained. A future streaming or
formula-only descriptor scheme could reduce large-input memory without paying
that repeated address-calculation cost.

Only Command Line Tools are installed on this machine, so Xcode's Metal System
Trace occupancy counters were unavailable. Device timestamps, equal
power/non-power behavior, and the 19.9x single-core speedup establish broad
parallel use; an occupancy-counter capture remains desirable with full Xcode.

The true 64-bit Metal source/index path was also measured with nine-run
medians at 1,048,575 entries:

| path | wall seconds | device seconds | million entries/s |
| --- | ---: | ---: | ---: |
| Metal, 32-bit | 0.012152 | 0.005213 | 86.29 |
| Metal, 64-bit | 0.016224 | 0.009304 | 64.63 |
| Metal, 64-bit with bit-63 parents forced | 0.021428 | 0.012072 | 48.93 |

The 64-bit descriptor and permutation path is 1.34x slower by wall time. It
normally uses parallel 32-bit parent words at routing levels whose node-local
indexes fit below bit 31, while retaining 64-bit global offsets. Levels that
genuinely exceed that range use lock-backed bit-63 parent words, still with one
pair per GPU work item. `BENES_METAL_FORCE_PARENT64=1` exercises that fallback
on bounded inputs; the full test suite passes in both modes.

```sh
./build/benes-benchmark metal64 1048575 18 9
BENES_METAL_FORCE_PARENT64=1 \
  ./build/benes-benchmark metal64 1048575 18 9
```

`/usr/bin/time -l` reported 79.5 MB maximum resident set size for the
18-thread portable compressor at 1,048,575 entries and 50.1 MB host RSS for
Metal. The latter also reported a 179 MB peak footprint including driver/GPU
resources. These commands include the benchmark's full forward/inverse
verification pass; the timed compression measurements above do not.

## Lookup

1,048,575 entries and 100,000 seeded random lookups:

| middle placement | memory forward | memory inverse | cached file forward | cached file inverse |
| --- | ---: | ---: | ---: | ---: |
| separate | 3,145 ns | 3,142 ns | 9,459 ns | 9,553 ns |
| embedded input | 3,171 ns | 3,211 ns | 8,877 ns | 8,913 ns |
| embedded output | 3,215 ns | 3,185 ns | 8,818 ns | 8,902 ns |

```sh
for layout in separate input output; do
  ./build/benes-lookup-benchmark 1048575 100000 "$layout"
done
```

The explicit file reader avoids loading or mapping the entire blob, but each
lookup follows data-dependent packets. mmap remains preferable for high-rate
random lookup. Embedded placement improved this cached-file microbenchmark,
but it is a logical-locality option, not a guarantee about physical page reads.

## AVX-512

Collected on 2026-10-20 on an 11th Gen Intel Core i9-11900K at 3.50 GHz:

- 8 cores, 16 hardware threads, AVX-512F/BW/VL
- Ubuntu 22.04.5 LTS, kernel 5.15.0-187-generic
- g++ 11.4.0
- Release build with `BENES_ENABLE_AVX512=ON`
- TSC frequency 3.504 GHz, turbo up to 4.6 GHz

AVX-512 keeps portable CPU routing and vectorizes middle-permutation coding.
The benchmark's device column stays at zero because that work runs on the
host. `ctest` and `./build/benes-tests --extended` both passed. Portable
32-bit, portable 64-bit, AVX-512 32-bit, AVX-512 64-bit, and 16-thread AVX-512
outputs were byte-identical for every size in the compression table below,
plus 8, 4,097, and 43,690.

Blob sizes match the compression-size table. At 1,048,575 entries the blob is
2,449,430 bytes (2,449,406-byte payload). At 1,048,576 entries it is
2,449,432 bytes (2,449,408-byte payload). At 43,690 entries it is 77,165 bytes
(77,141-byte payload).

### Thread scaling

One seeded random permutation of 1,048,575 entries. Five-run medians:

| threads | portable seconds | portable million entries/s | portable speedup | AVX-512 seconds | AVX-512 million entries/s | AVX-512 speedup |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 0.409565 | 2.56 | 1.00x | 0.366627 | 2.86 | 1.00x |
| 2 | 0.226702 | 4.63 | 1.81x | 0.215550 | 4.86 | 1.70x |
| 4 | 0.142678 | 7.35 | 2.87x | 0.131169 | 7.99 | 2.80x |
| 8 | 0.118297 | 8.86 | 3.46x | 0.111237 | 9.43 | 3.30x |
| 16 | 0.102999 | 10.18 | 3.98x | 0.094230 | 11.13 | 3.89x |

AVX-512 is 11.7% faster than the portable encoder at one thread. Both keep
improving through all 16 hardware threads, but the curve flattens after four:
four threads are already 2.80x, and sixteen threads reach 3.89x. A later
16-thread pair at 1,048,576 entries measured 0.103658 s portable and
0.105843 s AVX-512, so the 16-thread gap is within run-to-run noise.

```sh
for t in 1 2 4 8 16; do
  ./build/benes-benchmark cpu 1048575 "$t" 5
  ./build/benes-benchmark avx512 1048575 "$t" 5
done
```

Sixteen-thread medians for the non-power-of-two sizes, five runs:

| n | portable seconds | portable million entries/s | AVX-512 seconds | AVX-512 million entries/s |
| ---: | ---: | ---: | ---: | ---: |
| 43,690 | 0.003956 | 11.04 | 0.004444 | 9.83 |
| 1,048,575 | 0.105901 | 9.90 | 0.103843 | 10.10 |
| 1,048,576 | 0.103658 | 10.12 | 0.105843 | 9.91 |

The 43,690-entry run is only a few milliseconds, so its rate is coarse. The
two neighboring million-entry cases take the same time, matching the size
table's lack of a power-of-two padding cliff.

```sh
for n in 43690 1048575 1048576; do
  ./build/benes-benchmark cpu "$n" 16 5
  ./build/benes-benchmark avx512 "$n" 16 5
done
```

### 64-bit source overhead

Paired five-run medians at 1,048,576 entries and one thread:

| path | seconds | million entries/s | relative to its 32-bit path |
| --- | ---: | ---: | ---: |
| portable, 32-bit | 0.409285 | 2.56 | 1.00x |
| portable, 64-bit | 0.445420 | 2.35 | 0.92x |
| AVX-512, 32-bit | 0.368720 | 2.84 | 1.00x |
| AVX-512, 64-bit | 0.408820 | 2.56 | 0.90x |

An earlier five-run AVX-512 pair on this host was 0.365730 s and 0.412088 s.
The 64-bit path is 8.8% slower for the portable encoder and 10.9% slower for
AVX-512. Both widths emit the same 2,449,432-byte blob.

Nine-run medians at 1,048,575 entries and 16 threads:

| path | seconds | million entries/s | relative to its 32-bit path |
| --- | ---: | ---: | ---: |
| portable, 32-bit | 0.115183 | 9.10 | 1.00x |
| portable, 64-bit | 0.134006 | 7.82 | 0.86x |
| AVX-512, 32-bit | 0.110921 | 9.45 | 1.00x |
| AVX-512, 64-bit | 0.127765 | 8.21 | 0.87x |

At 16 threads the 64-bit path is 16.3% slower for portable CPU and 15.2%
slower for AVX-512. The blob is again identical, 2,449,430 bytes.

```sh
./build/benes-benchmark cpu 1048576 1 5
./build/benes-benchmark cpu64 1048576 1 5
./build/benes-benchmark avx512 1048576 1 5
./build/benes-benchmark avx51264 1048576 1 5
./build/benes-benchmark cpu 1048575 16 9
./build/benes-benchmark cpu64 1048575 16 9
./build/benes-benchmark avx512 1048575 16 9
./build/benes-benchmark avx51264 1048575 16 9
```

### Cycles, instruction mix, and bandwidth

`perf stat -e cycles,instructions` covered one warmup compression plus five
measured compressions of 1,048,575 entries. Core cycles per entry below
attribute the process cycle count by the median compression's share of
elapsed time. The TSC on this host runs at 3.504 GHz while the core turbos
near 4.6 GHz, so TSC ticks per entry are lower than core cycles per entry.

| path | threads | core cycles/entry | instructions/cycle | TSC cycles/entry |
| --- | ---: | ---: | ---: | ---: |
| portable | 1 | 1,799 | 1.61 | 1,371 |
| AVX-512 | 1 | 1,600 | 1.75 | 1,219 |
| AVX-512 | 16 | 444 wall, 2,221 aggregate | 1.23 | 334 wall |

A `cycles:u` profile of one-thread AVX-512 compression puts 91.35% of user
cycles in the portable recursive router `encode_compact_node` and 0.84% in
`encode_factoradic_avx512`. That helper is a ZMM integer loop
(`vpcmpnleud`, masked `vpsubd`, `vpbroadcastd`, `vpaddd`) followed by scalar
128-bit factorial accumulation. The modest one-thread speedup follows from
how little of the runtime that loop is.

System-wide IMC counters during the same six-compression process, after
subtracting a 127.02 MiB/s read and 38.02 MiB/s write idle baseline:

| path | threads | DRAM read | DRAM write | average bandwidth |
| --- | ---: | ---: | ---: | ---: |
| AVX-512 | 1 | 313 MiB | 358 MiB | 306 MiB/s |
| AVX-512 | 16 | 1,085 MiB | 992 MiB | 3,179 MiB/s |

One thread retires about 31,000 demand L3 load misses per compression
(`mem_load_retired.l3_miss`) and about 1.26 million `longest_lat_cache.miss`
events. Sixteen threads retire about 87,000 demand L3 load misses and about
3.17 million `longest_lat_cache.miss` events per compression. The one-thread
rate is far below this CPU's DRAM ceiling; the encoder is limited by the
scalar routing work shown in the profile.

`/usr/bin/time -v` on the full benchmark process, including its forward and
inverse check of every entry, reported 112,248 kB maximum resident set for
16-thread AVX-512 at 1,048,576 entries and 100,248 kB for the portable
encoder. That wall time is dominated by the serial check. The compression
medians above exclude it.

### Lookup

1,048,575 entries and 100,000 seeded random lookups. The tool reports one
pass per mode. Compression uses the automatic backend, which selects AVX-512
here. The timed lookups stay on the portable CPU path. The file was written
immediately before the timed passes, so cold and warm file times agree and
both are page-cache hits.

| middle placement | memory forward | memory inverse | mmap forward | mmap inverse | file forward | file inverse |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| separate | 3,879.39 ns | 3,896.59 ns | 3,884.13 ns | 3,892.65 ns | 11,635.4 ns | 11,474.5 ns |
| embedded input | 3,918.23 ns | 3,933.61 ns | 3,934.64 ns | 3,938.11 ns | 11,251.4 ns | 11,686.8 ns |
| embedded output | 3,939.27 ns | 3,923.16 ns | 3,919.39 ns | 3,925.74 ns | 11,126.4 ns | 11,135.9 ns |

File columns are the warm pass. Embedded output is the fastest file case on
this host, by a few hundred nanoseconds. Memory and mmap agree. A separate
1,048,576-entry run with the default layout landed in the same range: about
3.77 µs in memory and about 11.6–12.3 µs from the file reader.

```sh
for layout in separate input output; do
  ./build/benes-lookup-benchmark 1048575 100000 "$layout"
done
./build/benes-lookup-benchmark 1048576 100000
```

## Remaining CUDA runs

CUDA has not been executed. Its separate 32-bit and 64-bit descriptors and
buffers, bit-63 atomic parents, grid-stride large-work dispatch, clustered
atomic switch packing, exact-bit middle offsets, and host serialization were
reviewed for parity with the tested Metal path, but no CUDA compiler or
hardware was available. On an NVIDIA machine:

1. Build with the CUDA backend enabled.
2. Run `ctest` and `benes-tests --extended`.
3. Run the exact size, non-power-of-two, and lookup-layout commands in the
   sections above.
4. Require both 32-bit and 64-bit source paths to match the single-thread
   portable bytes. The test suite includes backend-gated compact boundary
   cases through 4,097 entries.
5. Sweep threadblock sizes and capture occupancy, memory throughput, launch
   gaps, and kernel/device time on the selected GPU.

Do not treat source review or cross-compilation as runtime validation.

The x86-64 cross-compile and `otool` inspection command in `README.md` remains
the maintenance check available on Apple Silicon. AVX-512 runtime numbers are
in the section above.
