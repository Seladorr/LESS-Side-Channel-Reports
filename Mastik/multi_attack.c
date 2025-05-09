/*********************************************************************
 * multi_attack.c – Fisher-Yates + Prime/Probe for 0xADE1A1DE/Mastik *
 *********************************************************************/

 #include <stdio.h>
 #include <stdlib.h>
 #include <stdint.h>
 #include <string.h>
 #include <unistd.h>
 #include <time.h>
 #include <errno.h>
 
 #include <mastik/util.h>
 #include <mastik/l3.h>
 
 /* ----------------- Configuration -------------------------------- */
 #define SIZE_DEFAULT  256          /* Array size (256-400)           */
 #define LINE_SIZE     64           /* Cache line size                */
 #define OUTPUT_FILE   "probe_log.txt"
 #define FIXED_SEED    0xdeadbeef
 
 /* ----------------- Globals --------------------------------------- */
 static int  g_size;                /* Actual array size              */
 static int *g_arr  = NULL;         /* Victim array                   */
 
 static l3pp_t  g_l3  = NULL;       /* Mastik L3 handle              */
 static int     g_sets;             /* Total L3 sets                 */
 
 static uint8_t *g_map = NULL;      /* Bitmap of used sets           */
 static int     *g_mon = NULL;      /* Monitored set indices         */
 static int      g_nmon;            /* Number of monitored sets      */
 
 static FILE    *g_log = NULL;      /* Output file                   */
 
 /* ----------------- Helpers --------------------------------------- */
 static int ilog2(int x) { 
     int s = 0; 
     while ((1 << s) < x) ++s; 
     return s; 
 }
 
 static void die(const char *msg) {
     perror(msg);
     exit(EXIT_FAILURE);
 }
 
 /* Get monitored set indices (correct for your Mastik API) */
 static int *get_monitored_sets(int *count) {
     /* First call: get number of sets */
     *count = l3_getmonitoredset(g_l3, NULL, 0);
     
     /* Allocate buffer */
     int *sets = malloc(*count * sizeof(int));
     if (!sets) die("malloc sets");
     
     /* Second call: get actual set numbers */
     l3_getmonitoredset(g_l3, sets, *count);
     
     return sets;
 }
 
 /* Manual priming using set indices */
 static void manual_prime(void) {
    int count;
    int *sets = get_monitored_sets(&count);
    
    // Larger buffer to handle big set numbers
    uint8_t *dummy = aligned_alloc(LINE_SIZE, LINE_SIZE * 16 * 1024);
    if (!dummy) die("aligned_alloc failed");
    memset(dummy, 0, LINE_SIZE * 16 * 1024);

    for (int i = 0; i < count; i++) {
        // Ensure we stay within bounds
        uintptr_t offset = (sets[i] % (16 * 1024)) * LINE_SIZE;
        volatile uint8_t *p = dummy + offset;
        *p;  // Prime access
    }

    free(sets);
    free(dummy);
    asm volatile("mfence" ::: "memory");
}
 
 /* Discover which L3 sets the array uses */
 static void discover_mapping(void) {
     int shift = ilog2(LINE_SIZE);
     int ints_per_line = LINE_SIZE / sizeof(int);
 
     g_map = calloc(g_sets, 1);
     if (!g_map) die("calloc g_map");
 
     for (int i = 0; i < g_size; i += ints_per_line) {
         uintptr_t a = (uintptr_t)&g_arr[i];
         int set = (a >> shift) & (g_sets - 1);
         g_map[set] = 1;
     }
 }
 
 /* Configure monitored sets */
 static void setup_monitoring(void) {
     /* Count monitored sets */
     g_nmon = 0;
     for (int i = 0; i < g_sets; ++i) 
         if (g_map[i]) ++g_nmon;
 
     /* Allocate and populate list */
     g_mon = malloc(g_nmon * sizeof(int));
     if (!g_mon) die("malloc g_mon");
 
     int idx = 0;
     for (int i = 0; i < g_sets; ++i)
         if (g_map[i]) {
             g_mon[idx++] = i;
             l3_monitor(g_l3, i);
         }
 
     fprintf(g_log, "# Monitoring %d L3 sets\n", g_nmon);
 }
 
 /* Record probe results */
 static void probe_log(int swap_no) {
     uint16_t *lat = calloc(g_nmon, sizeof(uint16_t));
     if (!lat) die("calloc lat");
 
     l3_probe(g_l3, lat);
 
     fprintf(g_log, "Swap %03d", swap_no);
     for (int i = 0; i < g_nmon; ++i)
         fprintf(g_log, " | S%03d:%4u", g_mon[i], lat[i]);
     fputc('\n', g_log);
     fflush(g_log);
 
     free(lat);
 }
 
 /* Fisher-Yates shuffle with Prime+Probe */
 static void run_shuffle(void) {
     unsigned int seed = FIXED_SEED;
     int swaps = 0;
 
     for (int i = g_size - 1; i > 0; --i) {
         /* Prime */
         manual_prime();
         asm volatile("mfence" ::: "memory");
 
         /* Swap */
         int j = rand_r(&seed) % (i + 1);
         int tmp = g_arr[i];
         g_arr[i] = g_arr[j];
         g_arr[j] = tmp;
         asm volatile("mfence" ::: "memory");
 
         /* Probe */
         probe_log(++swaps);
     }
 }
 
 /* ----------------- Main ------------------------------------------ */
 int main(int argc, char **argv) {
     /* Initialize logging */
     g_log = fopen(OUTPUT_FILE, "w");
     if (!g_log) die("fopen");
 
     /* Set array size */
     g_size = (argc >= 2) ? atoi(argv[1]) : SIZE_DEFAULT;
     if (g_size < 256 || g_size > 400) {
         fprintf(stderr, "Using default size %d\n", SIZE_DEFAULT);
         g_size = SIZE_DEFAULT;
     }
 
     /* Allocate array */
     if (posix_memalign((void **)&g_arr, LINE_SIZE, g_size * sizeof(int)))
         die("posix_memalign");
     for (int i = 0; i < g_size; ++i) 
         g_arr[i] = i;
 
     /* Initialize Mastik */
     g_l3 = l3_prepare(NULL, NULL);
     if (!g_l3) die("l3_prepare");
     g_sets = l3_getSets(g_l3);
 
     /* Configure monitoring */
     discover_mapping();
     setup_monitoring();
 
     /* Warmup run */
     manual_prime();
     uint16_t *dummy = calloc(g_nmon, sizeof(uint16_t));
     l3_probe(g_l3, dummy);
     free(dummy);
 
     /* Execute shuffle */
     fprintf(g_log, "# Fisher-Yates shuffle begins\n");
     run_shuffle();
     fprintf(g_log, "# Completed\n");
 
     /* Cleanup */
     l3_release(g_l3);
     free(g_mon);
     free(g_map);
     free(g_arr);
     fclose(g_log);
 
     printf("Results saved to %s\n", OUTPUT_FILE);
     return 0;
 }
