// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct{
  struct spinlock lock;
  int cnt[PHYSTOP/PGSIZE];
}ref;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&ref.lock,"ref");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
  {
    ref.cnt[(uint64)p/PGSIZE] = 1;
    kfree(p);
  }
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // 只有当引用数为0时才释放，否则将引用数-1
  acquire(&ref.lock);
  if(--ref.cnt[(uint64)pa/PGSIZE] == 0){
    memset(pa, 1, PGSIZE);
    r = (struct run*)pa;
    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
    release(&ref.lock);
  }else{
    release(&ref.lock);
  }
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
  {
    acquire(&ref.lock);
    ref.cnt[(uint64)r/PGSIZE] = 1; // 设置初始值
    memset((char*)r, 5, PGSIZE); // fill with junk
    release(&ref.lock);
  }
  return (void*)r;
}

// 传入页表和虚拟地址
// 返回1是cow页，返回0不是
int cowpage(pagetable_t pagetable, uint64 va)
{
  if(va > MAXVA)
    return 0;
  pte_t *pte = walk(pagetable,va,0); //得到pte
  if(pte == 0)
    return 0;
  if((*pte & PTE_V) == 0)
    return 0;
  if((*pte & PTE_F )== 0)
    return 0;
  return 1;
}
// 传入页表和虚拟地址
// 返回分配的物理页的地址 返回0表示失败
void* cowalloc(pagetable_t pagetable, uint64 va)
{
  if(va % PGSIZE != 0)
    return 0;
  uint64 pa = walkaddr(pagetable, va); // 传入虚拟地址，得到对应的物理地址
  if(pa==0)
    return 0;
  pte_t* pte = walk(pagetable,va,0); //得到这个页表下这个虚拟地址的pte项

  if(krefcnt((char*)pa) == 1) // 当这个物理页只有一个引用的时候 
  { // 直接修改PTE对应的权限即可
    *pte |= PTE_W;
    *pte &= ~PTE_F;
    return (void*)pa;
  }
  else  // 这个物理页还有其他进程使用，就要为进程新分配一页并更新pte的内容
  {
    char* mem;
    if((mem = kalloc()) == 0)
      return 0;
    // 复制旧的内容到新的页面
    memmove(mem, (char*)pa, PGSIZE);

    // 清楚PTE_V,然后map
    *pte &= ~PTE_V;

    if(mappages(pagetable,va,PGSIZE,(uint64)mem,(PTE_FLAGS(*pte)|PTE_W)&~PTE_F) != 0)
    { // 说明map失败了
      kfree(mem); //释放刚分配的内存
      *pte|=PTE_V; //再重新设置为有效
      return 0; //标记失败
    }

    // 成功分配并map之后，将原来的页面引用-1
    kfree((char*)PGROUNDDOWN(pa));
    return mem;
  }
}

// 输入物理地址，得到这个物理页的引用数量
int krefcnt(void*pa)
{
  return ref.cnt[(uint64)pa/PGSIZE];
}

// 输入物理地址，将这个物理页的引用数量加一
int kaddrefcnt(void *pa)
{
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    return -1;
  acquire(&ref.lock);
  ++ref.cnt[(uint64)pa / PGSIZE];
  release(&ref.lock);
  return 0;
}
