// 这个文件描述的是内存中的文件描述结构，inode结构
// 内存中的文件结构体
struct file {
  enum { FD_NONE, FD_PIPE, FD_INODE, FD_DEVICE } type; // 文件类型
  int ref; // reference count 文件的引用数量
  char readable; 
  char writable; 
  struct pipe *pipe; // FD_PIPE
  struct inode *ip;  // FD_INODE and FD_DEVICE
  uint off;          // FD_INODE
  short major;       // FD_DEVICE
};  // 内核维护的文件描述符结构，记录文件类型、引用技术、读写权限、偏移量等

#define major(dev)  ((dev) >> 16 & 0xFFFF)
#define minor(dev)  ((dev) & 0xFFFF)
#define	mkdev(m,n)  ((uint)((m)<<16| (n)))



// in-memory copy of an inode
// 内存中的inode
struct inode {
  uint dev;           // Device number 设备号
  uint inum;          // Inode number inode编号
  int ref;            // Reference count 引用计数
  struct sleeplock lock; // protects everything below here 保护下面字段的锁
  int valid;          // inode has been read from disk? 是否从磁盘读入

  short type;         // copy of disk inode 文件类型
  short major;        // 主设备号
  short minor;        // 次设备号
  short nlink;        // 链接数
  uint size;          // 文件大小
  uint addrs[NDIRECT+2]; // 数据块地址
}; // 内存中的inode结构



// map major device number to device functions.
struct devsw {
  int (*read)(int, uint64, int);
  int (*write)(int, uint64, int);
}; // 设备驱动表

extern struct devsw devsw[];

#define CONSOLE 1
