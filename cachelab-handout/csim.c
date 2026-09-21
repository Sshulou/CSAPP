#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "cachelab.h"

typedef struct {
    int valid;
    unsigned long long tag;
    unsigned long long last_used;
} CacheLine;

typedef struct {
    CacheLine *lines;
} CacheSet;

typedef struct {
    CacheSet *sets;
    int set_count;
    int lines_per_set;
} Cache;

static int hits = 0;
static int misses = 0;
static int evictions = 0;
static int verbose = 0;
static unsigned long long timestamp = 0;

static void usage(char *program)
{
    printf("Usage: %s [-hv] -s <s> -E <E> -b <b> -t <tracefile>\n", program);
    printf("Options:\n");
    printf("  -h         Print this help message.\n");
    printf("  -v         Optional verbose flag.\n");
    printf("  -s <s>     Number of set index bits.\n");
    printf("  -E <E>     Number of lines per set.\n");
    printf("  -b <b>     Number of block offset bits.\n");
    printf("  -t <file>  Trace file.\n");
}

static Cache create_cache(int s, int E)
{
    int i;
    Cache cache;

    cache.set_count = 1 << s;
    cache.lines_per_set = E;
    cache.sets = calloc(cache.set_count, sizeof(CacheSet));
    if (cache.sets == NULL) {
        fprintf(stderr, "Failed to allocate cache sets\n");
        exit(1);
    }

    for (i = 0; i < cache.set_count; i++) {
        cache.sets[i].lines = calloc(E, sizeof(CacheLine));
        if (cache.sets[i].lines == NULL) {
            fprintf(stderr, "Failed to allocate cache lines\n");
            exit(1);
        }
    }

    return cache;
}

static void free_cache(Cache *cache)
{
    int i;

    for (i = 0; i < cache->set_count; i++) {
        free(cache->sets[i].lines);
    }
    free(cache->sets);
}

static void access_cache(Cache *cache, int s, int b, unsigned long long address)
{
    int i;
    int empty_index = -1;
    int lru_index = 0;
    unsigned long long lru_time;
    unsigned long long set_mask = (1ULL << s) - 1;
    int set_index = (address >> b) & set_mask;
    unsigned long long tag = address >> (s + b);
    CacheLine *lines = cache->sets[set_index].lines;

    timestamp++;

    for (i = 0; i < cache->lines_per_set; i++) {
        if (lines[i].valid && lines[i].tag == tag) {
            hits++;
            lines[i].last_used = timestamp;
            if (verbose) {
                printf(" hit");
            }
            return;
        }
    }

    misses++;
    if (verbose) {
        printf(" miss");
    }

    for (i = 0; i < cache->lines_per_set; i++) {
        if (!lines[i].valid) {
            empty_index = i;
            break;
        }
    }

    if (empty_index != -1) {
        lines[empty_index].valid = 1;
        lines[empty_index].tag = tag;
        lines[empty_index].last_used = timestamp;
        return;
    }

    evictions++;
    if (verbose) {
        printf(" eviction");
    }

    lru_time = lines[0].last_used;
    for (i = 1; i < cache->lines_per_set; i++) {
        if (lines[i].last_used < lru_time) {
            lru_time = lines[i].last_used;
            lru_index = i;
        }
    }

    lines[lru_index].tag = tag;
    lines[lru_index].last_used = timestamp;
}

static void replay_trace(Cache *cache, int s, int b, char *trace_file)
{
    char operation;
    char line[1000];
    int size;
    unsigned long long address;
    FILE *trace = fopen(trace_file, "r");

    if (trace == NULL) {
        fprintf(stderr, "Could not open trace file: %s\n", trace_file);
        exit(1);
    }

    while (fgets(line, sizeof(line), trace) != NULL) {
        if (sscanf(line, " %c %llx,%d", &operation, &address, &size) != 3) {
            continue;
        }

        if (operation == 'I') {
            continue;
        }

        if (verbose) {
            printf("%c %llx,%d", operation, address, size);
        }

        access_cache(cache, s, b, address);
        if (operation == 'M') {
            access_cache(cache, s, b, address);
        }

        if (verbose) {
            printf("\n");
        }
    }

    fclose(trace);
}

int main(int argc, char **argv)
{
    int opt;
    int s = -1;
    int E = -1;
    int b = -1;
    char *trace_file = NULL;
    Cache cache;

    while ((opt = getopt(argc, argv, "hvs:E:b:t:")) != -1) {
        switch (opt) {
        case 'h':
            usage(argv[0]);
            return 0;
        case 'v':
            verbose = 1;
            break;
        case 's':
            s = atoi(optarg);
            break;
        case 'E':
            E = atoi(optarg);
            break;
        case 'b':
            b = atoi(optarg);
            break;
        case 't':
            trace_file = optarg;
            break;
        default:
            usage(argv[0]);
            return 1;
        }
    }

    if (s < 0 || E <= 0 || b < 0 || trace_file == NULL) {
        usage(argv[0]);
        return 1;
    }

    cache = create_cache(s, E);
    replay_trace(&cache, s, b, trace_file);
    free_cache(&cache);

    printSummary(hits, misses, evictions);
    return 0;
}
