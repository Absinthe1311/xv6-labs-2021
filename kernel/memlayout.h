// Physical memory layout
// 这个文件定义了相关界限常量
// qemu -machine virt is set up like this,
// based on qemu's hw/riscv/virt.c:
//
// 00001000 -- boot ROM, provided by qemu

// (Core Local Interruptor)核心本地中断控制器 
// 负责每个CPU核心的定时器中断和软件中断
// 02000000 -- CLINT 

// (Platform-Level Interrupt Controller) 平台级中断控制器
// 用于管理和分发外部设备产生的中断信号到各个CPU
// 0C000000 -- PLIC 

// (Universal Asynchronous Receiver-Transmitter) 通用异步接收发送器
// 第一个串口设备，通常用于控制台的输入输出
// 10000000 -- uart0 

// Virtio disk 是QEMU虚拟机提供的虚拟磁盘设备，基于virtio标准接口
// 10001000 -- virtio disk 

// 80000000 -- boot ROM jumps here in machine mode
//             -kernel loads the kernel here
// unused RAM after 80000000.

// the kernel uses physical memory thus:
// 80000000 -- entry.S, then kernel text and data
// end -- start of kernel page allocation area
// PHYSTOP -- end RAM used by the kernel

// qemu puts UART registers here in physical memory.
#define UART0 0x10000000L  
#define UART0_IRQ 10

// virtio mmio interface
#define VIRTIO0 0x10001000
#define VIRTIO0_IRQ 1

#ifdef LAB_NET
#define E1000_IRQ 33
#endif

// core local interruptor (CLINT), which contains the timer.
#define CLINT 0x2000000L
#define CLINT_MTIMECMP(hartid) (CLINT + 0x4000 + 8*(hartid))
#define CLINT_MTIME (CLINT + 0xBFF8) // cycles since boot.

// qemu puts platform-level interrupt controller (PLIC) here.
#define PLIC 0x0c000000L
#define PLIC_PRIORITY (PLIC + 0x0)
#define PLIC_PENDING (PLIC + 0x1000)
#define PLIC_MENABLE(hart) (PLIC + 0x2000 + (hart)*0x100)
#define PLIC_SENABLE(hart) (PLIC + 0x2080 + (hart)*0x100)
#define PLIC_MPRIORITY(hart) (PLIC + 0x200000 + (hart)*0x2000)
#define PLIC_SPRIORITY(hart) (PLIC + 0x201000 + (hart)*0x2000)
#define PLIC_MCLAIM(hart) (PLIC + 0x200004 + (hart)*0x2000)
#define PLIC_SCLAIM(hart) (PLIC + 0x201004 + (hart)*0x2000)

// the kernel expects there to be RAM
// for use by the kernel and user pages
// from physical address 0x80000000 to PHYSTOP.
// 这里是物理内存的起始地址，QEMU启动后，内核就加载在这里
#define KERNBASE 0x80000000L
// PHYSTOP是物理地址的结束，xv6不会访问或分配PHYSTOP以上的物理内存
#define PHYSTOP (KERNBASE + 128*1024*1024)
// xv6一共有128MB的物理内存，这部分是被Kernel/User共用的

// map the trampoline page to the highest address,
// in both user and kernel space.
// 是一个特殊的跳板页，用于用户态和内核态的切换
#define TRAMPOLINE (MAXVA - PGSIZE) 

// map kernel stacks beneath the trampoline,
// each surrounded by invalid guard pages.
// 为每个内核线程（或进程）分配一个内核栈，并把这些栈安排在虚拟地址空间中，紧挨着trampoline下方
// trampoline|guard page|kstack 0|guard page|kstack 1|guard page|...
#define KSTACK(p) (TRAMPOLINE - (p)*2*PGSIZE - 3*PGSIZE)

// User memory layout.
// Address zero first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap
//   ...
//   USYSCALL (shared with kernel)
//   TRAPFRAME (p->trapframe, used by the trampoline)
//   TRAMPOLINE (the same page as in the kernel)
#define TRAPFRAME (TRAMPOLINE - PGSIZE)
//#ifdef LAB_PGTBL
// 一个只读页，用于加速系统调用
#define USYSCALL (TRAPFRAME - PGSIZE)

struct usyscall {
  int pid;  // Process ID
};
//#endif
