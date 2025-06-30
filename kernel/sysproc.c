#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// Trace system call
uint64
sys_trace(void)
{
  //printf("sys_trace called\n");
  // 我如何在这个地方将我的系统调用的编号保存到我的进程中呢
  struct proc *p = myproc(); //这样不确定是否能得到我的当前的进程
  // 我现在又如何得到我的系统调用的编号呢
  int n;
  argint(0, &n); // 获取系统调用的编号
  p->trace_mask = n; // 将系统调用的编号保存到当前进程的 trace_mask 字段中
  return 0;
}

// Sysinfo system call
// 系统调用功能：将 sysinfo 结构体的内容填充为当前系统的状态信息
// freemem: 设置为空闲内存的字节数
// nproc: 设置为state字段不为UNUSED的进程数

uint64
sys_sysinfo(void)
{
  struct sysinfo info;
  info.freemem = get_fremem();
  info.nproc = get_proc_cnt();

  uint64 user_addr;

  struct proc *cur_proc = myproc();
  // 获取调用时传递的参数
  if(argaddr(0, (uint64 *)&user_addr) < 0) {
    printf("die there 1\n");
    return -1; // 如果获取参数失败，返回错误
  }
  // 将系统信息结构体的内容复制到用户空间
  if(copyout(cur_proc->pagetable, user_addr, (char *)&info, sizeof(info)) < 0) {
    printf("die there 2\n");
    return -1; // 如果复制失败，返回错误
  }

  return 0;
}