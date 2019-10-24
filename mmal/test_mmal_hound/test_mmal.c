#include <stdio.h>
#include <assert.h>
#include "mmal.h"
#include <unistd.h>

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

    debug_arenas();

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

    debug_arenas();

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
    debug_arenas();

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
    log_info("Freeing first");
    debug_arenas();
    assert(h1->asize == 0);
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
    log_info("Freeing third");
    debug_arenas();
     Header *hdr = (Header*)(&first_arena[1]);
     // cyclic list
    assert(hdr->next->next->next == h1);
    /***********************************************************************/
    // Uvolneni prostredniho bloku
    log_info("Freeing second");
    mfree(p2);
    assert(first_arena == NULL); // cleanup arenas
    /**
     *                p1          p2          p3
     *   +-----+------+----------------------------------------+
     *   |Arena|Header|........................................|
     *   +-----+------+----------------------------------------+
     */
    // insert assert here
    debug_arenas();

    // Dalsi alokace se nevleze do existujici areny
    log_info("Mallocing p4");
    void *p4 = mmalloc(PAGE_SIZE*2);
    debug_arenas();
   
    log_info("Freeing p4");
    mfree(p4);
    debug_arenas();


    log_info("Mallocing p4");
    p4 = mmalloc(131028);
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
	log_info("%p", p4);
    memset(p4, 'k', 131028);
     debug_arenas();
    /***********************************************************************/
    log_info("REALLOCING p4");
    p4 = mrealloc(p4, PAGE_SIZE*2 + 2);

    assert(((char *)p4)[5] == 'k');
    assert(((char *)p4)[0] == 'k');
    assert(((char *)p4)[131028-1] == 'k');

    log_info("Freeing p4");
    mfree(p4);
    debug_arenas();


    log_info("Mallocing p4 so arena is exactly 2 pages");
    p4 = mmalloc(PAGE_SIZE*2 - sizeof(Header) - sizeof(Arena));
    debug_arenas();
    hdr = (Header*)(&first_arena[1]);
    assert(hdr->next == hdr);

    log_info("Mallocing p6 so arena is exactly 2 pages + 1");
    void *p6 = mmalloc(PAGE_SIZE*2 - sizeof(Header) - sizeof(Arena) + 1);
    debug_arenas();
    assert(first_arena->next->size == PAGE_SIZE *3);

    log_info("Mallocing p5 to huge number");
    void *p5 = mmalloc(PAGE_SIZE* 300);
    debug_arenas();


    log_info("Mallocing p2 to 0");
    p2 = mmalloc(0);
    debug_arenas();
    assert(p2 == NULL);

    log_info("Mallocing p2 to 1");
    p2 = mmalloc(1);
    debug_arenas();
    assert(p2 != NULL);

    /***********************************************************************/
    log_info("Freeing p4");
    mfree(p4);
    debug_arenas();
    assert(first_arena->next->next == NULL);

    log_info("Freeing p5");
    mfree(p5);
    debug_arenas();

    log_info("Freeing p6");
    mfree(p6);
    debug_arenas();
    assert(first_arena->next == NULL);
    hdr = (Header*)(&first_arena[1]);
    assert(hdr->asize == 0);
    assert(hdr->next->asize == 1);
    assert(hdr->next->next->asize == 0);
    assert(hdr->next->next->next == hdr);

    log_info("Freeing p2");
    mfree(p2);
    debug_arenas();


    log_info("Fuzz that shit");
    // YOOO Let's fuzz that shit

    #define REPS 5

    srand(time(NULL)); 
    void *ptrs[REPS];
    for(int i = 0; i < REPS; i ++){
        int fuzz = rand()%100000;
       ptrs[i] = mmalloc(fuzz);
       printf("Fuzz: %d\n", fuzz);
    }
    debug_arenas();


    log_info("TIME TO FREE IT");
    for(int i = 0; i < REPS; i ++){
       mfree(ptrs[i]);
    }
    debug_arenas();

    // ***** realloc testing
    log_info("Realloc basse, mallocing PAGE_SIZE * 4");
    p3 = mmalloc(PAGE_SIZE * 4);
    debug_arenas();

    log_info("shrinking to PAGE_SIZE");
    p3 = mrealloc(p3, PAGE_SIZE);
    debug_arenas();

       log_info("enlarging to PAGE_SIZE * 2 to merge");
    p3 = mrealloc(p3, PAGE_SIZE * 2);
    debug_arenas();

    log_info("enlarging to PAGE_SIZE * 2 to just change asize");
    p3 = mrealloc(p3, PAGE_SIZE * 3);
    debug_arenas();

    log_info("mallocing p2 to hold the first arena");
    p2 = mmalloc(1500);
    debug_arenas();

    log_info("enlarging to PAGE_SIZE * 10 to malloc again");
    p3 = mrealloc(p3, PAGE_SIZE * 10);
    debug_arenas();

    log_info("free p2");
    mfree(p2);
    debug_arenas();

    log_info("free p3");
    mfree(p3);
    debug_arenas();

    return 0;
}
