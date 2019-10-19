/**
 * Implementace My MALloc
 * Demonstracni priklad pro 1. ukol IPS/2019
 * Ales Smrcka
 */

#include "mmal.h"
#include <sys/mman.h> // mmap
#include <stdbool.h> // bool

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS 0x20
#endif
#define ALIGN_WORD(size) ((size + (sizeof(long))) / (sizeof(long))) * (sizeof(long))

#ifdef NDEBUG
/**
 * The structure header encapsulates data of a single memory block.
 */
typedef struct header Header;
struct header {

    /**
     * Pointer to the next header. Cyclic list. If there is no other block,
     * points to itself.
     */
    Header *next;

    /// size of the block
    size_t size;

    /**
     * Size of block in bytes allocated for program. asize=0 means the block
     * is not used by a program.
     */
    size_t asize;
};

/**
 * The arena structure.
 */
typedef struct arena Arena;
struct arena {

    /**
     * Pointer to the next arena. Single-linked list.
     */
    Arena *next;

    /// Arena size.
    size_t size;
};

#define PAGE_SIZE 128*1024

#endif

Arena *first_arena = NULL;

/**
 * Return size alligned to PAGE_SIZE
 */
static
size_t allign_page(size_t size)
{
    return ((size + (PAGE_SIZE)) / (PAGE_SIZE))*PAGE_SIZE;
}

/**
 * Allocate a new arena using mmap.
 * @param req_size requested size in bytes. Should be alligned to PAGE_SIZE.
 * @return pointer to a new arena, if successfull. NULL if error.
 */

/**
 *   +-----+------------------------------------+
 *   |Arena|....................................|
 *   +-----+------------------------------------+
 *
 *   |--------------- Arena.size ---------------|
 */
static
Arena *arena_alloc(size_t req_size)
{
    // FIXME
    (void)req_size;

    Arena *tmp_arena = NULL;
    tmp_arena = mmap(0, req_size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (tmp_arena == MAP_FAILED) {
      return NULL;
    }
    tmp_arena->size = req_size;
    tmp_arena->next = NULL;
    return tmp_arena;
}

/**
 * Header structure constructor (alone, not used block).
 * @param hdr       pointer to block metadata.
 * @param size      size of free block
 */
/**
 *   +-----+------+------------------------+----+
 *   | ... |Header|........................| ...|
 *   +-----+------+------------------------+----+
 *
 *                |-- Header.size ---------|
 */
static
void hdr_ctor(Header *hdr, size_t size)
{
    hdr->size = size;
    hdr->asize = 0;
    hdr->next = (Header *)(&first_arena[1]);
    return;
}

static
int hdr_can_split(Header *hdr, size_t req_size)
{
  if (hdr->size >= req_size + 2*sizeof(Header) && req_size % sizeof(long) == 0 && hdr->asize == 0){
    return 1;
  }
  return 0;
}

/**
 * Splits one block into two.
 * @param hdr       pointer to header of the big block
 * @param req_size  requested size of data in the (left) block.
 * @pre   (req_size % PAGE_SIZE) = 0
 * @pre   (hdr->size >= req_size + 2*sizeof(Header))
 * @return pointer to the new (right) block header.
 */
/**
 * Before:        |---- hdr->size ---------|
 *
 *    -----+------+------------------------+----
 *         |Header|........................|
 *    -----+------+------------------------+----
 *            \----hdr->next---------------^
 */
/**
 * After:         |- req_size -|
 *
 *    -----+------+------------+------+----+----
 *     ... |Header|............|Header|....|
 *    -----+------+------------+------+----+----
 *             \---next--------^  \--next--^
 */
static
Header *hdr_split(Header *hdr, size_t req_size)
{
    // Work with new Header
    Header *new;
    new = hdr + req_size;
    hdr_ctor(new, hdr->size - req_size - 2*sizeof(Header));
    new->next = hdr->next;
    //

    // Rework the old one
    hdr->size = req_size;
    hdr->asize = 0;
    hdr->next = new;
    return new;
}

/**
 * Detect if two blocks adjacent blocks could be merged.
 * @param left      left block
 * @param right     right block
 * @return true if two block are free and adjacent in the same arena.
 */
static
bool hdr_can_merge(Header *left, Header *right)
{
    // FIXME
    (void)left;
    (void)right;
    return false;
}

/**
 * Merge two adjacent free blocks.
 * @param left      left block
 * @param right     right block
 */
static
void hdr_merge(Header *left, Header *right)
{
    (void)left;
    (void)right;
    // FIXME
}

static
Header *best_fit(size_t size)
{
  Header *tmp_hdr = (Header *)(&first_arena[1]);
  Header *best_fit_hdr = NULL;
  int diff_swp = -1;
  int diff = -1;
  while(tmp_hdr != NULL){
    if (tmp_hdr->asize == 0){
      diff_swp = tmp_hdr->size - (ALIGN_WORD(size));
      if (diff_swp >= 0 && diff_swp < diff) {
        diff = diff_swp;
      }
      else if (diff == -1){
        diff = diff_swp;
        best_fit_hdr = tmp_hdr;
      }
    }
    tmp_hdr = tmp_hdr->next;
    if (tmp_hdr == (Header*)(&first_arena[1])) {
      break;
    }
  }

  // In case that no block was found -> new arena needs to be created
  if (best_fit_hdr == NULL){
    return NULL;
  }
  if (hdr_can_split(best_fit_hdr, ALIGN_WORD(size))) {
    best_fit_hdr->next = hdr_split(best_fit_hdr, ALIGN_WORD(size));
    return best_fit_hdr;
  }
  else {
    best_fit_hdr->asize = size;
    return best_fit_hdr;
  }



}

/**
 * Allocate memory. Use best-fit search of available block.
 * @param size      requested size for program
 * @return pointer to allocated data or NULL if error.
 */
void *mmalloc(size_t size)
{
    // FIXME
    Header *tmp;
    if (first_arena == NULL) {
      printf("\n----------- THERE SHOULD BE ONLY ONE FIRST_ARENA ALLOCATION -------------\n\n");
      first_arena = arena_alloc(allign_page(size));
      hdr_ctor((Header *)&first_arena[1], allign_page(size) - sizeof(Arena) - sizeof(Header));
      first_arena[1].next = hdr_split((Header *)(&first_arena[1]), ALIGN_WORD(size));
      tmp = (Header *)(&first_arena[1]);
      tmp->asize = 42;

      // My testing //////////////////////////////////////////////////////////
      printf("------> MMAP TEST | HEADER TEST <------\n");
      printf("------> %s        |          <------\n", first_arena == NULL ? "true" : "false");
      printf("---------> ARENA SIZE: %ld | H1 SIZE: %ld ASIZE: %ld | SECOND BLOCK SIZE: %ld ASIZE: %ld\n", first_arena->size, tmp->size, tmp->asize, tmp->next->size, tmp->next->asize);
      printf("_________________________\n");
      ///////////////////////////////////////////////////////////////////////

      return &tmp[1];
    }
    tmp = best_fit(size);
    return &tmp[1];



    return NULL;
}

/**
 * Free memory block.
 * @param ptr       pointer to previously allocated data
 */
void mfree(void *ptr)
{
    (void)ptr;
    // FIXME
}

/**
 * Reallocate previously allocated block.
 * @param ptr       pointer to previously allocated data
 * @param size      a new requested size. Size can be greater, equal, or less
 * then size of previously allocated block.
 * @return pointer to reallocated space.
 */
void *mrealloc(void *ptr, size_t size)
{
    // FIXME
    (void)ptr;
    (void)size;
    return NULL;
}
