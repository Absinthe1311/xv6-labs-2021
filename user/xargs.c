#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"
#include "kernel/fs.h"

int
main(int argc, char* argv[])
{
    int p[2]; // Pipe
    pipe(p);
    char *params[MAXARG];
    int i = 0;
    int j = 1;
    char s;

    if(fork()==0) //child
    {
        close(p[1]); //close the write end of the pipe
        params[0] = malloc(sizeof(char)*MAXARG);
        params[1] = malloc(sizeof(char)*MAXARG);
        // read the arguments from the pipe
        while(read(p[0], &s, 1) != 0)
        {
            if(s=='\n')
            {
                params[j][i] = '\0';
                j++;
                params[j] = malloc(sizeof(char)*MAXARG);
                i = 0;
            }
            else
            {
                params[j][i++] = s;
            }
        }
        exec(argv[1], params); // execute the command
        for(i = 0; i<=j;i++)
        {
            free(params[i]);
        }
        close(p[0]);
        exit(0);
    }
    else
    {
        close(p[0]); //close the read end of the pipe
        // send the arguments after the xargs to the child
        for(int k = 2; k< argc; k++)
        {
            write(p[1], argv[k], strlen(argv[k]));
            write(p[1],"\n",1);
        }
        // get the arguments from the stdin
        while(read(0, &s, 1) !=0) // read from the stdin
        {
            if(s == ' ')
            {
                write(p[1], "\n", 1);
            }
            else
            {
                write(p[1], &s, 1);
            }
        }
        write(p[1], "\n", 1); // write a newline to indicate the end of input
        close(p[1]);
        wait(0);
        exit(0);
    }
    exit(0);
}

