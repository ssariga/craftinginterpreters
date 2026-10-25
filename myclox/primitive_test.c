#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

static int checks = 0;

#define CHECK(condition)                                      \
    do {                                                      \
        checks++;                                             \
        if (!(condition)) {                                   \
            fprintf(stderr,                                   \
                    "FAIL at line %d: %s\n",                   \
                    __LINE__, #condition);                    \
            exit(EXIT_FAILURE);                               \
        }                                                     \
    } while (0)

static void expectNumber(Table* table, Value key, double expected)
{
    Value result = NIL_VAL;

    CHECK(tableGet(table, key, &result));
    CHECK(IS_NUMBER(result));
    CHECK(AS_NUMBER(result) == expected);
}

static void expectMissing(Table* table, Value key)
{
    Value result = NIL_VAL;
    CHECK(!tableGet(table, key, &result));
}

int main(void)
{
    //Use the exact initialization call from your working main.c.

    initMemory(16u * 1024u * 1024u);

    initVM();

    Table table;
    Table copy;

    initTable(&table);
    initTable(&copy);

    // 1. Looking up or deleting an absent key is safe.
    expectMissing(&table, NUMBER_VAL(42));
    CHECK(!tableDelete(&table, NUMBER_VAL(42)));

    // 2. Numbers, Booleans, nil, and strings are valid keys.
    CHECK(tableSet(&table, NUMBER_VAL(42), NUMBER_VAL(100)));
    CHECK(tableSet(&table, BOOL_VAL(true), NUMBER_VAL(200)));
    CHECK(tableSet(&table, BOOL_VAL(false), NUMBER_VAL(300)));
    CHECK(tableSet(&table, NIL_VAL, NUMBER_VAL(400)));

    Value stringKey = OBJ_VAL(copyString("hello", 5));
    CHECK(tableSet(&table, stringKey, NUMBER_VAL(500)));

    expectNumber(&table, NUMBER_VAL(42), 100);
    expectNumber(&table, BOOL_VAL(true), 200);
    expectNumber(&table, BOOL_VAL(false), 300);
    expectNumber(&table, NIL_VAL, 400);
    expectNumber(&table, stringKey, 500);

    // 3. Numeric 1 and Boolean true are different keys.
    CHECK(tableSet(&table, NUMBER_VAL(1), NUMBER_VAL(600)));

    expectNumber(&table, NUMBER_VAL(1), 600);
    expectNumber(&table, BOOL_VAL(true), 200);

    // 4. Replacing a value returns false, not "new key".
    CHECK(!tableSet(&table, NUMBER_VAL(42), NUMBER_VAL(101)));
    expectNumber(&table, NUMBER_VAL(42), 101);

    // 5. A nil value does not make an entry empty.
    CHECK(tableSet(&table, NUMBER_VAL(99), NIL_VAL));

   Value result = NUMBER_VAL(-1);
    CHECK(tableGet(&table, NUMBER_VAL(99), &result));
    CHECK(IS_NIL(result));

    // 6. Positive and negative zero refer to the same key.
    CHECK(tableSet(&table, NUMBER_VAL(0.0), NUMBER_VAL(700)));
    expectNumber(&table, NUMBER_VAL(-0.0), 700);

    CHECK(!tableSet(&table, NUMBER_VAL(-0.0), NUMBER_VAL(701)));
    expectNumber(&table, NUMBER_VAL(0.0), 701);

    // 7. Delete nil, verify absence, and insert it again.
    CHECK(tableDelete(&table, NIL_VAL));
    expectMissing(&table, NIL_VAL);
    CHECK(!tableDelete(&table, NIL_VAL));

    CHECK(tableSet(&table, NIL_VAL, NUMBER_VAL(401)));
    expectNumber(&table, NIL_VAL, 401);

    // 8. Force several capacity increases.
    int previousCapacity = table.capacity;

    for (int i = 0; i < 1000; i++) {
        CHECK(tableSet(&table,
                       NUMBER_VAL(1000 + i),
                       NUMBER_VAL(i * 10)));
    }

    CHECK(table.capacity > previousCapacity);

    for (int i = 0; i < 1000; i++) {
        expectNumber(&table, NUMBER_VAL(1000 + i), i * 10);
    }

    // Existing keys must survive resizing.
    expectNumber(&table, BOOL_VAL(true), 200);
    expectNumber(&table, BOOL_VAL(false), 300);
    expectNumber(&table, NIL_VAL, 401);
    expectNumber(&table, stringKey, 500);

    // 9. Delete half the numeric keys.
    for (int i = 0; i < 1000; i += 2) {
        CHECK(tableDelete(&table, NUMBER_VAL(1000 + i)));
    }

    // Deleted entries are absent; other entries remain reachable.
    for (int i = 0; i < 1000; i++) {
        if (i % 2 == 0) {
            expectMissing(&table, NUMBER_VAL(1000 + i));
        } else {
            expectNumber(&table, NUMBER_VAL(1000 + i), i * 10);
        }
    }

    // 10. Insert again after deletion.
    for (int i = 0; i < 1000; i += 2) {
        CHECK(tableSet(&table,
                       NUMBER_VAL(1000 + i),
                       NUMBER_VAL(-i)));
    }

    for (int i = 0; i < 1000; i++) {
        double expected = i % 2 == 0 ? -i : i * 10;
        expectNumber(&table, NUMBER_VAL(1000 + i), expected);
    }

    // 11. Copy live entries, not tombstones.
    CHECK(tableDelete(&table, NUMBER_VAL(1000)));

    tableAddAll(&table, &copy);

    expectMissing(&copy, NUMBER_VAL(1000));
    expectNumber(&copy, NUMBER_VAL(1001), 10);
    expectNumber(&copy, BOOL_VAL(true), 200);
    expectNumber(&copy, NIL_VAL, 401);
    expectNumber(&copy, stringKey, 500);

    // 12. String interning still returns the same object.
    ObjString* first = copyString("hello", 5);
    ObjString* second = copyString("hello", 5);

    CHECK(first == second);
    CHECK(first == AS_STRING(stringKey));

    freeTable(&copy);
    freeTable(&table);
    freeVM();

    printf("PASS: %d checks completed.\n", checks);

    return EXIT_SUCCESS;
}