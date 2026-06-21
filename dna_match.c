#if defined(D_NEXYS_A7)
   #include <bsp_printf.h>
   #include <bsp_mem_map.h>
   #include <bsp_version.h>
#else
   PRE_COMPILED_MSG("no platform was defined")
#endif
#include <psp_api.h>

#include <stdio.h>
#include <string.h>

#define MATCH     2
#define MISMATCH -1

#define GAP_OPEN -4
#define GAP_EXT  -1

#define MAX_LEN 32
#define NEG_INF (-1000000)

#define NUM_OF_REFS 8


static inline int max2(int a, int b)
{
    return (a > b) ? a : b;
}

static inline int max4(int a, int b, int c, int d)
{
    int m = a;
    if (b > m) m = b;
    if (c > m) m = c;
    if (d > m) m = d;
    return m;
}


/*
    STREAMING SMITH-WATERMAN (AFFINE GAP)
    Memory optimized version for RISC-V
*/
int smith_waterman_affine(const char *ref, const char *query)
{
    /*
        DP dimensions:
        rows = query length + 1
        cols = reference length + 1
    */
    int rows = strlen(query) + 1;
    int cols = strlen(ref) + 1;

    /*
        ROW-STREAMING DP STORAGE

        Instead of full matrices:
        M[i][j], I[i][j], D[i][j]

        We only keep:
        - previous row
        - current row
    */
    static int M_prev[MAX_LEN], M_curr[MAX_LEN];
    static int I_prev[MAX_LEN], I_curr[MAX_LEN];
    static int D_prev[MAX_LEN], D_curr[MAX_LEN];

    int best_score = 0; // Smith-Waterman keeps global maximum

    /*
        -----------------------------
        INITIALIZATION (ROW 0)
        -----------------------------
        - No alignment at start
        - M = 0
        - I, D = -INF (invalid states)
    */
    for (int j = 0; j < cols; j++)
    {
        M_prev[j] = 0;
        I_prev[j] = NEG_INF;
        D_prev[j] = NEG_INF;
    }

    /*
        -----------------------------
        MAIN DP LOOP
        -----------------------------
        Iterate over query (rows)
    */
    for (int i = 1; i < rows; i++)
    {
        /*
            First column initialization for current row
            (alignment against empty prefix)
        */
        M_curr[0] = 0;
        I_curr[0] = NEG_INF;
        D_curr[0] = NEG_INF;

        /*
            Iterate over reference (columns)
        */
        for (int j = 1; j < cols; j++)
        {
            /*
                MATCH / MISMATCH SCORE
            */
            int s = (query[i - 1] == ref[j - 1]) ? MATCH : MISMATCH;

            /*
                -------------------------
                INSERTION STATE (I)
                -------------------------
                Gap in reference sequence
                (vertical move in DP table)

                Transitions:
                - open gap from M
                - extend gap from I
            */
            I_curr[j] = max2(
                M_prev[j] + GAP_OPEN,
                I_prev[j] + GAP_EXT
            );

            /*
                -------------------------
                DELETION STATE (D)
                -------------------------
                Gap in query sequence
                (horizontal move in DP table)

                Transitions:
                - open gap from M
                - extend gap from D
            */
            D_curr[j] = max2(
                M_curr[j - 1] + GAP_OPEN,
                D_curr[j - 1] + GAP_EXT
            );

            /*
                -------------------------
                MATCH STATE (M)
                -------------------------
                Best local alignment ending at (i,j)

                Options:
                - restart alignment (0)
                - diagonal match/mismatch
                - come from insertion
                - come from deletion
            */
            M_curr[j] = max4(
                0,
                M_prev[j - 1] + s,
                I_curr[j],
                D_curr[j]
            );

            /*
                Track best local alignment score globally
            */
            if (M_curr[j] > best_score)
                best_score = M_curr[j];
        }

        /*
            -----------------------------
            ROW SWAP (CRITICAL STEP)
            -----------------------------
            Move current row → previous row
            for next iteration

            This is what enables O(n) memory usage
        */
        for (int j = 0; j < cols; j++)
        {
            M_prev[j] = M_curr[j];
            I_prev[j] = I_curr[j];
            D_prev[j] = D_curr[j];
        }
    }

    return best_score;
}


/*
    DRIVER CODE (your original style)
*/
int main(void)
{
    const char *query =
        "ACGTCGTACGTACGTA";

    int score[NUM_OF_REFS]; 

    const char *references[] =
    {
        "ACGTACGTACGTACGT",
        "ACGTTCGTACGTACGT",
        "ACGTACGGACGTACGT",
        "TTTTTTTTTTTTTTTT",
        "ACGTACGTTCGTACGT",
        "ACGTACGTACGTACGA",
        "ACGTTTGTACGTACGT",
        "ACGTACGTGCGTACGT"
    };


    // read time here
   int cyc_beg, cyc_end;

   pspMachinePerfMonitorEnableAll();
   pspMachinePerfCounterSet(D_PSP_COUNTER0, D_CYCLES_CLOCKS_ACTIVE);
   cyc_beg = pspMachinePerfCounterGet(D_PSP_COUNTER0);


    for(int i=0;i<NUM_OF_REFS;i++)
    {
        score[i] =
            smith_waterman_affine(
                references[i],
                query);

    }

   
    // read timer here and print execution time
   cyc_end = pspMachinePerfCounterGet(D_PSP_COUNTER0);

   printf("Cycles = %d\n", cyc_end-cyc_beg);

    for(int i=0;i<NUM_OF_REFS;i++)
    {
        printf(
            "Reference %d score = %d\n",
            i,
            score[i]);
    }

    return 0;
}
