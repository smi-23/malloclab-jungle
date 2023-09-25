/*********************************************************
 * explicit, segregate fit으로 구현했습니다.
 ********************************************************/
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
// test commit
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
    "latack789@naver.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

// 참고 https://dean30.tistory.com/45

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Basic constants and macros */
#define WSIZE 4             /* Word and header/footer size (bytes) */
#define DSIZE 8             /* Double word size (bytes) */
#define CHUNKSIZE (1 << 10) /* Extend heap by this amount (bytes) 힙 확장을 위한 기본 크기(=초기 빈 블록의 크기) */
#define LISTLIMIT 15        /* segregation_list의 개수 */

/* MAX ~ PREV_BLKP 함수는 힙에 접근 순회하는데 사용할 매크로 */
#define MAX(x, y) (x > y ? x : y)

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc)) // size와 할당 비트를 결합, header와 footer에 저장할 값(size, alloc)

/* Read and write a word at address p */
#define GET(p) (*(unsigned int *)(p))              // p가 참조하는 워드 반환(포인터라서 직접 역참조불가능 -> 타입 캐스팅)
#define PUT(p, val) (*(unsigned int *)(p) = (val)) // p에 val저장

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7) // 사이즈 (~0x7 : 11111000, '&' 연산으로 뒤에 세자리 없어짐)
#define GET_ALLOC(p) (GET(p) & 0x1) // 할당 상태를 판단하기 위한 매크로 함수 0이면 free, 1이면 alloc

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp)-WSIZE) // head의 주소를 알려주는 매크로 head의 주소는 bp앞이기 때문에 WSIZE만큼 뺌
// footer 포인터(header의 정보를 참조해서 가져오기 때문에, header의 정보를
// 변경했다면 변경된 위치의 footer가 반환됨에 유의
// 현재 사이즈 만큼 이동하면 다음 bp로 가게 되는데 여기서 head만큼 WSIZE빼고 한번더 WSIZE를 빼주면 현재 footer의 주소를 알려줌
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
// 현재 bp에서 head로 간뒤 사이즈를 읽어서 bp에서 사이즈만큼 이동하면 다음 bp를 알 수 있음
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE))) // 다음 블록의 포인터
// 현재 bp에서 DSIZE만큼 반대로 이동해 footer에서 이전 블록의 사이즈를 읽고 그 사이즈만큼 이동하면 이전 bp를 알 수 있음
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE))) // 이전 블록의 포인터

// bp를 이용해서 free블록 내의  predevvessor블록의 워드에 담긴 주소가 가리키는 곳으로 가는 이중포인터
#define PREP(bp) (*(void **)(bp))
// bp를 이용해서 free블록 내의 successor 블록(predeccessor블록 한칸 뒤)의 워드에 담긴 주소가 가리키는 곳으로 가는 이중 포인터
#define SUCP(bp) (*(void **)(bp + WSIZE))

static void *heap_listp;
static void *segregation_list[LISTLIMIT];
static void *extend_heap(size_t words);    // 가용 공간을 만들어 주는 함수
static void *coalesce(void *bp);           // 가용 블록들을 병합하는 함수
static void *find_fit(size_t asize);       // 가용 블록을 탐색하는 함수
static void place(void *bp, size_t asize); // 할당하는 함수
static void putFreeBlock(void *bp);        // 가용 리스트에 추가하는 함수
static void removeFreeBlock(void *bp);     // 가용 리스트에서 제거하는 함수

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void) // 최초 가용 블록으로 힙 생성
{
    int list;
    // 리스트 포인터를 NULL로 초기화
    for (list = 0; list < LISTLIMIT; list++)
    {
        segregation_list[list] = NULL;
    }
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1) // 4워드 크기의 힙 생성, heap_listp에 힙의 시작 주소값 할당
        return -1;
    PUT(heap_listp, 0);                            // Alignment padding
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); // Prologue header
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); // Prologue footer

    // 에필로그 header : 프로그램이 할당한 마지막 블록의 뒤에 위치하며, 블록이
    // 할당되지 않은 상태를 나타냄
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1)); // Epilogue header

    // heap_listp를 prologue header의 위치로 초기화
    heap_listp += DSIZE;
    if (extend_heap(4) == NULL)
        return -1;
    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) // 힙을 CHUNKSIZE bytes로 확장
        return -1;
    return 0;
}

// word단위로 인자를 받아서 heap을 생성함
static void *extend_heap(size_t words)
{
    // 요청한 크기를 인접 2워드의 배수(8바이트)로 반올림하여, 그 후에 추가적인 힙
    char *bp;
    size_t size; // 힙 영역의 크기를 담을 변순 선언

    /* Allocate an even number of words to maintain alignment */
    // 요청한 크기의 2워드의 배수로 반올림하고 추가 힙 공간을 요청함
    // 2워드의 가장 가까운 배수로 반올림 (홀수면 1 더해서 곱함)
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    // 힙 확장 mem_brk 지점을 old_ptr로 반환함
    // 새 메모리의 첫 부분을 bp로 두고 주소값은 int로는 못 받아서 long으로 casting
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    // 자연스럽게 bp는 header의 뒤가 됨
    PUT(HDRP(bp), PACK(size, 0));         // Free block header 에필로그 헤더를 헤더로 변경
    PUT(FTRP(bp), PACK(size, 0));         // Free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // new epilogue header 맨 끝에 새로운 에필로그 헤더를 삽입

    /* Coalesce if the previous block was free */
    return coalesce(bp); // 이전 블록이 비었다면 연결하고 bp를 반환
}

// 할당된 블록을 할칠 수 있는 경우 4가지에 따라 메모리 연결
// 합병 뒤 가용 블록의 주소를 반환
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // 이전 블록 할당 상태
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // 다음 블록 할당 상태
    size_t size = GET_SIZE(HDRP(bp));                   // 현재 블록 사이즈

    // 가용블록이 없으면 조건을 추가할 필요가 없다. 넣고 싶다면 if문을 수정해야 한다.
    //     /* case 1 */
    // if (prev_alloc && next_alloc) // 모두 할당된 경우
    // {
    //     putFreeBlock(bp);
    //     return bp;
    // }

    // 다음 블록만 빈 경우
    if (prev_alloc && !next_alloc)
    {
        /* case 2 */
        removeFreeBlock(NEXT_BLKP(bp)); // 다음 블록을 리스트에서 제거
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        // 현재 블록 헤더 재설정
        PUT(HDRP(bp), PACK(size, 0));
        // 다음 블록 푸터 재설정 (위에서 헤더를 재설정했으므로 FTRP(bp)는 합쳐질 다음 블록의 푸터가 됨)
        PUT(FTRP(bp), PACK(size, 0));
    }

    // 이전 블록만 빈 경우
    else if (!prev_alloc && next_alloc)
    {
        /* case 3 */
        removeFreeBlock(PREV_BLKP(bp)); // 이전 블록을 리스트에서 제거
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));            // 현재 블록 푸터 재설정
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); // 이전 블록 헤더 재설정
        bp = PREV_BLKP(bp);                      // 이전 블록 시작점으로 포인터 변경
    }

    // 이전 블록과 다음 블록 모두 빈 경우
    else if (!prev_alloc && !next_alloc)
    {
        /* case 4 */
        removeFreeBlock(PREV_BLKP(bp)); // 이전 블록을 리스트에서 제거
        removeFreeBlock(NEXT_BLKP(bp)); // 다음 블록을 리스트에서 제거

        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); // 이전 블록 헤더 재설정
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0)); // 다음 블록 푸터 재설정
        bp = PREV_BLKP(bp);                      // 이전 블록의 시작점으로 포인터 변경
    }
    putFreeBlock(bp); // 여러 case들을 통해 새롭게 만들어진 free block의 bp를 리스트에 추가
    return bp;        // 병합된 블록의 포인터 반환
}

/*
    mm_malloc - Allocate a block by incrementing the brk pointer.
    Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size) // 가용 리스트에서 블록 할당 하기
{
    size_t asize = 16; // Adjusted block size
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
    // 가용 블록을 가용리스트에서 검색하고 할당기는 요청한 블록을 배치
    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize); // 할당
        return bp;        // 새로 할당된 블록의 포인터 리턴
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);                 // 만약 맞는 크기의 가용 블록이 없다면 새로 힙을 늘림
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL) // 둘 중 더 큰 값을 사이즈로
        return NULL;
    place(bp, asize); // 새 힙에 메모리를 할당
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0)); // free로 바꿔줌
    PUT(FTRP(bp), PACK(size, 0)); // free로 바꿔줌
    coalesce(bp);                 // 인접 블록을 확인하고 free라면 병합
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
// size에 해당 블록의 사이즈가 변경되길 원하는 사이즈를 담는다.
void *mm_realloc(void *bp, size_t size)
{
    // 다음 블록과 병합
    size_t old_size = GET_SIZE(HDRP(bp));
    size_t new_size;          // 안되면 2 * wsize
    if (size <= DSIZE)        // 순수 payload의 크기가 8 바이트 이하이면
        new_size = 2 * DSIZE; // 최소 블록 크기 16바이트 할당 (헤더 4 + 푸터 4 + 저장공간 8)
    else
        new_size = DSIZE * ((size + (DSIZE) + (ALIGNMENT - 1)) / ALIGNMENT); // 8의 배수로 올림 처리

    // 만약 최소 할당 사이즈인 new_size보다 old_size가 더 크거나 같다면 바로 할당하고 bp를 반환
    if (new_size <= old_size)
    {
        return bp;
    }
    else
    {
        // 만약 다음 블록이 free이고, 다음블록과 할당하고자 했던 블록의 합이 new_size보다 크거나 같다면 병합 후 할당 가능
        size_t current_size = old_size + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        if (!GET_ALLOC(HDRP(NEXT_BLKP(bp))) && current_size >= new_size)
        {
            removeFreeBlock(NEXT_BLKP(bp));       // 병합할거니까 다음 블록을 가용리스트에서 삭제
            PUT(HDRP(bp), PACK(current_size, 1)); // 새로운 사이즈를 헤드에 기입하고 할당
            PUT(FTRP(bp), PACK(current_size, 1)); // footer도 마찬가지
            return bp;
        }
    }
    // 위 경우가 불가능 하다면 아래 수행
    // 새롭게 사이즈에 맞게 할당 후 기존 정보를 복사해서 넣어준 뒤, 기존 블록 free
    void *old_bp = bp;
    void *new_bp;
    size_t copySize;

    new_bp = mm_malloc(size);
    if (new_bp == NULL)
        return NULL;
    copySize = GET_SIZE(HDRP(old_bp));

    // 참이면 사이즈를 줄이는 경우, 거짓이면 사이즈만큼
    if (size < copySize)
        copySize = size;
    // 메모리의 특정한 부분으로부터 얼마까지의 부분을 다른 메모리 영역으로
    // 복사해주는 함수(old_bp로부터 copysize만큼의 문자를 new_bp로 복사해라

    memcpy(new_bp, old_bp, copySize); // 새 블록으로 복사
    mm_free(old_bp);                  // 예전 블록은 free
    return new_bp;
}

// first-fit 해당 블록의 사이즈가 속할 수 있는 사이즈 범위를 가진 연결리스트를 탐색 후 그 연결리스트 내에서 적절한 블록을 또 탐색
static void *find_fit(size_t asize)
{
    // 적절한 가용 블록을 검색하고 가용블록의 주소를 반환한다
    // first fit 검색을 수행한다. -> 리스트 처음부터 탐색하여 가용블록 찾기
    int list = 0; // 인덱스 역할
    void *bp;
    size_t size = asize;
    // 사이즈의 인덱스를 찾아주는 반복문
    while ((list < LISTLIMIT - 1) && (size > 1))
    {
        size >>= 1;
        list++;
    }
    // bp에 해당 인덱스의 연결리스트를 저장
    bp = segregation_list[list];
    while (list < LISTLIMIT)
    {
        bp = segregation_list[list];
        for (bp; bp != NULL; bp = SUCP(bp))
        {
            if (GET_SIZE(HDRP(bp)) >= asize) // 해당 가용블록이 asize보다 크거나 같다면 해당 bp를 리턴
            {
                return bp;
            }
        }
        list++;
    }
    return NULL;
}

static void place(void *bp, size_t asize)
{
    // 맞는 블록을 찾으면 요청한 블록을 배치하고 초과부분을 분할한다.
    size_t csize = GET_SIZE(HDRP(bp));
    removeFreeBlock(bp); // 할당하는 블록은 연결리스트에서 제거
    if ((csize - asize) >= (2 * DSIZE))
    {
        // 가용 블록에 사이즈 - 요청한 블록의 사이즈 각 더블워드*2 크거나 같을때
        // 요청 블록을 넣고 남은 사이즈는 가용 블록으로 분할
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));

        // 초과 부분은 가용으로 전환
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
        putFreeBlock(bp); // 연결리스트에 추가 해줌
    }
    else
    {
        // 할당하고 남은 블록이 더블워드*2보다 작다며 나누지 않고 할당
        //  남은 블록이 더블워드*2보다 작은 경우는 데이터를 담을 수 없음
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

// 연결리스트에 추가하는 함수
void putFreeBlock(void *bp)
{
    int size = GET_SIZE(HDRP(bp));
    int list = 0;

    // 해당 사이즈가 들어갈 수 있는 리스트를 탐색 후 인덱스를 구함
    while ((list < LISTLIMIT - 1) && (size > 1))
    {
        size >>= 1;
        list++;
    }

    PREP(bp) = NULL;
    SUCP(bp) = segregation_list[list]; // bp에 연결리스트의 처음을 저장

    if (segregation_list[list] != NULL)
        PREP(segregation_list[list]) = bp; //  기존 첫번째 노드의 pre를 수정

    segregation_list[list] = bp; // bp는 연결리스트의 처음을 가리킴
}

// 연결리스트에서 제거하는 함수
static void removeFreeBlock(void *bp)
{
    int size = GET_SIZE(HDRP(bp));
    int list = 0;

    // 해당 사이즈가 들어갈 수 있는 리스트를 탐색 후 인덱스를 구함
    while ((list < LISTLIMIT - 1) && (size > 1))
    {
        size >>= 1;
        list++;
    }
    void *head = segregation_list[list];

    if (bp == head)
    {
        if (SUCP(bp) != NULL)
        {
            PREP(SUCP(bp)) = NULL;
        }
        segregation_list[list] = SUCP(bp);
    }
    else
    {
        SUCP(PREP(bp)) = SUCP(bp);
        if (SUCP(bp) != NULL)
        {
            PREP(SUCP(bp)) = PREP(bp);
        }
    }
}