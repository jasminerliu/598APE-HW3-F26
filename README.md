# CS598APE HW3

## Artifact:

Run on course VM (4 cores).
 
Profiling showed 98-99% of cycles in `stabilize()`, so all of the optimizations are in that function.
 
To reproduce my baseline numbers, check out the original code, rebuild, and run:
```bash
git checkout <BASELINE_COMMIT>
make clean && make all
time ./sandpile.exe 401
cp identity.png identity-baseline.png
md5sum identity.png > baseline.md5
```
 
Every optimization below must produce a bit-identical image, since the sandpile model is abelian. After each checkout, rebuild and verify with `md5sum -c baseline.md5`.
 
My history has four checkpoint commits, each one a single optimization (or attempt). To isolate an individual optimization's effect, check out the commit before it, diff against the commit that introduced it (`git diff [PARENT] [COMMIT] -- sandpile.c`), and re-benchmark.
 
| Commit | Link | Optimization | Files changed | N=401 time | Root cause (from profiling) |
|---|---|---|---|---|---|
| Baseline | (original code) | none | none | 16.98 s | `stabilize()` was 98-99% of cycles. |
| `982787b` | [flat array + precomputation](https://github.com/jasminerliu/598APE-HW3-F26/commit/982787bab73c446ce640fb546b0ec0c128ddba6d) | (1) Replaced `int **grid` (one `malloc` per row) with a single contiguous (N+2)×(N+2) array. The zero border absorbs grains that fall off the grid, which removes the four boundary checks per toppling. (2) Precomputed `lo[i]`/`hi[i]`, the first and last active column per row, so the circle equation and pacman-mouth bound are no longer re-evaluated for every cell on every sweep. | `sandpile.c` | 7.84 s (2.2x) | Every sweep re-evaluated the circle equation for every cell, and each toppling did four boundary checks through a pointer-per-row grid |
| `dad9000` | [reduce work per toppling](https://github.com/jasminerliu/598APE-HW3-F26/commit/dad900076d391df1d3d52be9162d2511ed918685) | (3) A cell now topples `v >> 2` times in one visit and keeps `v & 3`, instead of one toppling per sweep. | `sandpile.c` | 6.81 s (2.5x) | Only ~13% gain|
| `4e43503` | [first parallelization attempt (Jacobi two-phase)](https://github.com/jasminerliu/598APE-HW3-F26/commit/4e43503ee246a0e4f07cb83bb62e0e23c9fef8ed) | (4) Split each sweep into a topple phase (record per-cell topple counts in a second array) and an absorb phase (each cell sums its neighbors' counts), both OpenMP `parallel for`. No write conflicts. **This was slower and was replaced by the next commit.** | `sandpile.c`, `Makefile` (`-fopenmp`) | 17.51 s (1 thread), 5.02 s (4 threads) | Scales well (3.5x on 4 threads) but 2.6x slower than the serial code on 1 thread. Jacobi moves grains one cell per sweep, so it likely needs many more sweeps, plus two passes and an extra array |
| `fc74d4a` | [working parallelization (row-coloring)](https://github.com/jasminerliu/598APE-HW3-F26/commit/fc74d4a723d572f16a95b6a67ef5bd3237c9fc85) | (5) Toppling row `i` only writes rows `i-1`, `i`, `i+1`, so rows three apart never conflict. Each sweep runs three phases (`i % 3 == 0`, then 1, then 2) with `#pragma omp parallel for schedule(static)` inside each phase. The update stays in place, so grains still travel several cells per sweep. | `sandpile.c`, `Makefile` (`-fopenmp`) | 7.09 s (1 thread), 3.69 s (2 threads), 2.16 s (4 threads), 7.9x | Keeps the fast in-place propagation of the serial code and removes the write conflicts. Scaling at 4 threads is 3.28x, likely limited by the three barriers per sweep, uneven row lengths, and memory bandwidth |
 
At N=1001, the original code takes 638.46 s and the final commit (`fc74d4a`) takes 74.99 s with the default thread count, about 8.5x faster.
 
I also tried `-march=native` (no measurable change), `OMP_PROC_BIND=close` thread pinning (slower: 2.65-2.84 s vs 2.16 s), and `schedule(static,1)` / `schedule(dynamic)` (appeared slower, but I ran those with pinning). These did not make it into the final code. See the report for details.
 
## Commands to run and verify:
 
### After flat array + precomputation
```bash
git checkout 982787b
make clean && make all
time ./sandpile.exe 401
md5sum -c baseline.md5
```
 
### After reducing work per toppling
```bash
git checkout dad9000
make clean && make all
time ./sandpile.exe 401
md5sum -c baseline.md5
```
 
### First parallelization attempt (Jacobi, slower)
```bash
git checkout 4e43503
make clean && make all
OMP_NUM_THREADS=1 bash -c 'time ./sandpile.exe 401'
OMP_NUM_THREADS=4 bash -c 'time ./sandpile.exe 401'
md5sum -c baseline.md5
```
 
### Working parallelization (row-coloring)
```bash
git checkout fc74d4a
make clean && make all
for t in 1 2 4; do
  echo "threads=$t"
  OMP_NUM_THREADS=$t bash -c 'time ./sandpile.exe 401'
  md5sum -c baseline.md5
done
```
 
### Full 1001x1001 run
```bash
git checkout fc74d4a
make clean && make all
time ./sandpile.exe 1001
```
The `baseline.md5` file here must come from the original code's N=1001 run (about 10 minutes on the course VM), so run that once in `tmux` or `screen`. Timings are wall-clock ("elapsed" in `perf stat`, "real" in `time`).
 
---

`sandpile.c` contains the implementation of finding the identity element of a 2D
abelian sandpile.

![Identity sandpile on a 1001x1001 pacman grid](identity-ref.png)

*Identity sandpile on a 1001x1001 pacman grid*


To compile the program, run:

```bash
make all
```

To clean existing build artifacts, run:

```bash
make clean
```

Usage (after compiling):

```bash
./sandpile.exe <N> 
```

where `N` is the grid size

Consider speeding up the generation of this 1001x1001 identity sandpile (which
takes approximately ~3m 40s on my local laptop under the default setup)

```bash
./sandpile.exe 1001
```

For an optional, ungraded challenge, see how large a sandpile you can compute
on the course VMs and report the grid size and runtime. There is no prescribed
upper limit on `N` for this challenge.

