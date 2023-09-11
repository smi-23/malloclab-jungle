 // size_t old_size = GET_SIZE(HDRP(bp));
    // size_t new_size = size + (2 * WSIZE); // 2*WISE는 헤더와 풋터

    // // new_size가 old_size보다 작거나 같으면 기존 bp 그대로 사용
    // if (new_size <= old_size)
    // {
    //     return bp;
    // }
    // // new_size가 old_size보다 크면 사이즈 변경
    // else
    // {
    //     size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    //     size_t current_size = old_size + GET_SIZE(HDRP(NEXT_BLKP(bp)));

    //     // next block이 가용상태이고 old, next block의 사이즈 합이 new_size보다 크면 그냥 그거 바로 합쳐서 쓰기
    //     if (!next_alloc && current_size >= new_size)
    //     {
    //         PUT(HDRP(bp), PACK(current_size, 1));
    //         PUT(FTRP(bp), PACK(current_size, 1));
    //         return bp;
    //     }
    //     // 아니면 새로 block 만들어서 거기로 옮기기
    //     else
    //     {
    //         void *new_bp = mm_malloc(new_size);
    //         place(new_bp, new_size);
    //         memcpy(new_bp, bp, new_size); // 메모리의 특정한 부분으로부터 얼마까지의 부분을 다른 메모리 영역으로 복사해주는 함수(old_bp로부터 new_size만큼의 문자를 new_bp로 복사해라!)
    //         mm_free(bp);
    //         return new_bp;
    //     }
    // }