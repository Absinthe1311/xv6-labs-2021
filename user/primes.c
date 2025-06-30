#include"kernel/types.h"
#include"kernel/stat.h"
#include"user/user.h"

void new_proc(int p[2])
{
    close(p[1]); //关闭写端
    int prime;
    if(read(p[0], &prime, 4) != 4)
    {
        fprintf(2, "read error\n");
        exit(1);
    }
    fprintf(0, "prime %d\n", prime); //打印素数
    int num;
    if(read(p[0], &num, 4)==4)//读取下一个数
    {
        int new_p[2];
        pipe(new_p); //创建新的管道
        if(fork() != 0)
        {
            close(new_p[0]);
            if(num % prime != 0) //如果不能被现在的prime整除，传到下一子进程
            {
                write(new_p[1], &num, 4);
            }
            while(read(p[0], &num, 4) == 4) //继续读取管道中的数
            {
                if(num % prime != 0) 
                {
                    write(new_p[1], &num, 4);
                }
            }
            close(p[0]); 
            close(new_p[1]);
            wait(0);
            exit(0);
        }
        else
        {
            new_proc(new_p);
            exit(0);
        }
    }
}

int
main(int argc, char * argv[])
{
    int p[2];
    pipe(p); //创建管道
    if(fork() == 0) //子进程
    {
        new_proc(p);
    }
    else //父进程
    {
        close(p[0]); //关闭读端
        for(int i = 2; i<=35;i++) //将2-35全写入管道
        {
            write(p[1], &i, 4);
        }
        close(p[1]); //关闭写端
        wait(0); //等待子进程结束
        exit(0);
    }
    return 0;
}