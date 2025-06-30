#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

// 这个函数用于获取格式化后的文件名
// 比如 a/b/file.txt 会返回"file.txt" 并且填充空格到 DIRSIZ 的长度
char*
fmtname(char *path)
{
    char *p;

    // Find first character after last slash.
    for(p=path+strlen(path); p >= path && *p != '/'; p--)
        ;
    p++;
    return p;
}

// 相等则打印出来
void 
equal_print(char * path, char* findname)
{
    char *name = fmtname(path); // 获取格式化后的文件名
    // printf("the name is: ");
    // printf("%s\n",name);
    // printf("the find name is: %s\n", findname);
    if(strcmp(name, findname) == 0){
        // printf("in the print function\n");
        printf("%s\n",path); //如果相等则将路径打印出来
    }
}

void
find(char *path, char *findname)
{
    char buf[512], *p;
    int fd; // 文件描述符
    struct dirent de; // 目录项结构体
    struct stat st; // 文件状态结构体

    if((fd = open(path, 0)) < 0){ // 通过路径打开这个文件，获取文件描述符
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    if(fstat(fd, &st) < 0){ //通过文件描述符和fstat函数读取这个文件的状态
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    switch(st.type){ //根据文件的类型来选择
    case T_FILE: //如果是普通文件
        equal_print(path, findname); //调用equal_print函数判断是否相等
        break;
    case T_DIR: //如果得到的是目录
        if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){ // 检查路径的长度是否超过buff大小
            printf("find: path too long\n");
            break;
        }
        strcpy(buf, path); //将路径复制到path中
        p = buf + strlen(buf); //p指向末尾
        *p++ = '/'; //再在末尾添加一个/

        while(read(fd, &de, sizeof(de)) == sizeof(de)){ //读取目录项
            if(de.inum == 0||(strcmp(de.name,".")==0) || (strcmp(de.name,"..")==0)) //如果目录项的inode号为0，说明这个目录项是空的
                continue;
            memmove(p, de.name, DIRSIZ); //将目录项的名字移动到buf中
            p[DIRSIZ] = 0; //在buf的末尾添加一个
            find(buf, findname); //递归调用find函数
        }
        break;
    }
    close(fd);
}

int
main(int argc, char *argv[])
{
    if(argc != 3){
        fprintf(2, "Usage: find <path> <name>\n");
        exit(1);
    }
    find(argv[1], argv[2]); //调用find函数
    exit(0); 
}