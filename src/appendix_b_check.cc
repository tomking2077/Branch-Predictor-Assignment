// Replay Appendix B from examples/appendix-b/{hybrid,local,global}.csv.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "branch.h"
#include "predictor.h"
#include "my_predictor.h"

static int fail_row;

static void expect_eq (int row, const char *what, int got, int exp) {
	if (got == exp) return;
	fprintf (stderr, "row %d %s: got %d expected %d\n", row, what, got, exp);
	fail_row = 1;
}

static int parse_tn (const char *s) {
	if (strcmp (s, "T") == 0) return 1;
	if (strcmp (s, "NT") == 0) return 0;
	return -1;
}

static int parse_bits (const char *s) {
	int v = 0;
	if (!*s) return -1;
	for (; *s; s++) {
		if (*s != '0' && *s != '1') return -1;
		v = (v << 1) | (*s - '0');
	}
	return v;
}

static FILE *open_csv (const char *dir, const char *name) {
	char path[512];
	char line[512];
	snprintf (path, sizeof path, "%s/%s", dir, name);
	FILE *fp = fopen (path, "r");
	if (!fp || !fgets (line, sizeof line, fp) || strncmp (line, "step,", 5) != 0) {
		fprintf (stderr, "cannot read %s\n", path);
		if (fp) fclose (fp);
		return 0;
	}
	return fp;
}

static int next_line (FILE *fp, char *line, int n, const char *name) {
	if (fgets (line, n, fp)) return 1;
	fprintf (stderr, "%s: unexpected end of file\n", name);
	return 0;
}

int main (int argc, char **argv) {
	const char *dir = argc > 1 ? argv[1] : "../examples/appendix-b";
	FILE *hy = open_csv (dir, "hybrid.csv");
	FILE *lo = open_csv (dir, "local.csv");
	FILE *gl = open_csv (dir, "global.csv");
	if (!hy || !lo || !gl) return 1;

	toy_pm_predictor p;
	int rows_ok = 0;
	char line[512];

	for (int step = 1; step <= 22; step++) {
		fail_row = 0;
		int hstep, taken, set, hit;
		int lstep, index, ctr[toy_pm_predictor::BIMODAL_SIZE];
		char pcbuf[32], ghr_s[16], xor_s[16], tag_s[16];
		char local_s[8], glob_s[8], final_s[8], pred_s[8];

		if (!next_line (hy, line, sizeof line, "hybrid.csv")) return 1;
		if (sscanf (line,
			    "%d,%31[^,],%d,%15[^,],%15[^,],%d,%15[^,],%7[^,],%d,%7[^,],%7s",
			    &hstep, pcbuf, &taken, ghr_s, xor_s, &set, tag_s,
			    local_s, &hit, glob_s, final_s) != 11 || hstep != step) {
			fprintf (stderr, "hybrid.csv: bad step %d\n", step);
			return 1;
		}

		if (!next_line (lo, line, sizeof line, "local.csv")) return 1;
		if (sscanf (line,
			    "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%7s",
			    &lstep, &index,
			    &ctr[0], &ctr[1], &ctr[2], &ctr[3], &ctr[4], &ctr[5], &ctr[6], &ctr[7],
			    &ctr[8], &ctr[9], &ctr[10], &ctr[11], &ctr[12], &ctr[13], &ctr[14], &ctr[15],
			    pred_s) != 19 || lstep != step || index < 0 || index > 15 ||
		    parse_tn (pred_s) != (ctr[index] >> 1)) {
			fprintf (stderr, "local.csv: bad step %d\n", step);
			return 1;
		}
		(void) local_s;

		branch_info b;
		memset (&b, 0, sizeof b);
		b.address = (unsigned) strtoul (pcbuf, 0, 16);
		b.br_flags = BR_CONDITIONAL;
		branch_update *up = p.predict (b);
		pm_update *u = (pm_update *) up;

		expect_eq (step, "ghr", u->ghr, parse_bits (ghr_s));
		expect_eq (step, "xor", u->xor_bits, parse_bits (xor_s));
		expect_eq (step, "set", u->gset, set);
		expect_eq (step, "tag", u->gtag, parse_bits (tag_s));
		expect_eq (step, "index", u->bimodal_index, index);
		expect_eq (step, "hit", u->global_hit ? 1 : 0, hit);
		expect_eq (step, "global", u->global_prediction ? 1 : 0, parse_tn (glob_s));
		expect_eq (step, "final", up->direction_prediction () ? 1 : 0, parse_tn (final_s));

		for (int k = 0; k < toy_pm_predictor::BIMODAL_SIZE; k++)
			expect_eq (step, "bimodal", p.bimodal[k], ctr[k]);

		for (int s = 0; s < toy_pm_predictor::GSETS; s++) {
			for (int w = 0; w < toy_pm_predictor::GWAYS; w++) {
				int gstep, gset, gway, valid, counter, lru;
				char gtag[16];
				if (!next_line (gl, line, sizeof line, "global.csv")) return 1;
				if (sscanf (line, "%d,%d,%d,%d,%15[^,],%d,%d",
					    &gstep, &gset, &gway, &valid, gtag, &counter, &lru) != 7 ||
				    gstep != step || gset != s || gway != w) {
					fprintf (stderr, "global.csv: bad row at step %d\n", step);
					return 1;
				}
				toy_pm_predictor::gentry &got = p.global[w][s];
				if (got.valid != valid || got.tag != parse_bits (gtag) ||
				    got.ctr != counter || got.lru != lru) {
					fprintf (stderr,
						 "row %d way %d set %d: got v=%d tag=%d ctr=%d lru=%d\n",
						 step, w, s, got.valid, got.tag, got.ctr, got.lru);
					fail_row = 1;
				}
			}
		}

		printf ("row %2d %s\n", step, fail_row ? "FAIL" : "OK");
		if (!fail_row) rows_ok++;
		p.update (up, taken, 0);
	}

	fclose (hy);
	fclose (lo);
	fclose (gl);
	printf ("%s: %d / 22 rows match the csv tables\n",
		rows_ok == 22 ? "PASS" : "FAIL", rows_ok);
	return rows_ok == 22 ? 0 : 1;
}
