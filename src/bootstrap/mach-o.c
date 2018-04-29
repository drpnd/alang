/*_
 * Copyright (c) 2017-2018 Hirochika Asai <asai@jar.jp>
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

#include "mach-o.h"
#include "code.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define MH_MAGIC_64             0xfeedfacfUL
#define CPUTYPE_X86_64          0x01000007UL
#define CPUSUBTYPE_X86_64       0x00000003UL

#define FILETYPE_OBJECT         1
#define FILETYPE_EXECUTE        2

#define SUBSECTIONS_VIA_SYMBOLS 0x2000

#define LC_SYMTAB               0x02
#define LC_DYSYMTAB             0x0b
#define LC_SEGMENT_64           0x19
#define LC_VERSION_MIN_MACOSX   0x24

#define N_STAB  0xe0
#define N_PEXT  0x10
#define N_TYPE  0x0e
#define N_EXT   0x01
/* N_TYPE values */
#define N_UNDF  0x0
#define N_ABS   0x2
#define N_SECT  0xe
#define N_PBUD  0xc
#define N_INDR  0xa

#define REFERENCE_FLAG_UNDEFINED_NON_LAZY   0x0
#define REFERENCE_FLAG_UNDEFINED_LAZY       0x1
#define REFERENCE_FLAG_DEFINED              0x2
#define REFERENCE_FLAG_PRIVATE_DEFINED      0x3
#define REFERENCE_FLAG_PRIVATE_UNDEFINED_NON_LAZY   0x4
#define REFERENCE_FLAG_PRIVATE_UNDEFINED_LAZY   0x5
#define REFERENCED_DYNAMICALLY              0x10
#define N_DESC_DISCARDED                    0x20
#define N_WEAK_REF                          0x40
#define N_WEAK_DEF                          0x80


#define S_REGULAR               0x0
#define S_ZEROFILL              0x1
#define S_CSTRING_LITERALS      0x2
#define S_4BYTE_LITERALS        0x3
#define S_8BYTE_LITERALS        0x4
#define S_LITERAL_POINTERS      0x5
#define S_NON_LAZY_SYMBOL_POINTERS      0x6
#define S_LAZY_SYMBOL_POINTERS  0x7
#define S_SYMBOL_STUBS          0x8
#define S_MOD_INIT_FUNC_POINTERS        0x9
#define S_MOD_TERM_FUNC_POINTERS        0xa
#define S_COALESCED             0xb
#define S_GB_ZEROFILL           0xc
#define S_INTERPOSING           0xd
#define S_16BYTE_LITERALS       0xe
#define S_DTRACE_DOF            0xf
#define S_LAZY_DYLIB_SYMBOL_POINTERS    0x10
#define S_THREAD_LOCAL_REGULAR  0x11
#define S_THREAD_LOCAL_ZEROFILL 0x12
#define S_THREAD_LOCAL_VARIABLES        0x13
#define S_THREAD_LOCAL_VARIABLE_POINTERS        0x14
#define S_THREAD_LOCAL_INIT_FUNCTION_POINTERS   0x15
#define S_INIT_FUNC_OFFSETS                     0x16

#define SECTION_ATTRIBUTES_USR   0xff000000
#define S_ATTR_PURE_INSTRUCTIONS 0x80000000
#define S_ATTR_NO_TOC            0x40000000
#define S_ATTR_STRIP_STATIC_SYMS 0x20000000
#define S_ATTR_NO_DEAD_STRIP     0x10000000
#define S_ATTR_LIVE_SUPPORT      0x08000000
#define S_ATTR_SELF_MODIFYING_CODE 0x04000000

#define S_ATTR_DEBUG             0x02000000
#define SECTION_ATTRIBUTES_SYS   0x00ffff00
#define S_ATTR_SOME_INSTRUCTIONS 0x00000400
#define S_ATTR_EXT_RELOC         0x00000200
#define S_ATTR_LOC_RELOC         0x00000100


enum reloc_type_x86_64
{
    X86_64_RELOC_UNSIGNED,
    X86_64_RELOC_SIGNED,
    X86_64_RELOC_BRANCH,
    X86_64_RELOC_GOT_LOAD,
    X86_64_RELOC_GOT,
    X86_64_RELOC_SUBTRACTOR,
    X86_64_RELOC_SIGNED_1,
    X86_64_RELOC_SIGNED_2,
    X86_64_RELOC_SIGNED_4,
    X86_64_RELOC_TLV,
};

/*
 * Mach-O 64-bit header
 */
struct mach_header_64 {
    uint32_t magic;
    uint32_t cputype;
    uint32_t cpusubtype;
    uint32_t filetype;
    /* # of load commands following the header structure */
    uint32_t ncmds;
    /* # of bytes occupied by the load commands */
    uint32_t sizeofcmds;
    uint32_t flags;
    uint32_t reserved;
} __attribute__ ((packed));

struct load_command {
    uint32_t cmd;
    uint32_t cmdsize;
} __attribute__ ((packed));

struct segment_command_64 {
    uint32_t cmd;
    uint32_t cmdsize;
    char segname[16];
    uint64_t vmaddr;
    uint64_t vmsize;
    uint64_t fileoff;
    uint64_t filesize;
    uint32_t maxprot;
    uint32_t initprot;
    uint32_t nsects;
    uint32_t flags;
} __attribute__ ((packed));

struct version_min_command {
    uint32_t cmd;
    uint32_t cmdsize;
    uint32_t version;
    uint32_t sdk;
};

struct symtab_command {
    uint32_t cmd;
    uint32_t cmdsize;
    uint32_t symoff;
    uint32_t nsyms;
    uint32_t stroff;
    uint32_t strsize;
};

struct dysymtab_command {
    uint32_t cmd;
    uint32_t cmdsize;

    uint32_t ilocalsym;
    uint32_t nlocalsym;
    uint32_t iextdefsym;
    uint32_t nextdefsym;
    uint32_t iundefsym;
    uint32_t nundefsym;

    uint32_t tocoff;
    uint32_t ntoc;

    uint32_t modtaboff;
    uint32_t nmodtab;

    uint32_t extrefsymoff;
    uint32_t nextrefsyms;

    uint32_t indirectsymoff;
    uint32_t nindirectsyms;

    uint32_t extreloff;
    uint32_t nextrel;

    uint32_t locreloff;
    uint32_t nlocrel;
} __attribute__ ((packed));

struct section_64 {
    char sectname[16];
    char segname[16];
    uint64_t addr;
    uint64_t size;
    uint32_t offset;
    uint32_t align;
    uint32_t reloff;
    uint32_t nreloc;
    uint32_t flags;
    uint32_t reserved1;
    uint32_t reserved2;
    uint32_t reserved3;
} __attribute__ ((packed));

struct relocation_info {
    int32_t r_address;
    uint32_t r_symbolnum:24,
        r_pcrel:1,
        r_length:2,
        r_extern:1,
        r_type:4;
} __attribute__ ((packed));

struct nlist_64 {
    union {
        int32_t n_strx;
    } n_un;
    uint8_t n_type;
    uint8_t n_sect;
    uint16_t n_desc;
    uint64_t n_value;
} __attribute__ ((packed));



/*
 * Write an execution binary
 */
int
mach_o_test2(struct code *code, FILE *fp)
{
    /* String table */
    char *strtab;
    size_t strtablen;

    struct mach_header_64 hdr;
    struct segment_command_64 seg;
    struct section_64 sect[3];
    struct version_min_command vercmd;
    struct symtab_command symtab;
    struct dysymtab_command dysymtab;
    struct relocation_info relinfo;
    struct nlist_64 nl[3];
    size_t nw;
    off_t off;
    size_t segoff;
    ssize_t i;

    /* Get the length of the string table */
    strtablen = 1;
    for ( i = 0; i < (ssize_t)code->symbols.size; i++ ) {
        strtablen += strlen(code->symbols.ents[i].name) + 1;
    }
    for ( i = 0; i < (ssize_t)code->dsyms.size; i++ ) {
        strtablen += strlen(code->dsyms.ents[i].name) + 1;
    }

    /* Allocate the string table */
    strtab = malloc(sizeof(char) * strtablen);
    if ( NULL == strtab ) {
        return -1;
    }

    /* Build the string table */
    strtab[0] = '\0';
    off = 1;
    for ( i = 0; i < (ssize_t)code->symbols.size; i++ ) {
        memcpy(strtab + off, code->symbols.ents[i].name,
               strlen(code->symbols.ents[i].name) + 1);
        off += strlen(code->symbols.ents[i].name) + 1;
    }
    for ( i = 0; i < (ssize_t)code->dsyms.size; i++ ) {
        memcpy(strtab + off, code->dsyms.ents[i].name,
               strlen(code->dsyms.ents[i].name) + 1);
        off += strlen(code->dsyms.ents[i].name) + 1;
    }

    /* Segment offset */
    segoff = 0x200;

    /* Header */
    hdr.magic = MH_MAGIC_64;
    hdr.cputype = CPUTYPE_X86_64;
    hdr.cpusubtype = CPUSUBTYPE_X86_64;
    hdr.filetype = FILETYPE_OBJECT;
    hdr.ncmds = 4;
    hdr.sizeofcmds = 0;         /* To be updated */
    hdr.flags = SUBSECTIONS_VIA_SYMBOLS;
    hdr.reserved = 0;

    /* Segment command (text segment) */
    seg.cmd = LC_SEGMENT_64;
    seg.cmdsize = sizeof(struct segment_command_64)
        + 2 * sizeof(struct section_64);
    memset(seg.segname, 0, 16);
    seg.vmaddr = 0;
    seg.vmsize = 0x38;//code->bin.len;
    seg.fileoff = segoff;
    seg.filesize = 0x38;//code->bin.len;
    seg.maxprot = 0x07;
    seg.initprot = 0x07;
    seg.nsects = 2;
    seg.flags = 0;

    /* Text section */
    memset(&sect[0], 0, sizeof(struct section_64));
    strcpy(sect[0].sectname, "__text");
    strcpy(sect[0].segname, "__TEXT");
    sect[0].addr = 0;
    sect[0].size = code->bin.len;
    sect[0].offset = segoff;
    sect[0].align = 4;
    sect[0].reloff = 0x248;
    sect[0].nreloc = 2;
    sect[0].flags = S_ATTR_PURE_INSTRUCTIONS | S_ATTR_SOME_INSTRUCTIONS;
    sect[0].reserved1 = 0;
    sect[0].reserved2 = 0;
    sect[0].reserved3 = 0;

    /* Data section */
    memset(&sect[1], 0, sizeof(struct section_64));
    strcpy(sect[1].sectname, "__data");
    strcpy(sect[1].segname, "__DATA");
    sect[1].addr = 0x30;
    sect[1].size = 4;
    sect[1].offset = 0x240;
    sect[1].align = 4;
    sect[1].reloff = 0;
    sect[1].nreloc = 0;
    sect[1].flags = S_REGULAR;
    sect[1].reserved1 = 0;
    sect[1].reserved2 = 0;
    sect[1].reserved3 = 0;

    /* Version min command */
    vercmd.cmd = LC_VERSION_MIN_MACOSX;
    vercmd.cmdsize = sizeof(struct version_min_command);
    vercmd.version = 0x000a0c00;
    vercmd.sdk = 0;

    /* Symtab command */
    symtab.cmd = LC_SYMTAB;
    symtab.cmdsize = sizeof(struct symtab_command);
    symtab.symoff = 0x260;      /* XXX */
    symtab.nsyms = code->symbols.size + code->dsyms.size;
    symtab.stroff = 0x300;      /* XXX */
    symtab.strsize = strtablen;

    /* Dysymtab command */
    dysymtab.cmd = LC_DYSYMTAB;
    dysymtab.cmdsize = sizeof(struct dysymtab_command);
    dysymtab.ilocalsym = 0;
    dysymtab.nlocalsym = code->dsyms.size;
    dysymtab.iextdefsym = code->dsyms.size;
    dysymtab.nextdefsym = code->symbols.size;
    dysymtab.iundefsym = code->symbols.size + code->dsyms.size;
    dysymtab.nundefsym = 0;
    dysymtab.tocoff = 0;
    dysymtab.ntoc = 0;
    dysymtab.modtaboff = 0;
    dysymtab.nmodtab = 0;
    dysymtab.extrefsymoff = 0;
    dysymtab.nextrefsyms = 0;
    dysymtab.indirectsymoff = 0;
    dysymtab.nindirectsyms = 0;
    dysymtab.extreloff = 0;
    dysymtab.nextrel = 0;
    dysymtab.locreloff = 0;
    dysymtab.nlocrel = 0;

    /* Update the size and offset values */
    hdr.sizeofcmds = seg.cmdsize + vercmd.cmdsize + symtab.cmdsize
        + dysymtab.cmdsize;

    /* Relocation information: Describes an item in the file that uses an
       address that needs to be updated when the address is changed. */
    relinfo.r_address = 0x0d;
    relinfo.r_symbolnum = 0;
    relinfo.r_pcrel = 1;
    relinfo.r_length = 2;
    relinfo.r_extern = 1;
    relinfo.r_type = 1;

    /* Symbols */
    nl[0].n_un.n_strx = 14;
    nl[0].n_type = N_SECT;
    nl[0].n_sect = 0x02;
    nl[0].n_desc = 0;
    nl[0].n_value = 0x30;

    nl[1].n_un.n_strx = 1;
    nl[1].n_type = N_SECT | N_EXT;
    nl[1].n_sect = 0x01;
    nl[1].n_desc = REFERENCE_FLAG_DEFINED;
    nl[1].n_value = 0;

    nl[2].n_un.n_strx = 7;
    nl[2].n_type = N_SECT | N_EXT;
    nl[2].n_sect = 0x01;
    nl[2].n_desc = REFERENCE_FLAG_DEFINED;
    nl[2].n_value = 8;

    nw = fwrite(&hdr, sizeof(struct mach_header_64), 1, fp);
    nw = fwrite(&seg, sizeof(struct segment_command_64), 1, fp);
    nw = fwrite(&sect[0], sizeof(struct section_64), 1, fp);
    nw = fwrite(&sect[1], sizeof(struct section_64), 1, fp);
    nw = fwrite(&vercmd, sizeof(struct version_min_command), 1, fp);
    nw = fwrite(&symtab, sizeof(struct symtab_command), 1, fp);
    nw = fwrite(&dysymtab, sizeof(struct dysymtab_command), 1, fp);
    fseeko(fp, segoff, SEEK_SET);
    fwrite(code->bin.text, code->bin.len, 1, fp);
    fseeko(fp, 0x240, SEEK_SET);
    fwrite("\x02\x00\x00\x00", 4, 1, fp);
    fseeko(fp, 0x248, SEEK_SET);
    fwrite(&relinfo, sizeof(struct relocation_info), 1, fp);
    relinfo.r_address = 0x16;
    fwrite(&relinfo, sizeof(struct relocation_info), 1, fp);
    fseeko(fp, 0x260, SEEK_SET);
    fwrite(&nl[0], sizeof(struct nlist_64), 1, fp);
    fwrite(&nl[1], sizeof(struct nlist_64), 1, fp);
    fwrite(&nl[2], sizeof(struct nlist_64), 1, fp);
    fseeko(fp, 0x300, SEEK_SET);
    fwrite(strtab, strtablen, 1, fp);



    return 0;
}


int
mach_o_test(FILE *fp)
{
    struct code_symbol symbols[2];
    symbols[0].name = "_func";
    symbols[0].pos = 0;
    symbols[1].name = "_func2";
    symbols[1].pos = 8;

    char *code = "\x48\x31\xc0\x48\xff\xc0\xc3\x90\x48\x31\xc0\x48\xff\xc0\x48\xff\xc0\xc3\x90\x90";
    size_t codelen = 20;
    char *strtab = "\x00_func\x00_func2\x00\x00\x00";
    size_t strtablen = 16;
    struct mach_header_64 hdr;
    struct segment_command_64 seg;
    struct section_64 sect[3];
    struct version_min_command vercmd;
    struct symtab_command symtab;
    struct dysymtab_command dysymtab;
    struct relocation_info relinfo[2];
    struct nlist_64 nl[2];
    size_t nw;



    /* nlist_64: Describes an entry in the symbol table for 64-bit
       architectures. */
    nl[0].n_un.n_strx = 1;
    nl[0].n_type = N_SECT | N_EXT;
    nl[0].n_sect = 0x01;
    nl[0].n_desc = REFERENCE_FLAG_DEFINED;
    nl[0].n_value = 0;

    nl[1].n_un.n_strx = 7;
    nl[1].n_type = N_SECT | N_EXT;
    nl[1].n_sect = 0x01;
    nl[1].n_desc = REFERENCE_FLAG_DEFINED;
    nl[1].n_value = 8;

    /* Relocation information: Describes an item in the file that uses an
       address that needs to be updated when the address is changed. */
    relinfo[0].r_address = 0;
    relinfo[0].r_symbolnum = 2;
    relinfo[0].r_pcrel = 0;
    relinfo[0].r_length = 3;    /* 4 bytes (PPC_RELOC_BR14) */
    relinfo[0].r_extern = 0;
    relinfo[0].r_type = 0;

    relinfo[1].r_address = 0;
    relinfo[1].r_symbolnum = 1;
    relinfo[1].r_pcrel = 0;
    relinfo[1].r_length = 3;    /* 4 bytes (PPC_RELOC_BR14) */
    relinfo[1].r_extern = 0;
    relinfo[1].r_type = 0;

    /* Dysymtab command */
    dysymtab.cmd = LC_DYSYMTAB;
    dysymtab.cmdsize = sizeof(struct dysymtab_command);
    dysymtab.ilocalsym = 0;
    dysymtab.nlocalsym = 0;
    dysymtab.iextdefsym = 0;
    dysymtab.nextdefsym = 2;
    dysymtab.iundefsym = 2;
    dysymtab.nundefsym = 0;
    dysymtab.tocoff = 0;
    dysymtab.ntoc = 0;
    dysymtab.modtaboff = 0;
    dysymtab.nmodtab = 0;
    dysymtab.extrefsymoff = 0;
    dysymtab.nextrefsyms = 0;
    dysymtab.indirectsymoff = 0;
    dysymtab.nindirectsyms = 0;
    dysymtab.extreloff = 0;
    dysymtab.nextrel = 0;
    dysymtab.locreloff = 0;
    dysymtab.nlocrel = 0;

    /* Symtab command */
    symtab.cmd = LC_SYMTAB;
    symtab.cmdsize = sizeof(struct symtab_command);
    symtab.symoff = 0x260;
    symtab.nsyms = 2;
    symtab.stroff = 0x280;
    symtab.strsize = strtablen;

    /* Version min command */
    vercmd.cmd = LC_VERSION_MIN_MACOSX;
    vercmd.cmdsize = 0x10;
    vercmd.version = 0x000a0c00;
    vercmd.sdk = 0;

    /* Section */
    memset(&sect[0], 0, sizeof(struct section_64));
    strcpy(sect[0].sectname, "__text");
    strcpy(sect[0].segname, "__TEXT");
    sect[0].addr = 0;
    sect[0].size = codelen;
    sect[0].offset = 0x1d0;
    sect[0].align = 4;
    sect[0].reloff = 0;
    sect[0].nreloc = 0;
    sect[0].flags = 0x80000400UL;
    sect[0].reserved1 = 0;
    sect[0].reserved2 = 0;
    sect[0].reserved3 = 0;

    memset(&sect[1], 0, sizeof(struct section_64));
    strncpy(sect[1].sectname, "__compact_unwind", 16);
    strcpy(sect[1].segname, "__LD");
    sect[1].addr = 0x18;
    sect[1].size = 0x20;
    sect[1].offset = 0x1e8;
    sect[1].align = 3;
    sect[1].reloff = 0x248;
    sect[1].nreloc = 2;
    sect[1].flags = 0x02000000UL;
    sect[1].reserved1 = 0;
    sect[1].reserved2 = 0;
    sect[1].reserved3 = 0;

    memset(&sect[2], 0, sizeof(struct section_64));
    strcpy(sect[2].sectname, "__eh_frame");
    strcpy(sect[2].segname, "__TEXT");
    sect[2].addr = 0x38;
    sect[2].size = 0x40;
    sect[2].offset = 0x208;
    sect[2].align = 3;
    sect[2].reloff = 0;
    sect[2].nreloc = 0;
    sect[2].flags = 0x6800000bUL;
    sect[2].reserved1 = 0;
    sect[2].reserved2 = 0;
    sect[2].reserved3 = 0;

    /* Segment command */
    seg.cmd = LC_SEGMENT_64;
    seg.cmdsize = 0x98;
    memset(seg.segname, 0, 16);
    seg.vmaddr = 0;
    //seg.vmsize = 0x78;
    seg.vmsize = 0x20;
    seg.fileoff = 0x1d0;
    //seg.filesize = 0x78;
    seg.filesize = 0x20;
    seg.maxprot = 0x07;
    seg.initprot = 0x07;
    seg.nsects = 3 - 2;
    seg.flags = 0;

    /* Header */
    hdr.magic = MH_MAGIC_64;
    hdr.cputype = CPUTYPE_X86_64;
    hdr.cpusubtype = CPUSUBTYPE_X86_64;
    hdr.filetype = FILETYPE_OBJECT;
    hdr.ncmds = 4;
    hdr.sizeofcmds = 0x1b0;
    hdr.flags = SUBSECTIONS_VIA_SYMBOLS;
    hdr.reserved = 0;

    nw = fwrite(&hdr, sizeof(struct mach_header_64), 1, fp);
    printf("POS: %lld\n", ftello(fp));

    nw = fwrite(&seg, sizeof(struct segment_command_64), 1, fp);
    printf("POS: %lld\n", ftello(fp));

    nw = fwrite(&sect[0], sizeof(struct section_64), 1, fp);
    //nw = fwrite(&sect[1], sizeof(struct section_64), 1, fp);
    //nw = fwrite(&sect[2], sizeof(struct section_64), 1, fp);
    printf("POS SECTION: %lld\n", ftello(fp));

    nw = fwrite(&vercmd, sizeof(struct version_min_command), 1, fp);
    printf("POS: %lld\n", ftello(fp));

    nw = fwrite(&symtab, sizeof(struct symtab_command), 1, fp);
    printf("POS: %lld\n", ftello(fp));

    nw = fwrite(&dysymtab, sizeof(struct dysymtab_command), 1, fp);
    printf("POS: %lld\n", ftello(fp));

    fseeko(fp, 0x1d0, SEEK_SET);
    printf("POS PROGRAM: %lld\n", ftello(fp));

    fwrite(code, codelen, 1, fp);
    printf("POS: %lld\n", ftello(fp));

    fseeko(fp, 0x248, SEEK_SET);
    printf("POS: %lld\n", ftello(fp));

    fwrite(&relinfo[0], sizeof(struct relocation_info), 1, fp);
    fwrite(&relinfo[1], sizeof(struct relocation_info), 1, fp);

    fseeko(fp, 0x260, SEEK_SET);
    printf("POS: %lld\n", ftello(fp));

    fwrite(&nl[0], sizeof(struct nlist_64), 1, fp);
    fwrite(&nl[1], sizeof(struct nlist_64), 1, fp);
    printf("POS: %lld\n", ftello(fp));

    fseeko(fp, 0x280, SEEK_SET);
    fwrite(strtab, strtablen, 1, fp);
    printf("POS: %lld\n", ftello(fp));

    return 0;




#if 0

    struct mach_header_64 hdr;
    size_t nw;

    /* Header */
    hdr.magic = MH_MAGIC_64;
    hdr.cputype = CPUTYPE_X86_64;
    hdr.cpusubtype = CPUSUBTYPE_X86_64;
    hdr.filetype = FILETYPE_OBJECT;
    hdr.ncmds = 4;
    hdr.sizeofcmds = 0x1b0;
    hdr.flags = SUBSECTIONS_VIA_SYMBOLS;
    hdr.reserved = 0;

    nw = fwrite(&hdr, sizeof(struct mach_header_64), 1, fp);
    printf("POS: %lld\n", ftello(fp));

    /* Segment command */
    struct segment_command_64 seg;
    seg.cmd = LC_SEGMENT_64;
    seg.cmdsize = 0x98;
    memset(seg.segname, 0, 16);
    seg.vmaddr = 0;
    seg.vmsize = 0x78;
    seg.fileoff = 0x1d0;
    seg.filesize = 0x78;
    seg.maxprot = 0x07;
    seg.initprot = 0x07;
    seg.nsects = 3 - 2;
    seg.flags = 0;

    nw = fwrite(&seg, sizeof(struct segment_command_64), 1, fp);
    printf("POS: %lld\n", ftello(fp));

    /* Section */
    struct section_64 sect;
    memset(&sect, 0, sizeof(struct section_64));
    strcpy(sect.sectname, "__text");
    strcpy(sect.segname, "__TEXT");
    sect.addr = 0;
    sect.size = 0x14;
    sect.offset = 0x1d0;
    sect.align = 4;
    sect.reloff = 0;
    sect.nreloc = 0;
    sect.flags = 0x80000400UL;
    sect.reserved1 = 0;
    sect.reserved2 = 0;
    sect.reserved3 = 0;

    nw = fwrite(&sect, sizeof(struct section_64), 1, fp);
    printf("POS SECTION: %lld\n", ftello(fp));

#if 0
    memset(&sect, 0, sizeof(struct section_64));
    strncpy(sect.sectname, "__compact_unwind", 16);
    strcpy(sect.segname, "__LD");
    sect.addr = 0x18;
    sect.size = 0x20;
    sect.offset = 0x1e8;
    sect.align = 3;
    sect.reloff = 0x248;
    sect.nreloc = 1;
    sect.flags = 0x02000000UL;
    sect.reserved1 = 0;
    sect.reserved2 = 0;
    sect.reserved3 = 0;

    nw = fwrite(&sect, sizeof(struct section_64), 1, fp);

    memset(&sect, 0, sizeof(struct section_64));
    strcpy(sect.sectname, "__eh_frame");
    strcpy(sect.segname, "__TEXT");
    sect.addr = 0x38;
    sect.size = 0x40;
    sect.offset = 0x208;
    sect.align = 3;
    sect.reloff = 0;
    sect.nreloc = 0;
    sect.flags = 0x6800000bUL;
    sect.reserved1 = 0;
    sect.reserved2 = 0;
    sect.reserved3 = 0;

    nw = fwrite(&sect, sizeof(struct section_64), 1, fp);
#endif

    /* Version min command */
    struct version_min_command vercmd;
    vercmd.cmd = LC_VERSION_MIN_MACOSX;
    vercmd.cmdsize = 0x10;
    vercmd.version = 0x000a0c00;
    vercmd.sdk = 0;

    nw = fwrite(&vercmd, sizeof(struct version_min_command), 1, fp);
    printf("POS: %lld\n", ftello(fp));

    /* Symtab command */
    struct symtab_command symtab;
    symtab.cmd = LC_SYMTAB;
    symtab.cmdsize = 0x18;
    symtab.symoff = 0x250;
    symtab.nsyms = 1;
    symtab.stroff = 0x260;
    symtab.strsize = 0x08;

    nw = fwrite(&symtab, sizeof(struct symtab_command), 1, fp);
    printf("POS: %lld\n", ftello(fp));

    /* Dysymtab command */
    struct dysymtab_command dysymtab;
    dysymtab.cmd = LC_DYSYMTAB;
    dysymtab.cmdsize = 0x50;
    dysymtab.ilocalsym = 0;
    dysymtab.nlocalsym = 0;
    dysymtab.iextdefsym = 0;
    dysymtab.nextdefsym = 1;
    dysymtab.iundefsym = 1;
    dysymtab.nundefsym = 0;
    dysymtab.tocoff = 0;
    dysymtab.ntoc = 0;
    dysymtab.modtaboff = 0;
    dysymtab.nmodtab = 0;
    dysymtab.extrefsymoff = 0;
    dysymtab.nextrefsyms = 0;
    dysymtab.indirectsymoff = 0;
    dysymtab.nindirectsyms = 0;
    dysymtab.extreloff = 0;
    dysymtab.nextrel = 0;
    dysymtab.locreloff = 0;
    dysymtab.nlocrel = 0;

    nw = fwrite(&dysymtab, sizeof(struct dysymtab_command), 1, fp);
    printf("POS: %lld\n", ftello(fp));

    fseeko(fp, 0x1d0, SEEK_SET);
    printf("POS PROGRAM: %lld\n", ftello(fp));

    fwrite("\x48\x31\xc0\x48\xff\xc0\xc3", 7, 1, fp);
    printf("POS: %lld\n", ftello(fp));

    fseeko(fp, 0x248, SEEK_SET);
    printf("POS: %lld\n", ftello(fp));

    struct relocation_info relinfo;
    relinfo.r_address = 0;
    relinfo.r_symbolnum = 1;
    relinfo.r_pcrel = 0;
    relinfo.r_length = 3;
    relinfo.r_extern = 0;
    relinfo.r_type = 0;
    fwrite(&relinfo, sizeof(struct relocation_info), 1, fp);

    fseeko(fp, 0x250, SEEK_SET);
    printf("POS: %lld\n", ftello(fp));

    struct nlist_64 nl;
    nl.n_un.n_strx = 1;
    nl.n_type = 0x0f;
    nl.n_sect = 0x01;
    nl.n_desc = 0;
    nl.n_value = 0;

    fwrite(&nl, sizeof(struct nlist_64), 1, fp);
    printf("POS: %lld\n", ftello(fp));

    fwrite("\x00_func\x00\x00", 8, 1, fp);
    printf("POS: %lld\n", ftello(fp));

    return 0;
#endif
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
