#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

/* Initialize the personal allocator before any VM or table allocation. */
#ifndef BENCH_INIT_MEMORY
#define BENCH_INIT_MEMORY() initMemory(64u * 1024u * 1024u)
#endif

/* Store the final result observably, outside the timed region. */
static volatile uint64_t sink;

/* Correctness checks are independent of assert() and remain in Release builds. */
static void require(int condition)
{
    if (!condition)
    {
        fprintf(stderr, "Benchmark correctness check failed.\n");
        exit(1);
    }
}

/* Return process CPU time in seconds, not elapsed wall-clock time. */
static double seconds(void)
{
    clock_t t = clock();
    if (t == (clock_t)-1)
    {
        fprintf(stderr, "clock() unavailable.\n");
        exit(1);
    }
    return (double)t / CLOCKS_PER_SEC;
}

/* Populate numeric keys before timing lookup or deletion workloads. */
static void fill(Table* t, int n)
{
    for (int i = 0; i < n; i++)
    {
        require(tableSet(t, NUMBER_VAL(i), NUMBER_VAL(i)));
    }
}

/*
 * Print CSV results only after timing has finished.
 * count includes live entries and tombstones in this table implementation.
 */
static void report(const char* name, int n, uint64_t ops,
                   double elapsed, Table* t) {
    printf("%s,%d,%llu,%.9f,%.3f,%d,%d,%llu\n", name, n,
           (unsigned long long)ops, elapsed,
           elapsed * 1e9 / (double)ops, t->capacity, t->count,
           (unsigned long long)sink);
}

int main(int argc, char** argv)
{
    /* Each invocation runs one workload with a fresh VM and table. */
    if (argc < 2 || argc > 4)
    {
        fprintf(stderr,
                "Usage: %s MODE [N=20000] [ROUNDS=100]\n"
                "Modes: insert hit miss deleted_miss clean_miss "
                "churn string_hit\n", argv[0]);
        return 1;
    }

    const char* mode = argv[1];
    long parsedN = argc > 2 ? strtol(argv[2], NULL, 10) : 20000;
    long parsedR = argc > 3 ? strtol(argv[3], NULL, 10) : 100;

    /* Limit sizes so the default workload and numeric checks stay manageable. */
    if (parsedN < 2 || parsedN > 100000 || parsedR < 1 || parsedR > 10000)
    {
        fprintf(stderr,"Require 2 <= N <= 100000 and " "1 <= ROUNDS <= 10000.\n");
        return 1;
    }

    int n = (int)parsedN;
    int rounds = (int)parsedR;

    BENCH_INIT_MEMORY();
    initVM();

    Table t;
    initTable(&t);

    uint64_t ops = 0;
    uint64_t checksum = 0;
    uint64_t found = 0;
    double start = 0;
    double elapsed = 0;

    // Tests inserting new numeric keys, including table growth.
    if (strcmp(mode, "insert") == 0)
    {
        /*
         * Include initial allocation, growth, and rehashing in this timing.
         * Each key is unique, so every tableSet() should return true.
         * ROUNDS is intentionally ignored for this single insertion pass.
         */
        start = seconds();
        for (int i = 0; i < n; i++)
        {
            checksum += tableSet(&t, NUMBER_VAL(i), NUMBER_VAL(i));
        }
        elapsed = seconds() - start;
        ops = (uint64_t)n;
        require(checksum == (uint64_t)n);

    }
    else if (strcmp(mode, "hit") == 0 || strcmp(mode, "miss") == 0)
    {
        //tests  looking up keys that exist and also
        //tests looking up keys that were never inserted.

        /* Prepare the table outside the timed region. */
        fill(&t, n);
        int hit = strcmp(mode, "hit") == 0;

        /*
         * Hits query [0, n); misses query [n, 2*n).
         * Repeating the same ordered scan measures a steady access pattern,
         * not random access or a cold-cache workload.
         */
        start = seconds();
        for (int r = 0; r < rounds; r++)
        {
            for (int i = 0; i < n; i++)
            {
                Value v = NIL_VAL;
                bool ok = tableGet(&t, NUMBER_VAL(hit ? i : n + i), &v);
                found += ok;
                if (ok) checksum += (uint64_t)AS_NUMBER(v);
            }
        }
        elapsed = seconds() - start;
        ops = (uint64_t)n * rounds;

        require(found == (hit ? ops : 0));
        if (hit)
        {
            require(checksum == (uint64_t)rounds * n * (n - 1) / 2);
        }

    }
    else if (strcmp(mode, "deleted_miss") == 0 ||  strcmp(mode, "clean_miss") == 0)
    {
        //tests looking up deleted keys while tombstones remain.
        /* Create the same live keys and deleted-key queries in both modes. */
        fill(&t, n);
        int deleted = n / 2;
        for (int i = 0; i < deleted; i++)
        {
            require(tableDelete(&t, NUMBER_VAL(i)));
        }
        int originalCapacity = t.capacity;

        if (strcmp(mode, "clean_miss") == 0)
        {
            Table clean;
            initTable(&clean);

            /*
             * Grow a fresh table to the original capacity through tableSet().
             * The temporary negative keys are only setup scaffolding.
             */
            for (int i = 0; clean.capacity < originalCapacity; i++) {
                require(tableSet(&clean, NUMBER_VAL(-1 - i), NIL_VAL));
            }

            /*
             * Test-fixture access to internal fields: discard scaffolding
             * while retaining the allocated array. Numeric keys own no objects.
             * This is not a general-purpose public table-clearing API.
             */
            for (int i = 0; i < clean.capacity; i++) {
                clean.entries[i].key = NIL_VAL;
                clean.entries[i].value = NIL_VAL;
                clean.entries[i].state = ENTRY_EMPTY;
            }
            clean.count = 0;

            /* Copy only live entries; tombstones are not transferred. */
            tableAddAll(&t, &clean);
            freeTable(&t);
            t = clean;
            require(t.capacity == originalCapacity);
        }

        /*
         * Query deleted keys. Searches cannot stop at tombstones, so comparing
         * these modes reveals deletion-history costs at the same capacity.
         * Setup, deletion, and rebuilding are excluded from this timing.
         */
        start = seconds();
        for (int r = 0; r < rounds; r++)
        {
            for (int i = 0; i < deleted; i++)
            {
                Value v;
                found += tableGet(&t, NUMBER_VAL(i), &v);
            }
        }
        elapsed = seconds() - start;
        ops = (uint64_t)deleted * rounds;
        require(found == 0);
        checksum = found;

    }
    else if (strcmp(mode, "churn") == 0)
    {
        //tests repeatedly deleting and reinserting keys.
        fill(&t, n);

        /*
         * Delete and immediately reinsert each existing key.
         * This is same-key churn, not a stream of newly arriving keys.
         * Count deletion and insertion as two separate operations.
         */
        start = seconds();
        for (int r = 0; r < rounds; r++)
        {
            for (int i = 0; i < n; i++)
            {
                checksum += tableDelete(&t, NUMBER_VAL(i));
                checksum += tableSet(&t, NUMBER_VAL(i), NUMBER_VAL(i));
            }
        }
        elapsed = seconds() - start;
        ops = (uint64_t)n * rounds * 2;
        require(checksum == ops);

    } else if (strcmp(mode, "string_hit") == 0)
    {
        //tests looking up existing interned string keys.
        Value* keys = ALLOCATE(Value, n);

        /*
         * Format, allocate, intern, and insert strings BEFORE timing.
         * A shared prefix supplies similar-looking, but distinct, keys.
         * It does not guarantee that the hashes collide.
         */
        for (int i = 0; i < n; i++)
        {
            char buffer[96];
            int length = snprintf(buffer, sizeof(buffer),
                                  "shared_prefix_identifier_%08d", i);
            require(length > 0 && (size_t)length < sizeof(buffer));
            keys[i] = OBJ_VAL(copyString(buffer, length));
            require(tableSet(&t, keys[i], NUMBER_VAL(i)));
        }

        /* Measure lookups using existing objects and their cached hashes. */
        start = seconds();
        for (int r = 0; r < rounds; r++)
        {
            for (int i = 0; i < n; i++)
            {
                Value v = NIL_VAL;
                bool ok = tableGet(&t, keys[i], &v);
                found += ok;
                if (ok) checksum += (uint64_t)AS_NUMBER(v);
            }
        }
        elapsed = seconds() - start;
        ops = (uint64_t)n * rounds;
        require(found == ops);
        require(checksum == (uint64_t)rounds * n * (n - 1) / 2);

        /* Free the key-holding array; the VM still owns the string objects. */
        FREE_ARRAY(Value, keys, n);

    }
    else
    {
        fprintf(stderr, "Unknown benchmark mode: %s\n", mode);
        return 1;
    }

    /* Reporting and all cleanup are excluded from the measured interval. */
    sink = checksum;
    printf("mode,n,operations,cpu_seconds,ns_per_operation," "capacity,count,checksum\n");
    report(mode, n, ops, elapsed, &t);

    freeTable(&t);
    freeVM();

    return 0;
}