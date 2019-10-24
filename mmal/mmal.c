//xburka00
/**
 * Implementace My MALloc
 * Demonstracni priklad pro 1. ukol IPS/2019
 * Ales Smrcka
 */

/**
* vypracoval:
* Boris Burkalo, xburka00, 2BIT
* 24.10.2019
*/

#include "mmal.h"
#include <sys/mman.h> // mmap
#include <stdbool.h> // bool

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS 0x20
#endif

// Aligns anything to word
#define ALIGN_WORD(size) (((size - 1) + (sizeof(long))) / (sizeof(long))) * (sizeof(long))

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
    return (((size) + (PAGE_SIZE)) / (PAGE_SIZE))*PAGE_SIZE;
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


// Appends the newly created arena to the previous one.
static
void arena_append(Arena *a)
{
  Arena *tmp_arena = first_arena;

  while (tmp_arena->next != NULL) {
    tmp_arena = tmp_arena->next;
  }

  tmp_arena->next = a;
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
    char *new_char = (char *)hdr + req_size + sizeof(Header);
    new = (Header *)new_char;
    hdr_ctor(new, hdr->size - req_size - sizeof(Header));
    new->next = hdr->next;
    //

    // Rework the old one
    hdr->size = req_size;
    hdr->asize = 0;
    hdr->next = new;
    return new;
    //
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
    Arena *tmp_arena = first_arena;
    while (tmp_arena != NULL) {
      if (left == NULL || right == (Header *)&tmp_arena[1] || right == left) {
        return false;
      }
      tmp_arena = tmp_arena->next;
    }

    if (left->asize != 0 || right->asize != 0) {
      return false;
    }

    return true;
}

/**
 * Merge two adjacent free blocks.
 * @param left      left block
 * @param right     right block
 */
static
void hdr_merge(Header *left, Header *right)
{
    left->next = right->next;
    left->size = left->size + (right->size + sizeof(Header));
    left->asize = 0;
    right->size = 0;
    right->asize = 0;
}

// Searches for an ideal block for the desired size. If it finds one, the pointer to the
// blocks header is returned. In case no ideal block was found, the function returns NULL
static
Header *best_fit(size_t size)
{
  Header *tmp_hdr = (Header *)(&first_arena[1]);
  Header *best_fit_hdr = NULL;
  Arena *tmp_arena = first_arena;
  int diff_swp = -1;
  int diff = -1;
  while(tmp_arena != NULL){
    while(tmp_hdr != NULL){
      if (tmp_hdr->asize == 0 && size <= tmp_arena->size && tmp_hdr->size >= size){
        diff_swp = tmp_hdr->size - (ALIGN_WORD(size));
        if (diff_swp >= 0 && diff_swp < diff) {
          diff = diff_swp;
          best_fit_hdr = tmp_hdr;
        }
        else if (diff == -1){
          diff = diff_swp;
          best_fit_hdr = tmp_hdr;
        }
      }
      tmp_hdr = tmp_hdr->next;
      if (tmp_hdr == (Header*)(&tmp_arena[1])) {
        tmp_arena = tmp_arena->next;
        if (tmp_arena == NULL) {
          break;
        }
        tmp_hdr = (Header *)(&tmp_arena[1]);
      }
    }
  }

  if (best_fit_hdr == NULL){
    return NULL;
  }
  if (hdr_can_split(best_fit_hdr, ALIGN_WORD(size))) {
    best_fit_hdr->next = hdr_split(best_fit_hdr, ALIGN_WORD(size));
    best_fit_hdr->asize = size;
    return best_fit_hdr;
  }
  else {
    best_fit_hdr->asize = size;
    return best_fit_hdr;
  }
}


// Finds a previous header and returns it.
static
Header *hdr_prev(Header *act)
{
  Header *tmp_hdr = act;

  if (act == (Header *)&first_arena[1]) return (Header *)&first_arena[1];
  while(1){
    if (tmp_hdr->next == act) {
      return tmp_hdr;
    }
    tmp_hdr = tmp_hdr->next;
  }
  return NULL;
}

/**
 * Allocate memory. Use best-fit search of available block.
 * @param size      requested size for program
 * @return pointer to allocated data or NULL if error.
 */
void *mmalloc(size_t size)
{
    if (size == 0) {
      return NULL;
    }


    Header *tmp;
    Arena *tmp_arena = NULL;
    // If no other are was previously created, then the function creates one with its own header. Then
    // splits it and returns the wanted pointer.
    // If there already is an existing arena, its not large enough, it does the same thing and appends the arena to the first arena.
    if (first_arena == NULL) {
      first_arena = arena_alloc(allign_page(size + sizeof(Header) + sizeof(Arena)));
      if (first_arena == NULL) {
        fprintf(stderr, "Failed to map memory\n");
        return;
      }
      hdr_ctor((Header *)&first_arena[1], allign_page(size + sizeof(Header) + sizeof(Arena)) - sizeof(Arena) - sizeof(Header));
      if (hdr_can_split((Header *)(&first_arena[1]), ALIGN_WORD(size))) {
        hdr_split((Header *)(&first_arena[1]), ALIGN_WORD(size));
      }
      tmp = (Header *)(&first_arena[1]);
      tmp->asize = size;
      return &tmp[1];
    }
    tmp = best_fit(size);
    if (tmp == NULL) {
      tmp_arena = arena_alloc(allign_page(size + sizeof(Header) + sizeof(Arena)));
      if (tmp_arena == NULL) {
        fprintf(stderr, "Failed to map memory\n");
        return;
      }
      hdr_ctor((Header *)(&tmp_arena[1]), allign_page(size + sizeof(Header) + sizeof(Arena)) - sizeof(Arena) - sizeof(Header));

      tmp = (Header *)(&tmp_arena[1]);
      tmp->next = tmp;

      if (hdr_can_split((Header *)(&tmp_arena[1]), ALIGN_WORD(size))) {
        hdr_split((Header *)(&tmp_arena[1]), ALIGN_WORD(size));
      }
      tmp->asize = size;
      arena_append(tmp_arena);
    }
    return &tmp[1];



    return NULL;
}

static
void merge_nearest(Header *tmp_free)
{
  while(1) {
    if (hdr_can_merge(tmp_free, tmp_free->next) == false && hdr_can_merge(hdr_prev(tmp_free), tmp_free) == false) {
      return;
    }

    //// Maybe else if ??? FIXME SOMETHING IS POTENTIONALLY INCORRECT HERE
    if (hdr_can_merge(tmp_free, tmp_free->next)) {
      hdr_merge(tmp_free, tmp_free->next);
    }
    if (hdr_can_merge(hdr_prev(tmp_free), tmp_free)) {
      tmp_free = hdr_prev(tmp_free);
      hdr_merge(tmp_free, tmp_free->next);
    }
  }
}

/**
 * Free memory block.
 * @param ptr       pointer to previously allocated data
 */
void mfree(void *ptr)
{
  Header *tmp_free = (Header *)(&first_arena[1]);
  Arena * tmp_arena = first_arena;

  //
  while(1){
    if (ptr == &tmp_free[1]){
      tmp_free->asize = 0;
      merge_nearest(tmp_free);
      return;
    }
    tmp_free = tmp_free->next;
    if (tmp_free == (Header *)(&tmp_arena[1])) {
      tmp_arena = tmp_arena->next;
      if (tmp_arena == NULL) {
        return;
      }
      tmp_free = (Header *)(&tmp_arena[1]);
    }
  }


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
    Header *tmp = (Header *)ptr - 1;
    size_t ptr_size = tmp->size;


    // If the needed size is equal, the function just returns the same pointer,
    // however, if the size is less then act_block_size, then the actual block is
    // reduced to the actual size needed
    if (tmp->size >= size) {
      if (tmp->size > size) {
        hdr_split(tmp, ALIGN_WORD(size));
        tmp->asize = size;
        tmp = tmp->next;
        merge_nearest(tmp);
      }
      else {
        tmp->asize = size;
      }
      return ptr;
    }
    else {
      tmp->asize = 0;
      merge_nearest(tmp);
      if (tmp->size >= size) {
        void *ret_ptr = &tmp[1];
        for (unsigned i = 0; i < size; i++){
          ((char *)ret_ptr)[i] = ((char *)ptr)[i];
        }
        if (hdr_can_split(tmp, ALIGN_WORD(size))) {
          hdr_split(tmp, ALIGN_WORD(size));
        }
        tmp->asize = size;
        return ret_ptr;
      }
      else{
        tmp->asize = ptr_size;
        void *new_ptr = mmalloc(size);
        for (unsigned i = 0; i < tmp->asize; i++){
          ((char *)new_ptr)[i] = ((char *)ptr)[i];
        }
        mfree(ptr);
        return new_ptr;
      }
    }
}
