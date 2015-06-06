#include <config.h>
#include <phabos/assert.h>
#include <phabos/utils.h>
#include <phabos/kprintf.h>
#include <phabos/fs.h>

#include <stdint.h>
#include <errno.h>
#include <stdio.h>

#define ARCH_ELF_MACHINE    EM_ARM // FIXME
#define ARCH_ELF_CLASS      ELFCLASS32

#define EI_MAG0     0
#define EI_MAG1     1
#define EI_MAG2     2
#define EI_MAG3     3
#define EI_CLASS    4
#define EI_DATA     5
#define EI_VERSION  6
#define EI_PAD      7
#define EI_NIDENT   16

#define ELFMAG0 0x7f
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'

#define ELFCLASSNONE    0
#define ELFCLASS32      1
#define ELFCLASS64      2

#define ELFDATANONE 0
#define ELFDATA2LSB 1
#define ELFDATA2MSB 2

#define ET_NONE   0
#define ET_REL    1
#define ET_EXEC   2
#define ET_DYN    3
#define ET_CORE   4
#define ET_LOPROC 0xff00
#define ET_HIPROC 0xffff

#define EM_ARM 0x28

#define EV_NONE    0
#define EV_CURRENT 1

#define SHT_NULL        0
#define SHT_PROGBITS    1
#define SHT_SYMTAB      2
#define SHT_STRTAB      3
#define SHT_RELA        4
#define SHT_HASH        5
#define SHT_DYNAMIC     6
#define SHT_NOTE        7
#define SHT_NOBITS      8
#define SHT_REL         9
#define SHT_SHLIB       10
#define SHT_DYNSYM      11
#define SHT_LOPROC      0x70000000
#define SHT_HIPROC      0x7fffffff
#define SHT_LOUSER      0x80000000
#define SHT_HIUSER      0x8fffffff

#define SHF_WRITE       0x1
#define SHF_ALLOC       0x2
#define SHF_EXECINSTR   0x4
#define SHF_MASKPROC    0xf0000000

#define PT_NULL     0
#define PT_LOAD     1
#define PT_DYNAMIC  2
#define PT_INTERP   3
#define PT_NOTE     4
#define PT_SHLIB    5
#define PT_PHDR     6
#define PT_LOPROC   0x70000000
#define PT_HIPROC   0x7fffffff

#define PF_X        0x1
#define PF_W        0x2
#define PF_R        0x4
#define PF_MASKPROC 0xf0000000

#define elf_printf(x...) printf("elf-loader: " x);

typedef uint32_t Elf32_Addr;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Off;
typedef int32_t Elf32_Sword;
typedef uint32_t Elf32_Word;

typedef struct {
    unsigned char   e_ident[EI_NIDENT];
    Elf32_Half      e_type;
    Elf32_Half      e_machine;
    Elf32_Word      e_version;
    Elf32_Addr      e_entry;
    Elf32_Off       e_phoff;
    Elf32_Off       e_shoff;
    Elf32_Word      e_flags;
    Elf32_Half      e_ehsize;
    Elf32_Half      e_phentsize;
    Elf32_Half      e_phnum;
    Elf32_Half      e_shentsize;
    Elf32_Half      e_shnum;
    Elf32_Half      e_shtrndx;
} Elf32_Ehdr;

typedef struct {
    Elf32_Word  sh_name;
    Elf32_Word  sh_type;
    Elf32_Word  sh_flags;
    Elf32_Addr  sh_addr;
    Elf32_Off   sh_offset;
    Elf32_Word  sh_size;
    Elf32_Word  sh_link;
    Elf32_Word  sh_info;
    Elf32_Word  sh_addralign;
    Elf32_Word  sh_entsize;
} Elf32_Shdr;

typedef struct {
    Elf32_Word  p_type;
    Elf32_Off   p_offset;
    Elf32_Addr  p_vaddr;
    Elf32_Addr  p_paddr;
    Elf32_Word  p_filesz;
    Elf32_Word  p_memsz;
    Elf32_Word  p_flags;
    Elf32_Word  p_align;
} Elf32_Phdr;

typedef struct {
    Elf32_Sword d_tag;
    union {
        Elf32_Word  d_val;
        Elf32_Addr  d_ptr;
    } d_un;
} Elf32_Dyn;

static int verify_file(const Elf32_Ehdr *hdr)
{
    RET_IF_FAIL(hdr, -EINVAL);

    if (hdr->e_ident[EI_MAG0] != ELFMAG0 ||
        hdr->e_ident[EI_MAG1] != ELFMAG1 ||
        hdr->e_ident[EI_MAG2] != ELFMAG2 ||
        hdr->e_ident[EI_MAG3] != ELFMAG3) {
        fprintf(stderr, "Not an ELF file\n");
        return -EINVAL;
    }

    if (hdr->e_ident[EI_CLASS] != ARCH_ELF_CLASS) {
        fprintf(stderr, "Binary class not supported.\n");
        return -EINVAL;
    }

#if defined(CONFIG_LITTLE_ENDIAN)
    if (hdr->e_ident[EI_DATA] != ELFDATA2LSB) {
        fprintf(stderr, "Little endian binaries not supported.\n");
        return -EINVAL;
    }
#elif defined(CONFIG_BIG_ENDIAN)
    if (hdr->e_ident[EI_DATA] != ELFDATA2MSB) {
        fprintf(stderr, "Big endian binaries not supported.\n");
        return -EINVAL;
    }
#else
    #error Byte ordering information not defined
#endif

    if (hdr->e_ident[EI_VERSION] != hdr->e_version) {
        fprintf(stderr, "Inconsistant ELF version. Aborting...\n");
    }

    if (hdr->e_type != ET_DYN) {
        fprintf(stderr, "Not an shared object\n");
        return -EINVAL;
    }

    if (hdr->e_machine != ARCH_ELF_MACHINE) {
        fprintf(stderr, "ELF Machine code not matching current architecture\n");
        return -EINVAL;
    }

    if (hdr->e_version != EV_CURRENT) {
        fprintf(stderr, "Unsupported ELF Header version\n");
        return -EINVAL;
    }

    return 0;
}

static int elf_load_segment_old(void *addr, Elf32_Phdr *phdr)
{
    char *entry_point_offset;

    RET_IF_FAIL(addr, -EINVAL);
    RET_IF_FAIL(phdr, -EINVAL);
    RET_IF_FAIL(phdr->p_type == PT_LOAD, -EINVAL);

    elf_printf("Segment:\n");
    elf_printf("\ttype   %x\n", phdr->p_type);
    elf_printf("\toffset %x\n", phdr->p_offset);
    elf_printf("\tvaddr  %x\n", phdr->p_vaddr);
    elf_printf("\tpaddr  %x\n", phdr->p_paddr);
    elf_printf("\tfilesz %x\n", phdr->p_filesz);
    elf_printf("\tmemsz  %x\n", phdr->p_memsz);
    elf_printf("\tflags: %x\n", phdr->p_flags);
    elf_printf("\talign: %x\n", phdr->p_align);

    char *segment = zalloc(phdr->p_memsz);
    memcpy(segment, addr + phdr->p_offset, phdr->p_filesz);

    entry_point_offset = segment;
    elf_printf("loaded segment at address: %p\n", segment);

    return 0;
}

static int elf_load_dynamic(void *addr, Elf32_Phdr *phdr);

static void elf_load_needed(void)
{
    extern unsigned char apps_libphabos_libphabos_elf[];

    void *addr = &apps_libphabos_libphabos_elf;

    int retval;
    Elf32_Ehdr *hdr = (Elf32_Ehdr*) addr;

    elf_printf("%s()\n", __func__);

    RET_IF_FAIL(hdr,);

    retval = verify_file(hdr);
    if (retval)
        return;

    RET_IF_FAIL(hdr->e_phnum,);

    for (int i = 0; i < hdr->e_phnum; i++) {
        Elf32_Phdr *phdr = (Elf32_Phdr*) (addr + hdr->e_phoff +
                                          i * hdr->e_phentsize);

        switch (phdr->p_type) {
        case PT_LOAD:
            elf_load_segment_old(addr, phdr);
            break;

        case PT_DYNAMIC:
            elf_load_dynamic(addr, phdr);
            break;

        default:
            elf_printf("ignoring segment of type: %x\n", phdr->p_type);
            break;
        }
    }

    elf_printf("%s() exit\n", __func__);
}

#define DT_NULL     0
#define DT_NEEDED   1
#define DT_PLTRELSZ 2
#define DT_PLTGOT   3
#define DT_HASH     4
#define DT_STRTAB   5
#define DT_SYMTAB   6
#define DT_RELA     7
#define DT_RELASZ   8
#define DT_RELAENT  9
#define DT_STRSZ    10
#define DT_SYMENT   11
#define DT_INIT     12
#define DT_FINI     13
#define DT_SONAME   14
#define DT_RPATH    15
#define DT_SYMBOLIC 16
#define DT_REL      17
#define DT_RELSZ    18
#define DT_RELENT   19
#define DT_PLTREL   20
#define DT_DEBUG    21
#define DT_TEXTREL  22
#define DT_JMPREL   23
#define DT_BIND_NOW 24
#define DT_LOPROC   0x70000000
#define DT_HIPROC   0x7fffffff

static int elf_load_dynamic(void *addr, Elf32_Phdr *phdr)
{
    const char *strtab = NULL;

    RET_IF_FAIL(addr, -EINVAL);
    RET_IF_FAIL(phdr, -EINVAL);
    RET_IF_FAIL(phdr->p_type == PT_DYNAMIC, -EINVAL);

    elf_printf("Segment:\n");
    elf_printf("\ttype   %x\n", phdr->p_type);
    elf_printf("\toffset %x\n", phdr->p_offset);
    elf_printf("\tvaddr  %x\n", phdr->p_vaddr);
    elf_printf("\tpaddr  %x\n", phdr->p_paddr);
    elf_printf("\tfilesz %x\n", phdr->p_filesz);
    elf_printf("\tmemsz  %x\n", phdr->p_memsz);
    elf_printf("\tflags: %x\n", phdr->p_flags);
    elf_printf("\talign: %x\n", phdr->p_align);

    Elf32_Dyn *dynamic = (Elf32_Dyn*) ((char*) addr + phdr->p_offset);
    size_t dynamic_count = phdr->p_filesz / sizeof(*dynamic);

    elf_printf("Dynamic:\n");
    for (int i = 0; i < dynamic_count; i++) {
        elf_printf("\td_tag: %d, d_un: %x\n",
                   dynamic[i].d_tag, dynamic[i].d_un.d_val);

        switch (dynamic[i].d_tag) {
        case DT_STRTAB:
            strtab = (const char*) addr + dynamic[i].d_un.d_ptr;
            break;
        }
    }

    for (int i = 0; i < dynamic_count; i++) {
        if (dynamic[i].d_tag != DT_NEEDED) {
            continue;
        }

        elf_printf("DT_NEEDED: %s\n", strtab + dynamic[i].d_un.d_val);
        elf_load_needed();
    }

    return 0;
}

void ld(void *addr)
{
    int retval;
    Elf32_Ehdr *hdr = (Elf32_Ehdr*) addr;

    elf_printf("%s()\n", __func__);

    RET_IF_FAIL(hdr,);

    retval = verify_file(hdr);
    if (retval)
        return;

    RET_IF_FAIL(hdr->e_phnum,);

    for (int i = 0; i < hdr->e_phnum; i++) {
        Elf32_Phdr *phdr = (Elf32_Phdr*) (addr + hdr->e_phoff +
                                          i * hdr->e_phentsize);

        switch (phdr->p_type) {
        case PT_LOAD:
            elf_load_segment_old(addr, phdr);
            break;

        case PT_DYNAMIC:
            elf_load_dynamic(addr, phdr);
            break;

        default:
            elf_printf("ignoring segment of type: %x\n", phdr->p_type);
            break;
        }
    }

#if 0
    int (*func)(int argc, char **argv);
    func = (int (*)(int, char**)) entry_point_offset + hdr->e_entry;

    elf_printf("entry point: %x or %p after relocation\n", hdr->e_entry, func);

    func(2, NULL);
#endif
}

int exec(char *addr)
{
    int retval;
    Elf32_Ehdr *hdr = (Elf32_Ehdr*) addr;
    char *entry_point_offset = NULL;

    RET_IF_FAIL(hdr, -EINVAL);

    retval = verify_file(hdr);
    if (retval)
        return retval;

    RET_IF_FAIL(hdr->e_phnum, -EINVAL);

    for (int i = 0; i < hdr->e_phnum; i++) {
        Elf32_Phdr *phdr = (Elf32_Phdr*) (addr + hdr->e_phoff +
                                          i * hdr->e_phentsize);

        if (phdr->p_type == PT_INTERP) {
            printf("executing interpreter: %s\n", addr + phdr->p_offset);
            ld(addr);
            return 0;
        }

        if (phdr->p_type != PT_LOAD) {
            elf_printf("ignoring segment of type: %u\n", phdr->p_type);
            continue;
        }

        elf_printf("Segment:\n");
        elf_printf("\ttype   %x\n", phdr->p_type);
        elf_printf("\toffset %x\n", phdr->p_offset);
        elf_printf("\tvaddr  %x\n", phdr->p_vaddr);
        elf_printf("\tpaddr  %x\n", phdr->p_paddr);
        elf_printf("\tfilesz %x\n", phdr->p_filesz);
        elf_printf("\tmemsz  %x\n", phdr->p_memsz);
        elf_printf("\tflags: %x\n", phdr->p_flags);
        elf_printf("\talign: %x\n", phdr->p_align);

        char *segment = zalloc(phdr->p_memsz);
        memcpy(segment, addr + phdr->p_offset, phdr->p_filesz);

        entry_point_offset = segment;
        elf_printf("loaded segment at address: %p\n", segment);
        break;
    }

    int (*func)(int argc, char **argv);
    func = (int (*)(int, char**)) entry_point_offset + hdr->e_entry;

    elf_printf("entry point: %x or %p after relocation\n", hdr->e_entry, func);

    func(2, NULL);

    return 0;
}

static int elf_read(int fd, void *buffer, size_t size)
{
    ssize_t nread = 0;
    ssize_t retval;

    do {
        retval = sys_read(fd, (char*) buffer + nread, size - nread);
        if (retval == 0)
            retval = -EINVAL;

        if (retval < 0)
            return retval;

        nread += retval;
    } while (nread != size);

    return 0;
}

static int elf_load_segment(Elf32_Phdr *phdr, int fd)
{
    int retval;
    char *segment;
    off_t pos;

    RET_IF_FAIL(phdr, -EINVAL);
    RET_IF_FAIL(fd >= 0, -EINVAL);

    segment = zalloc(phdr->p_memsz);
    if (!segment)
        return -ENOMEM;

    pos = sys_lseek(fd, 0, SEEK_CUR);
    if (pos < 0) {
        retval = (int) pos;
        goto error_lseek;
    }

    retval = sys_lseek(fd, phdr->p_offset, SEEK_SET);
    if (retval < 0)
        goto error_segment_lseek;

    retval = elf_read(fd, segment, phdr->p_filesz);
    if (retval)
        goto error_segment_lseek;

    // TODO attach segment to process

    return 0;

error_segment_lseek:
    sys_lseek(fd, pos, SEEK_SET);
error_lseek:
    kfree(segment);
    return retval;
}

static int elf_load_segments(Elf32_Ehdr *hdr, int fd)
{
    int retval;
    Elf32_Phdr *phdr;

    RET_IF_FAIL(hdr, -EINVAL);
    RET_IF_FAIL(fd >= 0, -EINVAL);

    if (!hdr->e_phnum)
        return -EINVAL;

    phdr = kmalloc(sizeof(*phdr), 0);
    if (!phdr)
        return -ENOMEM;

    retval = sys_lseek(fd, hdr->e_phoff, SEEK_SET);
    if (retval < 0)
        goto exit;

    for (int i = 0; i < hdr->e_phnum; i++) {
        retval = elf_read(fd, phdr, hdr->e_phentsize);
        if (retval)
            goto exit;

        if (phdr->p_type == PT_INTERP) {
            kprintf("requires an interpreter\n");
            //retval = elf_exec(); // FIXME
            goto exit;
        }
    }

    retval = sys_lseek(fd, hdr->e_phoff, SEEK_SET);
    if (retval < 0)
        goto exit;

    for (int i = 0; i < hdr->e_phnum; i++) {
        retval = elf_read(fd, phdr, hdr->e_phentsize);
        if (retval)
            goto exit; // TODO free every segment allocated so far

        if (phdr->p_type != PT_LOAD)
            continue;

        retval = elf_load_segment(phdr, fd);
        if (retval)
            goto exit; // TODO free every allocated so far
    }

exit:
    kfree(phdr);
    return retval;
}

int elf_exec(int fd)
{
    int retval;
    Elf32_Ehdr *hdr;

    RET_IF_FAIL(fd >= 0, -EINVAL);

    hdr = kmalloc(sizeof(*hdr), 0);
    if (!hdr)
        return -ENOMEM;

    retval = elf_read(fd, hdr, sizeof(*hdr));
    if (retval)
        goto exit;

    retval = verify_file(hdr);
    if (retval)
        goto exit;

    retval = elf_load_segments(hdr, fd);
    if (retval)
        goto exit;

exit:
    kfree(hdr);
    return retval;
}
