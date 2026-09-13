/*
 * mm.c - An explicit free-list allocator.
 *
 * Blocks are 8-byte aligned and store a 4-byte header and footer. Free
 * blocks keep predecessor and successor pointers in their payload area.
 * The allocator uses first-fit search, immediate coalescing, and block
 * splitting when the remainder is large enough to hold a free block.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* Basic constants and macros */
#define WSIZE       4
#define DSIZE       8
#define CHUNKSIZE   (1 << 12)

#define MAX(x, y)   ((x) > (y) ? (x) : (y))
#define PACK(size, alloc)  ((size) | (alloc))

#define GET(p)      (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp)       ((char *)(bp) - WSIZE)
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

#define PRED_PTR(bp)   ((char **)(bp))
#define SUCC_PTR(bp)   ((char **)((char *)(bp) + sizeof(void *)))
#define PRED(bp)       (*PRED_PTR(bp))
#define SUCC(bp)       (*SUCC_PTR(bp))

#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)
#define MINBLOCK (ALIGN(DSIZE + 2 * sizeof(void *)))

static char *heap_listp = NULL;
static char *free_listp = NULL;

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static void insert_free(void *bp);
static void remove_free(void *bp);

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1) {
        return -1;
    }

    PUT(heap_listp, 0);
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));
    heap_listp += 2 * WSIZE;
    free_listp = NULL;

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) {
        return -1;
    }

    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;

    if (size == 0) {
        return NULL;
    }

    asize = MAX(ALIGN(size + DSIZE), MINBLOCK);

    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL) {
        return NULL;
    }

    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size;
    void *bp;

    if (ptr == NULL) {
        return;
    }

    size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));

    bp = coalesce(ptr);
    insert_free(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    size_t asize;
    size_t oldsize;
    size_t nextsize;
    size_t totalsize;
    size_t copy_size;
    void *nextbp;
    void *newptr;

    if (ptr == NULL) {
        return mm_malloc(size);
    }

    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    asize = MAX(ALIGN(size + DSIZE), MINBLOCK);
    oldsize = GET_SIZE(HDRP(ptr));

    if (asize <= oldsize) {
        if (oldsize - asize >= MINBLOCK) {
            PUT(HDRP(ptr), PACK(asize, 1));
            PUT(FTRP(ptr), PACK(asize, 1));
            nextbp = NEXT_BLKP(ptr);
            PUT(HDRP(nextbp), PACK(oldsize - asize, 0));
            PUT(FTRP(nextbp), PACK(oldsize - asize, 0));
            insert_free(coalesce(nextbp));
        }
        return ptr;
    }

    nextbp = NEXT_BLKP(ptr);
    nextsize = GET_SIZE(HDRP(nextbp));
    if (!GET_ALLOC(HDRP(nextbp)) && oldsize + nextsize >= asize) {
        remove_free(nextbp);
        totalsize = oldsize + nextsize;
        PUT(HDRP(ptr), PACK(asize, 1));
        PUT(FTRP(ptr), PACK(asize, 1));

        if (totalsize - asize >= MINBLOCK) {
            nextbp = NEXT_BLKP(ptr);
            PUT(HDRP(nextbp), PACK(totalsize - asize, 0));
            PUT(FTRP(nextbp), PACK(totalsize - asize, 0));
            insert_free(nextbp);
        }
        else {
            PUT(HDRP(ptr), PACK(totalsize, 1));
            PUT(FTRP(ptr), PACK(totalsize, 1));
        }
        return ptr;
    }

    newptr = mm_malloc(size);
    if (newptr == NULL) {
        return NULL;
    }

    copy_size = oldsize - DSIZE;
    if (size < copy_size) {
        copy_size = size;
    }
    memcpy(newptr, ptr, copy_size);
    mm_free(ptr);
    return newptr;
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;

    if (size < MINBLOCK) {
        size = MINBLOCK;
    }

    if ((long)(bp = mem_sbrk(size)) == -1) {
        return NULL;
    }

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    bp = coalesce(bp);
    insert_free(bp);
    return bp;
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    void *prevbp;
    void *nextbp;

    if (prev_alloc && next_alloc) {
        return bp;
    }
    else if (prev_alloc && !next_alloc) {
        nextbp = NEXT_BLKP(bp);
        remove_free(nextbp);
        size += GET_SIZE(HDRP(nextbp));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc) {
        prevbp = PREV_BLKP(bp);
        remove_free(prevbp);
        size += GET_SIZE(HDRP(prevbp));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(prevbp), PACK(size, 0));
        bp = prevbp;
    }
    else {
        prevbp = PREV_BLKP(bp);
        nextbp = NEXT_BLKP(bp);
        remove_free(prevbp);
        remove_free(nextbp);
        size += GET_SIZE(HDRP(prevbp)) + GET_SIZE(HDRP(nextbp));
        PUT(HDRP(prevbp), PACK(size, 0));
        PUT(FTRP(nextbp), PACK(size, 0));
        bp = prevbp;
    }

    return bp;
}

static void *find_fit(size_t asize)
{
    void *bp;

    for (bp = free_listp; bp != NULL; bp = SUCC(bp)) {
        if (GET_SIZE(HDRP(bp)) >= asize) {
            return bp;
        }
    }

    return NULL;
}

static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    remove_free(bp);

    if (csize - asize >= MINBLOCK) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
        insert_free(bp);
    }
    else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

static void insert_free(void *bp)
{
    PRED(bp) = NULL;
    SUCC(bp) = free_listp;

    if (free_listp != NULL) {
        PRED(free_listp) = bp;
    }

    free_listp = bp;
}

static void remove_free(void *bp)
{
    if (PRED(bp) != NULL) {
        SUCC(PRED(bp)) = SUCC(bp);
    }
    else {
        free_listp = SUCC(bp);
    }

    if (SUCC(bp) != NULL) {
        PRED(SUCC(bp)) = PRED(bp);
    }
}













