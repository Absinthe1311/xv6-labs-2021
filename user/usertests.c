#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/syscall.h"
#include "kernel/memlayout.h"
#include "kernel/riscv.h"

//
// Tests xv6 system calls.  usertests without arguments runs them all
// and usertests <name> runs <name> test. The test runner creates for
// each test a process and based on the exit status of the process,
// the test runner reports "OK" or "FAILED".  Some tests result in
// kernel printing usertrap messages, which can be ignored if test
// prints "OK".
//

#define BUFSZ  ((MAXOPBLOCKS+2)*BSIZE)

char buf[BUFSZ];

// what if you pass ridiculous pointers to system calls
// that read user memory with copyin?
void
copyin(char *s)
{
  uint64 addrs[] = { 0x80000000LL, 0xffffffffffffffff };

  for(int ai = 0; ai < 2; ai++){
    uint64 addr = addrs[ai];
    
    int fd = open("copyin1", O_CREATE|O_WRONLY);
    if(fd < 0){
      printf("open(copyin1) failed\n");
      exit(1);
    }
    int n = write(fd, (void*)addr, 8192);
    if(n >= 0){
      printf("write(fd, %p, 8192) returned %d, not -1\n", addr, n);
      exit(1);
    }
    close(fd);
    unlink("copyin1");
    
    n = write(1, (char*)addr, 8192);
    if(n > 0){
      printf("write(1, %p, 8192) returned %d, not -1 or 0\n", addr, n);
      exit(1);
    }
    
    int fds[2];
    if(pipe(fds) < 0){
      printf("pipe() failed\n");
      exit(1);
    }
    n = write(fds[1], (char*)addr, 8192);
    if(n > 0){
      printf("write(pipe, %p, 8192) returned %d, not -1 or 0\n", addr, n);
      exit(1);
    }
    close(fds[0]);
    close(fds[1]);
  }
}

// what if you pass ridiculous pointers to system calls
// that write user memory with copyout?
void
copyout(char *s)
{
  uint64 addrs[] = { 0x80000000LL, 0xffffffffffffffff };

  for(int ai = 0; ai < 2; ai++){
    uint64 addr = addrs[ai];

    int fd = open("README", 0);
    if(fd < 0){
      printf("open(README) failed\n");
      exit(1);
    }
    int n = read(fd, (void*)addr, 8192);
    if(n > 0){
      printf("read(fd, %p, 8192) returned %d, not -1 or 0\n", addr, n);
      exit(1);
    }
    close(fd);

    int fds[2];
    if(pipe(fds) < 0){
      printf("pipe() failed\n");
      exit(1);
    }
    n = write(fds[1], "x", 1);
    if(n != 1){
      printf("pipe write failed\n");
      exit(1);
    }
    n = read(fds[0], (void*)addr, 8192);
    if(n > 0){
      printf("read(pipe, %p, 8192) returned %d, not -1 or 0\n", addr, n);
      exit(1);
    }
    close(fds[0]);
    close(fds[1]);
  }
}

// what if you pass ridiculous string pointers to system calls?
void
copyinstr1(char *s)
{
  uint64 addrs[] = { 0x80000000LL, 0xffffffffffffffff };

  for(int ai = 0; ai < 2; ai++){
    uint64 addr = addrs[ai];

    int fd = open((char *)addr, O_CREATE|O_WRONLY);
    if(fd >= 0){
      printf("open(%p) returned %d, not -1\n", addr, fd);
      exit(1);
    }
  }
}

// what if a string system call argument is exactly the size
// of the kernel buffer it is copied into, so that the null
// would fall just beyond the end of the kernel buffer?
void
copyinstr2(char *s)
{
  char b[MAXPATH+1];

  for(int i = 0; i < MAXPATH; i++)
    b[i] = 'x';
  b[MAXPATH] = '\0';
  
  int ret = unlink(b);
  if(ret != -1){
    printf("unlink(%s) returned %d, not -1\n", b, ret);
    exit(1);
  }

  int fd = open(b, O_CREATE | O_WRONLY);
  if(fd != -1){
    printf("open(%s) returned %d, not -1\n", b, fd);
    exit(1);
  }

  ret = link(b, b);
  if(ret != -1){
    printf("link(%s, %s) returned %d, not -1\n", b, b, ret);
    exit(1);
  }

  char *args[] = { "xx", 0 };
  ret = exec(b, args);
  if(ret != -1){
    printf("exec(%s) returned %d, not -1\n", b, fd);
    exit(1);
  }

  int pid = fork();
  if(pid < 0){
    printf("fork failed\n");
    exit(1);
  }
  if(pid == 0){
    static char big[PGSIZE+1];
    for(int i = 0; i < PGSIZE; i++)
      big[i] = 'x';
    big[PGSIZE] = '\0';
    char *args2[] = { big, big, big, 0 };
    ret = exec("echo", args2);
    if(ret != -1){
      printf("exec(echo, BIG) returned %d, not -1\n", fd);
      exit(1);
    }
    exit(747); // OK
  }

  int st = 0;
  wait(&st);
  if(st != 747){
    printf("exec(echo, BIG) succeeded, should have failed\n");
    exit(1);
  }
}

// what if a string argument crosses over the end of last user page?
void
copyinstr3(char *s)
{
  sbrk(8192);
  uint64 top = (uint64) sbrk(0);
  if((top % PGSIZE) != 0){
    sbrk(PGSIZE - (top % PGSIZE));
  }
  top = (uint64) sbrk(0);
  if(top % PGSIZE){
    printf("oops\n");
    exit(1);
  }

  char *b = (char *) (top - 1);
  *b = 'x';

  int ret = unlink(b);
  if(ret != -1){
    printf("unlink(%s) returned %d, not -1\n", b, ret);
    exit(1);
  }

  int fd = open(b, O_CREATE | O_WRONLY);
  if(fd != -1){
    printf("open(%s) returned %d, not -1\n", b, fd);
    exit(1);
  }

  ret = link(b, b);
  if(ret != -1){
    printf("link(%s, %s) returned %d, not -1\n", b, b, ret);
    exit(1);
  }

  char *args[] = { "xx", 0 };
  ret = exec(b, args);
  if(ret != -1){
    printf("exec(%s) returned %d, not -1\n", b, fd);
    exit(1);
  }
}

// See if the kernel refuses to read/write user memory that the
// application doesn't have anymore, because it returned it.
void
rwsbrk()
{
  int fd, n;
  
  uint64 a = (uint64) sbrk(8192);

  if(a == 0xffffffffffffffffLL) {
    printf("sbrk(rwsbrk) failed\n");
    exit(1);
  }
  
  if ((uint64) sbrk(-8192) ==  0xffffffffffffffffLL) {
    printf("sbrk(rwsbrk) shrink failed\n");
    exit(1);
  }

  fd = open("rwsbrk", O_CREATE|O_WRONLY);
  if(fd < 0){
    printf("open(rwsbrk) failed\n");
    exit(1);
  }
  n = write(fd, (void*)(a+4096), 1024);
  if(n >= 0){
    printf("write(fd, %p, 1024) returned %d, not -1\n", a+4096, n);
    exit(1);
  }
  close(fd);
  unlink("rwsbrk");

  fd = open("README", O_RDONLY);
  if(fd < 0){
    printf("open(rwsbrk) failed\n");
    exit(1);
  }
  n = read(fd, (void*)(a+4096), 10);
  if(n >= 0){
    printf("read(fd, %p, 10) returned %d, not -1\n", a+4096, n);
    exit(1);
  }
  close(fd);
  
  exit(0);
}

// test O_TRUNC.
void
truncate1(char *s)
{
  char buf[32];
  
  unlink("truncfile");
  int fd1 = open("truncfile", O_CREATE|O_WRONLY|O_TRUNC);
  write(fd1, "abcd", 4);
  close(fd1);

  int fd2 = open("truncfile", O_RDONLY);
  int n = read(fd2, buf, sizeof(buf));
  if(n != 4){
    printf("%s: read %d bytes, wanted 4\n", s, n);
    exit(1);
  }

  fd1 = open("truncfile", O_WRONLY|O_TRUNC);

  int fd3 = open("truncfile", O_RDONLY);
  n = read(fd3, buf, sizeof(buf));
  if(n != 0){
    printf("aaa fd3=%d\n", fd3);
    printf("%s: read %d bytes, wanted 0\n", s, n);
    exit(1);
  }

  n = read(fd2, buf, sizeof(buf));
  if(n != 0){
    printf("bbb fd2=%d\n", fd2);
    printf("%s: read %d bytes, wanted 0\n", s, n);
    exit(1);
  }
  
  write(fd1, "abcdef", 6);

  n = read(fd3, buf, sizeof(buf));
  if(n != 6){
    printf("%s: read %d bytes, wanted 6\n", s, n);
    exit(1);
  }

  n = read(fd2, buf, sizeof(buf));
  if(n != 2){
    printf("%s: read %d bytes, wanted 2\n", s, n);
    exit(1);
  }

  unlink("truncfile");

  close(fd1);
  close(fd2);
  close(fd3);
}

// write to an open FD whose file has just been truncated.
// this causes a write at an offset beyond the end of the file.
// such writes fail on xv6 (unlike POSIX) but at least
// they don't crash.
void
truncate2(char *s)
{
  unlink("truncfile");

  int fd1 = open("truncfile", O_CREATE|O_TRUNC|O_WRONLY);
  write(fd1, "abcd", 4);

  int fd2 = open("truncfile", O_TRUNC|O_WRONLY);

  int n = write(fd1, "x", 1);
  if(n != -1){
    printf("%s: write returned %d, expected -1\n", s, n);
    exit(1);
  }

  unlink("truncfile");
  close(fd1);
  close(fd2);
}

void
truncate3(char *s)
{
  int pid, xstatus;

  close(open("truncfile", O_CREATE|O_TRUNC|O_WRONLY));
  
  pid = fork();
  if(pid < 0){
    printf("%s: fork failed\n", s);
    exit(1);
  }

  if(pid == 0){
    for(int i = 0; i < 100; i++){
      char buf[32];
      int fd = open("truncfile", O_WRONLY);
      if(fd < 0){
        printf("%s: open failed\n", s);
        exit(1);
      }
      int n = write(fd, "1234567890", 10);
      if(n != 10){
        printf("%s: write got %d, expected 10\n", s, n);
        exit(1);
      }
      close(fd);
      fd = open("truncfile", O_RDONLY);
      read(fd, buf, sizeof(buf));
      close(fd);
    }
    exit(0);
  }

  for(int i = 0; i < 150; i++){
    int fd = open("truncfile", O_CREATE|O_WRONLY|O_TRUNC);
    if(fd < 0){
      printf("%s: open failed\n", s);
      exit(1);
    }
    int n = write(fd, "xxx", 3);
    if(n != 3){
      printf("%s: write got %d, expected 3\n", s, n);
      exit(1);
    }
    close(fd);
  }

  wait(&xstatus);
  unlink("truncfile");
  exit(xstatus);
}
  

// does chdir() call iput(p->cwd) in a transaction?
void
iputtest(char *s)
{
  if(mkdir("iputdir") < 0){
    printf("%s: mkdir failed\n", s);
    exit(1);
  }
  if(chdir("iputdir") < 0){
    printf("%s: chdir iputdir failed\n", s);
    exit(1);
  }
  if(unlink("../iputdir") < 0){
    printf("%s: unlink ../iputdir failed\n", s);
    exit(1);
  }
  if(chdir("/") < 0){
    printf("%s: chdir / failed\n", s);
    exit(1);
  }
}

// does exit() call iput(p->cwd) in a transaction?
void
exitiputtest(char *s)
{
  int pid, xstatus;

  pid = fork();
  if(pid < 0){
    printf("%s: fork failed\n", s);
    exit(1);
  }
  if(pid == 0){
    if(mkdir("iputdir") < 0){
      printf("%s: mkdir failed\n", s);
      exit(1);
    }
    if(chdir("iputdir") < 0){
      printf("%s: child chdir failed\n", s);
      exit(1);
    }
    if(unlink("../iputdir") < 0){
      printf("%s: unlink ../iputdir failed\n", s);
      exit(1);
    }
    exit(0);
  }
  wait(&xstatus);
  exit(xstatus);
}

// does the error path in open() for attempt to write a
// directory call iput() in a transaction?
// needs a hacked kernel that pauses just after the namei()
// call in sys_open():
//    if((ip = namei(path)) == 0)
//      return -1;
//    {
//      int i;
//      for(i = 0; i < 10000; i++)
//        yield();
//    }
void
openiputtest(char *s)
{
  int pid, xstatus;

  if(mkdir("oidir") < 0){
    printf("%s: mkdir oidir failed\n", s);
    exit(1);
  }
  pid = fork();
  if(pid < 0){
    printf("%s: fork failed\n", s);
    exit(1);
  }
  if(pid == 0){
    int fd = open("oidir", O_RDWR);
    if(fd >= 0){
      printf("%s: open directory for write succeeded\n", s);
      exit(1);
    }
    exit(0);
  }
  sleep(1);
  if(unlink("oidir") != 0){
    printf("%s: unlink failed\n", s);
    exit(1);
  }
  wait(&xstatus);
  exit(xstatus);
}

// simple file system tests

void
opentest(char *s)
{
  int fd;

  fd = open("echo", 0);
  if(fd < 0){
    printf("%s: open echo failed!\n", s);
    exit(1);
  }
  close(fd);
  fd = open("doesnotexist", 0);
  if(fd >= 0){
    printf("%s: open doesnotexist succeeded!\n", s);
    exit(1);
  }
}

void
writetest(char *s)
{
  int fd;
  int i;
  enum { N=100, SZ=10 };
  
  fd = open("small", O_CREATE|O_RDWR);
  if(fd < 0){
    printf("%s: error: creat small failed!\n", s);
    exit(1);
  }
  for(i = 0; i < N; i++){
    if(write(fd, "aaaaaaaaaa", SZ) != SZ){
      printf("%s: error: write aa %d new file failed\n", s, i);
      exit(1);
    }
    if(write(fd, "bbbbbbbbbb", SZ) != SZ){
      printf("%s: error: write bb %d new file failed\n", s, i);
      exit(1);
    }
  }
  close(fd);
  fd = open("small", O_RDONLY);
  if(fd < 0){
    printf("%s: error: open small failed!\n", s);
    exit(1);
  }
  i = read(fd, buf, N*SZ*2);
  if(i != N*SZ*2){
    printf("%s: read failed\n", s);
    exit(1);
  }
  close(fd);

  if(unlink("small") < 0){
    printf("%s: unlink small failed\n", s);
    exit(1);
  }
}

void
writebig(char *s)
{
  int i, fd, n;

  fd = open("big", O_CREATE|O_RDWR);
  if(fd < 0){
    printf("%s: error: creat big failed!\n", s);
    exit(1);
  }

  for(i = 0; i < MAXFILE; i++){
    ((int*)buf)[0] = i;
    if(write(fd, buf, BSIZE) != BSIZE){
      printf("%s: error: write big file failed\n", s, i);
      exit(1);
    }
  }

  close(fd);

  fd = open("big", O_RDONLY);
  if(fd < 0){
    printf("%s: error: open big failed!\n", s);
    exit(1);
  }

  n = 0;
  for(;;){
    i = read(fd, buf, BSIZE);
    if(i == 0){
      if(n == MAXFILE - 1){
        printf("%s: read only %d blocks from big", s, n);
        exit(1);
      }
      break;
    } else if(i != BSIZE){
      printf("%s: read failed %d\n", s, i);
      exit(1);
    }
    if(((int*)buf)[0] != n){
      printf("%s: read content of block %d is %d\n", s,
             n, ((int*)buf)[0]);
      exit(1);
    }
    n++;
  }
  close(fd);
  if(unlink("big") < 0){
    printf("%s: unlink big failed\n", s);
    exit(1);
  }
}

// many creates, followed by unlink test
void
createtest(char *s)
{
  int i, fd;
  enum { N=52 };

  char name[3];
  name[0] = 'a';
  name[2] = '\0';
  for(i = 0; i < N; i++){
    name[1] = '0' + i;
    fd = open(name, O_CREATE|O_RDWR);
    close(fd);
  }
  name[0] = 'a';
  name[2] = '\0';
  for(i = 0; i < N; i++){
    name[1] = '0' + i;
    unlink(name);
  }
}

void dirtest(char *s)
{
  if(mkdir("dir0") < 0){
    printf("%s: mkdir failed\n", s);
    exit(1);
  }

  if(chdir("dir0") < 0){
    printf("%s: chdir dir0 failed\n", s);
    exit(1);
  }

  if(chdir("..") < 0){
    printf("%s: chdir .. failed\n", s);
    exit(1);
  }

  if(unlink("dir0") < 0){
    printf("%s: unlink dir0 failed\n", s);
    exit(1);
  }
}

void
exectest(char *s)
{
  int fd, xstatus, pid;
  char *echoargv[] = { "echo", "OK", 0 };
  char buf[3];

  unlink("echo-ok");
  pid = fork();
  if(pid < 0) {
     printf("%s: fork failed\n", s);
     exit(1);
  }
  if(pid == 0) {
    close(1);
    fd = open("echo-ok", O_CREATE|O_WRONLY);
    if(fd < 0) {
      printf("%s: create failed\n", s);
      exit(1);
    }
    if(fd != 1) {
      printf("%s: wrong fd\n", s);
      exit(1);
    }
    if(exec("echo", echoargv) < 0){
      printf("%s: exec echo failed\n", s);
      exit(1);
    }
    // won't get to here
  }
  if (wait(&xstatus) != pid) {
    printf("%s: wait failed!\n", s);
  }
  if(xstatus != 0)
    exit(xstatus);

  fd = open("echo-ok", O_RDONLY);
  if(fd < 0) {
    printf("%s: open failed\n", s);
    exit(1);
  }
  if (read(fd, buf, 2) != 2) {
    printf("%s: read failed\n", s);
    exit(1);
  }
  unlink("echo-ok");
  if(buf[0] == 'O' && buf[1] == 'K')
    exit(0);
  else {
    printf("%s: wrong output\n", s);
    exit(1);
  }

}

// simple fork and pipe read/write

void
pipe1(char *s)
{
  int fds[2], pid, xstatus;
  int seq, i, n, cc, total;
  enum { N=5, SZ=1033 };
  
  if(pipe(fds) != 0){
    printf("%s: pipe() failed\n", s);
    exit(1);
  }
  pid = fork();
  seq = 0;
  if(pid == 0){
    close(fds[0]);
    for(n = 0; n < N; n++){
      for(i = 0; i < SZ; i++)
        buf[i] = seq++;
      if(write(fds[1], buf, SZ) != SZ){
        printf("%s: pipe1 oops 1\n", s);
        exit(1);
      }
    }
    exit(0);
  } else if(pid > 0){
    close(fds[1]);
    total = 0;
    cc = 1;
    while((n = read(fds[0], buf, cc)) > 0){
      for(i = 0; i < n; i++){
        if((buf[i] & 0xff) != (seq++ & 0xff)){
          printf("%s: pipe1 oops 2\n", s);
          return;
        }
      }
      total += n;
      cc = cc * 2;
      if(cc > sizeof(buf))
        cc = sizeof(buf);
    }
    if(total != N * SZ){
      printf("%s: pipe1 oops 3 total %d\n", total);
      exit(1);
    }
    close(fds[0]);
    wait(&xstatus);
    exit(xstatus);
  } else {
    printf("%s: fork() failed\n", s);
    exit(1);
  }
}


// test if child is killed (status = -1)
void
killstatus(char *s)
{
  int xst;
  
  for(int i = 0; i < 100; i++){
    int pid1 = fork();
    if(pid1 < 0){
      printf("%s: fork failed\n", s);
      exit(1);
    }
    if(pid1 == 0){
      while(1) {
        getpid();
      }
      exit(0);
    }
    sleep(1);
    kill(pid1);
    wait(&xst);
    if(xst != -1) {
       printf("%s: status should be -1\n", s);
       exit(1);
    }
  }
  exit(0);
}

// meant to be run w/ at most two CPUs
void
preempt(char *s)
{
  int pid1, pid2, pid3;
  int pfds[2];

  pid1 = fork();
  if(pid1 < 0) {
    printf("%s: fork failed", s);
    exit(1);
  }
  if(pid1 == 0)
    for(;;)
      ;

  pid2 = fork();
  if(pid2 < 0) {
    printf("%s: fork failed\n", s);
    exit(1);
  }
  if(pid2 == 0)
    for(;;)
      ;

  pipe(pfds);
  pid3 = fork();
  if(pid3 < 0) {
     printf("%s: fork failed\n", s);
     exit(1);
  }
  if(pid3 == 0){
    close(pfds[0]);
    if(write(pfds[1], "x", 1) != 1)
      printf("%s: preempt write error", s);
    close(pfds[1]);
    for(;;)
      ;
  }

  close(pfds[1]);
  if(read(pfds[0], buf, sizeof(buf)) != 1){
    printf("%s: preempt read error", s);
    return;
  }
  close(pfds[0]);
  printf("kill... ");
  kill(pid1);
  kill(pid2);
  kill(pid3);
  printf("wait... ");
  wait(0);
  wait(0);
  wait(0);
}

// try to find any races between exit and wait
void
exitwait(char *s)
{
  int i, pid;

  for(i = 0; i < 100; i++){
    pid = fork();
    if(pid < 0){
      printf("%s: fork failed\n", s);
      exit(1);
    }
    if(pid){
      int xstate;
      if(wait(&xstate) != pid){
        printf("%s: wait wrong pid\n", s);
        exit(1);
      }
      if(i != xstate) {
        printf("%s: wait wrong exit status\n", s);
        exit(1);
      }
    } else {
      exit(i);
    }
  }
}

// try to find races in the reparenting
// code that handles a parent exiting
// when it still has live children.
void
reparent(char *s)
{
  int master_pid = getpid();
  for(int i = 0; i < 200; i++){
    int pid = fork();
    if(pid < 0){
      printf("%s: fork failed\n", s);
      exit(1);
    }
    if(pid){
      if(wait(0) != pid){
        printf("%s: wait wrong pid\n", s);
        exit(1);
      }
    } else {
      int pid2 = fork();
      if(pid2 < 0){
        kill(master_pid);
        exit(1);
      }
      exit(0);
    }
  }
  exit(0);
}

// what if two children exit() at the same time?
void
twochildren(char *s)
{
  for(int i = 0; i < 1000; i++){
    int pid1 = fork();
    if(pid1 < 0){
      printf("%s: fork failed\n", s);
      exit(1);
    }
    if(pid1 == 0){
      exit(0);
    } else {
      int pid2 = fork();
      if(pid2 < 0){
        printf("%s: fork failed\n", s);
        exit(1);
      }
      if(pid2 == 0){
        exit(0);
      } else {
        wait(0);
        wait(0);
      }
    }
  }
}

// concurrent forks to try to expose locking bugs.
void
forkfork(char *s)
{
  enum { N=2 };
  
  for(int i = 0; i < N; i++){
    int pid = fork();
    if(pid < 0){
      printf("%s: fork failed", s);
      exit(1);
    }
    if(pid == 0){
      for(int j = 0; j < 200; j++){
        int pid1 = fork();
        if(pid1 < 0){
          exit(1);
        }
        if(pid1 == 0){
          exit(0);
        }
        wait(0);
      }
      exit(0);
    }
  }

  int xstatus;
  for(int i = 0; i < N; i++){
    wait(&xstatus);
    if(xstatus != 0) {
      printf("%s: fork in child failed", s);
      exit(1);
    }
  }
}

void
forkforkfork(char *s)
{
  unlink("stopforking");

  int pid = fork();
  if(pid < 0){
    printf("%s: fork failed", s);
    exit(1);
  }
  if(pid == 0){
    while(1){
      int fd = open("stopforking", 0);
      if(fd >= 0){
        exit(0);
      }
      if(fork() < 0){
        close(open("stopforking", O_CREATE|O_RDWR));
      }
    }

    exit(0);
  }

  sleep(20); // two seconds
  close(open("stopforking", O_CREATE|O_RDWR));
  wait(0);
  sleep(10); // one second
}

// regression test. does reparent() violate the parent-then-child
// locking order when giving away a child to init, so that exit()
// deadlocks against init's wait()? also used to trigger a "panic:
// release" due to exit() releasing a different p->parent->lock than
// it acquired.
void
reparent2(char *s)
{
  for(int i = 0; i < 800; i++){
    int pid1 = fork();
    if(pid1 < 0){
      printf("fork failed\n");
      exit(1);
    }
    if(pid1 == 0){
      fork();
      fork();
      exit(0);
    }
    wait(0);
  }

  exit(0);
}

// allocate all mem, free it, and allocate again
void
mem(char *s)
{
  void *m1, *m2;
  int pid;

  if((pid = fork()) == 0){
    m1 = 0;
    while((m2 = malloc(10001)) != 0){
      *(char**)m2 = m1;
      m1 = m2;
    }
    while(m1){
      m2 = *(char**)m1;
      free(m1);
      m1 = m2;
    }
    m1 = malloc(1024*20);
    if(m1 == 0){
      printf("couldn't allocate mem?!!\n", s);
      exit(1);
    }
    free(m1);
    exit(0);
  } else {
    int xstatus;
    wait(&xstatus);
    if(xstatus == -1){
      // probably page fault, so might be lazy lab,
      // so OK.
      exit(0);
    }
    exit(xstatus);
  }
}

// More file system tests

// two processes write to the same file descriptor
// is the offset shared? does inode locking work?
void
sharedfd(char *s)
{
  int fd, pid, i, n, nc, np;
  enum { N = 1000, SZ=10};
  char buf[SZ];

  unlink("sharedfd");
  fd = open("sharedfd", O_CREATE|O_RDWR);
  if(fd < 0){
    printf("%s: cannot open sharedfd for writing", s);
    exit(1);
  }
  pid = fork();
  memset(buf, pid==0?'c':'p', sizeof(buf));
  for(i = 0; i < N; i++){
    if(write(fd, buf, sizeof(buf)) != sizeof(buf)){
      printf("%s: write sharedfd failed\n", s);
      exit(1);
    }
  }
  if(pid == 0) {
    exit(0);
  } else {
    int xstatus;
    wait(&xstatus);
    if(xstatus != 0)
      exit(xstatus);
  }
  
  close(fd);
  fd = open("sharedfd", 0);
  if(fd < 0){
    printf("%s: cannot open sharedfd for reading\n", s);
    exit(1);
  }
  nc = np = 0;
  while((n = read(fd, buf, sizeof(buf))) > 0){
    for(i = 0; i < sizeof(buf); i++){
      if(buf[i] == 'c')
        nc++;
      if(buf[i] == 'p')
        np++;
    }
  }
  close(fd);
  unlink("sharedfd");
  if(nc == N*SZ && np == N*SZ){
    exit(0);
  } else {
    printf("%s: nc/np test fails\n", s);
    exit(1);
  }
}

// four processes write different files at the same
// time, to test block allocation.
void
fourfiles(char *s)
{
  int fd, pid, i, j, n, total, pi;
  char *names[] = { "f0", "f1", "f2", "f3" };
  char *fname;
  enum { N=12, NCHILD=4, SZ=500 };
  
  for(pi = 0; pi < NCHILD; pi++){
    fname = names[pi];
    unlink(fname);

    pid = fork();
    if(pid < 0){
      printf("fork failed\n", s);
      exit(1);
    }

    if(pid == 0){
      fd = open(fname, O_CREATE | O_RDWR);
      if(fd < 0){
        printf("create failed\n", s);
        exit(1);
      }

      memset(buf, '0'+pi, SZ);
      for(i = 0; i < N; i++){
        if((n = write(fd, buf, SZ)) != SZ){
          printf("write failed %d\n", n);
          exit(1);
        }
      }
      exit(0);
    }
  }

  int xstatus;
  for(pi = 0; pi < NCHILD; pi++){
    wait(&xstatus);
    if(xstatus != 0)
      exit(xstatus);
  }

  for(i = 0; i < NCHILD; i++){
    fname = names[i];
    fd = open(fname, 0);
    total = 0;
    while((n = read(fd, buf, sizeof(buf))) > 0){
      for(j = 0; j < n; j++){
        if(buf[j] != '0'+i){
          printf("wrong char\n", s);
          exit(1);
        }
      }
      total += n;
    }
    close(fd);
    if(total != N*SZ){
      printf("wrong length %d\n", total);
      exit(1);
    }
    unlink(fname);
  }
}

// four processes create and delete different files in same directory
void
createdelete(char *s)
{
  enum { N = 20, NCHILD=4 };
  int pid, i, fd, pi;
  char name[32];

  for(pi = 0; pi < NCHILD; pi++){
    pid = fork();
    if(pid < 0){
      printf("fork failed\n", s);
      exit(1);
    }

    if(pid == 0){
      name[0] = 'p' + pi;
      name[2] = '\0';
      for(i = 0; i < N; i++){
        name[1] = '0' + i;
        fd = open(name, O_CREATE | O_RDWR);
        if(fd < 0){
          printf("%s: create failed\n", s);
          exit(1);
        }
        close(fd);
        if(i > 0 && (i % 2 ) == 0){
          name[1] = '0' + (i / 2);
          if(unlink(name) < 0){
            printf("%s: unlink failed\n", s);
            exit(1);
          }
        }
      }
      exit(0);
    }
  }

  int xstatus;
  for(pi = 0; pi < NCHILD; pi++){
    wait(&xstatus);
    if(xstatus != 0)
      exit(1);
  }

  name[0] = name[1] = name[2] = 0;
  for(i = 0; i < N; i++){
    for(pi = 0; pi < NCHILD; pi++){
      name[0] = 'p' + pi;
      name[1] = '0' + i;
      fd = open(name, 0);
      if((i == 0 || i >= N/2) && fd < 0){
        printf("%s: oops createdelete %s didn't exist\n", s, name);
        exit(1);
      } else if((i >= 1 && i < N/2) && fd >= 0){
        printf("%s: oops createdelete %s did exist\n", s, name);
        exit(1);
      }
      if(fd >= 0)
        close(fd);
    }
  }

  for(i = 0; i < N; i++){
    for(pi = 0; pi < NCHILD; pi++){
      name[0] = 'p' + i;
      name[1] = '0' + i;
      unlink(name);
    }
  }
}

// can I unlink a file and still read it?
void
unlinkread(char *s)
{
  enum { SZ = 5 };
  int fd, fd1;

  fd = open("unlinkread", O_CREATE | O_RDWR);
  if(fd < 0){
    printf("%s: create unlinkread failed\n", s);
    exit(1);
  }
  write(fd, "hello", SZ);
  close(fd);

  fd = open("unlinkread", O_RDWR);
  if(fd < 0){
    printf("%s: open unlinkread failed\n", s);
    exit(1);
  }
  if(unlink("unlinkread") != 0){
    printf("%s: unlink unlinkread failed\n", s);
    exit(1);
  }

  fd1 = open("unlinkread", O_CREATE | O_RDWR);
  write(fd1, "yyy", 3);
  close(fd1);

  if(read(fd, buf, sizeof(buf)) != SZ){
    printf("%s: unlinkread read failed", s);
    exit(1);
  }
  if(buf[0] != 'h'){
    printf("%s: unlinkread wrong data\n", s);
    exit(1);
  }
  if(write(fd, buf, 10) != 10){
    printf("%s: unlinkread write failed\n", s);
    exit(1);
  }
  close(fd);
  unlink("unlinkread");
}

void
linktest(char *s)
{
  enum { SZ = 5 };
  int fd;

  unlink("lf1");
  unlink("lf2");

  fd = open("lf1", O_CREATE|O_RDWR);
  if(fd < 0){
    printf("%s: create lf1 failed\n", s);
    exit(1);
  }
  if(write(fd, "hello", SZ) != SZ){
    printf("%s: write lf1 failed\n", s);
    exit(1);
  }
  close(fd);

  if(link("lf1", "lf2") < 0){
    printf("%s: link lf1 lf2 failed\n", s);
    exit(1);
  }
  unlink("lf1");

  if(open("lf1", 0) >= 0){
    printf("%s: unlinked lf1 but it is still there!\n", s);
    exit(1);
  }

  fd = open("lf2", 0);
  if(fd < 0){
    printf("%s: open lf2 failed\n", s);
    exit(1);
  }
  if(read(fd, buf, sizeof(buf)) != SZ){
    printf("%s: read lf2 failed\n", s);
    exit(1);
  }
  close(fd);

  if(link("lf2", "lf2") >= 0){
    printf("%s: link lf2 lf2 succeeded! oops\n", s);
    exit(1);
  }

  unlink("lf2");
  if(link("lf2", "lf1") >= 0){
    printf("%s: link non-existent succeeded! oops\n", s);
    exit(1);
  }

  if(link(".", "lf1") >= 0){
    printf("%s: link . lf1 succeeded! oops\n", s);
    exit(1);
  }
}

// test concurrent create/link/unlink of the same file
void
concreate(char *s)
{
  enum { N = 40 };
  char file[3];
  int i, pid, n, fd;
  char fa[N];
  struct {
    ushort inum;
    char name[DIRSIZ];
  } de;

  file[0] = 'C';
  file[2] = '\0';
  for(i = 0; i < N; i++){
    file[1] = '0' + i;
    unlink(file);
    pid = fork();
    if(pid && (i % 3) == 1){
      link("C0", file);
    } else if(pid == 0 && (i % 5) == 1){
      link("C0", file);
    } else {
      fd = open(file, O_CREATE | O_RDWR);
      if(fd < 0){
        printf("concreate create %s failed\n", file);
        exit(1);
      }
      close(fd);
    }
    if(pid == 0) {
      exit(0);
    } else {
      int xstatus;
      wait(&xstatus);
      if(xstatus != 0)
        exit(1);
    }
  }

  memset(fa, 0, sizeof(fa));
  fd = open(".", 0);
  n = 0;
  while(read(fd, &de, sizeof(de)) > 0){
    if(de.inum == 0)
      continue;
    if(de.name[0] == 'C' && de.name[2] == '\0'){
      i = de.name[1] - '0';
      if(i < 0 || i >= sizeof(fa)){
        printf("%s: concreate weird file %s\n", s, de.name);
        exit(1);
      }
      if(fa[i]){
        printf("%s: concreate duplicate file %s\n", s, de.name);
        exit(1);
      }
      fa[i] = 1;
      n++;
    }
  }
  close(fd);

  if(n != N){
    printf("%s: concreate not enough files in directory listing\n", s);
    exit(1);
  }

  for(i = 0; i < N; i++){
    file[1] = '0' + i;
    pid = fork();
    if(pid < 0){
      printf("%s: fork failed\n", s);
      exit(1);
    }
    if(((i % 3) == 0 && pid == 0) ||
       ((i % 3) == 1 && pid != 0)){
      close(open(file, 0));
      close(open(file, 0));
      close(open(file, 0));
      close(open(file, 0));
      close(open(file, 0));
      close(open(file, 0));
    } else {
      unlink(file);
      unlink(file);
      unlink(file);
      unlink(file);
      unlink(file);
      unlink(file);
    }
    if(pid == 0)
      exit(0);
    else
      wait(0);
  }
}

// another concurrent link/unlink/create test,
// to look for deadlocks.
void
linkunlink(char *s)
{
  int pid, i;

  unlink("x");
  pid = fork();
  if(pid < 0){
    printf("%s: fork failed\n", s);
    exit(1);
  }

  unsigned int x = (pid ? 1 : 97);
  for(i = 0; i < 100; i++){
    x = x * 1103515245 + 12345;
    if((x % 3) == 0){
      close(open("x", O_RDWR | O_CREATE));
    } else if((x % 3) == 1){
      link("cat", "x");
    } else {
      unlink("x");
    }
  }

  if(pid)
    wait(0);
  else
    exit(0);
}

// directory that uses indirect blocks
void
bigdir(char *s)
{
  enum { N = 500 };
  int i, fd;
  char name[10];

  unlink("bd");

  fd = open("bd", O_CREATE);
  if(fd < 0){
    printf("%s: bigdir create failed\n", s);
    exit(1);
  }
  close(fd);

  for(i = 0; i < N; i++){
    name[0] = 'x';
    name[1] = '0' + (i / 64);
    name[2] = '0' + (i % 64);
    name[3] = '\0';
    if(link("bd", name) != 0){
      printf("%s: bigdir link(bd, %s) failed\n", s, name);
      exit(1);
    }
  }

  unlink("bd");
  for(i = 0; i < N; i++){
    name[0] = 'x';
    name[1] = '0' + (i / 64);
    name[2] = '0' + (i % 64);
    name[3] = '\0';
    if(unlink(name) != 0){
      printf("%s: bigdir unlink failed", s);
      exit(1);
    }
  }
}

void
subdir(char *s)
{
  int fd, cc;

  unlink("ff");
  if(mkdir("dd") != 0){
    printf("%s: mkdir dd failed\n", s);
    exit(1);
  }

  fd = open("dd/ff", O_CREATE | O_RDWR);
  if(fd < 0){
    printf("%s: create dd/ff failed\n", s);
    exit(1);
  }
  write(fd, "ff", 2);
  close(fd);

  if(unlink("dd") >= 0){
    printf("%s: unlink dd (non-empty dir) succeeded!\n", s);
    exit(1);
  }

  if(mkdir("/dd/dd") != 0){
    printf("subdir mkdir dd/dd failed\n", s);
    exit(1);
  }

  fd = open("dd/dd/ff", O_CREATE | O_RDWR);
  if(fd < 0){
    printf("%s: create dd/dd/ff failed\n", s);
    exit(1);
  }
  write(fd, "FF", 2);
  close(fd);

  fd = open("dd/dd/../ff", 0);
  if(fd < 0){
    printf("%s: open dd/dd/../ff failed\n", s);
    exit(1);
  }
  cc = read(fd, buf, sizeof(buf));
  if(cc != 2 || buf[0] != 'f'){
    printf("%s: dd/dd/../ff wrong content\n", s);
    exit(1);
  }
  close(fd);

  if(link("dd/dd/ff", "dd/dd/ffff") != 0){
    printf("link dd/dd/ff dd/dd/ffff failed\n", s);
    exit(1);
  }

  if(unlink("dd/dd/ff") != 0){
    printf("%s: unlink dd/dd/ff failed\n", s);
    exit(1);
  }
  if(open("dd/dd/ff", O_RDONLY) >= 0){
    printf("%s: open (unlinked) dd/dd/ff succeeded\n", s);
    exit(1);
  }

  if(chdir("dd") != 0){
    printf("%s: chdir dd failed\n", s);
    exit(1);
  }
  if(chdir("dd/../../dd") != 0){
    printf("%s: chdir dd/../../dd failed\n", s);
    exit(1);
  }
  if(chdir("dd/../../../dd") != 0){
    printf("chdir dd/../../dd failed\n", s);
    exit(1);
  }
  if(chdir("./..") != 0){
    printf("%s: chdir ./.. failed\n", s);
    exit(1);
  }

  fd = open("dd/dd/ffff", 0);
  if(fd < 0){
    printf("%s: open dd/dd/ffff failed\n", s);
    exit(1);
  }
  if(read(fd, buf, sizeof(buf)) != 2){
    printf("%s: read dd/dd/ffff wrong len\n", s);
    exit(1);
  }
  close(fd);

  if(open("dd/dd/ff", O_RDONLY) >= 0){
    printf("%s: open (unlinked) dd/dd/ff succeeded!\n", s);
    exit(1);
  }

  if(open("dd/ff/ff", O_CREATE|O_RDWR) >= 0){
    printf("%s: create dd/ff/ff succeeded!\n", s);
    exit(1);
  }
  if(open("dd/xx/ff", O_CREATE|O_RDWR) >= 0){
    printf("%s: create dd/xx/ff succeeded!\n", s);
    exit(1);
  }
  if(open("dd", O_CREATE) >= 0){
    printf("%s: create dd succeeded!\n", s);
    exit(1);
  }
  if(open("dd", O_RDWR) >= 0){
    printf("%s: open dd rdwr succeeded!\n", s);
    exit(1);
  }
  if(open("dd", O_WRONLY) >= 0){
    printf("%s: open dd wronly succeeded!\n", s);
    exit(1);
  }
  if(link("dd/ff/ff", "dd/dd/xx") == 0){
    printf("%s: link dd/ff/ff dd/dd/xx succeeded!\n", s);
    exit(1);
  }
  if(link("dd/xx/ff", "dd/dd/xx") == 0){
    printf("%s: link dd/xx/ff dd/dd/xx succeeded!\n", s);
    exit(1);
  }
  if(link("dd/ff", "dd/dd/ffff") == 0){
    printf("%s: link dd/ff dd/dd/ffff succeeded!\n", s);
    exit(1);
  }
  if(mkdir("dd/ff/ff") == 0){
    printf("%s: mkdir dd/ff/ff succeeded!\n", s);
    exit(1);
  }
  if(mkdir("dd/xx/ff") == 0){
    printf("%s: mkdir dd/xx/ff succeeded!\n", s);
    exit(1);
  }
  if(mkdir("dd/dd/ffff") == 0){
    printf("%s: mkdir dd/dd/ffff succeeded!\n", s);
    exit(1);
  }
  if(unlink("dd/xx/ff") == 0){
    printf("%s: unlink dd/xx/ff succeeded!\n", s);
    exit(1);
  }
  if(unlink("dd/ff/ff") == 0){
    printf("%s: unlink dd/ff/ff succeeded!\n", s);
    exit(1);
  }
  if(chdir("dd/ff") == 0){
    printf("%s: chdir dd/ff succeeded!\n", s);
    exit(1);
  }
  if(chdir("dd/xx") == 0){
    printf("%s: chdir dd/xx succeeded!\n", s);
    exit(1);
  }

  if(unlink("dd/dd/ffff") != 0){
    printf("%s: unlink dd/dd/ff failed\n", s);
    exit(1);
  }
  if(unlink("dd/ff") != 0){
    printf("%s: unlink dd/ff failed\n", s);
    exit(1);
  }
  if(unlink("dd") == 0){
    printf("%s: unlink non-empty dd succeeded!\n", s);
    exit(1);
  }
  if(unlink("dd/dd") < 0){
    printf("%s: unlink dd/dd failed\n", s);
    exit(1);
  }
  if(unlink("dd") < 0){
    printf("%s: unlink dd failed\n", s);
    exit(1);
  }
}

// test writes that are larger than the log.
void
bigwrite(char *s)
{
  int fd, sz;

  unlink("bigwrite");
  for(sz = 499; sz < (MAXOPBLOCKS+2)*BSIZE; sz += 471){
    fd = open("bigwrite", O_CREATE | O_RDWR);
    if(fd < 0){
      printf("%s: cannot create bigwrite\n", s);
      exit(1);
    }
    int i;
    for(i = 0; i < 2; i++){
      int cc = write(fd, buf, sz);
      if(cc != sz){
        printf("%s: write(%d) ret %d\n", s, sz, cc);
        exit(1);
      }
    }
    close(fd);
    unlink("bigwrite");
  }
}

// concurrent writes to try to provoke deadlock in the virtio disk
// driver.
void
manywrites(char *s)
{
  int nchildren = 4;
  int howmany = 30; // increase to look for deadlock
  
  for(int ci = 0; ci < nchildren; ci++){
    int pid = fork();
    if(pid < 0){
      printf("fork failed\n");
      exit(1);
    }

    if(pid == 0){
      char name[3];
      name[0] = 'b';
      name[1] = 'a' + ci;
      name[2] = '\0';
      unlink(name);
      
      for(int iters = 0; iters < howmany; iters++){
        for(int i = 0; i < ci+1; i++){
          int fd = open(name, O_CREATE | O_RDWR);
          if(fd < 0){
            printf("%s: cannot create %s\n", s, name);
            exit(1);
          }
          int sz = sizeof(buf);
          int cc = write(fd, buf, sz);
          if(cc != sz){
            printf("%s: write(%d) ret %d\n", s, sz, cc);
            exit(1);
          }
          close(fd);
        }
        unlink(name);
      }

      unlink(name);
      exit(0);
    }
  }

  for(int ci = 0; ci < nchildren; ci++){
    int st = 0;
    wait(&st);
    if(st != 0)
      exit(st);
  }
  exit(0);
}

void
bigfile(char *s)
{
  enum { N = 20, SZ=600 };
  int fd, i, total, cc;

  unlink("bigfile.dat");
  fd = open("bigfile.dat", O_CREATE | O_RDWR);
  if(fd < 0){
    printf("%s: cannot create bigfile", s);
    exit(1);
  }
  for(i = 0; i < N; i++){
    memset(buf, i, SZ);
    if(write(fd, buf, SZ) != SZ){
      printf("%s: write bigfile failed\n", s);
      exit(1);
    }
  }
  close(fd);

  fd = open("bigfile.dat", 0);
  if(fd < 0){
    printf("%s: cannot open bigfile\n", s);
    exit(1);
  }
  total = 0;
  for(i = 0; ; i++){
    cc = read(fd, buf, SZ/2);
    if(cc < 0){
      printf("%s: read bigfile failed\n", s);
      exit(1);
    }
    if(cc == 0)
      break;
    if(cc != SZ/2){
      printf("%s: short read bigfile\n", s);
      exit(1);
    }
    if(buf[0] != i/2 || buf[SZ/2-1] != i/2){
      printf("%s: read bigfile wrong data\n", s);
      exit(1);
    }
    total += cc;
  }
  close(fd);
  if(total != N*SZ){
    printf("%s: read bigfile wrong total\n", s);
    exit(1);
  }
  unlink("bigfile.dat");
}

void
fourteen(char *s)
{
  int fd;

  // DIRSIZ is 14.

  if(mkdir("12345678901234") != 0){
    printf("%s: mkdir 12345678901234 failed\n", s);
    exit(1);
  }
  if(mkdir("12345678901234/123456789012345") != 0){
    printf("%s: mkdir 12345678901234/123456789012345 failed\n", s);
    exit(1);
  }
  fd = open("123456789012345/123456789012345/123456789012345", O_CREATE);
  if(fd < 0){
    printf("%s: create 123456789012345/123456789012345/123456789012345 failed\n", s);
    exit(1);
  }
  close(fd);
  fd = open("12345678901234/12345678901234/12345678901234", 0);
  if(fd < 0){
    printf("%s: open 12345678901234/12345678901234/12345678901234 failed\n", s);
    exit(1);
  }
  close(fd);

  if(mkdir("12345678901234/12345678901234") == 0){
    printf("%s: mkdir 12345678901234/12345678901234 succeeded!\n", s);
    exit(1);
  }
  if(mkdir("123456789012345/12345678901234") == 0){
    printf("%s: mkdir 12345678901234/123456789012345 succeeded!\n", s);
    exit(1);
  }

  // clean up
  unlink("123456789012345/12345678901234");
  unlink("12345678901234/12345678901234");
  unlink("12345678901234/12345678901234/12345678901234");
  unlink("123456789012345/123456789012345/123456789012345");
  unlink("12345678901234/123456789012345");
  unlink("12345678901234");
}

void
rmdot(char *s)
{
  if(mkdir("dots") != 0){
    printf("%s: mkdir dots failed\n", s);
    exit(1);
  }
  if(chdir("dots") != 0){
    printf("%s: chdir dots failed\n", s);
    exit(1);
  }
  if(unlink(".") == 0){
    printf("%s: rm . worked!\n", s);
    exit(1);
  }
  if(unlink("..") == 0){
    printf("%s: rm .. worked!\n", s);
    exit(1);
  }
  if(chdir("/") != 0){
    printf("%s: chdir / failed\n", s);
    exit(1);
  }
  if(unlink("dots/.") == 0){
    printf("%s: unlink dots/. worked!\n", s);
    exit(1);
  }
  if(unlink("dots/..") == 0){
    printf("%s: unlink dots/.. worked!\n", s);
    exit(1);
  }
  if(unlink("dots") != 0){
    printf("%s: unlink dots failed!\n", s);
    exit(1);
  }
}

void
dirfile(char *s)
{
  int fd;

  fd = open("dirfile", O_CREATE);
  if(fd < 0){
    printf("%s: create dirfile failed\n", s);
    exit(1);
  }
  close(fd);
  if(chdir("dirfile") == 0){
    printf("%s: chdir dirfile succeeded!\n", s);
    exit(1);
  }
  fd = open("dirfile/xx", 0);
  if(fd >= 0){
    printf("%s: create dirfile/xx succeeded!\n", s);
    exit(1);
  }
  fd = open("dirfile/xx", O_CREATE);
  if(fd >= 0){
    printf("%s: create dirfile/xx succeeded!\n", s);
    exit(1);
  }
  if(mkdir("dirfile/xx") == 0){
    printf("%s: mkdir dirfile/xx succeeded!\n", s);
    exit(1);
  }
  if(unlink("dirfile/xx") == 0){
    printf("%s: unlink dirfile/xx succeeded!\n", s);
    exit(1);
  }
  if(link("README", "dirfile/xx") == 0){
    printf("%s: link to dirfile/xx succeeded!\n", s);
    exit(1);
  }
  if(unlink("dirfile") != 0){
    printf("%s: unlink dirfile failed!\n", s);
    exit(1);
  }

  fd = open(".", O_RDWR);
  if(fd >= 0){
    printf("%s: open . for writing succeeded!\n", s);
    exit(1);
  }
  fd = open(".", 0);
  if(write(fd, "x", 1) > 0){
    printf("%s: write . succeeded!\n", s);
    exit(1);
  }
  close(fd);
}

// test that iput() is called at the end of _namei().
// also tests empty file names.
void
iref(char *s)
{
  int i, fd;

  for(i = 0; i < NINODE + 1; i++){
    if(mkdir("irefd") != 0){
      printf("%s: mkdir irefd failed\n", s);
      exit(1);
    }
    if(chdir("irefd") != 0){
      printf("%s: chdir irefd failed\n", s);
      exit(1);
    }

    mkdir("");
    link("README", "");
    fd = open("", O_CREATE);
    if(fd >= 0)
      close(fd);
    fd = open("xx", O_CREATE);
    if(fd >= 0)
      close(fd);
    unlink("xx");
  }

  // clean up
  for(i = 0; i < NINODE + 1; i++){
    chdir("..");
    unlink("irefd");
  }

  chdir("/");
}

// test that fork fails gracefully
// the forktest binary also does this, but it runs out of proc entries first.
// inside the bigger usertests binary, we run out of memory first.
void
forktest(char *s)
{
  enum{ N = 1000 };
  int n, pid;

  for(n=0; n<N; n++){
    pid = fork();
    if(pid < 0)
      break;
    if(pid == 0)
      exit(0);
  }

  if (n == 0) {
    printf("%s: no fork at all!\n", s);
    exit(1);
  }

  if(n == N){
    printf("%s: fork claimed to work 1000 times!\n", s);
    exit(1);
  }

  for(; n > 0; n--){
    if(wait(0) < 0){
      printf("%s: wait stopped early\n", s);
      exit(1);
    }
  }

  if(wait(0) != -1){
    printf("%s: wait got too many\n", s);
    exit(1);
  }
}

void
sbrkbasic(char *s)
{
  enum { TOOMUCH=1024*1024*1024};
  int i, pid, xstatus;
  char *c, *a, *b;

  // does sbrk() return the expected failure value?
  pid = fork();
  if(pid < 0){
    printf("fork failed in sbrkbasic\n");
    exit(1);
  }
  if(pid == 0){
    a = sbrk(TOOMUCH);
    if(a == (char*)0xffffffffffffffffL){
      // it's OK if this fails.
      exit(0);
    }
    
    for(b = a; b < a+TOOMUCH; b += 4096){
      *b = 99;
    }
    
    // we should not get here! either sbrk(TOOMUCH)
    // should have failed, or (with lazy allocation)
    // a pagefault should have killed this process.
    exit(1);
  }

  wait(&xstatus);
  if(xstatus == 1){
    printf("%s: too much memory allocated!\n", s);
    exit(1);
  }

  // can one sbrk() less than a page?
  a = sbrk(0);
  for(i = 0; i < 5000; i++){
    b = sbrk(1);
    if(b != a){
      printf("%s: sbrk test failed %d %x %x\n", s, i, a, b);
      exit(1);
    }
    *b = 1;
    a = b + 1;
  }
  pid = fork();
  if(pid < 0){
    printf("%s: sbrk test fork failed\n", s);
    exit(1);
  }
  c = sbrk(1);
  c = sbrk(1);
  if(c != a + 1){
    printf("%s: sbrk test failed post-fork\n", s);
    exit(1);
  }
  if(pid == 0)
    exit(0);
  wait(&xstatus);
  exit(xstatus);
}

void
sbrkmuch(char *s)
{
  enum { BIG=100*1024*1024 };
  char *c, *oldbrk, *a, *lastaddr, *p;
  uint64 amt;

  oldbrk = sbrk(0);

  // can one grow address space to something big?
  a = sbrk(0);
  amt = BIG - (uint64)a;
  p = sbrk(amt);
  if (p != a) {
    printf("%s: sbrk test failed to grow big address space; enough phys mem?\n", s);
    exit(1);
  }

  // touch each page to make sure it exists.
  char *eee = sbrk(0);
  for(char *pp = a; pp < eee; pp += 4096)
    *pp = 1;

  lastaddr = (char*) (BIG-1);
  *lastaddr = 99;

  // can one de-allocate?
  a = sbrk(0);
  c = sbrk(-PGSIZE);
  if(c == (char*)0xffffffffffffffffL){
    printf("%s: sbrk could not deallocate\n", s);
    exit(1);
  }
  c = sbrk(0);
  if(c != a - PGSIZE){
    printf("%s: sbrk deallocation produced wrong address, a %x c %x\n", s, a, c);
    exit(1);
  }

  // can one re-allocate that page?
  a = sbrk(0);
  c = sbrk(PGSIZE);
  if(c != a || sbrk(0) != a + PGSIZE){
    printf("%s: sbrk re-allocation failed, a %x c %x\n", s, a, c);
    exit(1);
  }
  if(*lastaddr == 99){
    // should be zero
    printf("%s: sbrk de-allocation didn't really deallocate\n", s);
    exit(1);
  }

  a = sbrk(0);
  c = sbrk(-(sbrk(0) - oldbrk));
  if(c != a){
    printf("%s: sbrk downsize failed, a %x c %x\n", s, a, c);
    exit(1);
  }
}

// can we read the kernel's memory?
void
kernmem(char *s)
{
  char *a;
  int pid;

  for(a = (char*)(KERNBASE); a < (char*) (KERNBASE+2000000); a += 50000){
    pid = fork();
    if(pid < 0){
      printf("%s: fork failed\n", s);
      exit(1);
    }
    if(pid == 0){
      printf("%s: oops could read %x = %x\n", s, a, *a);
      exit(1);
    }
    int xstatus;
    wait(&xstatus);
    if(xstatus != -1)  // did kernel kill child?
      exit(1);
  }
}

// user code should not be able to write to addresses above MAXVA.
void
MAXVAplus(char *s)
{
  volatile uint64 a = MAXVA;
  for( ; a != 0; a <<= 1){
    int pid;
    pid = fork();
    if(pid < 0){
      printf("%s: fork failed\n", s);
      exit(1);
    }
    if(pid == 0){
      *(char*)a = 99;
      printf("%s: oops wrote %x\n", s, a);
      exit(1);
    }
    int xstatus;
    wait(&xstatus);
    if(xstatus != -1)  // did kernel kill child?
      exit(1);
  }
}

// if we run the system out of memory, does it clean up the last
// failed allocation?
void
sbrkfail(char *s)
{
  enum { BIG=100*1024*1024 };
  int i, xstatus;
  int fds[2];
  char scratch;
  char *c, *a;
  int pids[10];
  int pid;
 
  if(pipe(fds) != 0){
    printf("%s: pipe() failed\n", s);
    exit(1);
  }
  for(i = 0; i < sizeof(pids)/sizeof(pids[0]); i++){
    if((pids[i] = fork()) == 0){
      // allocate a lot of memory
      sbrk(BIG - (uint64)sbrk(0));
      write(fds[1], "x", 1);
      // sit around until killed
      for(;;) sleep(1000);
    }
    if(pids[i] != -1)
      read(fds[0], &scratch, 1);
  }

  // if those failed allocations freed up the pages they did allocate,
  // we'll be able to allocate here
  c = sbrk(PGSIZE);
  for(i = 0; i < sizeof(pids)/sizeof(pids[0]); i++){
    if(pids[i] == -1)
      continue;
    kill(pids[i]);
    wait(0);
  }
  if(c == (char*)0xffffffffffffffffL){
    printf("%s: failed sbrk leaked memory\n", s);
    exit(1);
  }

  // test running fork with the above allocated page 
  pid = fork();
  if(pid < 0){
    printf("%s: fork failed\n", s);
    exit(1);
  }
  if(pid == 0){
    // allocate a lot of memory.
    // this should produce a page fault,
    // and thus not complete.
    a = sbrk(0);
    sbrk(10*BIG);
    int n = 0;
    for (i = 0; i < 10*BIG; i += PGSIZE) {
      n += *(a+i);
    }
    // print n so the compiler doesn't optimize away
    // the for loop.
    printf("%s: allocate a lot of memory succeeded %d\n", s, n);
    exit(1);
  }
  wait(&xstatus);
  if(xstatus != -1 && xstatus != 2)
    exit(1);
}

  
// test reads/writes from/to allocated memory
void
sbrkarg(char *s)
{
  char *a;
  int fd, n;

  a = sbrk(PGSIZE);
  fd = open("sbrk", O_CREATE|O_WRONLY);
  unlink("sbrk");
  if(fd < 0)  {
    printf("%s: open sbrk failed\n", s);
    exit(1);
  }
  if ((n = write(fd, a, PGSIZE)) < 0) {
    printf("%s: write sbrk failed\n", s);
    exit(1);
  }
  close(fd);

  // test writes to allocated memory
  a = sbrk(PGSIZE);
  if(pipe((int *) a) != 0){
    printf("%s: pipe() failed\n", s);
    exit(1);
  } 
}

void
validatetest(char *s)
{
  int hi;
  uint64 p;

  hi = 1100*1024;
  for(p = 0; p <= (uint)hi; p += PGSIZE){
    // try to crash the kernel by passing in a bad string pointer
    if(link("nosuchfile", (char*)p) != -1){
      printf("%s: link should not succeed\n", s);
      exit(1);
    }
  }
}

// does uninitialized data start out zero?
char uninit[10000];
void
bsstest(char *s)
{
  int i;

  for(i = 0; i < sizeof(uninit); i++){
    if(uninit[i] != '\0'){
      printf("%s: bss test failed\n", s);
      exit(1);
    }
  }
}

// does exec return an error if the arguments
// are larger than a page? or does it write
// below the stack and wreck the instructions/data?
void
bigargtest(char *s)
{
  int pid, fd, xstatus;

  unlink("bigarg-ok");
  pid = fork();
  if(pid == 0){
    static char *args[MAXARG];
    int i;
    for(i = 0; i < MAXARG-1; i++)
      args[i] = "bigargs test: failed\n                                                                                                                                                                                                       ";
    args[MAXARG-1] = 0;
    exec("echo", args);
    fd = open("bigarg-ok", O_CREATE);
    close(fd);
    exit(0);
  } else if(pid < 0){
    printf("%s: bigargtest: fork failed\n", s);
    exit(1);
  }
  
  wait(&xstatus);
  if(xstatus != 0)
    exit(xstatus);
  fd = open("bigarg-ok", 0);
  if(fd < 0){
    printf("%s: bigarg test failed!\n", s);
    exit(1);
  }
  close(fd);
}

// what happens when the file system runs out of blocks?
// answer: balloc panics, so this test is not useful.
void
fsfull()
{
  int nfiles;
  int fsblocks = 0;

  printf("fsfull test\n");

  for(nfiles = 0; ; nfiles++){
    char name[64];
    name[0] = 'f';
    name[1] = '0' + nfiles / 1000;
    name[2] = '0' + (nfiles % 1000) / 100;
    name[3] = '0' + (nfiles % 100) / 10;
    name[4] = '0' + (nfiles % 10);
    name[5] = '\0';
    printf("writing %s\n", name);
    int fd = open(name, O_CREATE|O_RDWR);
    if(fd < 0){
      printf("open %s failed\n", name);
      break;
    }
    int total = 0;
    while(1){
      int cc = write(fd, buf, BSIZE);
      if(cc < BSIZE)
        break;
      total += cc;
      fsblocks++;
    }
    printf("wrote %d bytes\n", total);
    close(fd);
    if(total == 0)
      break;
  }

  while(nfiles >= 0){
    char name[64];
    name[0] = 'f';
    name[1] = '0' + nfiles / 1000;
    name[2] = '0' + (nfiles % 1000) / 100;
    name[3] = '0' + (nfiles % 100) / 10;
    name[4] = '0' + (nfiles % 10);
    name[5] = '\0';
    unlink(name);
    nfiles--;
  }

  printf("fsfull test finished\n");
}

void argptest(char *s)
{
  int fd;
  fd = open("init", O_RDONLY);
  if (fd < 0) {
    printf("%s: open failed\n", s);
    exit(1);
  }
  read(fd, sbrk(0) - 1, -1);
  close(fd);
}

// check that there's an invalid page beneath
// the user stack, to catch stack overflow.
void
stacktest(char *s)
{
  int pid;
  int xstatus;
  
  pid = fork();
  if(pid == 0) {
    char *sp = (char *) r_sp();
    sp -= PGSIZE;
    // the *sp should cause a trap.
    printf("%s: stacktest: read below stack %p\n", s, *sp);
    exit(1);
  } else if(pid < 0){
    printf("%s: fork failed\n", s);
    exit(1);
  }
  wait(&xstatus);
  if(xstatus == -1)  // kernel killed child?
    exit(0);
  else
    exit(xstatus);
}

// regression test. copyin(), copyout(), and copyinstr() used to cast
// the virtual page address to uint, which (with certain wild system
// call arguments) resulted in a kernel page faults.
void
pgbug(char *s)
{
  char *argv[1];
  argv[0] = 0;
  exec((char*)0xeaeb0b5b00002f5e, argv);

  pipe((int*)0xeaeb0b5b00002f5e);

  exit(0);
}

// regression test. does the kernel panic if a process sbrk()s its
// size to be less than a page, or zero, or reduces the break by an
// amount too small to cause a page to be freed?
void
sbrkbugs(char *s)
{
  int pid = fork();
  if(pid < 0){
    printf("fork failed\n");
    exit(1);
  }
  if(pid == 0){
    int sz = (uint64) sbrk(0);
    // free all user memory; there used to be a bug that
    // would not adjust p->sz correctly in this case,
    // causing exit() to panic.
    sbrk(-sz);
    // user page fault here.
    exit(0);
  }
  wait(0);

  pid = fork();
  if(pid < 0){
    printf("fork failed\n");
    exit(1);
  }
  if(pid == 0){
    int sz = (uint64) sbrk(0);
    // set the break to somewhere in the very first
    // page; there used to be a bug that would incorrectly
    // free the first page.
    sbrk(-(sz - 3500));
    exit(0);
  }
  wait(0);

  pid = fork();
  if(pid < 0){
    printf("fork failed\n");
    exit(1);
  }
  if(pid == 0){
    // set the break in the middle of a page.
    sbrk((10*4096 + 2048) - (uint64)sbrk(0));

    // reduce the break a bit, but not enough to
    // cause a page to be freed. this used to cause
    // a panic.
    sbrk(-10);

    exit(0);
  }
  wait(0);

  exit(0);
}

// if process size was somewhat more than a page boundary, and then
// shrunk to be somewhat less than that page boundary, can the kernel
// still copyin() from addresses in the last page?
void
sbrklast(char *s)
{
  uint64 top = (uint64) sbrk(0);
  if((top % 4096) != 0)
    sbrk(4096 - (top % 4096));
  sbrk(4096);
  sbrk(10);
  sbrk(-20);
  top = (uint64) sbrk(0);
  char *p = (char *) (top - 64);
  p[0] = 'x';
  p[1] = '\0';
  int fd = open(p, O_RDWR|O_CREATE);
  write(fd, p, 1);
  close(fd);
  fd = open(p, O_RDWR);
  p[0] = '\0';
  read(fd, p, 1);
  if(p[0] != 'x')
    exit(1);
}

// does sbrk handle signed int32 wrap-around with
// negative arguments?
void
sbrk8000(char *s)
{
  sbrk(0x80000004);
  volatile char *top = sbrk(0);
  *(top-1) = *(top-1) + 1;
}

// regression test. does write() with an invalid buffer pointer cause
// a block to be allocated for a file that is then not freed when the
// file is deleted? if the kernel has this bug, it will panic: balloc:
// out of blocks. assumed_free may need to be raised to be more than
// the number of free blocks. this test takes a long time.
void
badwrite(char *s)
{
  int assumed_free = 600;
  
  unlink("junk");
  for(int i = 0; i < assumed_free; i++){
    int fd = open("junk", O_CREATE|O_WRONLY);
    if(fd < 0){
      printf("open junk failed\n");
      exit(1);
    }
    write(fd, (char*)0xffffffffffL, 1);
    close(fd);
    unlink("junk");
  }

  int fd = open("junk", O_CREATE|O_WRONLY);
  if(fd < 0){
    printf("open junk failed\n");
    exit(1);
  }
  if(write(fd, "x", 1) != 1){
    printf("write failed\n");
    exit(1);
  }
  close(fd);
  unlink("junk");

  exit(0);
}

// regression test. test whether exec() leaks memory if one of the
// arguments is invalid. the test passes if the kernel doesn't panic.
void
badarg(char *s)
{
  for(int i = 0; i < 50000; i++){
    char *argv[2];
    argv[0] = (char*)0xffffffff;
    argv[1] = 0;
    exec("echo", argv);
  }
  
  exit(0);
}

// test the exec() code that cleans up if it runs out
// of memory. it's really a test that such a condition
// doesn't cause a panic.
void
execout(char *s)
{
  for(int avail = 0; avail < 15; avail++){
    int pid = fork();
    if(pid < 0){
      printf("fork failed\n");
      exit(1);
    } else if(pid == 0){
      // allocate all of memory.
      while(1){
        uint64 a = (uint64) sbrk(4096);
        if(a == 0xffffffffffffffffLL)
          break;
        *(char*)(a + 4096 - 1) = 1;
      }

      // free a few pages, in order to let exec() make some
      // progress.
      for(int i = 0; i < avail; i++)
        sbrk(-4096);
      
      close(1);
      char *args[] = { "echo", "x", 0 };
      exec("echo", args);
      exit(0);
    } else {
      wait((int*)0);
    }
  }

  exit(0);
}

//
// use sbrk() to count how many free physical memory pages there are.
// touches the pages to force allocation.
// because out of memory with lazy allocation results in the process
// taking a fault and being killed, fork and report back.
//
int
countfree()
{
  int fds[2];

  if(pipe(fds) < 0){
    printf("pipe() failed in countfree()\n");
    exit(1);
  }
  
  int pid = fork();

  if(pid < 0){
    printf("fork failed in countfree()\n");
    exit(1);
  }

  if(pid == 0){
    close(fds[0]);
    
    while(1){
      uint64 a = (uint64) sbrk(4096);
      if(a == 0xffffffffffffffff){
        break;
      }

      // modify the memory to make sure it's really allocated.
      *(char *)(a + 4096 - 1) = 1;

      // report back one more page.
      if(write(fds[1], "x", 1) != 1){
        printf("write() failed in countfree()\n");
        exit(1);
      }
    }

    exit(0);
  }

  close(fds[1]);

  int n = 0;
  while(1){
    char c;
    int cc = read(fds[0], &c, 1);
    if(cc < 0){
      printf("read() failed in countfree()\n");
      exit(1);
    }
    if(cc == 0)
      break;
    n += 1;
  }

  close(fds[0]);
  wait((int*)0);
  
  return n;
}

// run each test in its own process. run returns 1 if child's exit()
// indicates success.
int
run(void f(char *), char *s) {
  int pid;
  int xstatus;

  printf("test %s: ", s);
  if((pid = fork()) < 0) {
    printf("runtest: fork error\n");
    exit(1);
  }
  if(pid == 0) {
    f(s);
    exit(0);
  } else {
    wait(&xstatus);
    if(xstatus != 0) 
      printf("FAILED\n");
    else
      printf("OK\n");
    return xstatus == 0;
  }
}

int
main(int argc, char *argv[])
{
  int continuous = 0;
  char *justone = 0;

  if(argc == 2 && strcmp(argv[1], "-c") == 0){
    continuous = 1;
  } else if(argc == 2 && strcmp(argv[1], "-C") == 0){
    continuous = 2;
  } else if(argc == 2 && argv[1][0] != '-'){
    justone = argv[1];
  } else if(argc > 1){
    printf("Usage: usertests [-c] [testname]\n");
    exit(1);
  }
  
  struct test {
    void (*f)(char *);
    char *s;
  } tests[] = {
    {MAXVAplus, "MAXVAplus"},
    {manywrites, "manywrites"},
    {execout, "execout"},
    {copyin, "copyin"},
    {copyout, "copyout"},
    {copyinstr1, "copyinstr1"},
    {copyinstr2, "copyinstr2"},
    {copyinstr3, "copyinstr3"},
    {rwsbrk, "rwsbrk" },
    {truncate1, "truncate1"},
    {truncate2, "truncate2"},
    {truncate3, "truncate3"},
    {reparent2, "reparent2"},
    {pgbug, "pgbug" },
    {sbrkbugs, "sbrkbugs" },
    // {badwrite, "badwrite" },
    {badarg, "badarg" },
    {reparent, "reparent" },
    {twochildren, "twochildren"},
    {forkfork, "forkfork"},
    {forkforkfork, "forkforkfork"},
    {argptest, "argptest"},
    {createdelete, "createdelete"},
    {linkunlink, "linkunlink"},
    {linktest, "linktest"},
    {unlinkread, "unlinkread"},
    {concreate, "concreate"},
    {subdir, "subdir"},
    {fourfiles, "fourfiles"},
    {sharedfd, "sharedfd"},
    {dirtest, "dirtest"},
    {exectest, "exectest"},
    {bigargtest, "bigargtest"},
    {bigwrite, "bigwrite"},
    {bsstest, "bsstest"},
    {sbrkbasic, "sbrkbasic"},
    {sbrkmuch, "sbrkmuch"},
    {kernmem, "kernmem"},
    {sbrkfail, "sbrkfail"},
    {sbrkarg, "sbrkarg"},
    {sbrklast, "sbrklast"},
    {sbrk8000, "sbrk8000"},
    {validatetest, "validatetest"},
    {stacktest, "stacktest"},
    {opentest, "opentest"},
    {writetest, "writetest"},
    {writebig, "writebig"},
    {createtest, "createtest"},
    {openiputtest, "openiput"},
    {exitiputtest, "exitiput"},
    {iputtest, "iput"},
    {mem, "mem"},
    {pipe1, "pipe1"},
    {killstatus, "killstatus"},
    {preempt, "preempt"},
    {exitwait, "exitwait"},
    {rmdot, "rmdot"},
    {fourteen, "fourteen"},
    {bigfile, "bigfile"},
    {dirfile, "dirfile"},
    {iref, "iref"},
    {forktest, "forktest"},
    {bigdir, "bigdir"}, // slow
    { 0, 0},
  };

  if(continuous){
    printf("continuous usertests starting\n");
    while(1){
      int fail = 0;
      int free0 = countfree();
      for (struct test *t = tests; t->s != 0; t++) {
        if(!run(t->f, t->s)){
          fail = 1;
          break;
        }
      }
      if(fail){
        printf("SOME TESTS FAILED\n");
        if(continuous != 2)
          exit(1);
      }
      int free1 = countfree();
      if(free1 < free0){
        printf("FAILED -- lost %d free pages\n", free0 - free1);
        if(continuous != 2)
          exit(1);
      }
    }
  }

  printf("usertests starting\n");
  int free0 = countfree();
  int free1 = 0;
  int fail = 0;
  for (struct test *t = tests; t->s != 0; t++) {
    if((justone == 0) || strcmp(t->s, justone) == 0) {
      if(!run(t->f, t->s))
        fail = 1;
    }
  }

  if(fail){
    printf("SOME TESTS FAILED\n");
    exit(1);
  } else if((free1 = countfree()) < free0){
    printf("FAILED -- lost some free pages %d (out of %d)\n", free1, free0);
    exit(1);
  } else {
    printf("ALL TESTS PASSED\n");
    exit(0);
  }
}

// #include "kernel/param.h"
// #include "kernel/types.h"
// #include "kernel/stat.h"
// #include "user/user.h"
// #include "kernel/fs.h"
// #include "kernel/fcntl.h"
// #include "kernel/syscall.h"
// #include "kernel/memlayout.h"
// #include "kernel/riscv.h"

// #define BUFSZ  (MAXOPBLOCKS+2)*BSIZE

// char buf[BUFSZ];
// char name[3];
// char *echoargv[] = { "echo", "ALL", "TESTS", "PASSED", 0 };

// // does chdir() call iput(p->cwd) in a transaction?
// void
// iputtest(void)
// {
//   printf("iput test\n");

//   if(mkdir("iputdir") < 0){
//     printf("mkdir failed\n");
//     exit();
//   }
//   if(chdir("iputdir") < 0){
//     printf("chdir iputdir failed\n");
//     exit();
//   }
//   if(unlink("../iputdir") < 0){
//     printf("unlink ../iputdir failed\n");
//     exit();
//   }
//   if(chdir("/") < 0){
//     printf("chdir / failed\n");
//     exit();
//   }
//   printf("iput test ok\n");
// }

// // does exit() call iput(p->cwd) in a transaction?
// void
// exitiputtest(void)
// {
//   int pid;

//   printf("exitiput test\n");

//   pid = fork();
//   if(pid < 0){
//     printf("fork failed\n");
//     exit();
//   }
//   if(pid == 0){
//     if(mkdir("iputdir") < 0){
//       printf("mkdir failed\n");
//       exit();
//     }
//     if(chdir("iputdir") < 0){
//       printf("child chdir failed\n");
//       exit();
//     }
//     if(unlink("../iputdir") < 0){
//       printf("unlink ../iputdir failed\n");
//       exit();
//     }
//     exit();
//   }
//   wait();
//   printf("exitiput test ok\n");
// }

// // does the error path in open() for attempt to write a
// // directory call iput() in a transaction?
// // needs a hacked kernel that pauses just after the namei()
// // call in sys_open():
// //    if((ip = namei(path)) == 0)
// //      return -1;
// //    {
// //      int i;
// //      for(i = 0; i < 10000; i++)
// //        yield();
// //    }

// void
// openiputtest(void)
// {
//   int pid;

//   printf("openiput test\n");
//   if(mkdir("oidir") < 0){
//     printf("mkdir oidir failed\n");
//     exit();
//   }
//   pid = fork();
//   if(pid < 0){
//     printf("fork failed\n");
//     exit();
//   }
//   if(pid == 0){
//     int fd = open("oidir", O_RDWR);
//     if(fd >= 0){
//       printf("open directory for write succeeded\n");
//       exit();
//     }
//     exit();
//   }
//   sleep(1);
//   if(unlink("oidir") != 0){
//     printf("unlink failed\n");
//     exit();
//   }
//   wait();
//   printf("openiput test ok\n");
// }

// // simple file system tests

// void
// opentest(void)
// {
//   int fd;

//   printf("open test\n");
//   fd = open("echo", 0);
//   if(fd < 0){
//     printf("open echo failed!\n");
//     exit();
//   }
//   close(fd);
//   fd = open("doesnotexist", 0);
//   if(fd >= 0){
//     printf("open doesnotexist succeeded!\n");
//     exit();
//   }
//   printf("open test ok\n");
// }

// void
// writetest(void)
// {
//   int fd;
//   int i;
//   enum { N=100, SZ=10 };
  
//   printf("small file test\n");
//   fd = open("small", O_CREATE|O_RDWR);
//   if(fd >= 0){
//     printf("creat small succeeded; ok\n");
//   } else {
//     printf("error: creat small failed!\n");
//     exit();
//   }
//   for(i = 0; i < N; i++){
//     if(write(fd, "aaaaaaaaaa", SZ) != SZ){
//       printf("error: write aa %d new file failed\n", i);
//       exit();
//     }
//     if(write(fd, "bbbbbbbbbb", SZ) != SZ){
//       printf("error: write bb %d new file failed\n", i);
//       exit();
//     }
//   }
//   printf("writes ok\n");
//   close(fd);
//   fd = open("small", O_RDONLY);
//   if(fd >= 0){
//     printf("open small succeeded ok\n");
//   } else {
//     printf("error: open small failed!\n");
//     exit();
//   }
//   i = read(fd, buf, N*SZ*2);
//   if(i == N*SZ*2){
//     printf("read succeeded ok\n");
//   } else {
//     printf("read failed\n");
//     exit();
//   }
//   close(fd);

//   if(unlink("small") < 0){
//     printf("unlink small failed\n");
//     exit();
//   }
//   printf("small file test ok\n");
// }

// void
// writetest1(void)
// {
//   int i, fd, n;

//   printf("big files test\n");

//   fd = open("big", O_CREATE|O_RDWR);
//   if(fd < 0){
//     printf("error: creat big failed!\n");
//     exit();
//   }

//   for(i = 0; i < MAXFILE; i++){
//     ((int*)buf)[0] = i;
//     if(write(fd, buf, BSIZE) != BSIZE){
//       printf("error: write big file failed\n", i);
//       exit();
//     }
//   }

//   close(fd);

//   fd = open("big", O_RDONLY);
//   if(fd < 0){
//     printf("error: open big failed!\n");
//     exit();
//   }

//   n = 0;
//   for(;;){
//     i = read(fd, buf, BSIZE);
//     if(i == 0){
//       if(n == MAXFILE - 1){
//         printf("read only %d blocks from big", n);
//         exit();
//       }
//       break;
//     } else if(i != BSIZE){
//       printf("read failed %d\n", i);
//       exit();
//     }
//     if(((int*)buf)[0] != n){
//       printf("read content of block %d is %d\n",
//              n, ((int*)buf)[0]);
//       exit();
//     }
//     n++;
//   }
//   close(fd);
//   if(unlink("big") < 0){
//     printf("unlink big failed\n");
//     exit();
//   }
//   printf("big files ok\n");
// }

// void
// createtest(void)
// {
//   int i, fd;
//   enum { N=52 };
  
//   printf("many creates, followed by unlink test\n");

//   name[0] = 'a';
//   name[2] = '\0';
//   for(i = 0; i < N; i++){
//     name[1] = '0' + i;
//     fd = open(name, O_CREATE|O_RDWR);
//     close(fd);
//   }
//   name[0] = 'a';
//   name[2] = '\0';
//   for(i = 0; i < N; i++){
//     name[1] = '0' + i;
//     unlink(name);
//   }
//   printf("many creates, followed by unlink; ok\n");
// }

// void dirtest(void)
// {
//   printf("mkdir test\n");

//   if(mkdir("dir0") < 0){
//     printf("mkdir failed\n");
//     exit();
//   }

//   if(chdir("dir0") < 0){
//     printf("chdir dir0 failed\n");
//     exit();
//   }

//   if(chdir("..") < 0){
//     printf("chdir .. failed\n");
//     exit();
//   }

//   if(unlink("dir0") < 0){
//     printf("unlink dir0 failed\n");
//     exit();
//   }
//   printf("mkdir test ok\n");
// }

// void
// exectest(void)
// {
//   printf("exec test\n");
//   if(exec("echo", echoargv) < 0){
//     printf("exec echo failed\n");
//     exit();
//   }
// }

// // simple fork and pipe read/write

// void
// pipe1(void)
// {
//   int fds[2], pid;
//   int seq, i, n, cc, total;
//   enum { N=5, SZ=1033 };
  
//   if(pipe(fds) != 0){
//     printf("pipe() failed\n");
//     exit();
//   }
//   pid = fork();
//   seq = 0;
//   if(pid == 0){
//     close(fds[0]);
//     for(n = 0; n < N; n++){
//       for(i = 0; i < SZ; i++)
//         buf[i] = seq++;
//       if(write(fds[1], buf, SZ) != SZ){
//         printf("pipe1 oops 1\n");
//         exit();
//       }
//     }
//     exit();
//   } else if(pid > 0){
//     close(fds[1]);
//     total = 0;
//     cc = 1;
//     while((n = read(fds[0], buf, cc)) > 0){
//       for(i = 0; i < n; i++){
//         if((buf[i] & 0xff) != (seq++ & 0xff)){
//           printf("pipe1 oops 2\n");
//           return;
//         }
//       }
//       total += n;
//       cc = cc * 2;
//       if(cc > sizeof(buf))
//         cc = sizeof(buf);
//     }
//     if(total != N * SZ){
//       printf("pipe1 oops 3 total %d\n", total);
//       exit();
//     }
//     close(fds[0]);
//     wait();
//   } else {
//     printf("fork() failed\n");
//     exit();
//   }
//   printf("pipe1 ok\n");
// }

// // meant to be run w/ at most two CPUs
// void
// preempt(void)
// {
//   int pid1, pid2, pid3;
//   int pfds[2];

//   printf("preempt: ");
//   pid1 = fork();
//   if(pid1 < 0) {
//     printf("fork failed");
//     exit();
//   }
//   if(pid1 == 0)
//     for(;;)
//       ;

//   pid2 = fork();
//   if(pid2 < 0) {
//     printf("fork failed\n");
//     exit();
//   }
//   if(pid2 == 0)
//     for(;;)
//       ;

//   pipe(pfds);
//   pid3 = fork();
//   if(pid3 < 0) {
//      printf("fork failed\n");
//      exit();
//   }
//   if(pid3 == 0){
//     close(pfds[0]);
//     if(write(pfds[1], "x", 1) != 1)
//       printf("preempt write error");
//     close(pfds[1]);
//     for(;;)
//       ;
//   }

//   close(pfds[1]);
//   if(read(pfds[0], buf, sizeof(buf)) != 1){
//     printf("preempt read error");
//     return;
//   }
//   close(pfds[0]);
//   printf("kill... ");
//   kill(pid1);
//   kill(pid2);
//   kill(pid3);
//   printf("wait... ");
//   wait();
//   wait();
//   wait();
//   printf("preempt ok\n");
// }

// // try to find any races between exit and wait
// void
// exitwait(void)
// {
//   int i, pid;

//   printf("exitwait test\n");

//   for(i = 0; i < 100; i++){
//     pid = fork();
//     if(pid < 0){
//       printf("fork failed\n");
//       exit();
//     }
//     if(pid){
//       if(wait() != pid){
//         printf("wait wrong pid\n");
//         exit();
//       }
//     } else {
//       exit();
//     }
//   }
//   printf("exitwait ok\n");
// }

// // try to find races in the reparenting
// // code that handles a parent exiting
// // when it still has live children.
// void
// reparent(void)
// {
//   int master_pid = getpid();
  
//   printf("reparent test\n");

//   for(int i = 0; i < 200; i++){
//     int pid = fork();
//     if(pid < 0){
//       printf("fork failed\n");
//       exit();
//     }
//     if(pid){
//       if(wait() != pid){
//         printf("wait wrong pid\n");
//         exit();
//       }
//     } else {
//       int pid2 = fork();
//       if(pid2 < 0){
//         printf("fork failed\n");
//         kill(master_pid);
//         exit();
//       }
//       if(pid2 == 0){
//         exit();
//       } else {
//         exit();
//       }
//     }
//   }
//   printf("reparent ok\n");
// }

// // what if two children exit() at the same time?
// void
// twochildren(void)
// {
//   printf("twochildren test\n");

//   for(int i = 0; i < 1000; i++){
//     int pid1 = fork();
//     if(pid1 < 0){
//       printf("fork failed\n");
//       exit();
//     }
//     if(pid1 == 0){
//       exit();
//     } else {
//       int pid2 = fork();
//       if(pid2 < 0){
//         printf("fork failed\n");
//         exit();
//       }
//       if(pid2 == 0){
//         exit();
//       } else {
//         wait();
//         wait();
//       }
//     }
//   }
//   printf("twochildren ok\n");
// }

// // concurrent forks to try to expose locking bugs.
// void
// forkfork(void)
// {
//   int ppid = getpid();
//   enum { N=2 };
  
//   printf("forkfork test\n");

//   for(int i = 0; i < N; i++){
//     int pid = fork();
//     if(pid < 0){
//       printf("fork failed");
//       exit();
//     }
//     if(pid == 0){
//       for(int j = 0; j < 200; j++){
//         int pid1 = fork();
//         if(pid1 < 0){
//           printf("fork failed\n");
//           kill(ppid);
//           exit();
//         }
//         if(pid1 == 0){
//           exit();
//         }
//         wait();
//       }
//       exit();
//     }
//   }

//   for(int i = 0; i < N; i++){
//     wait();
//   }

//   printf("forkfork ok\n");
// }

// void
// forkforkfork(void)
// {
//   printf("forkforkfork test\n");

//   unlink("stopforking");

//   int pid = fork();
//   if(pid < 0){
//     printf("fork failed");
//     exit();
//   }
//   if(pid == 0){
//     while(1){
//       int fd = open("stopforking", 0);
//       if(fd >= 0){
//         exit();
//       }
//       if(fork() < 0){
//         close(open("stopforking", O_CREATE|O_RDWR));
//       }
//     }

//     exit();
//   }

//   sleep(20); // two seconds
//   close(open("stopforking", O_CREATE|O_RDWR));
//   wait();
//   sleep(10); // one second

//   printf("forkforkfork ok\n");
// }

// void
// mem(void)
// {
//   void *m1, *m2;
//   int pid, ppid;

//   printf("mem test\n");
//   ppid = getpid();
//   if((pid = fork()) == 0){
//     m1 = 0;
//     while((m2 = malloc(10001)) != 0){
//       *(char**)m2 = m1;
//       m1 = m2;
//     }
//     while(m1){
//       m2 = *(char**)m1;
//       free(m1);
//       m1 = m2;
//     }
//     m1 = malloc(1024*20);
//     if(m1 == 0){
//       printf("couldn't allocate mem?!!\n");
//       kill(ppid);
//       exit();
//     }
//     free(m1);
//     printf("mem ok\n");
//     exit();
//   } else {
//     wait();
//   }
// }

// // More file system tests

// // two processes write to the same file descriptor
// // is the offset shared? does inode locking work?
// void
// sharedfd(void)
// {
//   int fd, pid, i, n, nc, np;
//   enum { N = 1000, SZ=10};
//   char buf[SZ];

//   printf("sharedfd test\n");

//   unlink("sharedfd");
//   fd = open("sharedfd", O_CREATE|O_RDWR);
//   if(fd < 0){
//     printf("fstests: cannot open sharedfd for writing");
//     return;
//   }
//   pid = fork();
//   memset(buf, pid==0?'c':'p', sizeof(buf));
//   for(i = 0; i < N; i++){
//     if(write(fd, buf, sizeof(buf)) != sizeof(buf)){
//       printf("fstests: write sharedfd failed\n");
//       break;
//     }
//   }
//   if(pid == 0)
//     exit();
//   else
//     wait();
//   close(fd);
//   fd = open("sharedfd", 0);
//   if(fd < 0){
//     printf("fstests: cannot open sharedfd for reading\n");
//     return;
//   }
//   nc = np = 0;
//   while((n = read(fd, buf, sizeof(buf))) > 0){
//     for(i = 0; i < sizeof(buf); i++){
//       if(buf[i] == 'c')
//         nc++;
//       if(buf[i] == 'p')
//         np++;
//     }
//   }
//   close(fd);
//   unlink("sharedfd");
//   if(nc == N*SZ && np == N*SZ){
//     printf("sharedfd ok\n");
//   } else {
//     printf("sharedfd oops %d %d\n", nc, np);
//     exit();
//   }
// }

// // four processes write different files at the same
// // time, to test block allocation.
// void
// fourfiles(void)
// {
//   int fd, pid, i, j, n, total, pi;
//   char *names[] = { "f0", "f1", "f2", "f3" };
//   char *fname;
//   enum { N=12, NCHILD=4, SZ=500 };
  
//   printf("fourfiles test\n");

//   for(pi = 0; pi < NCHILD; pi++){
//     fname = names[pi];
//     unlink(fname);

//     pid = fork();
//     if(pid < 0){
//       printf("fork failed\n");
//       exit();
//     }

//     if(pid == 0){
//       fd = open(fname, O_CREATE | O_RDWR);
//       if(fd < 0){
//         printf("create failed\n");
//         exit();
//       }

//       memset(buf, '0'+pi, SZ);
//       for(i = 0; i < N; i++){
//         if((n = write(fd, buf, SZ)) != SZ){
//           printf("write failed %d\n", n);
//           exit();
//         }
//       }
//       exit();
//     }
//   }

//   for(pi = 0; pi < NCHILD; pi++){
//     wait();
//   }

//   for(i = 0; i < NCHILD; i++){
//     fname = names[i];
//     fd = open(fname, 0);
//     total = 0;
//     while((n = read(fd, buf, sizeof(buf))) > 0){
//       for(j = 0; j < n; j++){
//         if(buf[j] != '0'+i){
//           printf("wrong char\n");
//           exit();
//         }
//       }
//       total += n;
//     }
//     close(fd);
//     if(total != N*SZ){
//       printf("wrong length %d\n", total);
//       exit();
//     }
//     unlink(fname);
//   }

//   printf("fourfiles ok\n");
// }

// // four processes create and delete different files in same directory
// void
// createdelete(void)
// {
//   enum { N = 20, NCHILD=4 };
//   int pid, i, fd, pi;
//   char name[32];

//   printf("createdelete test\n");

//   for(pi = 0; pi < NCHILD; pi++){
//     pid = fork();
//     if(pid < 0){
//       printf("fork failed\n");
//       exit();
//     }

//     if(pid == 0){
//       name[0] = 'p' + pi;
//       name[2] = '\0';
//       for(i = 0; i < N; i++){
//         name[1] = '0' + i;
//         fd = open(name, O_CREATE | O_RDWR);
//         if(fd < 0){
//           printf("create failed\n");
//           exit();
//         }
//         close(fd);
//         if(i > 0 && (i % 2 ) == 0){
//           name[1] = '0' + (i / 2);
//           if(unlink(name) < 0){
//             printf("unlink failed\n");
//             exit();
//           }
//         }
//       }
//       exit();
//     }
//   }

//   for(pi = 0; pi < NCHILD; pi++){
//     wait();
//   }

//   name[0] = name[1] = name[2] = 0;
//   for(i = 0; i < N; i++){
//     for(pi = 0; pi < NCHILD; pi++){
//       name[0] = 'p' + pi;
//       name[1] = '0' + i;
//       fd = open(name, 0);
//       if((i == 0 || i >= N/2) && fd < 0){
//         printf("oops createdelete %s didn't exist\n", name);
//         exit();
//       } else if((i >= 1 && i < N/2) && fd >= 0){
//         printf("oops createdelete %s did exist\n", name);
//         exit();
//       }
//       if(fd >= 0)
//         close(fd);
//     }
//   }

//   for(i = 0; i < N; i++){
//     for(pi = 0; pi < NCHILD; pi++){
//       name[0] = 'p' + i;
//       name[1] = '0' + i;
//       unlink(name);
//     }
//   }

//   printf("createdelete ok\n");
// }

// // can I unlink a file and still read it?
// void
// unlinkread(void)
// {
//   enum { SZ = 5 };
//   int fd, fd1;

//   printf("unlinkread test\n");
//   fd = open("unlinkread", O_CREATE | O_RDWR);
//   if(fd < 0){
//     printf("create unlinkread failed\n");
//     exit();
//   }
//   write(fd, "hello", SZ);
//   close(fd);

//   fd = open("unlinkread", O_RDWR);
//   if(fd < 0){
//     printf("open unlinkread failed\n");
//     exit();
//   }
//   if(unlink("unlinkread") != 0){
//     printf("unlink unlinkread failed\n");
//     exit();
//   }

//   fd1 = open("unlinkread", O_CREATE | O_RDWR);
//   write(fd1, "yyy", 3);
//   close(fd1);

//   if(read(fd, buf, sizeof(buf)) != SZ){
//     printf("unlinkread read failed");
//     exit();
//   }
//   if(buf[0] != 'h'){
//     printf("unlinkread wrong data\n");
//     exit();
//   }
//   if(write(fd, buf, 10) != 10){
//     printf("unlinkread write failed\n");
//     exit();
//   }
//   close(fd);
//   unlink("unlinkread");
//   printf("unlinkread ok\n");
// }

// void
// linktest(void)
// {
//   enum { SZ = 5 };
//   int fd;

//   printf("linktest\n");

//   unlink("lf1");
//   unlink("lf2");

//   fd = open("lf1", O_CREATE|O_RDWR);
//   if(fd < 0){
//     printf("create lf1 failed\n");
//     exit();
//   }
//   if(write(fd, "hello", SZ) != SZ){
//     printf("write lf1 failed\n");
//     exit();
//   }
//   close(fd);

//   if(link("lf1", "lf2") < 0){
//     printf("link lf1 lf2 failed\n");
//     exit();
//   }
//   unlink("lf1");

//   if(open("lf1", 0) >= 0){
//     printf("unlinked lf1 but it is still there!\n");
//     exit();
//   }

//   fd = open("lf2", 0);
//   if(fd < 0){
//     printf("open lf2 failed\n");
//     exit();
//   }
//   if(read(fd, buf, sizeof(buf)) != SZ){
//     printf("read lf2 failed\n");
//     exit();
//   }
//   close(fd);

//   if(link("lf2", "lf2") >= 0){
//     printf("link lf2 lf2 succeeded! oops\n");
//     exit();
//   }

//   unlink("lf2");
//   if(link("lf2", "lf1") >= 0){
//     printf("link non-existant succeeded! oops\n");
//     exit();
//   }

//   if(link(".", "lf1") >= 0){
//     printf("link . lf1 succeeded! oops\n");
//     exit();
//   }

//   printf("linktest ok\n");
// }

// // test concurrent create/link/unlink of the same file
// void
// concreate(void)
// {
//   enum { N = 40 };
//   char file[3];
//   int i, pid, n, fd;
//   char fa[N];
//   struct {
//     ushort inum;
//     char name[DIRSIZ];
//   } de;

//   printf("concreate test\n");
//   file[0] = 'C';
//   file[2] = '\0';
//   for(i = 0; i < N; i++){
//     file[1] = '0' + i;
//     unlink(file);
//     pid = fork();
//     if(pid && (i % 3) == 1){
//       link("C0", file);
//     } else if(pid == 0 && (i % 5) == 1){
//       link("C0", file);
//     } else {
//       fd = open(file, O_CREATE | O_RDWR);
//       if(fd < 0){
//         printf("concreate create %s failed\n", file);
//         exit();
//       }
//       close(fd);
//     }
//     if(pid == 0)
//       exit();
//     else
//       wait();
//   }

//   memset(fa, 0, sizeof(fa));
//   fd = open(".", 0);
//   n = 0;
//   while(read(fd, &de, sizeof(de)) > 0){
//     if(de.inum == 0)
//       continue;
//     if(de.name[0] == 'C' && de.name[2] == '\0'){
//       i = de.name[1] - '0';
//       if(i < 0 || i >= sizeof(fa)){
//         printf("concreate weird file %s\n", de.name);
//         exit();
//       }
//       if(fa[i]){
//         printf("concreate duplicate file %s\n", de.name);
//         exit();
//       }
//       fa[i] = 1;
//       n++;
//     }
//   }
//   close(fd);

//   if(n != N){
//     printf("concreate not enough files in directory listing\n");
//     exit();
//   }

//   for(i = 0; i < N; i++){
//     file[1] = '0' + i;
//     pid = fork();
//     if(pid < 0){
//       printf("fork failed\n");
//       exit();
//     }
//     if(((i % 3) == 0 && pid == 0) ||
//        ((i % 3) == 1 && pid != 0)){
//       close(open(file, 0));
//       close(open(file, 0));
//       close(open(file, 0));
//       close(open(file, 0));
//     } else {
//       unlink(file);
//       unlink(file);
//       unlink(file);
//       unlink(file);
//     }
//     if(pid == 0)
//       exit();
//     else
//       wait();
//   }

//   printf("concreate ok\n");
// }

// // another concurrent link/unlink/create test,
// // to look for deadlocks.
// void
// linkunlink()
// {
//   int pid, i;

//   printf("linkunlink test\n");

//   unlink("x");
//   pid = fork();
//   if(pid < 0){
//     printf("fork failed\n");
//     exit();
//   }

//   unsigned int x = (pid ? 1 : 97);
//   for(i = 0; i < 100; i++){
//     x = x * 1103515245 + 12345;
//     if((x % 3) == 0){
//       close(open("x", O_RDWR | O_CREATE));
//     } else if((x % 3) == 1){
//       link("cat", "x");
//     } else {
//       unlink("x");
//     }
//   }

//   if(pid)
//     wait();
//   else
//     exit();

//   printf("linkunlink ok\n");
// }

// // directory that uses indirect blocks
// void
// bigdir(void)
// {
//   enum { N = 500 };
//   int i, fd;
//   char name[10];

//   printf("bigdir test\n");
//   unlink("bd");

//   fd = open("bd", O_CREATE);
//   if(fd < 0){
//     printf("bigdir create failed\n");
//     exit();
//   }
//   close(fd);

//   for(i = 0; i < N; i++){
//     name[0] = 'x';
//     name[1] = '0' + (i / 64);
//     name[2] = '0' + (i % 64);
//     name[3] = '\0';
//     if(link("bd", name) != 0){
//       printf("bigdir link failed\n");
//       exit();
//     }
//   }

//   unlink("bd");
//   for(i = 0; i < N; i++){
//     name[0] = 'x';
//     name[1] = '0' + (i / 64);
//     name[2] = '0' + (i % 64);
//     name[3] = '\0';
//     if(unlink(name) != 0){
//       printf("bigdir unlink failed");
//       exit();
//     }
//   }

//   printf("bigdir ok\n");
// }

// void
// subdir(void)
// {
//   int fd, cc;

//   printf("subdir test\n");

//   unlink("ff");
//   if(mkdir("dd") != 0){
//     printf("subdir mkdir dd failed\n");
//     exit();
//   }

//   fd = open("dd/ff", O_CREATE | O_RDWR);
//   if(fd < 0){
//     printf("create dd/ff failed\n");
//     exit();
//   }
//   write(fd, "ff", 2);
//   close(fd);

//   if(unlink("dd") >= 0){
//     printf("unlink dd (non-empty dir) succeeded!\n");
//     exit();
//   }

//   if(mkdir("/dd/dd") != 0){
//     printf("subdir mkdir dd/dd failed\n");
//     exit();
//   }

//   fd = open("dd/dd/ff", O_CREATE | O_RDWR);
//   if(fd < 0){
//     printf("create dd/dd/ff failed\n");
//     exit();
//   }
//   write(fd, "FF", 2);
//   close(fd);

//   fd = open("dd/dd/../ff", 0);
//   if(fd < 0){
//     printf("open dd/dd/../ff failed\n");
//     exit();
//   }
//   cc = read(fd, buf, sizeof(buf));
//   if(cc != 2 || buf[0] != 'f'){
//     printf("dd/dd/../ff wrong content\n");
//     exit();
//   }
//   close(fd);

//   if(link("dd/dd/ff", "dd/dd/ffff") != 0){
//     printf("link dd/dd/ff dd/dd/ffff failed\n");
//     exit();
//   }

//   if(unlink("dd/dd/ff") != 0){
//     printf("unlink dd/dd/ff failed\n");
//     exit();
//   }
//   if(open("dd/dd/ff", O_RDONLY) >= 0){
//     printf("open (unlinked) dd/dd/ff succeeded\n");
//     exit();
//   }

//   if(chdir("dd") != 0){
//     printf("chdir dd failed\n");
//     exit();
//   }
//   if(chdir("dd/../../dd") != 0){
//     printf("chdir dd/../../dd failed\n");
//     exit();
//   }
//   if(chdir("dd/../../../dd") != 0){
//     printf("chdir dd/../../dd failed\n");
//     exit();
//   }
//   if(chdir("./..") != 0){
//     printf("chdir ./.. failed\n");
//     exit();
//   }

//   fd = open("dd/dd/ffff", 0);
//   if(fd < 0){
//     printf("open dd/dd/ffff failed\n");
//     exit();
//   }
//   if(read(fd, buf, sizeof(buf)) != 2){
//     printf("read dd/dd/ffff wrong len\n");
//     exit();
//   }
//   close(fd);

//   if(open("dd/dd/ff", O_RDONLY) >= 0){
//     printf("open (unlinked) dd/dd/ff succeeded!\n");
//     exit();
//   }

//   if(open("dd/ff/ff", O_CREATE|O_RDWR) >= 0){
//     printf("create dd/ff/ff succeeded!\n");
//     exit();
//   }
//   if(open("dd/xx/ff", O_CREATE|O_RDWR) >= 0){
//     printf("create dd/xx/ff succeeded!\n");
//     exit();
//   }
//   if(open("dd", O_CREATE) >= 0){
//     printf("create dd succeeded!\n");
//     exit();
//   }
//   if(open("dd", O_RDWR) >= 0){
//     printf("open dd rdwr succeeded!\n");
//     exit();
//   }
//   if(open("dd", O_WRONLY) >= 0){
//     printf("open dd wronly succeeded!\n");
//     exit();
//   }
//   if(link("dd/ff/ff", "dd/dd/xx") == 0){
//     printf("link dd/ff/ff dd/dd/xx succeeded!\n");
//     exit();
//   }
//   if(link("dd/xx/ff", "dd/dd/xx") == 0){
//     printf("link dd/xx/ff dd/dd/xx succeeded!\n");
//     exit();
//   }
//   if(link("dd/ff", "dd/dd/ffff") == 0){
//     printf("link dd/ff dd/dd/ffff succeeded!\n");
//     exit();
//   }
//   if(mkdir("dd/ff/ff") == 0){
//     printf("mkdir dd/ff/ff succeeded!\n");
//     exit();
//   }
//   if(mkdir("dd/xx/ff") == 0){
//     printf("mkdir dd/xx/ff succeeded!\n");
//     exit();
//   }
//   if(mkdir("dd/dd/ffff") == 0){
//     printf("mkdir dd/dd/ffff succeeded!\n");
//     exit();
//   }
//   if(unlink("dd/xx/ff") == 0){
//     printf("unlink dd/xx/ff succeeded!\n");
//     exit();
//   }
//   if(unlink("dd/ff/ff") == 0){
//     printf("unlink dd/ff/ff succeeded!\n");
//     exit();
//   }
//   if(chdir("dd/ff") == 0){
//     printf("chdir dd/ff succeeded!\n");
//     exit();
//   }
//   if(chdir("dd/xx") == 0){
//     printf("chdir dd/xx succeeded!\n");
//     exit();
//   }

//   if(unlink("dd/dd/ffff") != 0){
//     printf("unlink dd/dd/ff failed\n");
//     exit();
//   }
//   if(unlink("dd/ff") != 0){
//     printf("unlink dd/ff failed\n");
//     exit();
//   }
//   if(unlink("dd") == 0){
//     printf("unlink non-empty dd succeeded!\n");
//     exit();
//   }
//   if(unlink("dd/dd") < 0){
//     printf("unlink dd/dd failed\n");
//     exit();
//   }
//   if(unlink("dd") < 0){
//     printf("unlink dd failed\n");
//     exit();
//   }

//   printf("subdir ok\n");
// }

// // test writes that are larger than the log.
// void
// bigwrite(void)
// {
//   int fd, sz;

//   printf("bigwrite test\n");

//   unlink("bigwrite");
//   for(sz = 499; sz < (MAXOPBLOCKS+2)*BSIZE; sz += 471){
//     fd = open("bigwrite", O_CREATE | O_RDWR);
//     if(fd < 0){
//       printf("cannot create bigwrite\n");
//       exit();
//     }
//     int i;
//     for(i = 0; i < 2; i++){
//       int cc = write(fd, buf, sz);
//       if(cc != sz){
//         printf("write(%d) ret %d\n", sz, cc);
//         exit();
//       }
//     }
//     close(fd);
//     unlink("bigwrite");
//   }

//   printf("bigwrite ok\n");
// }

// void
// bigfile(void)
// {
//   enum { N = 20, SZ=600 };
//   int fd, i, total, cc;

//   printf("bigfile test\n");

//   unlink("bigfile");
//   fd = open("bigfile", O_CREATE | O_RDWR);
//   if(fd < 0){
//     printf("cannot create bigfile");
//     exit();
//   }
//   for(i = 0; i < N; i++){
//     memset(buf, i, SZ);
//     if(write(fd, buf, SZ) != SZ){
//       printf("write bigfile failed\n");
//       exit();
//     }
//   }
//   close(fd);

//   fd = open("bigfile", 0);
//   if(fd < 0){
//     printf("cannot open bigfile\n");
//     exit();
//   }
//   total = 0;
//   for(i = 0; ; i++){
//     cc = read(fd, buf, SZ/2);
//     if(cc < 0){
//       printf("read bigfile failed\n");
//       exit();
//     }
//     if(cc == 0)
//       break;
//     if(cc != SZ/2){
//       printf("short read bigfile\n");
//       exit();
//     }
//     if(buf[0] != i/2 || buf[SZ/2-1] != i/2){
//       printf("read bigfile wrong data\n");
//       exit();
//     }
//     total += cc;
//   }
//   close(fd);
//   if(total != N*SZ){
//     printf("read bigfile wrong total\n");
//     exit();
//   }
//   unlink("bigfile");

//   printf("bigfile test ok\n");
// }

// void
// fourteen(void)
// {
//   int fd;

//   // DIRSIZ is 14.
//   printf("fourteen test\n");

//   if(mkdir("12345678901234") != 0){
//     printf("mkdir 12345678901234 failed\n");
//     exit();
//   }
//   if(mkdir("12345678901234/123456789012345") != 0){
//     printf("mkdir 12345678901234/123456789012345 failed\n");
//     exit();
//   }
//   fd = open("123456789012345/123456789012345/123456789012345", O_CREATE);
//   if(fd < 0){
//     printf("create 123456789012345/123456789012345/123456789012345 failed\n");
//     exit();
//   }
//   close(fd);
//   fd = open("12345678901234/12345678901234/12345678901234", 0);
//   if(fd < 0){
//     printf("open 12345678901234/12345678901234/12345678901234 failed\n");
//     exit();
//   }
//   close(fd);

//   if(mkdir("12345678901234/12345678901234") == 0){
//     printf("mkdir 12345678901234/12345678901234 succeeded!\n");
//     exit();
//   }
//   if(mkdir("123456789012345/12345678901234") == 0){
//     printf("mkdir 12345678901234/123456789012345 succeeded!\n");
//     exit();
//   }

//   printf("fourteen ok\n");
// }

// void
// rmdot(void)
// {
//   printf("rmdot test\n");
//   if(mkdir("dots") != 0){
//     printf("mkdir dots failed\n");
//     exit();
//   }
//   if(chdir("dots") != 0){
//     printf("chdir dots failed\n");
//     exit();
//   }
//   if(unlink(".") == 0){
//     printf("rm . worked!\n");
//     exit();
//   }
//   if(unlink("..") == 0){
//     printf("rm .. worked!\n");
//     exit();
//   }
//   if(chdir("/") != 0){
//     printf("chdir / failed\n");
//     exit();
//   }
//   if(unlink("dots/.") == 0){
//     printf("unlink dots/. worked!\n");
//     exit();
//   }
//   if(unlink("dots/..") == 0){
//     printf("unlink dots/.. worked!\n");
//     exit();
//   }
//   if(unlink("dots") != 0){
//     printf("unlink dots failed!\n");
//     exit();
//   }
//   printf("rmdot ok\n");
// }

// void
// dirfile(void)
// {
//   int fd;

//   printf("dir vs file\n");

//   fd = open("dirfile", O_CREATE);
//   if(fd < 0){
//     printf("create dirfile failed\n");
//     exit();
//   }
//   close(fd);
//   if(chdir("dirfile") == 0){
//     printf("chdir dirfile succeeded!\n");
//     exit();
//   }
//   fd = open("dirfile/xx", 0);
//   if(fd >= 0){
//     printf("create dirfile/xx succeeded!\n");
//     exit();
//   }
//   fd = open("dirfile/xx", O_CREATE);
//   if(fd >= 0){
//     printf("create dirfile/xx succeeded!\n");
//     exit();
//   }
//   if(mkdir("dirfile/xx") == 0){
//     printf("mkdir dirfile/xx succeeded!\n");
//     exit();
//   }
//   if(unlink("dirfile/xx") == 0){
//     printf("unlink dirfile/xx succeeded!\n");
//     exit();
//   }
//   if(link("README", "dirfile/xx") == 0){
//     printf("link to dirfile/xx succeeded!\n");
//     exit();
//   }
//   if(unlink("dirfile") != 0){
//     printf("unlink dirfile failed!\n");
//     exit();
//   }

//   fd = open(".", O_RDWR);
//   if(fd >= 0){
//     printf("open . for writing succeeded!\n");
//     exit();
//   }
//   fd = open(".", 0);
//   if(write(fd, "x", 1) > 0){
//     printf("write . succeeded!\n");
//     exit();
//   }
//   close(fd);

//   printf("dir vs file OK\n");
// }

// // test that iput() is called at the end of _namei()
// void
// iref(void)
// {
//   int i, fd;

//   printf("empty file name\n");

//   for(i = 0; i < NINODE + 1; i++){
//     if(mkdir("irefd") != 0){
//       printf("mkdir irefd failed\n");
//       exit();
//     }
//     if(chdir("irefd") != 0){
//       printf("chdir irefd failed\n");
//       exit();
//     }

//     mkdir("");
//     link("README", "");
//     fd = open("", O_CREATE);
//     if(fd >= 0)
//       close(fd);
//     fd = open("xx", O_CREATE);
//     if(fd >= 0)
//       close(fd);
//     unlink("xx");
//   }

//   chdir("/");
//   printf("empty file name OK\n");
// }

// // test that fork fails gracefully
// // the forktest binary also does this, but it runs out of proc entries first.
// // inside the bigger usertests binary, we run out of memory first.
// void
// forktest(void)
// {
//   enum{ N = 1000 };
//   int n, pid;

//   printf("fork test\n");

//   for(n=0; n<N; n++){
//     pid = fork();
//     if(pid < 0)
//       break;
//     if(pid == 0)
//       exit();
//   }

//   if (n == 0) {
//     printf("no fork at all!\n");
//     exit();
//   }

//   if(n == N){
//     printf("fork claimed to work 1000 times!\n");
//     exit();
//   }

//   for(; n > 0; n--){
//     if(wait() < 0){
//       printf("wait stopped early\n");
//       exit();
//     }
//   }

//   if(wait() != -1){
//     printf("wait got too many\n");
//     exit();
//   }

//   printf("fork test OK\n");
// }

// void
// sbrktest(void)
// {
//   enum { BIG=100*1024*1024, TOOMUCH=1024*1024*1024};
//   int i, fds[2], pids[10], pid, ppid;
//   char *c, *oldbrk, scratch, *a, *b, *lastaddr, *p;
//   uint64 amt;
//   int fd;
//   int n;

//   printf("sbrk test\n");
//   oldbrk = sbrk(0);

//   // does sbrk() return the expected failure value?
//   a = sbrk(TOOMUCH);
//   if(a != (char*)0xffffffffffffffffL){
//     printf("sbrk(<toomuch>) returned %p\n", a);
//     exit();
//   }

//   // can one sbrk() less than a page?
//   a = sbrk(0);
//   for(i = 0; i < 5000; i++){
//     b = sbrk(1);
//     if(b != a){
//       printf("sbrk test failed %d %x %x\n", i, a, b);
//       exit();
//     }
//     *b = 1;
//     a = b + 1;
//   }
//   pid = fork();
//   if(pid < 0){
//     printf("sbrk test fork failed\n");
//     exit();
//   }
//   c = sbrk(1);
//   c = sbrk(1);
//   if(c != a + 1){
//     printf("sbrk test failed post-fork\n");
//     exit();
//   }
//   if(pid == 0)
//     exit();
//   wait();

//   // can one grow address space to something big?
//   a = sbrk(0);
//   amt = BIG - (uint64)a;
//   p = sbrk(amt);
//   if (p != a) {
//     printf("sbrk test failed to grow big address space; enough phys mem?\n");
//     exit();
//   }
//   lastaddr = (char*) (BIG-1);
//   *lastaddr = 99;

//   // can one de-allocate?
//   a = sbrk(0);
//   c = sbrk(-PGSIZE);
//   if(c == (char*)0xffffffffffffffffL){
//     printf("sbrk could not deallocate\n");
//     exit();
//   }
//   c = sbrk(0);
//   if(c != a - PGSIZE){
//     printf("sbrk deallocation produced wrong address, a %x c %x\n", a, c);
//     exit();
//   }

//   // can one re-allocate that page?
//   a = sbrk(0);
//   c = sbrk(PGSIZE);
//   if(c != a || sbrk(0) != a + PGSIZE){
//     printf("sbrk re-allocation failed, a %x c %x\n", a, c);
//     exit();
//   }
//   if(*lastaddr == 99){
//     // should be zero
//     printf("sbrk de-allocation didn't really deallocate\n");
//     exit();
//   }

//   a = sbrk(0);
//   c = sbrk(-(sbrk(0) - oldbrk));
//   if(c != a){
//     printf("sbrk downsize failed, a %x c %x\n", a, c);
//     exit();
//   }

//   // can we read the kernel's memory?
//   for(a = (char*)(KERNBASE); a < (char*) (KERNBASE+2000000); a += 50000){
//     ppid = getpid();
//     pid = fork();
//     if(pid < 0){
//       printf("fork failed\n");
//       exit();
//     }
//     if(pid == 0){
//       printf("oops could read %x = %x\n", a, *a);
//       kill(ppid);
//       exit();
//     }
//     wait();
//   }
    
//   // if we run the system out of memory, does it clean up the last
//   // failed allocation?
//   if(pipe(fds) != 0){
//     printf("pipe() failed\n");
//     exit();
//   }
//   for(i = 0; i < sizeof(pids)/sizeof(pids[0]); i++){
//     if((pids[i] = fork()) == 0){
//       // allocate a lot of memory
//       sbrk(BIG - (uint64)sbrk(0));
//       write(fds[1], "x", 1);
//       // sit around until killed
//       for(;;) sleep(1000);
//     }
//     if(pids[i] != -1)
//       read(fds[0], &scratch, 1);
//   }

//   // if those failed allocations freed up the pages they did allocate,
//   // we'll be able to allocate here
//   c = sbrk(PGSIZE);
//   for(i = 0; i < sizeof(pids)/sizeof(pids[0]); i++){
//     if(pids[i] == -1)
//       continue;
//     kill(pids[i]);
//     wait();
//   }
//   if(c == (char*)0xffffffffffffffffL){
//     printf("failed sbrk leaked memory\n");
//     exit();
//   }

//   // test running fork with the above allocated page 
//   ppid = getpid();
//   pid = fork();
//   if(pid < 0){
//     printf("fork failed\n");
//     exit();
//   }

//   // test out of memory during sbrk
//   if(pid == 0){
//     // allocate a lot of memory
//     a = sbrk(0);
//     sbrk(10*BIG);
//     int n = 0;
//     for (i = 0; i < 10*BIG; i += PGSIZE) {
//       n += *(a+i);
//     }
//     printf("allocate a lot of memory succeeded %d\n", n);
//     kill(ppid);
//     exit();
//   }
//   wait();

//   // test reads from allocated memory
//   a = sbrk(PGSIZE);
//   fd = open("sbrk", O_CREATE|O_WRONLY);
//   unlink("sbrk");
//   if(fd < 0)  {
//     printf("open sbrk failed\n");
//     exit();
//   }
//   if ((n = write(fd, a, 10)) < 0) {
//     printf("write sbrk failed\n");
//     exit();
//   }
//   close(fd);

//   // test writes to allocated memory
//   a = sbrk(PGSIZE);
//   if(pipe((int *) a) != 0){
//     printf("pipe() failed\n");
//     exit();
//   } 

//   if(sbrk(0) > oldbrk)
//     sbrk(-(sbrk(0) - oldbrk));

//   printf("sbrk test OK\n");
// }

// void
// validatetest(void)
// {
//   int hi;
//   uint64 p;

//   printf("validate test\n");
//   hi = 1100*1024;

//   for(p = 0; p <= (uint)hi; p += PGSIZE){
//     // try to crash the kernel by passing in a bad string pointer
//     if(link("nosuchfile", (char*)p) != -1){
//       printf("link should not succeed\n");
//       exit();
//     }
//   }

//   printf("validate ok\n");
// }

// // does unintialized data start out zero?
// char uninit[10000];
// void
// bsstest(void)
// {
//   int i;

//   printf("bss test\n");
//   for(i = 0; i < sizeof(uninit); i++){
//     if(uninit[i] != '\0'){
//       printf("bss test failed\n");
//       exit();
//     }
//   }
//   printf("bss test ok\n");
// }

// // does exec return an error if the arguments
// // are larger than a page? or does it write
// // below the stack and wreck the instructions/data?
// void
// bigargtest(void)
// {
//   int pid, fd;

//   unlink("bigarg-ok");
//   pid = fork();
//   if(pid == 0){
//     static char *args[MAXARG];
//     int i;
//     for(i = 0; i < MAXARG-1; i++)
//       args[i] = "bigargs test: failed\n                                                                                                                                                                                                       ";
//     args[MAXARG-1] = 0;
//     printf("bigarg test\n");
//     exec("echo", args);
//     printf("bigarg test ok\n");
//     fd = open("bigarg-ok", O_CREATE);
//     close(fd);
//     exit();
//   } else if(pid < 0){
//     printf("bigargtest: fork failed\n");
//     exit();
//   }
//   wait();
//   fd = open("bigarg-ok", 0);
//   if(fd < 0){
//     printf("bigarg test failed!\n");
//     exit();
//   }
//   close(fd);
//   unlink("bigarg-ok");
// }

// // what happens when the file system runs out of blocks?
// // answer: balloc panics, so this test is not useful.
// void
// fsfull()
// {
//   int nfiles;
//   int fsblocks = 0;

//   printf("fsfull test\n");

//   for(nfiles = 0; ; nfiles++){
//     char name[64];
//     name[0] = 'f';
//     name[1] = '0' + nfiles / 1000;
//     name[2] = '0' + (nfiles % 1000) / 100;
//     name[3] = '0' + (nfiles % 100) / 10;
//     name[4] = '0' + (nfiles % 10);
//     name[5] = '\0';
//     printf("writing %s\n", name);
//     int fd = open(name, O_CREATE|O_RDWR);
//     if(fd < 0){
//       printf("open %s failed\n", name);
//       break;
//     }
//     int total = 0;
//     while(1){
//       int cc = write(fd, buf, BSIZE);
//       if(cc < BSIZE)
//         break;
//       total += cc;
//       fsblocks++;
//     }
//     printf("wrote %d bytes\n", total);
//     close(fd);
//     if(total == 0)
//       break;
//   }

//   while(nfiles >= 0){
//     char name[64];
//     name[0] = 'f';
//     name[1] = '0' + nfiles / 1000;
//     name[2] = '0' + (nfiles % 1000) / 100;
//     name[3] = '0' + (nfiles % 100) / 10;
//     name[4] = '0' + (nfiles % 10);
//     name[5] = '\0';
//     unlink(name);
//     nfiles--;
//   }

//   printf("fsfull test finished\n");
// }

// void argptest()
// {
//   int fd;
//   fd = open("init", O_RDONLY);
//   if (fd < 0) {
//     fprintf(2, "open failed\n");
//     exit();
//   }
//   read(fd, sbrk(0) - 1, -1);
//   close(fd);
//   printf("arg test passed\n");
// }

// unsigned long randstate = 1;
// unsigned int
// rand()
// {
//   randstate = randstate * 1664525 + 1013904223;
//   return randstate;
// }

// // check that there's an invalid page beneath
// // the user stack, to catch stack overflow.
// void
// stacktest()
// {
//   int pid;
//   int ppid = getpid();
  
//   printf("stack guard test\n");
//   pid = fork();
//   if(pid == 0) {
//     char *sp = (char *) r_sp();
//     sp -= PGSIZE;
//     // the *sp should cause a trap.
//     printf("stacktest: read below stack %p\n", *sp);
//     printf("stacktest: test FAILED\n");
//     kill(ppid);
//     exit();
//   } else if(pid < 0){
//     printf("fork failed\n");
//     exit();
//   }
//   wait();
//   printf("stack guard test ok\n");
// }

// int
// main(int argc, char *argv[])
// {
//   printf("usertests starting\n");

//   if(open("usertests.ran", 0) >= 0){
//     printf("already ran user tests -- rebuild fs.img\n");
//     exit();
//   }
//   close(open("usertests.ran", O_CREATE));

//   reparent();
//   twochildren();
//   forkfork();
//   forkforkfork();
  
//   argptest();
//   createdelete();
//   linkunlink();
//   concreate();
//   fourfiles();
//   sharedfd();

//   bigargtest();
//   bigwrite();
//   bigargtest();
//   bsstest();
//   sbrktest();
//   validatetest();
//   stacktest();
  
//   opentest();
//   writetest();
//   writetest1();
//   createtest();

//   openiputtest();
//   exitiputtest();
//   iputtest();

//   mem();
//   pipe1();
//   preempt();
//   exitwait();

//   rmdot();
//   fourteen();
//   bigfile();
//   subdir();
//   linktest();
//   unlinkread();
//   dirfile();
//   iref();
//   forktest();
//   bigdir(); // slow

//   exectest();

//   exit();
// }
