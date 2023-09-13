/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "memlib.h"
#include "mm.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "team 1",
    /* First member's full name */
    "Garam Jo",
    /* First member's email address */
    "latack789@naer.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

// 참고 https://dean30.tistory.com/45

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

/* Basic constants and macros */
#define WSIZE 4            /* Word and header/footer size (bytes) */
#define DSIZE 8            /* Double word size (bytes) */
#define CHUNKSIZE (1 << 7) /* Extend heap by this amount (bytes) 힙 확장을 위한 기본 크기(=초기 빈 블록의 크기) */
#define LISTLIMIT 10       /* segre list의 개수 */

/* MAX ~ PREV_BLKP 함수는 힙에 접근 순회하는데 사용할 매크로 */
#define MAX(x, y) (x > y ? x : y)

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc)) // size와 할당 비트를 결합, header와 footer에 저장할 값

/* Read and write a word at address p */
#define GET(p) (*(unsigned int *)(p))              // p가 참조하는 워드 반환(포인터라서 직접 역참조불가능 -> 타입 캐스팅)
#define PUT(p, val) (*(unsigned int *)(p) = (val)) // p에 val저장

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7) // 사이즈 (~0x7 : 11111000, '&' 연산으로 뒤에 세자리 없어짐)
#define GET_ALLOC(p) (GET(p) & 0x1) // 할당 상태

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp)-WSIZE) // header 포인터
// footer 포인터(header의 정보를 참조해서 가져오기 때문에, header의 정보를
// 변경했다면 변경된 위치의 footer가 반환됨에 유의
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE))) // 다음 블록의 포인터
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))   // 이전 블록의 포인터

#define PREP(bp) (*(void **)(bp))
#define SUCP(bp) (*(void **)(bp + WSIZE))

static void *heap_listp;
static void *free_listp;
static void *segregation_list[LISTLIMIT];
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static void putFreeBlock(void *bp);
static void removeFreeBlock(void *bp);

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void) // 최초 가용 블록으로 힙 생성
{
    int list;
    for (list = 0; list < LISTLIMIT; list++)
    {
        segregation_list[list] = NULL;
        // printf("segregation_list[%d] = %d\n", list, segregation_list[list]);
    }
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1) // 4워드 크기의 힙 생성, heap_listp에 힙의 시작 주소값 할당
        return -1;
    PUT(heap_listp, 0);                                // Alignment padding
    PUT(heap_listp + (1 * WSIZE), PACK(2 * DSIZE, 1)); // Prologue header
    PUT(heap_listp + (2 * WSIZE), PACK(2 * DSIZE, 1)); // Prologue footer

    // 에필로그 header : 프로그램이 할당한 마지막 블록의 뒤에 위치하며, 블록이
    // 할당되지 않은 상태를 나타냄
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1)); // Epilogue header
    free_listp = heap_listp + DSIZE;
    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) // 힙을 CHUNKSIZE bytes로 확장
        return -1;
    return 0;
}

static void *extend_heap(size_t words)
{
    // 요청한 크기를 인접 2워드의 배수(8바이트)로 반올림하여, 그 후에 추가적인 힙
    // 공간 요청
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    // 요청한 크기의 2워드의 배수로 반올림하고 추가 힙 공간을 요청함
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE; // 2워드의 가장 가까운 배수로 반올림 (홀수면 1 더해서 곱함)
    if ((long)(bp = mem_sbrk(size)) == -1)                    // 힙 확장 mem_brk 지점을 old_ptr로 반환함
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         // Free block header
    PUT(FTRP(bp), PACK(size, 0));         // Free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // new epilogue header

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

// 할당된 블록을 할칠 수 있는 경우 4가지에 따라 메모리 연결
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // 이전 블록 할당 상태
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // 다음 블록 할당 상태
    size_t size = GET_SIZE(HDRP(bp));                   // 현재 블록 사이즈

    // 가용블록이 없으면 조건을 추가할 필요가 없다. 맨 밑에서 free_listp에 넣어줌
    // if (prev_alloc && next_alloc) // 모두 할당된 경우
    // {
    //     /* case 1 */
    //     return bp;
    // }
    if (prev_alloc && !next_alloc) // 다음 블록만 빈 경우
    {
        /* case 2 */
        removeFreeBlock(NEXT_BLKP(bp)); // explicit에서 추가
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0)); // 현재 블록 헤더 재설정
        PUT(FTRP(bp), PACK(size, 0)); // 다음 블록 푸터 재설정 (위에서 헤더를 재설정했으므로 FTRP(bp)는 합쳐질 다음 브르록의 푸터가 됨)
    }
    else if (!prev_alloc && next_alloc) // 이전 블록만 빈 경우
    {
        /* case 3 */
        removeFreeBlock(PREV_BLKP(bp)); // explicit에서 추가
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));            // 현재 블록 푸터 재설정
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); // 이전 블록 헤더 재설정
        bp = PREV_BLKP(bp);                      // 이전 블록 시작점으로 포인터 변경
    }
    else if (!prev_alloc && !next_alloc) // 이전 블록과 다음 블록 모두 빈 경우
    {
        /* case 4 */
        removeFreeBlock(PREV_BLKP(bp)); // explicit에서 추가
        removeFreeBlock(NEXT_BLKP(bp)); // explicit에서 추가

        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); // 이전 블록 헤더 재설정
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0)); // 다음 블록 푸터 재설정
        bp = PREV_BLKP(bp);                      // 이전 블록의 시작점으로 포인터 변경
    }
    putFreeBlock(bp);
    return bp; // 병합된 블록의 포인터 반환
}

/*
    mm_malloc - Allocate a block by incrementing the brk pointer.
    Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size) // 가용 리스트에서 블록 할당 하기
{
    size_t asize;      // Adjusted block size
    size_t extendsize; // Amount to extend heap if no fit
    char *bp;

    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    // size를 바탕으로 헤더와 푸터의 공간 확보
    // 8바이트는 정렬조건을 만족하기 위해
    // 추가 8바이트는 헤더와 푸터 오버헤드를 위해서 확보
    if (size <= DSIZE)     // 8 바이트 이하이면
        asize = 2 * DSIZE; // 최소 블록 크기 16바이트 할당 (헤더 4 + 푸터 4 + 저장공간 8)
    else
        asize = DSIZE * ((size + (DSIZE) + (ALIGNMENT - 1)) / ALIGNMENT); // 8의 배수로 올림 처리

    /* Search the free list for a fit */
    // 가용 블록을 가용리스트에서 검색하고 할당기는 요청한 블록을 배치
    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize); // 할당
        return bp;        // 새로 할당된 블록의 포인터 리턴
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *bp, size_t size)
{
    size_t old_size = GET_SIZE(HDRP(bp));
    size_t new_size;          // 안되면 2 * wsize
    if (size <= DSIZE)        // 8 바이트 이하이면
        new_size = 2 * DSIZE; // 최소 블록 크기 16바이트 할당 (헤더 4 + 푸터 4 + 저장공간 8)
    else
        new_size = DSIZE * ((size + (DSIZE) + (ALIGNMENT - 1)) / ALIGNMENT); // 8의 배수로 올림 처리

    if (new_size <= old_size)
    {
        return bp;
    }
    else
    {
        size_t current_size = old_size + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        if (!GET_ALLOC(HDRP(NEXT_BLKP(bp))) && current_size >= new_size)
        {
            removeFreeBlock(NEXT_BLKP(bp));
            PUT(HDRP(bp), PACK(current_size, 1));
            PUT(FTRP(bp), PACK(current_size, 1));
            return bp;
        }
    }
    void *old_bp = bp;
    void *new_bp;
    size_t copySize;

    new_bp = mm_malloc(size);
    if (new_bp == NULL)
        return NULL;
    copySize = GET_SIZE(HDRP(old_bp));
    if (size < copySize)
        copySize = size;
    // 메모리의 특정한 부분으로부터 얼마까지의 부분을 다른 메모리 영역으로
    // 복사해주는 함수(old_bp로부터 copysize만큼의 문자를 new_bp로 복사해라
    memcpy(new_bp, old_bp, copySize);
    mm_free(old_bp);
    return new_bp;
}

// first fit 방법
static void *find_fit(size_t asize)
{
    // 적절한 가용 블록을 검색하고 가용블록의 주소를 반환한다
    // first fit 검색을 수행한다. -> 리스트 처음부터 탐색하여 가용블록 찾기
    void *bp;
    int list = 0;
    size_t searchsize = asize;

    while (list < LISTLIMIT)
    {
        if ((list == LISTLIMIT - 1) || (searchsize <= 1) && (segregation_list[list] != NULL))
        {
            bp = segregation_list[list];
            while ((bp != NULL) && (searchsize > GET_SIZE(HDRP(bp)))){
                bp = SUCP(bp);
            }
            if (bp != NULL)
                return bp;
        }
        asize >>= 1;
        list++;
    }
    return NULL;
}

static void place(void *bp, size_t asize)
{
    // 맞는 블록을 찾으면 요청한 블록을 배치하고 초과부분을 분할한다.
    size_t csize = GET_SIZE(HDRP(bp));
    removeFreeBlock(bp); // free_listp에서 해당 블록 제거
    if ((csize - asize) >= (2 * DSIZE))
    {
        // 가용 블록에 사이즈 - 요청한 블록의 사이즈 각 더블워드*2 크거나 같을때
        // 요청 블록을 넣고 남은 사이즈는 가용 블록으로 분할
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
        // putFreeBlock(bp); // 남은 브록을 free_listp에 추가
        coalesce(bp);
    }
    else
    {
        // 할당하고 남은 블록이 더블워드*2보다 작다며 나누지 않고 할당
        //  남은 블록이 더블워드*2보다 작은 경우는 데이터를 담을 수 없음
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

// 이해 안됨 질문하기
// 새로 반환되거나 생성된 가용 블록을 free list 첫 부분에 넣는다
void putFreeBlock(void *bp)
{
    int size = GET_SIZE(HDRP(bp));
    int list = 0;
    while ((list < LISTLIMIT - 1) && (size > 1))
    {
        size >>= 1;
        list++;
    }
    PREP(bp) = NULL;
    SUCP(bp) = segregation_list[list];
    if (segregation_list[list] != NULL)
        PREP(segregation_list[list]) = bp;
    segregation_list[list] = bp;
}

// 이해 안됨 질문하기
static void removeFreeBlock(void *bp)
{
    int size = GET_SIZE(HDRP(bp));
    int list = 0;
    while ((list < LISTLIMIT - 1) && (size > 1))
    {
        size >>= 1;
        list++;
    }
    if (PREP(bp) != NULL)
        SUCP(PREP(bp)) = SUCP(bp);
    else
        segregation_list[list] = SUCP(bp);
    if (SUCP(bp) != NULL)
        PREP(SUCP(bp)) = PREP(bp);
}