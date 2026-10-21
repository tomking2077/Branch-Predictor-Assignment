# Branch Predictor Assignment

The predictor is built on the CBP2 simulation infrastructure in `cbp2-infrastructure-v2/`. That tree is the original codebase. Its `README` and `doc/index.html` describe the trace format, the driver, and how a predictor class is called. The work for this assignment is on top of that: `src/my_predictor.h` implements the Appendix B hybrid predictor, and `src/appendix_b_check.cc` checks it against the worked example.

## Directory structure

```
report.tex, report.pdf          assignment write-up and results
cbp2-infrastructure-v2/
  README                        original infrastructure notes
  doc/index.html                original driver and predictor API
  run, run_extra                csh scripts that score every trace
  examples/appendix-b/          expected tables for the worked example
    hybrid.csv                  one row per branch (PC, outcome, predictions)
    local.csv                   16 bimodal counters at each step
    global.csv                  valid, tag, counter, and LRU for each way
  src/
    my_predictor.h              bimodal table, history register, 2-way global table
    appendix_b_check.cc         reads the three CSVs and checks the predictor
    predict.cc                  trace driver; prints MPKI
    Makefile
  traces/                       one .trace.bz2 per benchmark
```

`make` writes `predict` and `appendix_b_check` into `src/`. Those binaries are build products.

## Run the predictor

From `cbp2-infrastructure-v2/src`:

```bash
make
./predict ../traces/164.gzip/gzip.trace.bz2
```

The program prints mispredictions per kilo-instruction for that trace. Each trace is 100 million instructions.

To score every trace, the infrastructure script is csh:

```bash
cd cbp2-infrastructure-v2
csh ./run traces
```

If `csh` is not installed, the same loop in bash is:

```bash
cd cbp2-infrastructure-v2
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

`make` must be run in `src/` first. The script shells out to `/usr/bin/bzip2 -dc` to read the traces.

## Run the checker

The checker replays the 22-branch Appendix B example. It reads `examples/appendix-b/` and compares the history register, the XOR index, the bimodal counters, and the global table.

```bash
cd cbp2-infrastructure-v2/src
make appendix_b_check
./appendix_b_check
```

From that directory it finds `../examples/appendix-b` on its own. Pass another directory if the CSVs live somewhere else:

```bash
./appendix_b_check ../examples/appendix-b
```

A matching predictor prints `PASS: 22 / 22 rows match the csv tables`.
