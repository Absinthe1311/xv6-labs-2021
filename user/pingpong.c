#include"kernel/types.h"
#include"kernel/stat.h"
#include"user/user.h"

// pipe用法解释
// int pipefd[2];
// pipe(pipefd);
// pipefd[0]是读端，pipfd[1]是写端
// write(pipefd[1], "hello", 5); // 向管道写
// char buf[10];
// read(pipefd[0], buf, 5); // 从管道读

int 
main(int argc, char* argv[])
{
    int pipefd[2];
    if(pipe(pipefd) < 0){
        fprintf(2, "pipe error\n");
        exit(1);
    }
    if(fork()==0) //子进程
    {
        char buffer[1];
        read(pipefd[0], buffer, 1); //从管道读取数据
        close(pipefd[0]); //关闭读端
        fprintf(0, "%d: received ping\n", getpid());
        write(pipefd[1], buffer, 1); //向管道再写入数据
    }
    else //父进程
    {
        char buffer[1];
        buffer[0] = 'a';
        write(pipefd[1], buffer, 1); //向管道写入数据
        close(pipefd[1]); //关闭写端
        read(pipefd[0], buffer,1); //再从管道读取数据
        fprintf(0, "%d: received pong\n", getpid());
        close(pipefd[0]); //关闭读端
    }
    exit(0);
}