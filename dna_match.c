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
#define HW_TIMEOUT_POLLS 10000u

// Smith-Waterman Accelerator register map.
// Hackathon accelerator base address = 0x80001300
#define ACCEL_SW_CONTROL          0x80001300
#define ACCEL_SW_LENGTHS          0x80001304
#define ACCEL_SW_QUERY_REG_BASE   0x80001308
#define ACCEL_SW_REF_REG_BASE     0x80001328
#define ACCEL_SW_RESULT           0x80001348

#define SW_CTRL_GO_BIT            0x00000001u
#define SW_CTRL_DONE_BIT          0x80000000u

#define READ_GPIO(addr)         (*(volatile unsigned int *)(addr))
#define WRITE_GPIO(addr, value) { (*(volatile unsigned int *)(addr)) = (value); }

static inline unsigned int pack_chars(char c0, char c1, char c2, char c3)
{
   return  ((unsigned int)(c0 & 0xFF))
        | (((unsigned int)(c1 & 0xFF)) <<  8)
        | (((unsigned int)(c2 & 0xFF)) << 16)
        | (((unsigned int)(c3 & 0xFF)) << 24);
}

static void write_hw_sequence(unsigned int base_addr, const char *seq, int len)
{
   for (int i = 0; i < 8; i++) {
      int idx = i * 4;

      char c0 = (idx < len)     ? seq[idx]     : 0;
      char c1 = (idx + 1 < len) ? seq[idx + 1] : 0;
      char c2 = (idx + 2 < len) ? seq[idx + 2] : 0;
      char c3 = (idx + 3 < len) ? seq[idx + 3] : 0;

      WRITE_GPIO(base_addr + (i * 4), pack_chars(c0, c1, c2, c3));
   }
}

static int M[MAX_LEN + 1][MAX_LEN + 1];
static int I[MAX_LEN + 1][MAX_LEN + 1];
static int D[MAX_LEN + 1][MAX_LEN + 1];

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

static int smith_waterman_hw_prepare_query(const char *query)
{
   int q_len = strlen(query);

   if (q_len > MAX_LEN) {
      return NEG_INF;
   }

   write_hw_sequence(ACCEL_SW_QUERY_REG_BASE, query, q_len);
   return q_len;
}

int smith_waterman_hw_run_ref(const char *ref, int q_len)
{
   int r_len = strlen(ref);
   unsigned int polls = 0;

   if ((q_len > MAX_LEN) || (r_len > MAX_LEN)) {
      return NEG_INF;
   }

   unsigned int packed_lens = (q_len & 0xFF) | ((r_len & 0xFF) << 8);

   WRITE_GPIO(ACCEL_SW_LENGTHS, packed_lens);
   write_hw_sequence(ACCEL_SW_REF_REG_BASE, ref, r_len);
   WRITE_GPIO(ACCEL_SW_CONTROL, SW_CTRL_GO_BIT);

   while ((READ_GPIO(ACCEL_SW_CONTROL) & SW_CTRL_DONE_BIT) == 0) {
      polls++;

      if (polls >= HW_TIMEOUT_POLLS) {
         return NEG_INF;
      }
   }

   return (int)READ_GPIO(ACCEL_SW_RESULT);
}

int smith_waterman_hw(const char *ref, const char *query)
{
   int q_len = smith_waterman_hw_prepare_query(query);

   if (q_len < 0) {
      return NEG_INF;
   }

   return smith_waterman_hw_run_ref(ref, q_len);
}

static unsigned int read_cycle_counter(void)
{
    return (unsigned int)pspMachinePerfCounterGet(D_PSP_COUNTER0);
}

int main(void)
{
    const char *query = "ACGTCGTACGTACGTA";

    const char *references[NUM_OF_REFS] =
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

    int sw_score[NUM_OF_REFS];
    int hw_score[NUM_OF_REFS];

    unsigned int cyc_beg;
    unsigned int cyc_end;
    unsigned int sw_cycles;
    unsigned int hw_cycles;

    int q_len;

    pspMachinePerfMonitorEnableAll();
    pspMachinePerfCounterSet(D_PSP_COUNTER0, D_CYCLES_CLOCKS_ACTIVE);

    cyc_beg = read_cycle_counter();

    for (int i = 0; i < NUM_OF_REFS; i++) {
        sw_score[i] = smith_waterman_affine(references[i], query);
    }

    cyc_end = read_cycle_counter();
    sw_cycles = cyc_end - cyc_beg;

    q_len = smith_waterman_hw_prepare_query(query);

    if (q_len < 0) {
        for (int i = 0; i < NUM_OF_REFS; i++) {
            hw_score[i] = NEG_INF;
        }

        hw_cycles = 0;
    }
    else {
        cyc_beg = read_cycle_counter();

        for (int i = 0; i < NUM_OF_REFS; i++) {
            hw_score[i] = smith_waterman_hw_run_ref(references[i], q_len);

            if (hw_score[i] == NEG_INF) {
                for (int j = i + 1; j < NUM_OF_REFS; j++) {
                    hw_score[j] = NEG_INF;
                }

                break;
            }
        }

        cyc_end = read_cycle_counter();
        hw_cycles = cyc_end - cyc_beg;
    }

    printf("Smith-Waterman comparison\n");
    printf("Software cycles = %u\n", sw_cycles);
    printf("Hardware cycles = %u\n", hw_cycles);

    if (hw_cycles != 0 && hw_score[0] == NEG_INF) {
        printf("Hardware timeout: check bitstream/register map\n");
    }

    if (hw_cycles != 0 && hw_score[0] != NEG_INF) {
        printf("Speedup x100 = %u\n", (sw_cycles * 100u) / hw_cycles);
    }

    printf("\n");
    printf("Ref | SW score | HW score | Match\n");
    printf("----------------------------------\n");

    for (int i = 0; i < NUM_OF_REFS; i++) {
        printf("%3d | %8d | %8d | %s\n",
               i,
               sw_score[i],
               hw_score[i],
               (sw_score[i] == hw_score[i]) ? "YES" : "NO");
    }

    return 0;
}
