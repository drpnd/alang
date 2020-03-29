/*_
 * Copyright (c) 2020 Hirochika Asai <asai@jar.jp>
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "../arch.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

typedef uint32_t Elf32_Addr;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Off;
typedef int32_t Elf32_Sword;
typedef uint32_t Elf32_Word;
/* and unsigned char */

typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;
typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef int32_t Elf64_Sword;
typedef uint64_t Elf64_Xword;
typedef int64_t Elf64_Sxword;
/* and unsigned char */

typedef struct {
    unsigned char       e_ident[16]; /* Elf Identification */
    Elf64_Half          e_type;      /* Object file type */
    Elf64_Half          e_machine;   /* Machine type */
    Elf64_Word          e_version;   /* Object file version */
    Elf64_Addr          e_entry;     /* Entry point address */
    Elf64_Off           e_phoff;     /* Program header offset */
    Elf64_Off           e_shoff;     /* Section header offset */
    Elf64_Word          e_flags;     /* Processor-specific flags */
    Elf64_Half          e_ehsize;    /* ELF header size */
    Elf64_Half          e_phentsize; /* Size of program header entry */
    Elf64_Half          e_phnum;     /* Number of program header entries */
    Elf64_Half          e_shentsize; /* Size of section header entry */
    Elf64_Half          e_shnum;     /* Number of section header entries */
    Elf64_Half          e_shstrndx;  /* Section name string table index */
} __attribute__ ((packed)) Elf64_Ehdr;

enum {
    EI_MAG0 = 0,                /* File identification */
    EI_MAG1 = 1,
    EI_MAG2 = 2,
    EI_MAG3 = 3,
    EI_CLASS = 4,               /* File class */
    EI_DATA = 5,                /* Data encoding */
    EI_VERSION = 6,             /* File version */
    EI_OSABI = 7,               /* OS/ABI identification */
    EI_ABIVERSION = 8,          /* ABI version */
    EI_PAD = 9,                 /* Start of padding bytes */
    EI_NIDENT = 16,             /* Size of e_ident[] */
};

/* Object File Classes, e_ident[EI_CLASS] */
enum {
    ELFCLASS32 = 1,             /* 32-bit objects */
    ELFCLASS64 = 2,             /* 64-bit objects */
};

/* Data Encodings, e_ident[EI_DATA] */
enum {
    ELFDATA2LSB = 1,            /* Object file data structures are little-
                                   endian */
    ELFDATA2MSB = 2,            /* Object file data structures are big-endian */
};

#define EV_CURRENT      1

/* Operating System and ABI Identifiers, e_ident[EI_OSABI] */
enum {
    ELFOSABI_SYSV = 0,          /* System V ABI */
    ELFOSABI_HPUX = 1,          /* HP-UX operating system */
    ELFOSABI_NETBSD = 2,        /* NetBSD */
    ELFOSABI_LINUX = 3,         /* GNU/Linux */
    ELFOSABI_FREEBSD = 9,       /* FreeBSD */
    ELFOSABI_OPENBSD = 12,      /* OpenBSD */
    ELFOSABI_STANDALONE = 255,  /* Standalone (embedded) application */
};

/* e_machine */
enum {
    EM_X86 = 0x03,
    EM_ARM = 0x28,
    EM_X86_64 = 0x3e,
    EM_AARCH64 = 0xb7,
};

/* Object File Types, e_type */
enum {
    ET_NONE = 0,                /* No file type */
    ET_REL = 1,                 /* Relocatable object file */
    ET_EXEC = 2,                /* Executable file */
    ET_DYN = 3,                 /* Shared object file */
    ET_CORE = 4,                /* Core file */
    ET_LOOS = 0xfe00,           /* Environment-specific use */
    ET_HIOS = 0xfeff,
    ET_LOPROC = 0xff00,         /* Processor-specific use */
    ET_HIPROC = 0xffff,
};

/* Special section indices */
enum {
    SHN_UNDEF = 0,              /* Used to mark an undefined or meaningless
                                   section reference */
    SHN_LOPROC = 0xff00,        /* Processor-specific use */
    SHN_HIPROC = 0xff1f,
    SHN_LOOS = 0xff20,          /* Environment-specific use */
    SHN_HIOS = 0xff3f,
    SHN_ABS = 0xfff1,           /* Indices that the corresponding reference is
                                   an absolute value */
    SHN_COMMON = 0xfff2,        /* Indicates a symbol that has been declared as
                                   a common block (Fortran COMMON or C tentative
                                   declaration) */
};

/* Section header entries */
typedef struct {
    Elf64_Word          sh_name;        /* Section name */
    Elf64_Word          sh_type;        /* Section type */
    Elf64_Xword         sh_flags;       /* Section attributes */
    Elf64_Addr          sh_addr;        /* Virtual address in memory */
    Elf64_Off           sh_offset;      /* Offset in file */
    Elf64_Xword         sh_size;        /* Size of section */
    Elf64_Word          sh_link;        /* Link to other section */
    Elf64_Word          sh_info;        /* Miscellaneous information */
    Elf64_Xword         sh_addralign;   /* Address alignment boundary */
    Elf64_Xword         sh_entsize;     /* Size of entries, if section has
                                           table */
} __attribute__ ((packed)) Elf64_Shdr;

/* Section Types, sh_type */
enum {
    SHT_NULL = 0,               /* Marks an unused section header */
    SHT_PROGBITS = 1,           /* Contains information defined by the
                                   program */
    SHT_SYMTAB = 2,             /* Contains a linker symbol table */
    SHT_STRTAB = 3,             /* Contains a string table */
    SHT_RELA = 4,               /* Contains "Rela" type relocation entries */
    SHT_HASH = 5,               /* Contains a symbol hash table */
    SHT_DYNAMIC = 6,            /* Contains dynamic linking tables */
    SHT_NOTE = 7,               /* Contains note information */
    SHT_NOBITS = 8,             /* Contains uninitialized space; does not occupy
                                   any space in the file */
    SHT_REL = 9,                /* Contains "Rel" type relocation entries */
    SHT_SHLIB = 10,             /* Reserved */
    SHT_DYNSYM = 11,            /* Contains a dynamic loader symbol table */
    SHT_LOOS = 0x60000000,      /* Environment-specific use */
    SHT_HIOS = 0x6fffffff,
    SHT_LOPROC = 0x70000000,    /* Processor-specific use */
    SHT_HIPROC = 0x7fffffff,
};

/* Section Attributes, sh_flags */
enum {
    SHF_WRITE = 0x1,            /* Section contains writable data */
    SHF_ALLOC = 0x2,            /* Section is allocated in memory image of
                                   program */
    SHF_EXECINSTR = 0x4,        /* Section contains executable instructions */
    SHF_INFO = 0x40,
    SHF_MASKOS = 0x0f000000,    /* Environment-specific use */
    SHF_MASKPROC = 0xf0000000,  /* Processor-specific use */
};

/* Standard sections
   Section Name         Section Type    Flags
   .bss                 SHT_NOBITS      A, W    Uninitialized data
   .data                SHT_PROGBITS    A, W    Initialized data
   .interp              SHT_PROGBITS    [A]     Program interpreter path name
   .rodata              SHT_PROGBITS    A       Read-only data
   .text                SHT_PROGBITS    A, X    Executable code
   .comment             SHT_PROGBITS    none    Version control information
   .dynamic             SHT_DYNAMIC     A[, W]  Dynamic linking tables
   .dynstr              SHT_STRTAB      A       String table for .dynamic
   .dynsym              SHT_DYNSYM      A       Symbol table for dynamic linking
   etc...
 */

/* Symbol table */
typedef struct {
    Elf64_Word          st_name;        /* Symbol name */
    unsigned char       st_info;        /* Type and Binding attributes */
    unsigned char       st_other;       /* Reserved */
    Elf64_Half          st_shndx;       /* Section table index */
    Elf64_Addr          st_value;       /* Symbol value */
    Elf64_Xword         st_size;        /* Sizeof object (e.g., common) */
} __attribute__ ((packed)) Elf64_Sym;

/* Symbol Bindings */
enum {
    STB_LOCAL = 0,              /* Not visible outside the object file */
    STB_GLOBAL = 1,             /* Global symbol, visible to all object files */
    STB_WEAK = 2,               /* Global scope, but with lower precedence than
                                   global symbols */
    STB_LOOS = 10,              /* Environement-specific use */
    STB_HIOS = 12,
    STB_LOPROC = 13,            /* Processor-specific use */
    STB_HIPROC = 15,
};

/* Symbol Types */
enum {
    STT_NOTYPE = 0,             /* No type specified (e.g., an absolute
                                   symbol) */
    STT_OBJECT = 1,             /* Data object */
    STT_FUNC = 2,               /* Function entry point */
    STT_SECTION = 3,            /* Symbol is associated with a section */
    STT_FILE = 4,               /* Source file associated with the object
                                   file */
    STT_LOOS = 10,              /* Environement-specific use */
    STT_HIOS = 12,
    STT_LOPROC = 13,            /* Processor-specific use */
    STT_HIPROC = 15,
};

#define ELF32_ST_BIND(info)             ((info) >> 4)
#define ELF32_ST_TYPE(info)             ((info) & 0xf)
#define ELF32_ST_INFO(bind, type)       (((bind)<<4)+((type)&0xf))

#define ELF64_ST_BIND(info)             ((info) >> 4)
#define ELF64_ST_TYPE(info)             ((info) & 0xf)
#define ELF64_ST_INFO(bind, type)       (((bind)<<4)+((type)&0xf))

/* ELF-64 Relocation Entries */
typedef struct {
    Elf64_Addr          r_offset;       /* Address of reference */
    Elf64_Xword         r_info;         /* Symbol index and type of
                                           relocation */
} __attribute__ ((packed)) Elf64_Rel;
typedef struct {
    Elf64_Addr          r_offset;       /* Address of reference */
    Elf64_Xword         r_info;         /* Symbol index and type of
                                           relocation */
    Elf64_Sxword        r_addend;       /* Constant part of expression */
} __attribute__ ((packed)) Elf64_Rela;

#define ELF64_R_SYM(i)      ((i) >> 32)
#define ELF64_R_TYPE(i)     ((i) & 0xffffffffL)
#define ELF64_R_INFO(s, t)  (((Elf64_Xword)(s) << 32) + ((t) & 0xffffffffL))

/* Program header table entry */
typedef struct {
    Elf64_Word          p_type;         /* Type of segment */
    Elf64_Word          p_flags;        /* Segment attributes */
    Elf64_Off           p_offset;       /* Offset in file */
    Elf64_Addr          p_vaddr;        /* Virtual address in memory */
    Elf64_Addr          p_paddr;        /* Reserved */
    Elf64_Xword         p_filesz;       /* Size of segment in file */
    Elf64_Xword         p_memsz;        /* Size of segment in memory */
    Elf64_Xword         p_align;        /* Alignment of segment */
} __attribute__ ((packed)) Elf64_Phdr;

/* Segment Types, p_type */
enum {
    PT_NULL = 0,                /* Unused entry */
    PT_LOAD = 1,                /* Loadable segment */
    PT_DYNAMIC = 2,             /* Dynamic linking tables */
    PT_INTERP = 3,              /* Program interpreter path name */
    PT_NOTE = 4,                /* Note sections */
    PT_SHLIB = 5,               /* Reserved */
    PT_PHDR = 6,                /* Program header table */
    PT_GNU_STACK = 0x6474e551,  /* GNU Stack */
    PT_LOOS = 0x60000000,       /* Environement-specific use */
    PT_HIOS = 0x6fffffff,
    PT_LOPROC = 0x70000000,     /* Processor-specific use */
    PT_HIPROC = 0x7fffffff,
};

/* Segment Attributes, p_flags */
enum {
    PF_X = 0x1,                 /* Execute permission */
    PF_W = 0x2,                 /* Write permission */
    PF_R = 0x4,                 /* Read permission */
    PF_MASKOS = 0x00ff0000,     /* These flag bits are reserved for
                                   environment-specific use */
    PF_MASKPROC = 0xff000000,   /* These flag bits are reserved for
                                   processor-specific use */
};

/* Dynamic table */
typedef struct {
    Elf64_Sxword        d_tag;
    union {
        Elf64_Xword     d_val;
        Elf64_Addr      d_ptr;
    } d_un;
} Elf64_Dyn;

/* Dynamic table entries */
enum {
    DT_NULL = 0,                /* d_un = ignored; Marks the end of the dynamic
                                   array */
    DT_NEEDED = 1,              /* d_un = d_val; The string table offset of the
                                   name of a needed library */
    DT_PLTRELSZ = 2,            /* d_un = d_val; Total size, in bytes, of the
                                   relocation entries associated with the
                                   procedure linkage table */
    DT_PLTGOT = 3,
    DT_HASH = 4,
    DT_STRTAB = 5,
    DT_SYMTAB = 6,
    DT_RELA = 7,
    DT_RELASZ = 8,
    DT_RELAENT = 9,
    DT_STRSZ = 10,
    DT_SYMENT = 11,
    DT_INIT = 12,
    DT_FINI = 13,
    DT_SONAME = 14,
    DT_RPATH = 15,
    DT_SYMBOLIC = 16,
    DT_REL = 17,
    DT_RELSZ = 18,
    DT_RELENT = 19,
    DT_PLTREL = 20,
    DT_DEBUG = 21,
    DT_TEXTREL = 22,
    DT_JMPREL = 23,
    DT_BIND_NOW = 24,
    DT_INIT_ARRAY = 25,
    DT_FINI_ARRAY = 26,
    DT_INIT_ARRAYSZ = 27,
    DT_FINI_ARRAYSZ = 28,
    DT_LOOS = 0x60000000,
    DT_HIOS = 0x6fffffff,
    DT_LOPROC = 0x70000000,
    DT_HIPROC = 0x7fffffff,
};

enum {
    R_X86_64_NONE = 0,
    R_X86_64_64 = 1,
    R_X86_64_PC32 = 2,
    R_X86_64_GOT32 = 3,
    R_X86_64_PLT32 = 4,
    R_X86_64_COPY = 5,
    R_X86_64_GLOB_DAT = 6,
    R_X86_64_JUMP_SLOT = 7,
    R_X86_64_RELATIVE = 8,
    R_X86_64_GOTPCREL = 9,
    R_X86_64_32 = 10,
    R_X86_64_32S = 11,
    R_X86_64_16 = 12,
    R_X86_64_PC16 = 13,
    R_X86_64_8 = 14,
    R_X86_64_PC8 = 15,
    R_X86_64_DTPMOD64 = 16,
    R_X86_64_DTPOFF64 = 17,
    R_X86_64_TPOFF64 = 18,
    R_X86_64_TLSGD = 19,
    R_X86_64_SLSLD = 20,
    R_X86_64_DTPOFF32 = 21,
    R_X86_64_GOTTPOFF = 22,
    R_X86_64_TPOFF32 = 23,
    R_X86_64_PC64 = 24,
    R_X86_64_GOTOFF64 = 25,
    R_X86_64_GOTPC32 = 26,
    R_X86_64_SIZE32 = 32,
    R_X86_64_SIZE64 = 33,
    R_X86_64_GOTPC32_TLSDESC = 34,
    R_X86_64_TLSDESC_CALL = 35,
    R_X86_64_TLSDESC = 36,
    R_X86_64_IRELATIVE = 37,
};

/*
 * Calculatet the hash value
 */
static unsigned long
elf64_hash(const unsigned char *name)
{
    unsigned long h = 0, g;

    while ( *name ) {
        h = (h << 4) + *name++;
        g = h & 0xf0000000;
        if ( g ) {
            h ^= g >> 24;
            h &= 0x0fffffff;
        }
    }

    return h;
}

/*
 * Export
 */
int
elf_export(FILE *fp, arch_code_t *code)
{
    Elf64_Ehdr hdr;
    char *strtab;
    size_t strtablen;
    ssize_t stroff;
    char shstrtab[256];
    size_t shstrtablen;
    off_t shoff;
    int i;
    ssize_t nw;
    int nsects;
    Elf64_Sym *syms;
    Elf64_Rela *rela;

    /* Build the sections */
    shstrtablen = 0;
    Elf64_Shdr shdr_null = {
        .sh_name = 0,
        .sh_type = SHT_NULL,
        .sh_flags = 0,
        .sh_addr = 0,
        .sh_offset = 0,
        .sh_size = 0,
        .sh_link = 0,
        .sh_info = 0,
        .sh_addralign = 0,
        .sh_entsize = 0,
    };
    strcpy(shstrtab + shstrtablen, "");
    shstrtablen += strlen("") + 1;

    Elf64_Shdr shdr_text = {
        .sh_name = shstrtablen,
        .sh_type = SHT_PROGBITS,
        .sh_flags = SHF_ALLOC | SHF_EXECINSTR,
        .sh_addr = 0,
        .sh_offset = 0,
        .sh_size = code->text.size,
        .sh_link = 0,
        .sh_info = 0,
        .sh_addralign = 8, /* 2^3 */
        .sh_entsize = 0,
    };
    strcpy(shstrtab + shstrtablen, ".text");
    shstrtablen += strlen(".text") + 1;

    Elf64_Shdr shdr_rela = {
        .sh_name = shstrtablen,
        .sh_type = SHT_RELA,
        .sh_flags = SHF_WRITE | SHF_ALLOC,
        .sh_addr = 0,
        .sh_offset = 0,
        .sh_size = 0,
        .sh_link = 6,         /* Symbol table reference by relocations */
        .sh_info = 1,         /* Section index of section to which the
                                 relocations apply */
        .sh_addralign = 8, /* 2^3 */
        .sh_entsize = sizeof(Elf64_Rela),
    };
    strcpy(shstrtab + shstrtablen, ".text.rela");
    shstrtablen += strlen(".text.rela") + 1;

    Elf64_Shdr shdr_data = {
        .sh_name = shstrtablen,
        .sh_type = SHT_PROGBITS,
        .sh_flags = SHF_WRITE | SHF_ALLOC,
        .sh_addr = 0,
        .sh_offset = 0,
        .sh_size = 0,
        .sh_link = 0,
        .sh_info = 0,
        .sh_addralign = 1,
        .sh_entsize = 0,
    };
    strcpy(shstrtab + shstrtablen, ".data");
    shstrtablen += strlen(".data") + 1;

    Elf64_Shdr shdr_bss = {
        .sh_name = shstrtablen,
        .sh_type = SHT_NOBITS,
        .sh_flags = SHF_WRITE | SHF_ALLOC,
        .sh_addr = 0,
        .sh_offset = 0,
        .sh_size = 0,
        .sh_link = 0,
        .sh_info = 0,
        .sh_addralign = 1,
        .sh_entsize = 0,
    };
    strcpy(shstrtab + shstrtablen, ".bss");
    shstrtablen += strlen(".bss") + 1;

    Elf64_Shdr shdr_shstrtab = {
        .sh_name = shstrtablen,
        .sh_type = SHT_STRTAB,
        .sh_flags = 0,
        .sh_addr = 0,
        .sh_offset = 0,
        .sh_size = 0x100,
        .sh_link = 0,
        .sh_info = 0,
        .sh_addralign = 1,
        .sh_entsize = 0,
    };
    strcpy(shstrtab + shstrtablen, ".shstrtab");
    shstrtablen += strlen(".shstrtab") + 1;

    Elf64_Shdr shdr_symtab = {
        .sh_name = shstrtablen,
        .sh_type = SHT_SYMTAB,
        .sh_flags = 0,
        .sh_addr = 0,
        .sh_offset = 0,
        .sh_size = 0,
        .sh_link = 7,
        .sh_info = 4,           /* Index of first non-local symbol */
        .sh_addralign = 1,
        .sh_entsize = sizeof(Elf64_Sym),
    };
    strcpy(shstrtab + shstrtablen, ".symtab");
    shstrtablen += strlen(".symtab") + 1;

    Elf64_Shdr shdr_strtab = {
        .sh_name = shstrtablen,
        .sh_type = SHT_STRTAB,
        .sh_flags = 0,
        .sh_addr = 0,
        .sh_offset = 0,
        .sh_size = 0,
        .sh_link = 0,
        .sh_info = 0,
        .sh_addralign = 1,
        .sh_entsize = 0,
    };
    strcpy(shstrtab + shstrtablen, ".strtab");
    shstrtablen += strlen(".strtab") + 1;

    /* Align */
    shstrtablen = ((shstrtablen + 7) / 8) * 8;

    /* FIXME: Relocation */
    rela = alloca(sizeof(Elf64_Rela) * 1);
    if ( NULL == rela ) {
        return -1;
    }
    rela[0].r_offset = 27;
    rela[0].r_info = ELF64_R_INFO(3, R_X86_64_PC32); /* .bss */
    rela[0].r_addend = -4;

    /* .strtab */
    strtablen = 1;
    for ( i = 0; i < code->sym.n; i++ ) {
        /* Add the length of the label */
        strtablen += strlen(code->sym.syms[i].label) + 1;
    }
    /* Align */
    strtablen = ((strtablen + 7) / 8) * 8;

    nsects = 8;
    shdr_text.sh_offset = sizeof(Elf64_Ehdr);

    /* Allocate */
    strtab = alloca(strtablen);
    if ( NULL == strtab ) {
        return -1;
    }
    memset(strtab, 0, strtablen);
    syms = alloca(sizeof(Elf64_Sym) * (code->sym.n + 4));
    if ( NULL == syms ) {
        return -1;
    }
    /* Null */
    syms[0].st_name = 0;
    syms[0].st_info = 0;
    syms[0].st_other = 0;
    syms[0].st_shndx = 0;
    syms[0].st_value = 0;
    syms[0].st_size = 0;

    /* .text */
    syms[1].st_name = 0;
    syms[1].st_info = ELF64_ST_INFO(STB_LOCAL, STT_SECTION);
    syms[1].st_other = 0;
    syms[1].st_shndx = 1;
    syms[1].st_value = shdr_text.sh_offset;
    syms[1].st_size = 0;

    /* .data */
    syms[2].st_name = 0;
    syms[2].st_info = ELF64_ST_INFO(STB_LOCAL, STT_SECTION);
    syms[2].st_other = 0;
    syms[2].st_shndx = 3;
    syms[2].st_value = 0;
    syms[2].st_size = 0;

    /* .bss */
    syms[3].st_name = 0;
    syms[3].st_info = ELF64_ST_INFO(STB_LOCAL, STT_SECTION);
    syms[3].st_other = 0;
    syms[3].st_shndx = 4;
    syms[3].st_value = 0;
    syms[3].st_size = 0;

    stroff = 1;
    for ( i = 0; i < code->sym.n; i++ ) {
        /* nlist_64: Describes an entry in the symbol table for 64-bit
           architectures. */
        syms[i + 4].st_name = stroff;
        switch ( code->sym.syms[i].type ) {
        case ARCH_SYM_LOCAL:
            syms[i + 4].st_info = ELF64_ST_INFO(STB_LOCAL, STT_NOTYPE);
            break;
        case ARCH_SYM_GLOBAL:
            syms[i + 4].st_info = ELF64_ST_INFO(STB_GLOBAL, STT_NOTYPE);
            break;
        case ARCH_SYM_FUNC:
            syms[i + 4].st_info = ELF64_ST_INFO(STB_GLOBAL, STT_FUNC);
            break;
        default:
            return -1;
        }
        syms[i + 4].st_other = 0;
        syms[i + 4].st_shndx = 1; /* .text */
        syms[i + 4].st_value = code->sym.syms[i].pos;
        syms[i + 4].st_size = code->sym.syms[i].size;

        /* Symbol table */
        strcpy(strtab + stroff, code->sym.syms[i].label);
        stroff += strlen(code->sym.syms[i].label) + 1;
    }

    /* FIXME */
    shdr_bss.sh_offset = sizeof(Elf64_Ehdr) + code->text.size;
    shdr_bss.sh_size = 8;
    shdr_rela.sh_offset = sizeof(Elf64_Ehdr) + code->text.size + 8;
    shdr_rela.sh_size = sizeof(Elf64_Rela) * 1;
    shoff = sizeof(Elf64_Ehdr) + code->text.size + 8 + sizeof(Elf64_Rela) * 1;
    shdr_shstrtab.sh_offset = shoff + sizeof(Elf64_Shdr) * nsects;
    shdr_shstrtab.sh_size = shstrtablen;
    shdr_symtab.sh_offset = shdr_shstrtab.sh_offset + shstrtablen;
    shdr_symtab.sh_size = sizeof(Elf64_Sym) * (code->sym.n + 4);
    shdr_strtab.sh_offset = shdr_symtab.sh_offset + shdr_symtab.sh_size;
    shdr_strtab.sh_size = strtablen;

    /* ELF header */
    hdr.e_ident[EI_MAG0] = '\x7f';
    hdr.e_ident[EI_MAG1] = 'E';
    hdr.e_ident[EI_MAG2] = 'L';
    hdr.e_ident[EI_MAG3] = 'F';
    hdr.e_ident[EI_CLASS] = ELFCLASS64;
    hdr.e_ident[EI_DATA] = ELFDATA2LSB;
    hdr.e_ident[EI_VERSION] = EV_CURRENT;
    hdr.e_ident[EI_OSABI] = ELFOSABI_SYSV;
    hdr.e_ident[EI_ABIVERSION] = 0;
    hdr.e_ident[EI_PAD] = 0;
    hdr.e_type = ET_REL;
    hdr.e_machine = EM_X86_64;
    hdr.e_version = 1;
    hdr.e_entry = 0;
    hdr.e_phoff = 0;
    hdr.e_shoff = shoff;
    hdr.e_flags = 0;
    hdr.e_ehsize = sizeof(Elf64_Ehdr);
    hdr.e_phentsize = 0;
    hdr.e_phnum = 0;
    hdr.e_shentsize = sizeof(Elf64_Shdr);
    hdr.e_shnum = nsects;
    hdr.e_shstrndx = 5;

    /* Write the header */
    nw = fwrite(&hdr, sizeof(Elf64_Ehdr), 1, fp);
    if ( nw != 1 ) {
        return -1;
    }

    /* Write the program */
    nw = fwrite(code->text.s, 1, code->text.size, fp);
    if ( nw != (ssize_t)code->text.size ) {
        return -1;
    }

    /* FIXME: Write the relocation */
    uint8_t buf[8];
    memset(buf, 0, 8);
    nw = fwrite(buf, 1, 8, fp);
    if ( nw != (ssize_t)8 ) {
        return -1;
    }
    nw = fwrite(rela, sizeof(Elf64_Rela), 1, fp);
    if ( nw != 1 ) {
        return -1;
    }

    /* Write the section headers */
    nw = fwrite(&shdr_null, sizeof(Elf64_Shdr), 1, fp);
    if ( nw != 1 ) {
        return -1;
    }
    nw = fwrite(&shdr_text, sizeof(Elf64_Shdr), 1, fp);
    if ( nw != 1 ) {
        return -1;
    }
    nw = fwrite(&shdr_rela, sizeof(Elf64_Shdr), 1, fp);
    if ( nw != 1 ) {
        return -1;
    }
    nw = fwrite(&shdr_data, sizeof(Elf64_Shdr), 1, fp);
    if ( nw != 1 ) {
        return -1;
    }
    nw = fwrite(&shdr_bss, sizeof(Elf64_Shdr), 1, fp);
    if ( nw != 1 ) {
        return -1;
    }
    nw = fwrite(&shdr_shstrtab, sizeof(Elf64_Shdr), 1, fp);
    if ( nw != 1 ) {
        return -1;
    }
    nw = fwrite(&shdr_symtab, sizeof(Elf64_Shdr), 1, fp);
    if ( nw != 1 ) {
        return -1;
    }
    nw = fwrite(&shdr_strtab, sizeof(Elf64_Shdr), 1, fp);
    if ( nw != 1 ) {
        return -1;
    }

    /* Write the section header string table */
    nw = fwrite(shstrtab, 1, shstrtablen, fp);
    if ( nw != (ssize_t)shstrtablen ) {
        return -1;
    }

    /* Write the symbol headers */
    nw = fwrite(syms, sizeof(Elf64_Sym), code->sym.n + 4, fp);
    if ( nw != (ssize_t)(code->sym.n + 4) ) {
        return -1;
    }

    /* Write the string table */
    nw = fwrite(strtab, 1, strtablen, fp);
    if ( nw != (ssize_t)strtablen ) {
        return -1;
    }

    return 0;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
