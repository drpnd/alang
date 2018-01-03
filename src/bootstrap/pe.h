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

#ifndef _PE_H
#define _PE_H

#include <stdio.h>
#include <stdint.h>

/*
 * MZ header, MZ is the MS-DOS exe format starting with "MZ" as its signature.
 */
typedef struct _IMAGE_DOS_HEADER {
    /* Signature */
    uint16_t e_magic;         /* "MZ" 0x5a4d */
    /* # of bytes in the last page */
    uint16_t e_cblp;
    /* # of whole/partial pages (in 512-byte paging) */
    uint16_t e_cp;
    /* # of relocation entries in the relocation table */
    uint16_t e_crlc;
    /* # of paragraphs taken up by the header */
    uint16_t e_cparhdr;
    /* # of paragraphs required by the program */
    uint16_t e_minalloc;
    /* # of paragraphs requested by the program */
    uint16_t e_maxalloc;
    /* Relocatable segment address for %ss */
    uint16_t e_ss;
    /* Initial value for %sp */
    uint16_t e_sp;
    /* Checksum so that the sum of all words in the file are zero */
    uint16_t e_csum;
    /* Initial value for %ip */
    uint16_t e_ip;
    /* Relocatable segment address for %cs */
    uint16_t e_cs;
    /* The absolute offset to the relocation table */
    uint16_t e_lfarlc;
    /* Overlay number */
    uint16_t e_ovno;
    uint16_t e_res[4];
    /* OEM identifier */
    uint16_t e_oemid;
    /* OEM information */
    uint16_t e_oeminfo;
    uint16_t e_res2[10];
    /* File address of new exe header */
    uint32_t e_lfanew;
} __attribute__((packed)) IMAGE_DOS_HEADER;


#define IMAGE_DOS_MAGIC 0x5a4d
#define IMAGE_NT_MAGIC  0x00004550

#define IMAGE_FILE_MACHINE_I386 0x014c
#define IMAGE_FILE_MACHINE_IA64 0x0200
#define IMAGE_FILE_MACHINE_AMD64 0x8664

#define IMAGE_FILE_RELOCS_STRIPPED      0x0001
#define IMAGE_FILE_EXECUTABLE_IMAGE     0x0002
#define IMAGE_FILE_LINE_NUMS_STRIPPED   0x0004
#define IMAGE_FILE_LOCAL_SYMS_STRIPPED  0x0008
#define IMAGE_FILE_AGGRESIVE_WS_TRIM    0x0010
#define IMAGE_FILE_LARGE_ADDRESS_AWARE  0x0020
#define IMAGE_FILE_BYTES_REVERSED_LO    0x0080
#define IMAGE_FILE_32BIT_MACHINE        0x0100
#define IMAGE_FILE_DEBUG_STRIPPED       0x0200
#define IMAGE_FILE_REMOVABLE_RUN_FROM_SWAP      0x0400
#define IMAGE_FILE_NET_RUN_FROM_SWAP    0x0800
#define IMAGE_FILE_SYSTEM               0x1000
#define IMAGE_FILE_DLL                  0x2000
#define IMAGE_FILE_UP_SYSTEM_ONLY       0x4000
#define IMAGE_FILE_BYTES_REVERSED_HI    0x8000

#define IMAGE_NT_OPTIONAL_HDR32_MAGIC   0x10b
#define IMAGE_NT_OPTIONAL_HDR64_MAGIC   0x20b
#define IMAGE_ROM_OPTIONAL_HDR_MAGIC    0x107

#define IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE 0x0040
#define IMAGE_DLLCHARACTERISTICS_FORCE_INTEGRITY 0x0080
#define IMAGE_DLLCHARACTERISTICS_NX_COMPAT 0x0100
#define IMAGE_DLLCHARACTERISTICS_NO_ISOLATION 0x0200
#define IMAGE_DLLCHARACTERISTICS_NO_SEH 0x0400
#define IMAGE_DLLCHARACTERISTICS_NO_BIND 0x0800
#define IMAGE_DLLCHARACTERISTICS_WDM_DRIVER 0x2000
#define IMAGE_DLLCHARACTERISTICS_TERMINAL_SERVER_AWARE 0x8000

/*
 * File header
 */
typedef struct _IMAGE_FILE_HEADER {
    /* IMAGE_FILE_MACHINE_XXX */
    uint16_t Machine;
    uint16_t NumberOfSections;
    uint32_t TimeDateStamp;
    uint32_t PointerToSymbolTable;
    uint32_t NumberOfSymbols;
    uint16_t SizeOfOptionalHeader;
    uint16_t Characteristics;
} __attribute__((packed)) IMAGE_FILE_HEADER;

/*
 * Image data directory
 * 0: Export table address and size
 * 1: Import table address and size
 * 2: Resource table address and size
 * 3: Exception table address and size
 * 4: Certificate table address and size
 * 5: Base relocation table address and size
 * 6: Debugging information starting address and size
 * 7: Architecture-specific data address and size
 * 8: Global pointer register relative virtual address
 * 9: Thread local storage (TLS) table address and size
 * 10: Load configuration table address and size
 * 11: Bound import table address and size
 * 12: Import address table address and size
 * 13: Delay import descriptor address and size
 * 14: The CLR header address and size
 */
typedef struct _IMAGE_DATA_DIRECTORY {
    uint32_t VirtualAddress;
    uint32_t Size;
} __attribute__ ((packed)) IMAGE_DATA_DIRECTORY;

/*
 * Optional header (32-bit)
 */
typedef struct _IMAGE_OPTIONAL_HEADER {
    uint16_t Magic; // 0x010b - PE32, 0x020b - PE32+ (64 bit)
    uint8_t  MajorLinkerVersion;
    uint8_t  MinorLinkerVersion;
    uint32_t SizeOfCode;
    uint32_t SizeOfInitializedData;
    uint32_t SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
    uint32_t BaseOfData;
    uint32_t ImageBase;
    uint32_t SectionAlignment;
    uint32_t FileAlignment;
    uint16_t MajorOperatingSystemVersion;
    uint16_t MinorOperatingSystemVersion;
    uint16_t MajorImageVersion;
    uint16_t MinorImageVersion;
    uint16_t MajorSubsystemVersion;
    uint16_t MinorSubsystemVersion;
    uint32_t Win32VersionValue;
    uint32_t SizeOfImage;
    uint32_t SizeOfHeaders;
    /* Image file checksum */
    uint32_t CheckSum;
    uint16_t Subsystem;
    uint16_t DllCharacteristics;
    uint32_t SizeOfStackReserve;
    uint32_t SizeOfStackCommit;
    uint32_t SizeOfHeapReserve;
    uint32_t SizeOfHeapCommit;
    uint32_t LoaderFlags;
    uint32_t NumberOfRvaAndSizes;
    IMAGE_DATA_DIRECTORY DataDirectory[0];
} __attribute__((packed)) IMAGE_OPTIONAL_HEADER;

/*
 * Optional header (64-bit)
 */
typedef struct _IMAGE_OPTIONAL_HEADER64 {
    uint16_t Magic; // 0x010b - PE32, 0x020b - PE32+ (64 bit)
    uint8_t  MajorLinkerVersion;
    uint8_t  MinorLinkerVersion;
    uint32_t SizeOfCode;
    uint32_t SizeOfInitializedData;
    uint32_t SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
    uint64_t ImageBase;
    uint32_t SectionAlignment;
    uint32_t FileAlignment;
    uint16_t MajorOperatingSystemVersion;
    uint16_t MinorOperatingSystemVersion;
    uint16_t MajorImageVersion;
    uint16_t MinorImageVersion;
    uint16_t MajorSubsystemVersion;
    uint16_t MinorSubsystemVersion;
    uint32_t Win32VersionValue;
    uint32_t SizeOfImage;
    uint32_t SizeOfHeaders;
    uint32_t CheckSum;
    uint16_t Subsystem;
    uint16_t DllCharacteristics;
    uint64_t SizeOfStackReserve;
    uint64_t SizeOfStackCommit;
    uint64_t SizeOfHeapReserve;
    uint64_t SizeOfHeapCommit;
    uint32_t LoaderFlags;
    uint32_t NumberOfRvaAndSizes;
    IMAGE_DATA_DIRECTORY DataDirectory[0];
} __attribute__((packed)) IMAGE_OPTIONAL_HEADER64;

/*
 * NT header (32-bit)
 */
typedef struct _IMAGE_NT_HEADER {
    /* Signature */
    uint32_t Signature;         /* PE\0\0 */
    /* File header */
    IMAGE_FILE_HEADER FileHeader;
    /* Optional header */
    IMAGE_OPTIONAL_HEADER OptionalHeader;
} __attribute__((packed)) IMAGE_NT_HEADER;

/*
 * NT header (64-bit)
 */
typedef struct _IMAGE_NT_HEADER64 {
    /* Signature */
    uint32_t Signature;         /* PE\0\0 */
    /* File header */
    IMAGE_FILE_HEADER FileHeader;
    /* Optional header */
    IMAGE_OPTIONAL_HEADER64 OptionalHeader;
} __attribute__((packed)) IMAGE_NT_HEADER64;

#ifdef __cplusplus
extern "C" {
#endif

    void pe_out(void);

#ifdef __cplusplus
}
#endif

#endif /* _PE_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
