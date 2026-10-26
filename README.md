# Branch Predictor Assignment

The predictor is built on the infrastructure from the Championship Branch Prediction competition. The September 14, 2006 update of that tree dropped the extra traces and added `src/compress`, the engine that pre-processed the traces shipped here. `doc/index.html` documents the trace format, the driver, and how a predictor class is called. The assignment handout tells you to build inside `cbp2-infrastructure-v2/`. In this repository that tree is the root: `src/`, `traces/`, and `run` are here. `src/my_predictor.h` has two hybrid predictors. `toy_pm_predictor` is the Appendix B worksheet, and `appendix_b_check.cc` checks it against the worked example. `pm_predictor` is the same index split at Pentium M sizes (4096 bimodal counters, 512 sets, 4 ways, 15-bit history) and allocates a global entry only when the bimodal prediction was wrong. `predict.cc` runs `pm_predictor`.

## Directory structure

```
README.md                     how to build and run this assignment
latex/                        assignment write-up (report.tex, report.pdf)
reference/                    assignment PDF, paper, and infrastructure archive
doc/index.html                original driver and predictor API
run, run_extra                csh scripts that score every trace
examples/appendix-b/          expected tables for the worked example
  hybrid.csv                  one row per branch (PC, outcome, predictions)
  local.csv                   16 bimodal counters at each step
  global.csv                  valid, tag, counter, and LRU for each way
src/
  my_predictor.h              toy_pm_predictor and pm_predictor
  appendix_b_check.cc         reads the three CSVs and checks the predictor
  predict.cc                  trace driver; prints MPKI
  Makefile
  compress/                   engine that pre-processed the traces
traces/                       one .trace.bz2 per benchmark
```

`make` writes `predict` and `appendix_b_check` into `src/`. Those binaries are build products.

## Run the predictor

The handout's `cd cbp2-infrastructure-v2` is the root of this repository. Run every block below from that root, in a fresh shell.

Build, then score one trace. The program prints mispredictions per kilo-instruction. Each trace is 100 million instructions.

```bash
make -C src
./src/predict traces/164.gzip/gzip.trace.bz2
```

Score every trace. `run` calls `./src/predict` and reads the traces with `/usr/bin/bzip2 -dc`.

```bash
make -C src
csh ./run traces
```

If `csh` is not installed:

```bash
make -C src
sum=0; n=0
while IFS= read -r t; do
  printf "%-50s\t" "$t"
  mpki=$(./src/predict "$t" | tail -1 | sed -e 's/ MPKI//')
  printf "%s\n" "$mpki"
  sum=$(printf "3\nk\n%s\n%s\n+\np\n" "$sum" "$mpki" | dc)
  n=$((n+1))
done < <(find traces -name '*.trace.*' | sort)
printf "average MPKI: "
printf "3\nk\n%s\n%s\n/p\n" "$sum" "$n" | dc
echo
```

## Run the checker

The checker replays the 22-branch Appendix B example against `toy_pm_predictor`. It reads `examples/appendix-b/` relative to the current directory, so it has to be started from `src`.

```bash
make -C src appendix_b_check
(cd src && ./appendix_b_check)
```

A match prints `PASS: 22 / 22 rows match the csv tables`. Pass another directory if the CSVs live somewhere else:

```bash
(cd src && ./appendix_b_check /path/to/appendix-b)
```
