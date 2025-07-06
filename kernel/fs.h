// On-disk file system format.
// Both the kernel and user programs use this header file.
// 这个文件描述的磁盘文件系统的核心结构体和常量等等

#define ROOTINO  1   // root i-number  根目录的inode编号
#define BSIZE 1024  // block size  每个磁盘块大小为1024字节

// Disk layout:
// [ boot block | super block | log | inode blocks |
//                                          free bit map | data blocks]
//
// mkfs computes the super block and builds an initial file system. The
// super block describes the disk layout:
struct superblock {
  uint magic;        // Must be FSMAGIC
  uint size;         // Size of file system image (blocks)  文件系统总块数
  uint nblocks;      // Number of data blocks  数据块数量
  uint ninodes;      // Number of inodes.  inode数量
  uint nlog;         // Number of log blocks 日志块数量
  uint logstart;     // Block number of first log block 日志区起始块号
  uint inodestart;   // Block number of first inode block inode起始块号
  uint bmapstart;    // Block number of first free map block 空闲位图区起始块号
};

#define FSMAGIC 0x10203040 // 文件系统数，用于识别文件系统类型

//#define NDIRECT 12  // inode直接块数量 12->11
#define NDIRECT 11 // 直接块的数量
#define NINDIRECT (BSIZE / sizeof(uint)) // 一个间接块能存放的块号数量
#define MAXFILE (NDIRECT + NINDIRECT + NINDIRECT*NINDIRECT) // 单个文件最大可包含的数据块数

// On-disk inode structure 磁盘上的inode结构， 描述单个文件/目录的元数据
struct dinode {
  short type;           // File type 文件类型
  short major;          // Major device number (T_DEVICE only) 主设备号
  short minor;          // Minor device number (T_DEVICE only) 次设备号
  short nlink;          // Number of links to inode in file system 文件系统中指向该inode的链接数
  uint size;            // Size of file (bytes) 文件大小（字节）
  //uint addrs[NDIRECT+1];   // Data block addresses 数据块地址（直接块+ 1个间接块）
  uint addrs[NDIRECT+2]; // 数据块地址
};


// Inodes per block.
#define IPB           (BSIZE / sizeof(struct dinode)) 

// Block containing inode i
#define IBLOCK(i, sb)     ((i) / IPB + sb.inodestart) // inode i 所在的块号

// Bitmap bits per block
#define BPB           (BSIZE*8)

// Block of free map containing bit for block b
#define BBLOCK(b, sb) ((b)/BPB + sb.bmapstart)

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

struct dirent { //目录项的结构
  ushort inum;  // inode编号
  char name[DIRSIZ]; //文件名
};

