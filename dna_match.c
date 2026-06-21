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

#define MATCH       2
#define MISMATCH   -1

#define GAP_OPEN   -4
#define GAP_EXT    -1

#define MAX_LEN   32

#define NEG_INF  (-1000000)

#define NUM_OF_REFS 8

static int M[MAX_LEN][MAX_LEN];
static int I[MAX_LEN][MAX_LEN];
static int D[MAX_LEN][MAX_LEN];

static inline int max2(int a, int b)
{
    return (a > b) ? a : b;
}

static inline int max3(int a, int b, int c)
{
    int m = a;

    if(b > m) m = b;
    if(c > m) m = c;

    return m;
}

static inline int max4(int a,int b,int c,int d)
{
    int m = a;

    if(b > m) m = b;
    if(c > m) m = c;
    if(d > m) m = d;

    return m;
}

int smith_waterman_affine(
        const char *ref,
        const char *query)
{
    int rows = strlen(query) + 1;
    int cols = strlen(ref) + 1;

    int best_score = 0;

    /* Only the boundary row and column need initialisation.
       The inner loop writes every other cell before it is read. */
    for(int j = 0; j < cols; j++)
    {
        M[0][j] = 0;
        I[0][j] = NEG_INF;
        D[0][j] = NEG_INF;
    }
    for(int i = 1; i < rows; i++)
    {
        M[i][0] = 0;
        I[i][0] = NEG_INF;
        D[i][0] = NEG_INF;
    }

    for(int i = 1; i < rows; i++)
    {
        const int *restrict M_prev = M[i-1];
        const int *restrict I_prev = I[i-1];
        int       *restrict M_cur  = M[i];
        int       *restrict I_cur  = I[i];
        int       *restrict D_cur  = D[i];

        const char q = query[i-1];

        /* Keep three values in registers to avoid re-reading cells
           we just wrote or brought in from the previous column. */
        int m_diag = M_prev[0];   /* M[i-1][j-1] */
        int m_left = 0;           /* M[i][j-1],  boundary = 0       */
        int d_left = NEG_INF;     /* D[i][j-1],  boundary = NEG_INF */

        for(int j = 1; j < cols; j++)
        {
            int s = (q == ref[j-1]) ? MATCH : MISMATCH;

            int i_val = max2(M_prev[j] + GAP_OPEN, I_prev[j] + GAP_EXT);
            I_cur[j] = i_val;

            int d_val = max2(m_left + GAP_OPEN, d_left + GAP_EXT);
            D_cur[j] = d_val;
            d_left = d_val;

            int new_m = max4(0, m_diag + s, i_val, d_val);
            m_diag = M_prev[j];
            m_left = new_m;
            M_cur[j] = new_m;

            if(new_m > best_score)
                best_score = new_m;
        }
    }

    return best_score;
}

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
