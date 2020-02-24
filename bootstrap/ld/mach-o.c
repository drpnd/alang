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

#include <stdint.h>

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
mach_o_export(void)
{
    struct mach_header_64 hdr;
    int sizeofcmds;
    int ncmds;

    /* Example */
    ncmds = 4;
    sizeofcmds = 0x1b0;

    /* Header */
    hdr.magic = MH_MAGIC_64;
    hdr.cputype = CPUTYPE_X86_64;
    hdr.cpusubtype = CPUSUBTYPE_X86_64;
    hdr.filetype = FILETYPE_OBJECT;
    hdr.ncmds = ncmds;
    hdr.sizeofcmds = sizeofcmds;
    hdr.flags = SUBSECTIONS_VIA_SYMBOLS;
    hdr.reserved = 0;

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
