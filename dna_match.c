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

    for(int i=0;i<rows;i++)
    {
        for(int j=0;j<cols;j++)
        {
            M[i][j] = 0;
            I[i][j] = NEG_INF;
            D[i][j] = NEG_INF;
        }
    }

    for(int i=1;i<rows;i++)
    {
        for(int j=1;j<cols;j++)
        {
            int s;

            if(query[i-1] == ref[j-1])
                s = MATCH;
            else
                s = MISMATCH;

            /* -------------------------- */
            /* Insertion matrix           */
            /* -------------------------- */

            I[i][j] =
                max2(
                    M[i-1][j] + GAP_OPEN,
                    I[i-1][j] + GAP_EXT);

            /* -------------------------- */
            /* Deletion matrix            */
            /* -------------------------- */

            D[i][j] =
                max2(
                    M[i][j-1] + GAP_OPEN,
                    D[i][j-1] + GAP_EXT);

            /* -------------------------- */
            /* Match matrix               */
            /* -------------------------- */

            M[i][j] =
                max4(
                    0,
                    M[i-1][j-1] + s,
                    I[i][j],
                    D[i][j]);

            if(M[i][j] > best_score)
                best_score = M[i][j];
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
