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

#include "../compile.h"
#include "../arch.h"
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

#define REFERENCE_FLAG_UNDEFINED_NON_LAZY               0x0
#define REFERENCE_FLAG_UNDEFINED_LAZY                   0x1
#define REFERENCE_FLAG_DEFINED                          0x2
#define REFERENCE_FLAG_PRIVATE_DEFINED                  0x3
#define REFERENCE_FLAG_PRIVATE_UNDEFINED_NON_LAZY       0x4
#define REFERENCE_FLAG_PRIVATE_UNDEFINED_LAZY           0x5
#define REFERENCED_DYNAMICALLY                          0x10
#define N_DESC_DISCARDED                                0x20
#define N_WEAK_REF                                      0x40
#define N_WEAK_DEF                                      0x80

#define S_REGULAR                                       0x0
#define S_ZEROFILL                                      0x1
#define S_CSTRING_LITERALS                              0x2
#define S_4BYTE_LITERALS                                0x3
#define S_8BYTE_LITERALS                                0x4
#define S_LITERAL_POINTERS                              0x5
#define S_NON_LAZY_SYMBOL_POINTERS                      0x6
#define S_LAZY_SYMBOL_POINTERS                          0x7
#define S_SYMBOL_STUBS                                  0x8
#define S_MOD_INIT_FUNC_POINTERS                        0x9
#define S_MOD_TERM_FUNC_POINTERS                        0xa
#define S_COALESCED                                     0xb
#define S_GB_ZEROFILL                                   0xc
#define S_INTERPOSING                                   0xd
#define S_16BYTE_LITERALS                               0xe
#define S_DTRACE_DOF                                    0xf
#define S_LAZY_DYLIB_SYMBOL_POINTERS                    0x10
#define S_THREAD_LOCAL_REGULAR                          0x11
#define S_THREAD_LOCAL_ZEROFILL                         0x12
#define S_THREAD_LOCAL_VARIABLES                        0x13
#define S_THREAD_LOCAL_VARIABLE_POINTERS                0x14
#define S_THREAD_LOCAL_INIT_FUNCTION_POINTERS           0x15
#define S_INIT_FUNC_OFFSETS                             0x16

#define SECTION_ATTRIBUTES_USR          0xff000000
#define S_ATTR_PURE_INSTRUCTIONS        0x80000000
#define S_ATTR_NO_TOC                   0x40000000
#define S_ATTR_STRIP_STATIC_SYMS        0x20000000
#define S_ATTR_NO_DEAD_STRIP            0x10000000
#define S_ATTR_LIVE_SUPPORT             0x08000000
#define S_ATTR_SELF_MODIFYING_CODE      0x04000000

#define S_ATTR_DEBUG                    0x02000000
#define SECTION_ATTRIBUTES_SYS          0x00ffff00
#define S_ATTR_SOME_INSTRUCTIONS        0x00000400
#define S_ATTR_EXT_RELOC                0x00000200
#define S_ATTR_LOC_RELOC                0x00000100

enum reloc_type_x86_64 {
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
 * Export
 */
int
mach_o_export(FILE *fp, arch_code_t *code)
{
    struct mach_header_64 hdr;
    struct segment_command_64 seg;
    struct section_64 sect_text;
    struct section_64 sect_data;
    struct section_64 sect_bss;
    struct version_min_command vercmd;
    struct symtab_command symtab;
    struct relocation_info *relocinfo;
    struct nlist_64 *nl;
    int sizeofcmds;
    int ncmds;
    int nsects;
    ssize_t nw;
    int i;
    off_t codepoint;
    size_t codesize;
    char *strtab;
    size_t strtablen;
    ssize_t stroff;
    off_t symoff;

    /* FIXME: Relocation info */
    relocinfo = alloca(sizeof(struct relocation_info) * 1);
    if ( NULL == relocinfo ) {
        return -1;
    }
    relocinfo[0].r_address = 27;
    relocinfo[0].r_symbolnum = 3;
    relocinfo[0].r_pcrel = 1;
    relocinfo[0].r_length = 2;
    relocinfo[0].r_extern = 1;
    relocinfo[0].r_type = X86_64_RELOC_SIGNED;

    strtablen = 1;
    for ( i = 0; i < code->sym.n; i++ ) {
        /* Add the length of the label */
        strtablen += strlen(code->sym.syms[i].label) + 1;
    }
    /* Align */
    strtablen = ((strtablen + 7) / 8) * 8;

    /* Allocate */
    strtab = alloca(strtablen);
    if ( NULL == strtab ) {
        return -1;
    }
    memset(strtab, 0, strtablen);
    nl = alloca(sizeof(struct nlist_64) * code->sym.n);
    if ( NULL == nl ) {
        return -1;
    }

    stroff = 1;
    for ( i = 0; i < code->sym.n; i++ ) {
        /* nlist_64: Describes an entry in the symbol table for 64-bit
           architectures. */
        nl[i].n_un.n_strx = stroff;
        nl[i].n_type = N_SECT | N_EXT;
        if ( code->sym.syms[i].type == ARCH_SYM_LOCAL ) {
            nl[i].n_sect = 0x03;
        } else {
            nl[i].n_sect = 0x01;
        }
        nl[i].n_desc = REFERENCE_FLAG_DEFINED;
        nl[i].n_value = code->sym.syms[i].pos;

        /* Symbol table */
        strcpy(strtab + stroff, code->sym.syms[i].label);
        stroff += strlen(code->sym.syms[i].label) + 1;
    }

    /* Calculate the offsets */
    ncmds = 3;
    nsects = 3;
    sizeofcmds = sizeof(struct segment_command_64)
        + sizeof(struct section_64) * nsects
        + sizeof(struct version_min_command) + sizeof(struct symtab_command);
    codepoint = sizeof(struct mach_header_64) + sizeofcmds;
    codepoint = ((codepoint + 15) / 16) * 16;
    codesize = ((code->size + 15) / 16) * 16;
    /* FIXME */
    symoff = codepoint + codesize + 8 + sizeof(struct relocation_info) * 1;

    /* Header */
    hdr.magic = MH_MAGIC_64;
    hdr.cputype = CPUTYPE_X86_64;
    hdr.cpusubtype = CPUSUBTYPE_X86_64;
    hdr.filetype = FILETYPE_OBJECT;
    hdr.ncmds = ncmds;
    hdr.sizeofcmds = sizeofcmds;
    hdr.flags = SUBSECTIONS_VIA_SYMBOLS;
    hdr.reserved = 0;

    /* Segment command */
    seg.cmd = LC_SEGMENT_64;
    seg.cmdsize = sizeof(struct segment_command_64)
        + sizeof(struct section_64) * nsects;
    memset(seg.segname, 0, 16);
    seg.vmaddr = 0;
    seg.vmsize = codesize + 8;
    seg.fileoff = codepoint;
    seg.filesize = codesize + 8;
    seg.maxprot = 0x07;
    seg.initprot = 0x07;
    seg.nsects = nsects;
    seg.flags = 0;

    /* Text section */
    memset(&sect_text, 0, sizeof(struct section_64));
    strcpy(sect_text.sectname, "__text");
    strcpy(sect_text.segname, "__TEXT");
    sect_text.addr = 0;
    sect_text.size = code->size;
    sect_text.offset = codepoint;
    sect_text.align = 4;
    sect_text.reloff = codepoint + codesize + 8;
    sect_text.nreloc = 1;
    sect_text.flags = S_ATTR_PURE_INSTRUCTIONS | S_ATTR_SOME_INSTRUCTIONS;
    sect_text.reserved1 = 0;
    sect_text.reserved2 = 0;
    sect_text.reserved3 = 0;

    /* Data section */
    memset(&sect_data, 0, sizeof(struct section_64));
    strcpy(sect_data.sectname, "__data");
    strcpy(sect_data.segname, "__DATA");
    sect_data.addr = codesize;
    sect_data.size = 0;
    sect_data.offset = 0;
    sect_data.align = 2;
    sect_data.reloff = 0;
    sect_data.nreloc = 0;
    sect_data.flags = S_REGULAR;
    sect_data.reserved1 = 0;
    sect_data.reserved2 = 0;
    sect_data.reserved3 = 0;

    /* BSS section */
    memset(&sect_bss, 0, sizeof(struct section_64));
    strcpy(sect_bss.sectname, "__bss");
    strcpy(sect_bss.segname, "__DATA");
    sect_bss.addr = codesize;
    sect_bss.size = 8;          /* FIXME */
    sect_bss.offset = 0;
    sect_bss.align = 3;
    sect_bss.reloff = 0;
    sect_bss.nreloc = 0;
    sect_bss.flags = S_ZEROFILL;
    sect_bss.reserved1 = 0;
    sect_bss.reserved2 = 0;
    sect_bss.reserved3 = 0;

    /* Version min command */
    vercmd.cmd = LC_VERSION_MIN_MACOSX;
    vercmd.cmdsize = sizeof(struct version_min_command);
    vercmd.version = 0x000a0c00;
    vercmd.sdk = 0;

    /* Symtab command */
    symtab.cmd = LC_SYMTAB;
    symtab.cmdsize = sizeof(struct symtab_command);
    symtab.symoff = symoff;
    symtab.nsyms = code->sym.n;
    symtab.stroff = symoff + code->sym.n * sizeof(struct nlist_64);
    symtab.strsize = strtablen;


    /* Write the header */
    nw = fwrite(&hdr, sizeof(struct mach_header_64), 1, fp);
    if ( nw != 1 ) {
        return -1;
    }

    /* Write the segment command */
    nw = fwrite(&seg, sizeof(struct segment_command_64), 1, fp);
    if ( nw != 1 ) {
        return -1;
    }

    /* Write the text section */
    nw = fwrite(&sect_text, sizeof(struct section_64), 1, fp);
    if ( nw != 1 ) {
        return -1;
    }

    /* Write the data section */
    nw = fwrite(&sect_data, sizeof(struct section_64), 1, fp);
    if ( nw != 1 ) {
        return -1;
    }

    /* Write the bss section */
    nw = fwrite(&sect_bss, sizeof(struct section_64), 1, fp);
    if ( nw != 1 ) {
        return -1;
    }

    /* Write the version command */
    nw = fwrite(&vercmd, sizeof(struct version_min_command), 1, fp);
    if ( nw != 1 ) {
        return -1;
    }

    /* Write the symbol table command */
    nw = fwrite(&symtab, sizeof(struct symtab_command), 1, fp);
    if ( nw != 1 ) {
        return -1;
    }

    /* Write the program code */
    fseeko(fp, codepoint, SEEK_SET);
    nw = fwrite(code->s, 1, code->size, fp);
    if ( nw != (ssize_t)code->size ) {
        return -1;
    }
    fseeko(fp, codepoint + codesize, SEEK_SET);

    /* Write the data/bss (FIXME) */
    uint8_t buf[8];
    memset(buf, 0, 8);
    nw = fwrite(buf, 1, 8, fp);
    if ( nw != (ssize_t)8 ) {
        return -1;
    }

    /* Write the relocation info */
    nw = fwrite(relocinfo, sizeof(struct relocation_info), 1, fp);
    if ( nw != 1 ) {
        return -1;
    }

    /* Write the symbol table */
    fseeko(fp, symoff, SEEK_SET);
    for ( i = 0; i < code->sym.n; i++ ) {
        nw = fwrite(&nl[i], sizeof(struct nlist_64), 1, fp);
        if ( nw != 1 ) {
            return -1;
        }
    }

    /* Write the symbol table string */
    fseeko(fp, symoff + code->sym.n * sizeof(struct nlist_64), SEEK_SET);
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
