#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int i;
  char *nargv[MAXARG];

  if(argc < 3 || (argv[1][0] < '0' || argv[1][0] > '9')){
    fprintf(2, "Usage: %s mask command\n", argv[0]);
    exit(1);
  }

  if (trace(atoi(argv[1])) < 0) { // 这里执行了 trace 的系统调用
    fprintf(2, "%s: trace failed\n", argv[0]);
    exit(1);
  }
  
  for(i = 2; i < argc && i < MAXARG; i++){
    nargv[i-2] = argv[i];
  }
  exec(nargv[0], nargv);
  exit(0);
}
/*
实现想法：
前面使用 trace(atoi(argv[1])) 执行系统调用，在我的系统调用里面应该是将这个
argv[1] 赋值给 proc 里面的 trace_mask 字段。
修改fork()函数，确保创建子进程的时候会复制这个trace_mask字段
再在kernel/syscall.c的syscall函数中，根据trace_mask字段来打印出 进程-系统调用名称
*/