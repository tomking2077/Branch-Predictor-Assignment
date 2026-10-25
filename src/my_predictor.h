// my_predictor.h
// This file contains a sample gshare_predictor class.
// It is a simple 32,768-entry gshare with a history length of 15.

class gshare_update : public branch_update {
public:
	unsigned int index;
};

class gshare_predictor : public branch_predictor {
public:
#define HISTORY_LENGTH	15
#define TABLE_BITS	15
	gshare_update u;
	branch_info bi;
	unsigned int history;
	unsigned char tab[1<<TABLE_BITS];

	gshare_predictor (void) : history(0) { 
		memset (tab, 0, sizeof (tab));
	}

	branch_update *predict (branch_info & b) {
		bi = b;
		if (b.br_flags & BR_CONDITIONAL) {
			u.index = 
				  (history << (TABLE_BITS - HISTORY_LENGTH)) 
				^ (b.address & ((1<<TABLE_BITS)-1));
			u.direction_prediction (tab[u.index] >> 1);
		} else {
			u.direction_prediction (true);
		}
		u.target_prediction (0);
		return &u;
	}

	void update (branch_update *u, bool taken, unsigned int target) {
		if (bi.br_flags & BR_CONDITIONAL) {
			unsigned char *c = &tab[((gshare_update*)u)->index];
			if (taken) {
				if (*c < 3) (*c)++;
			} else {
				if (*c > 0) (*c)--;
			}
			history <<= 1;
			history |= taken;
			history &= (1<<HISTORY_LENGTH)-1;
		}
	}
};

//
// Pentium M hybrid branch predictors
// This class implements a simple hybrid branch predictor based on the Pentium M branch outcome prediction units. 
// Instead of implementing the complete Pentium M branch outcome predictors, the class below implements a hybrid 
// predictor that combines a bimodal predictor and a global predictor. 
class pm_update : public branch_update {
public:
	unsigned int bimodal_index;
	unsigned int ghr;
	unsigned int xor_bits;
	unsigned int gset;
	unsigned int gtag;
	bool global_hit;
	int global_way;
	bool global_prediction;
};

// Appendix B simplified hybrid: 16-entry bimodal + 2-way tagged global.
// PC bits used for prediction are address[5:2] (4 bits). GHR is 4 bits.
class pm_predictor : public branch_predictor {
public:
	enum { BIMODAL_SIZE = 16, GSETS = 4, GWAYS = 2, HIST_BITS = 4 };

	struct gentry {
		unsigned char valid;
		unsigned char tag;
		unsigned char ctr;
		unsigned char lru;	// 0 = MRU, 1 = LRU
	};

	pm_update u;
	branch_info bi;
	unsigned int history;
	unsigned char bimodal[BIMODAL_SIZE];
	gentry global[GWAYS][GSETS];

	static unsigned int pc_bits (unsigned int address) {
		return (address >> 2) & 0xF;
	}

	static void sat_update (unsigned char &c, bool taken) {
		if (taken) {
			if (c < 3) c++;
		} else {
			if (c > 0) c--;
		}
	}

	void touch_lru (unsigned int set, int way) {
		global[way][set].lru = 0;
		global[way ^ 1][set].lru = 1;
	}

	int alloc_way (unsigned int set) {
		for (int w = 0; w < GWAYS; w++) {
			if (!global[w][set].valid) return w;
		}
		return global[0][set].lru ? 0 : 1;
	}

	pm_predictor (void) : history(0) {
		for (int i = 0; i < BIMODAL_SIZE; i++)
			bimodal[i] = i & 3;
		for (int s = 0; s < GSETS; s++) {
			global[0][s].valid = 0;
			global[0][s].tag = 0;
			global[0][s].ctr = s;
			global[0][s].lru = 1;
			global[1][s].valid = 0;
			global[1][s].tag = 0;
			global[1][s].ctr = 3 - s;
			global[1][s].lru = 0;
		}
	}

	branch_update *predict (branch_info & b) {
		bi = b;
		u.global_hit = false;
		u.global_way = -1;
		u.ghr = history;
		u.target_prediction (0);

		if (!(b.br_flags & BR_CONDITIONAL)) {
			u.direction_prediction (true);
			return &u;
		}

		unsigned int bits = pc_bits (b.address);
		u.bimodal_index = bits;
		u.xor_bits = bits ^ history;
		u.gset = (u.xor_bits >> 2) & 3;
		u.gtag = u.xor_bits & 3;

		bool local_pred = bimodal[u.bimodal_index] >> 1;

		for (int w = 0; w < GWAYS; w++) {
			if (global[w][u.gset].valid && global[w][u.gset].tag == u.gtag) {
				u.global_hit = true;
				u.global_way = w;
				break;
			}
		}
		if (!u.global_hit)
			u.global_way = alloc_way (u.gset);
		u.global_prediction = global[u.global_way][u.gset].ctr >> 1;

		u.direction_prediction (u.global_hit ? u.global_prediction : local_pred);
		return &u;
	}

	void update (branch_update *pu, bool taken, unsigned int target) {
		(void) target;
		if (!(bi.br_flags & BR_CONDITIONAL)) return;

		pm_update *uu = (pm_update *) pu;
		sat_update (bimodal[uu->bimodal_index], taken);

		int way = uu->global_way;
		global[way][uu->gset].valid = 1;
		global[way][uu->gset].tag = uu->gtag;
		sat_update (global[way][uu->gset].ctr, taken);
		touch_lru (uu->gset, way);

		history = ((history << 1) | (taken ? 1 : 0)) & ((1 << HIST_BITS) - 1);
	}

};

// Trace predictor. Same selection and update rules as pm_predictor, at the
// Pentium M table sizes. A 15-bit outcome history stands in for the path
// register. address[16:2] is XORed with that history; bits [14:6] of the
// result select one of 512 sets and bits [5:0] are the tag. The bimodal
// table has 4096 counters indexed by address[11:0]. Two ways, as allowed
// in place of the Pentium M's four.
class scaled_pm_predictor : public branch_predictor {
public:
	enum { BIMODAL_SIZE = 4096, GSETS = 512, GWAYS = 2, HIST_BITS = 15, TAG_BITS = 6 };

	struct gentry {
		unsigned char valid;
		unsigned char tag;
		unsigned char ctr;
		unsigned char lru;	// 0 = MRU, 1 = LRU
	};

	pm_update u;
	branch_info bi;
	unsigned int history;
	unsigned char bimodal[BIMODAL_SIZE];
	gentry global[GWAYS][GSETS];

	static void sat_update (unsigned char &c, bool taken) {
		if (taken) {
			if (c < 3) c++;
		} else {
			if (c > 0) c--;
		}
	}

	void touch_lru (unsigned int set, int way) {
		global[way][set].lru = 0;
		global[way ^ 1][set].lru = 1;
	}

	int alloc_way (unsigned int set) {
		for (int w = 0; w < GWAYS; w++) {
			if (!global[w][set].valid) return w;
		}
		return global[0][set].lru ? 0 : 1;
	}

	scaled_pm_predictor (void) : history(0) {
		memset (bimodal, 0, sizeof (bimodal));
		memset (global, 0, sizeof (global));
	}

	branch_update *predict (branch_info & b) {
		bi = b;
		u.global_hit = false;
		u.global_way = -1;
		u.ghr = history;
		u.target_prediction (0);

		if (!(b.br_flags & BR_CONDITIONAL)) {
			u.direction_prediction (true);
			return &u;
		}

		unsigned int bits = (b.address >> 2) & ((1u << HIST_BITS) - 1);
		u.bimodal_index = b.address & (BIMODAL_SIZE - 1);
		u.xor_bits = bits ^ history;
		u.gset = (u.xor_bits >> TAG_BITS) & (GSETS - 1);
		u.gtag = u.xor_bits & ((1u << TAG_BITS) - 1);

		bool local_pred = bimodal[u.bimodal_index] >> 1;

		for (int w = 0; w < GWAYS; w++) {
			if (global[w][u.gset].valid && global[w][u.gset].tag == u.gtag) {
				u.global_hit = true;
				u.global_way = w;
				break;
			}
		}
		if (!u.global_hit)
			u.global_way = alloc_way (u.gset);
		u.global_prediction = global[u.global_way][u.gset].ctr >> 1;

		u.direction_prediction (u.global_hit ? u.global_prediction : local_pred);
		return &u;
	}

	void update (branch_update *pu, bool taken, unsigned int target) {
		(void) target;
		if (!(bi.br_flags & BR_CONDITIONAL)) return;

		pm_update *uu = (pm_update *) pu;
		sat_update (bimodal[uu->bimodal_index], taken);

		int way = uu->global_way;
		global[way][uu->gset].valid = 1;
		global[way][uu->gset].tag = uu->gtag;
		sat_update (global[way][uu->gset].ctr, taken);
		touch_lru (uu->gset, way);

		history = ((history << 1) | (taken ? 1 : 0)) & ((1u << HIST_BITS) - 1);
	}
};

//
// Complete Pentium M branch predictors for extra credit
// This class implements the complete Pentium M branch prediction units. 
// It implements both branch target prediction and branch outcome predicton. 
class cpm_update : public branch_update {
public:
        unsigned int index;
};

class cpm_predictor : public branch_predictor {
public:
        cpm_update u;

        cpm_predictor (void) {
        }

        branch_update *predict (branch_info & b) {
            u.direction_prediction (true);
            u.target_prediction (0);
            return &u;
        }

        void update (branch_update *u, bool taken, unsigned int target) {
        }

};


