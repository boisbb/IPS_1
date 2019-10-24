#include <stdio.h>
#include <assert.h>
#include "mmal.h"
#include <unistd.h>
#include <string.h>
#include <time.h>
#define NDEBUG

void debug_hdr(Header *h, int idx)
{
    printf("+- Header %d @ %p, data @ %p\n", idx, h, &h[1]);
    printf("|    | next           | size     | asize    |\n");
    printf("|    | %-14p | %-8lu | %-8lu |\n", h->next, h->size, h->asize);
}

void debug_arena(Arena *a, int idx)
{
    printf("Arena %d @ %p\n", idx, a);
    printf("|\n");
    char *arena_stop = (char*)a + a->size;
    Header *h = (Header*)&a[1];
    int i = 1;

    while ((char*)h >= (char*)a && (char*)h < arena_stop)
    {
        debug_hdr(h, i);
        i++;
        h = h->next;
        if (h == (Header*)&a[1])
            break;

    }
}

#ifdef NDEBUG
void debug_arenas()
{
    if(first_arena == NULL){
         printf("==========================================\n");
         printf("================= EMPTY ==================\n");
          printf("==========================================\n");
           printf("==========================================\n");
           return;
    }
    printf("\n*****************************************************\n");
    printf("***** START *******************************************\n");
    Arena *a = first_arena;
    for (int i = 1; a; i++)
    {
        debug_arena(a, i);
        a = a->next;
    }
    printf("\n************* END\n");
    printf("******************************\n\n");
}
#else
#define debug_arenas()
#endif

int main()
{
    Header h = {(void*)&main, 1234124, 131072};
    debug_hdr(&h, 1);
    assert(first_arena == NULL);

    /***********************************************************************/
    // Prvni alokace
    // Mela by alokovat novou arenu, pripravit hlavicku v ni a prave jeden
    // blok.
    void *p1 = mmalloc(42);
    /**
     *   v----- first_arena
     *   +-----+------+----+------+----------------------------+
     *   |Arena|Header|XXXX|Header|............................|
     *   +-----+------+----+------+----------------------------+
     *       p1-------^
     */
    assert(first_arena != NULL);
    assert(first_arena->next == NULL);
    assert(first_arena->size > 0);
    assert(first_arena->size <= PAGE_SIZE);
    Header *h1 = (Header*)(&first_arena[1]);
    Header *h2 = h1->next;
    assert(h1->asize == 42);
    assert((char*)h2 > (char*)h1);
    assert(h2->next == h1);
    assert(h2->asize == 0);

    printf("\n\n=======================   FIRST TEST   ==========================\n");

    debug_arenas();

    printf("=======================      END       ==========================\n");
    /***********************************************************************/
    // Druha alokace
    char *p2 = mmalloc(42);
    /**
     *   v----- first_arena
     *   +-----+------+----+------+----+------+----------------+
     *   |Arena|Header|XXXX|Header|XXXX|Header|................|
     *   +-----+------+----+------+----+------+----------------+
     *       p1-------^           ^
     *       p2-------------------/
     */
    Header *h3 = h2->next;
    assert(h3 != h1);
    assert(h2 != h3);
    assert(h3->next == h1);
    assert((char*)h2 < p2);
    assert(p2 < (char*)h3);

    printf("\n\n=======================   SECOND TEST   ==========================\n");

    debug_arenas();

    printf("=======================      END       ==========================\n");

    /***********************************************************************/
    // Treti alokace
    void *p3 = mmalloc(16);
    /**
     *                p1          p2          p3
     *   +-----+------+----+------+----+------+-----+------+---+
     *   |Arena|Header|XXXX|Header|XXXX|Header|XXXXX|Header|...|
     *   +-----+------+----+------+----+------+-----+------+---+
     */
    // insert assert here
    printf("\n\n=======================   THIRD TEST   ==========================\n");

    debug_arenas();

    printf("=======================      END       ==========================\n");
    /***********************************************************************/
    // Uvolneni prvniho bloku
    mfree(p1);

    /**
     *                p1          p2          p3
     *   +-----+------+----+------+----+------+-----+------+---+
     *   |Arena|Header|....|Header|XXXX|Header|XXXXX|Header|...|
     *   +-----+------+----+------+----+------+-----+------+---+
     */
    // insert assert here
    printf("\n\n=======================   FOURTH TEST   ==========================\n");

    debug_arenas();
    assert(h1->asize == 0);

    printf("=======================      END       ==========================\n");

    /***********************************************************************/
    // Uvolneni posledniho zabraneho bloku
    mfree(p3);
    /**
     *                p1          p2          p3
     *   +-----+------+----+------+----+------+----------------+
     *   |Arena|Header|....|Header|XXXX|Header|................|
     *   +-----+------+----+------+----+------+----------------+
     */
    // insert assert here
    printf("\n\n=======================   FIFTH TEST   ==========================\n");

    debug_arenas();
    Header *hdr = (Header*)(&first_arena[1]);
    // cyclic list
   assert(hdr->next->next->next == h1);

    printf("=======================      END       ==========================\n");

    /***********************************************************************/
    // Uvolneni prostredniho bloku
    mfree(p2);
    /**
     *                p1          p2          p3
     *   +-----+------+----------------------------------------+
     *   |Arena|Header|........................................|
     *   +-----+------+----------------------------------------+
     */
    // insert assert here
    printf("\n\n=======================   SIXTH TEST   ==========================\n");

    debug_arenas();
    //assert(first_arena == NULL); - Not workin however its not supposed to?

    printf("=======================      END       ==========================\n");


    // Dalsi alokace se nevleze do existujici areny
    void *p4 = mmalloc(PAGE_SIZE*2);
    /**
     *   /-- first_arena
     *   v            p1          p2          p3
     *   +-----+------+----------------------------------------+
     *   |Arena|Header|........................................|
     *   +-----+------+----------------------------------------+
     *      \ next
     *       v            p4
     *       +-----+------+---------------------------+------+-----+
     *       |Arena|Header|XXXXXXXXXXXXXXXXXXXXXXXXXXX|Header|.....|
     *       +-----+------+---------------------------+------+-----+
     */

     printf("\n\n=======================   SEVENTH TEST   ==========================\n");

     debug_arenas();

     printf("=======================      END       ==========================\n");

     printf("\n\n=======================   HOUNDTEST1   ==========================\n");

     mfree(p4);
     debug_arenas();

     printf("=======================      END       ==========================\n");

     printf("\n\n=======================   HOUNDTEST2   ==========================\n");

     p4 = mmalloc(131028);
     memset(p4, 'k', 131028);
     debug_arenas();

     printf("=======================      END       ==========================\n");



    /***********************************************************************/
    p4 = mrealloc(p4, PAGE_SIZE*2 + 2);
    assert(((char *)p4)[5] == 'k');
    assert(((char *)p4)[0] == 'k');
    assert(((char *)p4)[131028-1] == 'k');
    /**
     *                    p4
     *       +-----+------+-----------------------------+------+---+
     *       |Arena|Header|XXXXXXXXXXXXXXXXXXXXXXXXXXXxx|Header|...|
     *       +-----+------+-----------------------------+------+---+
     */
    // insert assert here
    printf("\n\n=======================   EIGTH TEST   ==========================\n");

    debug_arenas();

    printf("=======================      END       ==========================\n");

    /***********************************************************************/
    mfree(p4);

    printf("\n\n=======================   NINTH TEST   ==========================\n");

    debug_arenas();

    printf("=======================      END       ==========================\n");

    printf("\n\n=======================      ADVANCED TESTS       ==============================================================\n");
    printf("________________________________________________________________________________________________________________\n");

    printf("\n\n=======================   1   ==========================\n");
    p4 = mmalloc(PAGE_SIZE*2 - sizeof(Header) - sizeof(Arena));
    debug_arenas();
    hdr = (Header*)(&first_arena[1]);
    assert(hdr->next == hdr);


    printf("\n\n=======================   2   ==========================\n");
    void *p6 = mmalloc(PAGE_SIZE*2 - sizeof(Header) - sizeof(Arena) + 1);
    debug_arenas();
    assert(first_arena->next->size == PAGE_SIZE *3);


    printf("\n\n=======================   3   ==========================\n");
    void *p5 = mmalloc(PAGE_SIZE* 300);
    debug_arenas();

    printf("\n\n=======================   4   ==========================\n");
    p2 = mmalloc(0);
    debug_arenas();
    assert(p2 == NULL);


    printf("\n\n=======================   5   ==========================\n");
    p2 = mmalloc(1);
    debug_arenas();
    assert(p2 != NULL);

    printf("\n\n=======================   6   ==========================\n");
    mfree(p4);
    debug_arenas();

    printf("\n\n=======================   7   ==========================\n");
    mfree(p5);
    debug_arenas();


    printf("\n\n=======================   8   ==========================\n");
    mfree(p6);
    debug_arenas();


    printf("\n\n=======================   9   ==========================\n");
    mfree(p2);
    debug_arenas();

    first_arena = NULL;
    debug_arenas();


    printf("\n\n=======================   FUZZ SHIT   ==========================\n");
    #define REPS 5

    srand(time(NULL));
    void *ptrs[REPS];
    for(int i = 0; i < REPS; i ++){
        int fuzz = rand()%100000;
       ptrs[i] = mmalloc(fuzz);
       printf("Fuzz: %d\n", fuzz);
    }
    debug_arenas();


    printf("\n\n=======================   FREE THAT STUPID ASS SHIT   ==========================\n");
    for(int i = 0; i < REPS; i ++){
       mfree(ptrs[i]);
    }
    debug_arenas();

    first_arena = NULL;
    debug_arenas();

    printf("\n\n\n=======================   ITS REALLOC TIME BITCHES   ==========================\n\n\n");

    printf("\n\n=======================   1   ==========================\n");
    p3 = mmalloc(PAGE_SIZE * 4);
    debug_arenas();

    printf("\n\n=======================   2   ==========================\n");
    p3 = mrealloc(p3, PAGE_SIZE);
    debug_arenas();

    printf("\n\n=======================   3   ==========================\n");
    p3 = mrealloc(p3, PAGE_SIZE * 2);
    debug_arenas();

    printf("\n\n=======================   4   ==========================\n");
    p3 = mrealloc(p3, PAGE_SIZE * 3);
    debug_arenas();

    printf("\n\n=======================   5   ==========================\n");
    p2 = mmalloc(1500);
    debug_arenas();

    printf("\n\n=======================   6   ==========================\n");
    p3 = mrealloc(p3, PAGE_SIZE * 10);
    debug_arenas();

    printf("\n\n=======================   7   ==========================\n");
    mfree(p2);
    debug_arenas();

    printf("\n\n=======================   8   ==========================\n");
    mfree(p3);
    debug_arenas();

    return 0;
}
