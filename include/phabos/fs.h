#ifndef __FS_H__
#define __FS_H__

#include <sys/stat.h>
#include <sys/types.h>
#include <phabos/assert.h>
#include <phabos/mutex.h>

#define DT_UNKNOWN  0
#define DT_BLK      1
#define DT_CHR      2
#define DT_DIR      3
#define DT_FIFO     4
#define DT_LNK      5
#define DT_REG      6
#define DT_SOCK     7

#define PROT_EXEC   (1 << 0)
#define PROT_READ   (1 << 1)

#define MAP_SHARED  (1 << 0)

struct inode;
struct file;
struct phabos_dirent;

struct file_operations {
    int (*ioctl)(struct file *file, unsigned long cmd, ...);
    int (*getdents)(struct file *file, struct phabos_dirent *dirp,
                    size_t count);
    ssize_t (*write)(struct file *file, const void *buf, size_t count);
    ssize_t (*read)(struct file *file, void *buf, size_t count);
    void *(*mmap)(struct file *file, void *addr, size_t length, int prot,
                  int flags, off_t offset);
};

struct inode_operations {
    int (*mount)(struct inode *cwd);
    int (*mkdir)(struct inode *cwd, const char *name, mode_t mode);
    int (*mknod)(struct inode *cwd, const char *name, mode_t, dev_t dev);

    struct inode *(*lookup)(struct inode *cwd, const char *name);
};

struct fs {
    const char *name;

    struct file_operations file_ops;
    struct inode_operations inode_ops;
};

struct inode {
    struct fs *fs;
    void *inode;
    unsigned long flags;
    dev_t dev;

    atomic_t refcount;
    struct inode *mounted_inode;
    struct mutex dlock;
};

struct file {
    struct inode *inode;
    unsigned long flags;
    off_t offset;
};

struct fd {
    unsigned long flags;
    struct file *file;
};

struct phabos_dirent {
    unsigned long d_ino;
    unsigned long d_off;
    unsigned short d_reclen;
    char d_name[];
};

void fs_init(void);
int fs_register(struct fs *fs);

struct inode *inode_alloc(void);
int inode_init(struct inode *inode);
void inode_ref(struct inode *inode);
void inode_unref(struct inode *inode);

int open(const char *pathname, int flags, ...);
int close(int fd);
int mount(const char *source, const char *target, const char *filesystemtype,
          unsigned long mountflags, const void *data);
int mkdir(const char *pathname, mode_t mode);
int getdents(int fd, struct phabos_dirent *dirp, size_t count);
int mknod(const char *pathname, mode_t mode, dev_t dev);

struct fd *to_fd(int fdnum);
int allocate_fdnum(void);
int free_fdnum(int fdnum);

static inline bool is_directory(struct inode *inode)
{
    RET_IF_FAIL(inode, false);
    return inode->flags & S_IFDIR;
}

static inline bool is_regular_file(struct inode *inode)
{
    RET_IF_FAIL(inode, false);
    return inode->flags & S_IFREG;
}

static inline bool is_char_device(struct inode *inode)
{
    RET_IF_FAIL(inode, false);
    return inode->flags & S_IFCHR;
}

int sys_open(const char *pathname, int flags, mode_t mode);
int sys_close(int fd);
ssize_t sys_read(int fdnum, void *buf, size_t count);
off_t sys_lseek(int fdnum, off_t offset, int whence);
void *sys_mmap(void *addr, size_t length, int prot, int flags, int fd,
               off_t offset);

#endif /* __FS_H__ */

