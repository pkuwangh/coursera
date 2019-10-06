#include "types.h"
#include "stat.h"
#include "user.h"
#include "param.h"

// Memory allocator by Kernighan and Ritchie,
// The C programming Language, 2nd ed.  Section 8.7.

typedef long Align;

union header {
  struct {
    // header arranged in a linked list
    union header *ptr;
    uint size;
  } s;
  Align x;
};

typedef union header Header;

static Header base;
static Header *freep;

void
free(void *ap)
{
  Header *bp, *p;

  // find the header
  bp = (Header*)ap - 1;
  // find the insert position in the linked free list 
  for(p = freep; !(bp > p && bp < p->s.ptr); p = p->s.ptr)
    if(p >= p->s.ptr && (bp > p || bp < p->s.ptr))
      break;
  // bp should be inserted b/w p & p->s.ptr
  // try to merge w/ p
  if(bp + bp->s.size == p->s.ptr){
    bp->s.size += p->s.ptr->s.size;
    bp->s.ptr = p->s.ptr->s.ptr;
  } else
    bp->s.ptr = p->s.ptr;
  // try to merge w/ p->s.ptr
  if(p + p->s.size == bp){
    p->s.size += bp->s.size;
    p->s.ptr = bp->s.ptr;
  } else
    p->s.ptr = bp;
  freep = p;
}

static Header*
morecore(uint nu)
{
  char *p;
  Header *hp;

  if(nu < 4096)
    nu = 4096;
  // sbrk() increment the program's data space by increment bytes
  // it changes the location of the program break, which defines the end of the process' data segment
  // increasing the program break has the effect of allocating memory to the process
  p = sbrk(nu * sizeof(Header));
  if(p == (char*)-1)
    return 0;
  hp = (Header*)p;
  hp->s.size = nu;
  // calling free() to proper book-keeping
  free((void*)(hp + 1));
  return freep;
}

void*
malloc(uint nbytes)
{
  Header *p, *prevp;
  uint nunits;

  // # of units in header size
  nunits = (nbytes + sizeof(Header) - 1)/sizeof(Header) + 1;
  if((prevp = freep) == 0){
    // free pointer at addr 0
    base.s.ptr = freep = prevp = &base;
    base.s.size = 0;
  }
  // 2-pointer to traverse a linked list
  // start from the next of where freep points
  for(p = prevp->s.ptr; ; prevp = p, p = p->s.ptr){
    if(p->s.size >= nunits){
      // found one chunk that can fit in
      if(p->s.size == nunits)
        prevp->s.ptr = p->s.ptr;
      else {
        // split out the later part as allocated
        p->s.size -= nunits;
        p += p->s.size;
        p->s.size = nunits;
      }
      freep = prevp;
      return (void*)(p + 1);
    }
    // wrap around to freep, try to allocate more space
    if(p == freep)
      if((p = morecore(nunits)) == 0)
        return 0;
  }
}
