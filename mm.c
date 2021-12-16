
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
    "week06-04",
    /* First member's full name */
    "Jin",
    /* First member's email address */
    "tmdgus3901@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

//segregated fit 구현

/* Basic constants and macros */
#define WSIZE       4 // word와 header/footer의 고정 사이즈.
#define DSIZE       8 // Double word 사이즈
#define CHUNKSIZE   (1 << 12) //힙의 확장 사이즈, 초기 가용 블록과 힙 확장을 위한 기본 크기
#define INITCHUNKSIZE (1 << 6)
#define MAX(x,y)    ((x) > (y) ? (x) : (y)) // x,y중 큰 값을 구하는 매크로 함수 x> y 가 참이면 x, 거짓이면 y

//여기서 부터 가용리스트에 접근하고 방문하는 작은 매크로들을 정의한다.
/*Pack a size and allocated bit into a word */ 
// #define PACK(size, alloc) ((size) | (alloc)) // size와 alloc(할당여부) 값을 하나의 word로 묶는다. implict list에 이용
#define PACK(size, alloc) ( ((size) | (alloc))) //size와 alloc(할당여부)값을 하나의 word로 묶는다. explicit list에 이용.


/*Read and write a word at address p */
#define GET(p) (*(unsigned int *)(p)) //포인터 p가 기키는 주소의 값을 읽는다. p를 unsigned int형으로 형변환후 그 주소의 값을 가져온다. 
#define PUT(p, val) (*(unsigned int *)(p) = (val)) //포인터 p가 가리키는 주소에 val값을 쓴다.

/* Read the size and allocated fields from address p  */
#define GET_SIZE(p) (GET(p) & ~0x7) // 포인터 p가 가리키는 주소의 값의 하위 3비트를 버려서 header에서의 block size를 읽는다. 7은 16진수로 111.
#define GET_ALLOC(p) (GET(p) & 0x1) // 포인터 p가 가리키는 주소의 값의 하위 1비트를 읽어서 header에서의 alloc bit를 읽는다. 0이면 가용, 1이면 alloc

/* Given block ptr bp, compute address of its header and footer  bp(현재 블록의 포인터)로 현재 블록의 header위치와 footer위치 반환*/
#define HDRP(bp) ((char *)(bp) - WSIZE) //포인터 bp의 header 주소를 계산한다.
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) // 포인터 bp의 footer 주소를 계산한다.

/* Given block ptr bp, compute address of next and previous blocks 다음과 이전 블록의 포인터 반환*/
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)- WSIZE))) //다음 블록 bp 위치 반환 (bp + 현재 블록의 크기), 포인터bp를 이용해서 다음 블록의 주소를 얻는다.
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp)- DSIZE))) //이전 블록 bp 위치 반환 (bp - 이전 블록의 크기), 포인터bp를 이용해서 이전 블록의 주소를 얻는다.

#define PRED_FREEP(bp)  (*(void**)(bp)) //  *(GET(PRED_FREEP(bp))) == 다음 가용리스트의 bp // Predecessor
#define SUCC_FREEP(bp)  (*(void**)(bp + WSIZE)) //  *(GET(SUCC_FREEP(bp))) == 다음 가용리스트의 bp // Successor
// segreagated 추가된 부분
#define LISTLIMIT 20    /* 가용 리스트 size 클래스를 20개 만든다. */

// Declaration(선언)
static void *segregated_list[LISTLIMIT]; // 20개의 배열 포인터를 만든다.
static char *heap_listp = NULL; //할당기는 한개 정적 전역변수를 사용하며 항상 프롤로그 블록을 가리킨다.(작은 최적화로 프롤로그 블록 대신에 다음 블록을 가리키게 할 수 있다.)

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize); 
static void *place(void *bp, size_t asize);
static void removeBlock(void *bp); //할당을 해제하는 함수
static void insert_block(void * bp, size_t size); //가용 리스트에 블록을 넣는 함수


int mm_init(void) /* mm_malloc 이나 mm_free를 호출하기전 mm_init 함수 호출해서 힙을 초기화해야 한다. 최초 가용 블록으로 힙 생성하기.*/
{   
    int list;
    // segregated_list 초기화 해주는 부분.
    for (list = 0; list < LISTLIMIT; list ++){
        segregated_list[list] = NULL;
    }
    /* create the inintial empty heap */
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1) //heap_listp가 힙의 최댓값 이상을 요청한다면 fail, 16바이트를 호출한다.
        return -1;    
        
    PUT(heap_listp, 0); // 미사용 패딩
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); //Prologue header
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); //Prologue footer
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));  //Epilogue header

    heap_listp = heap_listp + 2 * WSIZE;
    
    
    /* extend_heap 함수를 호출해서 힙을 CHUNKSIZE 바이트로 확장하고 초기 가용 블록을 생성한다.
        여기 까지 할당기는 초기화를 완료하고, 어플리케이션으로 부터 할당과 반환 요청을 받을 준비를 완료하였다.*/
    if (extend_heap(INITCHUNKSIZE / WSIZE) == NULL) //WSIZE로 나눌지 DSIZE로 나눌지 정하자. 
        return -1;
    
    return 0;
}

static void *extend_heap(size_t words) 
/* 두가지 경우에 호출된다. 
1) 힙이 초기화 될때  
2) mm_malloc이 적당한 맞춤 fit을 찾지 못했을때, 정렬을 유지하기 위해 요청한 크기를 인접 2워드의 배수(8바이트)로 반올림 하며, 그 후에 메모리 시스템으로부터
    추가적인 힙 공간을 요청한다. */
{
    char *bp;
    size_t size; // size_t 자료형은 unsigned long 타입이고 4바이트의 크기를 가진다.
    // 삼항 연산자 ? 는 조건 ? 참일때 값 : 거짓일때의 값;
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE; 
    /*힙은 더블 워드 경계에서 시작하고, extend_heap 으로 가는 모든 호출은 그 크기가 더블 워드의 배수인 블록을 리턴한다. 따라서 mem_sbrk로의 모든 호출은 
      에필로그 블록의 헤더에 곧이어서 더블 워드 정렬된 메모리 블록을 리턴한다. */
    if ((long)(bp = mem_sbrk(size)) == -1) //공간 확장 실패
        return NULL;

    PUT(HDRP(bp), PACK(size, 0)); //이 헤더는 새 가용 블록의 헤더가 되고
    PUT(FTRP(bp), PACK(size, 0)); //이 푸터는 새 가용 블록의 푸터가 된다.
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // 이블록의 마지막 워드는 새 에필로그의 블록 헤더가 된다.
    /* extend_heap 블록 너머에 오지 않도록 배치한 블록 다음 공간을 블록이라 가정하고 epilogue header 배치
    (실제로는 존재하지 않는 블록) */    
    
    // Coalesce if the previous block was free
    // insert_block(bp, size); //? 이게 들어가면 터지는 이유? 
    return coalesce(bp); //마지막으로 이전 힙이 가용 블록으로 끝났다면, 두 개의 가용 블록을 통합하기 위해 coalesce함수를 호출하고, 통합된 블록의 블록 포인터를 반환한다.

}
/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    
    size_t asize; // adjusted block size
    size_t extendsize; //들어갈 자리가 없을 때 늘려야 하는 힙의 용량
    // char *bp;
    void *bp; 

    // Ignore spurious requests 할당 사이즈가 0이면 할당하지 않는다.
    if (size == 0)
        return NULL;
    if (size <= DSIZE) // 2words 이하의 사이즈는 4워드로 할당 요청(header 1word, footer 1word)
        asize = 2*DSIZE;    
    else // 8보다 크면 아래 만큼의 사이즈를 asize로 한다.
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE); //할당 요청의 용량이 2words 초과 시, 충분한 8byte의 배수 용량 할당 C언어는 파이썬과 다르게 나누기하면 몫을 취함
    
    // /* Search the free list for a fit */
    // if ((bp = find_fit(asize)) != NULL){//힙에서 할당할만한 블록을 찾은 다음 찾았으면 place 함수로 할당.
    //     place(bp,asize);
    //     return bp;
    // }
    bp = find_fit(asize);

    if (bp == NULL){
        // 할당할만한 블록을 찾지 못했으면 asize 와 CHUNKSIZE 중 큰 값을 찾는다.
        extendsize = MAX(asize, CHUNKSIZE); //

        if ((bp = extend_heap(extendsize)) == NULL)
            return NULL;
    }
    // 확장하여 커진 힙 위치에 할당한다.
    bp = place(bp, asize);
    return bp;    
}

/*
 * mm_free - Freeing a block does nothing.
 */
// 응용은 이전에 할당한 블록을 mm_free함수를 호출해서 반환하머 이것은 요청한 블록을 반환하고(bp), 인접 가용블록들을 경계 태그 연결 기술을 사용해서 통합한다.
void mm_free(void *bp) 
{   if (bp == 0)
        return; //잘못된 free 요청인 경우 함수 종료. 이전 프로시저로 return
    size_t size = GET_SIZE(HDRP(bp)); //bp의 header에서 block size를 읽어온다.

    // 실제로 데이터를 지우는 것이 아니라
    // header, footer의 최하위 1bit(1,할당된 상태)만을 0으로 수정.

    PUT(HDRP(bp), PACK(size, 0)); //bp의 header에 block size와 alloc = 0을 저장
    PUT(FTRP(bp), PACK(size, 0)); //bp의 footer에 block size와 alloc = 0을 저장

    coalesce(bp); //주위에 빈 블록이 있을 시 병합한다.
}

static void *coalesce(void *bp)
{   
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); 
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc){
        insert_block(bp, size);
        return bp;
    }

    else if (prev_alloc && !next_alloc) { //이전 블록 할당블록이고 다음블록이 가용 블록이면,
        removeBlock(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    else if (!prev_alloc && next_alloc){
        removeBlock(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        bp = PREV_BLKP(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    else if (!prev_alloc && !next_alloc){
        removeBlock(PREV_BLKP(bp));
        removeBlock(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp))); //여기도 바뀐게 있다 푸터 헤더
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    //연결된 블록을 가용리스트에 추가
    insert_block(bp, size);
    return bp;         
}


static void *find_fit(size_t asize)
{
    /*first-fit search */
    void *bp;
    int list = 0;
    size_t searchsize = asize;

    while (list < LISTLIMIT){
        if ((list == LISTLIMIT -1) || (searchsize <= 1) && (segregated_list[list] != NULL))
        {
            bp = segregated_list[list];
            while ((bp != NULL) && (asize > GET_SIZE(HDRP(bp))))
            {
                bp = SUCC_FREEP(bp);
            }
            if (bp != NULL){
                return bp;
            }
        }
        searchsize >>= 1;
        list ++;
    }
    
    return NULL;    //NO fit

}

static void *place(void *bp, size_t asize) //할당할 블록을 찾았을 경우 실제로 할당을 진행하는 함수. 할당할 블록을 가리키는 포인터bp와 할당할 크기를 갖는 asize.
{
    size_t csize = GET_SIZE(HDRP(bp));
    size_t remainder = csize - asize;
    // 할당될 블록이니 가용리스트 내부에서 제거해준다.
    removeBlock(bp);
    
    if (remainder <= 2*DSIZE){ // remainder 차이가 네 칸(16byte)보다 작다면 해당 블록 통째로 사용.
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
    else if(asize >= 100){ /*remainder >2*DSIZE 일때 asize >= 100 인 경우 */
        PUT(HDRP(bp), PACK(remainder, 0));
        PUT(FTRP(bp), PACK(remainder, 0));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(asize, 1));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(asize, 1));
        insert_block(bp, remainder);
        return NEXT_BLKP(bp);
    }
    else{
        PUT(HDRP(bp), PACK(asize, 1)); //외부 단편화 방지를 위해 bp의 헤더와 푸터에 사이즈를 asize 만 기록하고 alloc bit = 1 로 한다
        PUT(FTRP(bp), PACK(asize, 1));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(remainder, 0)); // 다음 블록의 헤더와 푸터에 remainder 값을 업데이트 하고 alloc 상태를 0으로 만든다.
        PUT(FTRP(NEXT_BLKP(bp)), PACK(remainder, 0));
        insert_block(NEXT_BLKP(bp), remainder);
    }
    return bp;
}


//항상 가용리스트 맨 뒤에 프롤로그 블록이 존재하고 있기 때문에 조건을 간소화할 수 있다.
static void removeBlock(void *bp) // 블록을 삭제하고 전 후 블록간의 관계를 조정해주는 함수
{   
    int list = 0;
    size_t size = GET_SIZE(HDRP(bp));

    while ((list < LISTLIMIT -1) && (size > 1))
    {
        size >>= 1;
        list ++;
    }
    
    if (SUCC_FREEP(bp) != NULL){
        if(PRED_FREEP(bp) != NULL){
            PRED_FREEP(SUCC_FREEP(bp)) = PRED_FREEP(bp);
            SUCC_FREEP(PRED_FREEP(bp)) = SUCC_FREEP(bp);
        }else{
            PRED_FREEP(SUCC_FREEP(bp)) = NULL;
            segregated_list[list] = SUCC_FREEP(bp);
        }
    }else{
        if (PRED_FREEP(bp) != NULL){
            SUCC_FREEP(PRED_FREEP(bp)) = NULL;
        }else{
            segregated_list[list] = NULL;
        }
    }
    return ; //반환형이 없는 보이드형 함수라서 return이없어도 된다. 터진다면 static 로 바꾸고 return을 넣어보자.
}

static void insert_block(void *bp, size_t size)
{
    int list = 0;
    void * search_ptr;
    void * insert_ptr = NULL;

    while ((list < LISTLIMIT - 1) && (size > 1)){
        size >>= 1;
        list ++;
    }

    search_ptr = segregated_list[list];
    while ((search_ptr != NULL) && (size > GET_SIZE(HDRP(search_ptr)))){
        insert_ptr = search_ptr;
        search_ptr = SUCC_FREEP(search_ptr);
    }

    if (search_ptr != NULL){
        if (insert_ptr != NULL){
            SUCC_FREEP(bp) = search_ptr;
            PRED_FREEP(bp) = insert_ptr;
            PRED_FREEP(search_ptr) = bp;
            SUCC_FREEP(insert_ptr) = bp;
        }else{
            SUCC_FREEP(bp) = search_ptr;
            PRED_FREEP(bp) = NULL;
            PRED_FREEP(search_ptr) = bp;
            segregated_list[list] = bp;
        }
    }else{
        if (insert_ptr != NULL){
            SUCC_FREEP(bp) = NULL;
            PRED_FREEP(bp) = insert_ptr;
            SUCC_FREEP(insert_ptr) = bp;
        }else{
            SUCC_FREEP(bp) = NULL;
            PRED_FREEP(bp) = NULL;
            segregated_list[list] = bp;
        }
    }
    return;  //함수가 반환이 없는 void형이라서 반환이 없어도 된다. 필요하다면 void형을 바꾸자. 
}


/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 기존에 malloc으로 동적 할당된 메모리 크기를 변경시켜주는 함수
 현재 메모리에 bp가 가르키는 사이즈를 할당할 만큼 충분하지 않다면 메모리의 다른 공간의 기존 크기의 공간 할당 + 기존에 있던 데이터를 복사한 후 추가로 메모리 할당
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    if (oldptr == NULL){
        return mm_malloc(size);
    }

    newptr = mm_malloc(size);

    if (newptr == NULL)
      return NULL;

    copySize = GET_SIZE(HDRP(oldptr)); //헤더로 부터 블록 사이즈를 읽는다.
    // copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}














