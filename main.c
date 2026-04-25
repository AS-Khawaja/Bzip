/*
 * main.c
 * ------
 * Phase 2 driver: RLE-1  →  BWT  pipeline.
 *
 * Usage:
 *   ./bzip2_phase2 <input_file>
 *
 * Steps performed:
 *   1. Load config.ini
 *   2. Run self-tests (RLE-1 + BWT)
 *   3. Divide input file into blocks
 *   4. Per block: RLE-1 encode  →  BWT encode
 *   5. Reassemble encoded output
 *   6. Round-trip verification (BWT decode → RLE-1 decode → compare)
 *   7. Print metrics
 */

#include "bzip2.h"
#include <time.h>

/* ── utility ────────────────────────────────────────────────── */
static int bytes_equal(const unsigned char *a, size_t alen,
                       const unsigned char *b, size_t blen)
{
    if (alen != blen) return 0;
    return memcmp(a, b, alen) == 0;
}

/* ── RLE-1 self-tests ───────────────────────────────────────── */
static void run_rle1_tests(void)
{
    printf("=== RLE-1 Self-Tests ===\n");

    struct { const char *label; const char *input; } cases[] = {
        { "All unique",        "ABCDEFGH"              },
        { "All same",          "AAAAAAAA"              },
        { "Mixed",             "ABBBCCCCD"             },
        { "Run of exactly 4",  "AAAA"                  },
        { "Run of 5",          "AAAAA"                 },
        { "Long run (20 A's)", "AAAAAAAAAAAAAAAAAAAA"  },
        { "Single byte",       "Z"                     },
        { "Two same",          "XX"                    },
    };
    int n = (int)(sizeof(cases) / sizeof(cases[0]));

    for (int t = 0; t < n; t++) {
        size_t         ilen    = strlen(cases[t].input);
        unsigned char *in      = (unsigned char *)cases[t].input;
        unsigned char *enc     = malloc(ilen * 2 + 8);
        unsigned char *dec     = malloc(ilen + 8);
        size_t         enc_len = 0, dec_len = 0;

        rle1_encode(in, ilen, enc, &enc_len);
        rle1_decode(enc, enc_len, dec, &dec_len);
        int ok = bytes_equal(in, ilen, dec, dec_len);
        printf("  [%s] %-25s  in=%zu enc=%zu dec=%zu  %s\n",
               ok ? "PASS" : "FAIL", cases[t].label,
               ilen, enc_len, dec_len, ok ? "v" : "X");
        free(enc); free(dec);
    }
    printf("========================\n\n");
}

/* ── BWT self-tests ─────────────────────────────────────────── */
static void run_bwt_tests(void)
{
    printf("=== BWT Self-Tests ===\n");

    struct {
        const char *label;
        const char *input;
        const char *expected_L; /* NULL = round-trip only */
        int         expected_pi;/* -1  = round-trip only */
    } cases[] = {
        { "BANANA (classic)",   "BANANA",      "NNBAAA", 3  },
        { "Single char A",      "A",           "A",      0  },
        { "Two same AA",        "AA",          "AA",     0  },
        { "Two diff AB",        "AB",          "BA",     0  },
        { "All same CCCC",      "CCCC",        "CCCC",   0  },
        { "ABRACADABRA",        "ABRACADABRA", NULL,    -1  },
        { "mississippi",        "mississippi", NULL,    -1  },
        { "abcabc",             "abcabc",      NULL,    -1  },
        { "Repeated ABABABAB",  "ABABABAB",    NULL,    -1  },
    };
    int n = (int)(sizeof(cases) / sizeof(cases[0]));
    int total = 0, passed = 0;

    for (int t = 0; t < n; t++) {
        size_t         ilen    = strlen(cases[t].input);
        unsigned char *in      = (unsigned char *)cases[t].input;
        unsigned char *m_out   = malloc(ilen);
        unsigned char *m_dec   = malloc(ilen);
        unsigned char *s_out   = malloc(ilen);
        unsigned char *s_dec   = malloc(ilen);

        /* Matrix method */
        int pi_m = -1;
        bwt_encode(in, ilen, m_out, &pi_m);
        bwt_decode(m_out, ilen, pi_m, m_dec);
        int m_rt  = bytes_equal(in, ilen, m_dec, ilen);
        int m_vec = 1;
        if (cases[t].expected_L)
            m_vec = (memcmp(m_out, cases[t].expected_L, ilen) == 0)
                    && (cases[t].expected_pi < 0 || pi_m == cases[t].expected_pi);
        int m_ok = m_rt && m_vec;

        /* Suffix-array method */
        int pi_s = -1;
        bwt_encode_sa(in, ilen, s_out, &pi_s);
        bwt_decode(s_out, ilen, pi_s, s_dec);
        int s_rt    = bytes_equal(in, ilen, s_dec, ilen);
        int s_match = bytes_equal(m_out, ilen, s_out, ilen) && (pi_s == pi_m);
        int s_ok    = s_rt && s_match;

        printf("  [matrix %-5s] %-22s pi=%-3d  rt=%s vec=%s\n",
               m_ok ? "PASS" : "FAIL", cases[t].label, pi_m,
               m_rt ? "v" : "X", m_vec ? "v" : "X");
        printf("  [sa     %-5s] %-22s pi=%-3d  rt=%s match=%s\n",
               s_ok ? "PASS" : "FAIL", cases[t].label, pi_s,
               s_rt ? "v" : "X", s_match ? "v" : "X");

        total += 2;
        if (m_ok) passed++;
        if (s_ok) passed++;

        free(m_out); free(m_dec); free(s_out); free(s_dec);
    }

    printf("  Result: %d / %d passed\n", passed, total);
    printf("======================\n\n");
}

/* ── main ───────────────────────────────────────────────────── */
int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return 1;
    }
    const char *input_file = argv[1];

    /* 1. Config */
    Config *cfg = load_config("config.ini");
    if (!cfg) {
        cfg = calloc(1, sizeof(Config));
        cfg->block_size     = DEFAULT_BLOCK_SIZE;
        cfg->rle1_enabled   = true;
        cfg->output_metrics = true;
        strncpy(cfg->bwt_type, "matrix", sizeof(cfg->bwt_type) - 1);
    }
    print_config(cfg);

    /* 2. Self-tests */
    run_rle1_tests();
    run_bwt_tests();

    /* 3. Block division */
    clock_t t0 = clock();
    BlockManager *manager = divide_into_blocks(input_file, cfg->block_size);
    if (!manager) { free_config(cfg); return 1; }

    /* Storage for round-trip verification */
    unsigned char **originals    = calloc((size_t)manager->num_blocks, sizeof(unsigned char *));
    size_t         *orig_sizes   = calloc((size_t)manager->num_blocks, sizeof(size_t));
    int            *primary_idxs = malloc((size_t)manager->num_blocks * sizeof(int));
    for (int i = 0; i < manager->num_blocks; i++) primary_idxs[i] = -1;

    size_t total_orig = 0, total_rle = 0, total_bwt = 0;

    printf("[pipeline] Processing %d block(s), bwt_type='%s'...\n",
           manager->num_blocks, cfg->bwt_type);

    /* 4. Per-block: RLE-1 → BWT */
    for (int i = 0; i < manager->num_blocks; i++) {
        Block *b = &manager->blocks[i];
        if (b->size == 0) continue;

        total_orig += b->original_size;

        /* Save original for verification */
        originals[i]  = malloc(b->size);
        orig_sizes[i] = b->size;
        if (originals[i]) memcpy(originals[i], b->data, b->size);

        /* RLE-1 */
        if (cfg->rle1_enabled) {
            size_t enc_max = b->size + b->size / 4 + 8;
            unsigned char *enc = malloc(enc_max);
            size_t enc_len = 0;
            rle1_encode(b->data, b->size, enc, &enc_len);
            free(b->data);
            b->data = enc;
            b->size = enc_len;
        }
        total_rle += b->size;

        /* BWT */
        unsigned char *bwt_buf = malloc(b->size);
        int pi = -1;
        bwt_encode_auto(cfg->bwt_type, b->data, b->size, bwt_buf, &pi);
        primary_idxs[i] = pi;
        free(b->data);
        b->data = bwt_buf;
        total_bwt += b->size;
    }

    clock_t t1 = clock();

    /* 5. Reassemble */
    char out_path[512];
    snprintf(out_path, sizeof(out_path), "%s_phase2.bwt", input_file);
    reassemble_blocks(manager, out_path);

    clock_t t2 = clock();

    /* 6. Metrics */
    if (cfg->output_metrics) {
        printf("\n[metrics] Original     : %zu bytes\n", total_orig);
        if (cfg->rle1_enabled)
            printf("[metrics] After RLE-1  : %zu bytes  (%.1f%%)\n",
                   total_rle, total_orig ? (double)total_rle/total_orig*100 : 0);
        printf("[metrics] After BWT    : %zu bytes  (length-preserving)\n", total_bwt);
        printf("[metrics] Pipeline     : %.2f ms\n",
               (double)(t1-t0)/CLOCKS_PER_SEC*1000.0);
        printf("[metrics] Total        : %.2f ms\n",
               (double)(t2-t0)/CLOCKS_PER_SEC*1000.0);
        printf("[metrics] Output       : %s\n", out_path);
    }

    /* 7. Round-trip verification */
    printf("\n[verify] BWT-decode -> RLE-1-decode -> compare original\n");
    int all_ok = 1;

    for (int i = 0; i < manager->num_blocks; i++) {
        Block *b = &manager->blocks[i];
        if (b->size == 0 || !originals[i]) continue;

        /* BWT decode */
        unsigned char *after_bwt = malloc(b->size + 4);
        bwt_decode(b->data, b->size, primary_idxs[i], after_bwt);

        /* RLE-1 decode */
        unsigned char *final;
        size_t final_len;
        if (cfg->rle1_enabled) {
            final = malloc(b->size * 4 + 8);
            rle1_decode(after_bwt, b->size, final, &final_len);
            free(after_bwt);
        } else {
            final     = after_bwt;
            final_len = b->size;
        }

        int ok = bytes_equal(originals[i], orig_sizes[i], final, final_len);
        if (!ok) {
            printf("  Block %d MISMATCH: orig=%zu decoded=%zu  X\n",
                   i, orig_sizes[i], final_len);
            all_ok = 0;
        }
        free(final);
        free(originals[i]);
    }

    printf("[verify] Round-trip: %s\n", all_ok ? "PASSED v" : "FAILED X");

    free(originals);
    free(orig_sizes);
    free(primary_idxs);
    free_block_manager(manager);
    free_config(cfg);
    return all_ok ? 0 : 1;
}
