/*_
 * Copyright (c) 2017 Hirochika Asai <asai@jar.jp>
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

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "pe.h"


struct mz_header {
    /* Signature */
    uint16_t signature;         /* "MZ "0x5a4d */
    /* # of bytes in the last page */
    uint16_t extra_bytes;
    /* # of whole/partial pages (in 512-byte paging) */
    uint16_t pages;
    /* # of relocation entries in the relocation table */
    uint16_t relocations;
    /* # of paragraphs taken up by the header */
    uint16_t header_size;
    /* # of paragraphs required by the program */
    uint16_t min_allocation;
    /* # of paragraphs requested by the program */
    uint16_t max_allocation;
    /* Relocatable segment address for %ss */
    uint16_t init_ss;
    /* Initial value for %sp */
    uint16_t init_sp;
    /* Checksum so that the sum of all words in the file are zero */
    uint16_t checksum;
    /* Initial value for %ip */
    uint16_t init_ip;
    /* Relocatable segment address for %cs */
    uint16_t init_cs;
    /* The absolute offset to the relocation table */
    uint16_t relocation_table;
    /* Overlay number */
    uint16_t overlay;
    uint16_t reserved[4];
    /* OEM identifier */
    uint16_t oem_id;
    /* OEM information */
    uint16_t oem_info;
    uint16_t reserved2[10];
    /* File address of new exe header */
    uint32_t e_lfanew;
} __attribute__ ((packed));



struct PeHeader {
    uint32_t mMagic; /* PE\0\0 or 0x00004550 */
    uint16_t mMachine;
    uint16_t mNumberOfSections;
    uint32_t mTimeDateStamp;
    uint32_t mPointerToSymbolTable;
    uint32_t mNumberOfSymbols;
    uint16_t mSizeOfOptionalHeader;
    uint16_t mCharacteristics;
} __attribute__ ((packed));

struct Pe32OptionalHeader {
    uint16_t mMagic; // 0x010b - PE32, 0x020b - PE32+ (64 bit)
    uint8_t  mMajorLinkerVersion;
    uint8_t  mMinorLinkerVersion;
    uint32_t mSizeOfCode;
    uint32_t mSizeOfInitializedData;
    uint32_t mSizeOfUninitializedData;
    uint32_t mAddressOfEntryPoint;
    uint32_t mBaseOfCode;
    uint32_t mBaseOfData;
    uint32_t mImageBase;
    uint32_t mSectionAlignment;
    uint32_t mFileAlignment;
    uint16_t mMajorOperatingSystemVersion;
    uint16_t mMinorOperatingSystemVersion;
    uint16_t mMajorImageVersion;
    uint16_t mMinorImageVersion;
    uint16_t mMajorSubsystemVersion;
    uint16_t mMinorSubsystemVersion;
    uint32_t mWin32VersionValue;
    uint32_t mSizeOfImage;
    uint32_t mSizeOfHeaders;
    uint32_t mCheckSum;
    uint16_t mSubsystem;
    uint16_t mDllCharacteristics;
    uint32_t mSizeOfStackReserve;
    uint32_t mSizeOfStackCommit;
    uint32_t mSizeOfHeapReserve;
    uint32_t mSizeOfHeapCommit;
    uint32_t mLoaderFlags;
    uint32_t mNumberOfRvaAndSizes;
} __attribute__ ((packed));

struct Pe64OptionalHeader {
    uint16_t mMagic; // 0x010b - PE32, 0x020b - PE32+ (64 bit)
    uint8_t  mMajorLinkerVersion;
    uint8_t  mMinorLinkerVersion;
    uint32_t mSizeOfCode;
    uint32_t mSizeOfInitializedData;
    uint32_t mSizeOfUninitializedData;
    uint32_t mAddressOfEntryPoint;
    uint32_t mBaseOfCode;
    uint32_t mBaseOfData;
    uint64_t mImageBase;
    uint32_t mSectionAlignment;
    uint32_t mFileAlignment;
    uint16_t mMajorOperatingSystemVersion;
    uint16_t mMinorOperatingSystemVersion;
    uint16_t mMajorImageVersion;
    uint16_t mMinorImageVersion;
    uint16_t mMajorSubsystemVersion;
    uint16_t mMinorSubsystemVersion;
    uint32_t mWin32VersionValue;
    uint32_t mSizeOfImage;
    uint32_t mSizeOfHeaders;
    uint32_t mCheckSum;
    uint16_t mSubsystem;
    uint16_t mDllCharacteristics;
    uint64_t mSizeOfStackReserve;
    uint64_t mSizeOfStackCommit;
    uint64_t mSizeOfHeapReserve;
    uint64_t mSizeOfHeapCommit;
    uint32_t mLoaderFlags;
    uint32_t mNumberOfRvaAndSizes;
} __attribute__ ((packed));


typedef struct _IMAGE_SECTION_HEADER {
    char Name[8];
    union {
        uint32_t PhysicalAddress;
        uint32_t VirtualSize;
    } Misc;
    uint32_t VirtualAddress;
    uint32_t SizeOfRawData;
    uint32_t PointerToRawData;
    uint32_t PointerToRealocations;
    uint32_t PointerToLinenumbers;
    uint16_t NumberOfRealocations;
    uint16_t NumberOfLinenumbers;
    uint32_t Characteristics;
} __attribute__((packed)) IMAGE_SECTION_HEADER;


static uint8_t pe_standard_mz[] = {
    0x4d, 0x5a, 0x80, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x10, 0x00, 0xff, 0xff, 0x00, 0x00,
    0x40, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00,

    0x0e, 0x1f, 0xba, 0x0e, 0x00, 0xb4, 0x09, 0xcd,
    0x21, 0xb8, 0x01, 0x4c, 0xcd, 0x21, 0x54, 0x68,
    0x69, 0x73, 0x20, 0x70, 0x72, 0x6f, 0x67, 0x72,
    0x61, 0x6d, 0x20, 0x63, 0x61, 0x6e, 0x6e, 0x6f,
    0x74, 0x20, 0x62, 0x65, 0x20, 0x72, 0x75, 0x6e,
    0x20, 0x69, 0x6e, 0x20, 0x44, 0x4f, 0x53, 0x20,
    0x6d, 0x6f, 0x64, 0x65, 0x2e, 0x0d, 0x0a, 0x24,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

void
pe_out(void)
{
    IMAGE_DOS_HEADER mz;
    IMAGE_NT_HEADER64 nt;
    static uint8_t mz_binary[] = {
        0x0e, 0x1f, 0xba, 0x0e, 0x00, 0xb4, 0x09, 0xcd,
        0x21, 0xb8, 0x01, 0x4c, 0xcd, 0x21, 0x54, 0x68,
        0x69, 0x73, 0x20, 0x70, 0x72, 0x6f, 0x67, 0x72,
        0x61, 0x6d, 0x20, 0x63, 0x61, 0x6e, 0x6e, 0x6f,
        0x74, 0x20, 0x62, 0x65, 0x20, 0x72, 0x75, 0x6e,
        0x20, 0x69, 0x6e, 0x20, 0x44, 0x4f, 0x53, 0x20,
        0x6d, 0x6f, 0x64, 0x65, 0x2e, 0x0d, 0x0a, 0x24,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

    /* Code length */
    int codelen = 0x200;

    /* Build MZ header and contents (hard-coded) */
    mz.e_magic = IMAGE_DOS_MAGIC;
    mz.e_cblp = 0x80;
    mz.e_cp = 1;
    mz.e_crlc = 0;
    mz.e_cparhdr = 4;
    mz.e_minalloc = 0x10;
    mz.e_maxalloc = 0xffff;
    mz.e_ss = 0;
    mz.e_sp = 0x140;
    mz.e_csum = 0;
    mz.e_ip = 0;
    mz.e_cs = 0;
    mz.e_lfarlc = 0x40;
    mz.e_ovno = 0;
    memset(mz.e_res, 0, sizeof(uint16_t) * 4);
    mz.e_oemid = 0;
    mz.e_oeminfo = 0;
    memset(mz.e_res2, 0, sizeof(uint16_t) * 10);
    mz.e_lfanew = 0x80;

    /* Build NT header */
    nt.Signature = IMAGE_NT_MAGIC;
    nt.FileHeader.Machine = IMAGE_FILE_MACHINE_AMD64;
    nt.FileHeader.NumberOfSections = 5;
    nt.FileHeader.TimeDateStamp = time(NULL);
    nt.FileHeader.PointerToSymbolTable = 0;
    nt.FileHeader.NumberOfSymbols = 0;
    nt.FileHeader.SizeOfOptionalHeader = 0x0f;
    nt.FileHeader.Characteristics = IMAGE_FILE_DLL
        | IMAGE_FILE_LARGE_ADDRESS_AWARE
        | IMAGE_FILE_LOCAL_SYMS_STRIPPED
        | IMAGE_FILE_LINE_NUMS_STRIPPED
        | IMAGE_FILE_EXECUTABLE_IMAGE;

    /* Optional header */
    nt.OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
    nt.OptionalHeader.MajorLinkerVersion = 1;
    nt.OptionalHeader.MinorLinkerVersion = 1;
    nt.OptionalHeader.SizeOfCode = codelen;
    nt.OptionalHeader.SizeOfInitializedData = 0x0600;
    nt.OptionalHeader.SizeOfUninitializedData = 0x0000;
    nt.OptionalHeader.AddressOfEntryPoint = 0x2018;
    nt.OptionalHeader.BaseOfCode = 0x1000;
    nt.OptionalHeader.ImageBase = 0x00400000ULL;
    nt.OptionalHeader.SectionAlignment = 0x1000;
    nt.OptionalHeader.FileAlignment = 0x0200;
    nt.OptionalHeader.MajorOperatingSystemVersion = 1;
    nt.OptionalHeader.MinorOperatingSystemVersion = 0;
    nt.OptionalHeader.MajorImageVersion = 0;
    nt.OptionalHeader.MinorImageVersion = 0;
    nt.OptionalHeader.MajorSubsystemVersion = 5;
    nt.OptionalHeader.MinorSubsystemVersion = 0;
    nt.OptionalHeader.Win32VersionValue = 0;
    nt.OptionalHeader.SizeOfImage = 0x5000;
    nt.OptionalHeader.SizeOfHeaders = 0x0400;
    nt.OptionalHeader.CheckSum = 0x0000d106;
    nt.OptionalHeader.Subsystem = 0x0a;
    nt.OptionalHeader.DllCharacteristics
        = IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE;
    nt.OptionalHeader.SizeOfStackReserve = 0x1000;
    nt.OptionalHeader.SizeOfStackCommit = 0x1000;
    nt.OptionalHeader.SizeOfHeapReserve = 0x10000;
    nt.OptionalHeader.SizeOfHeapCommit = 0x0000;
    nt.OptionalHeader.LoaderFlags = 0;
    nt.OptionalHeader.NumberOfRvaAndSizes = 0x0010;




    fwrite(&mz, sizeof(IMAGE_DOS_HEADER), 1, stdout);
    fwrite(mz_binary, 1, sizeof(mz_binary), stdout);
    fwrite(&nt, sizeof(IMAGE_NT_HEADER64), 1, stdout);





    struct PeHeader pe;
    struct Pe64OptionalHeader peopt;


    pe.mMagic = 0x00004550;
    pe.mMachine = IMAGE_FILE_MACHINE_AMD64;
    pe.mNumberOfSections = 0x0005;
    pe.mTimeDateStamp = time(NULL);
    pe.mPointerToSymbolTable = 0;
    pe.mNumberOfSymbols = 0;
    pe.mSizeOfOptionalHeader = 0x0f;
    pe.mCharacteristics = IMAGE_FILE_DLL
        | IMAGE_FILE_LARGE_ADDRESS_AWARE
        | IMAGE_FILE_LOCAL_SYMS_STRIPPED
        | IMAGE_FILE_LINE_NUMS_STRIPPED
        | IMAGE_FILE_EXECUTABLE_IMAGE;

    peopt.mMagic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
    peopt.mMajorLinkerVersion = 1;
    peopt.mMinorLinkerVersion = 1;
    peopt.mSizeOfCode = codelen;
    peopt.mSizeOfInitializedData = 0x0600;
    peopt.mSizeOfUninitializedData = 0x0000;
    peopt.mAddressOfEntryPoint = 0x2018;
    peopt.mBaseOfCode = 0x1000;
    peopt.mBaseOfData = 0x400000;
    peopt.mImageBase = 0x100000000ULL;
    peopt.mSectionAlignment = 0;
    peopt.mFileAlignment = 0x0200;
    peopt.mMajorOperatingSystemVersion = 1;
    peopt.mMinorOperatingSystemVersion = 0;
    peopt.mMajorImageVersion = 0;
    peopt.mMinorImageVersion = 0;
    peopt.mMajorSubsystemVersion = 5;
    peopt.mMinorSubsystemVersion = 0;
    peopt.mWin32VersionValue = 0;
    peopt.mSizeOfImage = 0x5000;
    peopt.mSizeOfHeaders = 0x0400;
    peopt.mCheckSum = 0x0000d106;
    peopt.mSubsystem = 0x0a;
    peopt.mDllCharacteristics = 0x40;
    peopt.mSizeOfStackReserve = 0x1000;
    peopt.mSizeOfStackCommit = 0x0000;
    peopt.mSizeOfHeapReserve = 0x10000;
    peopt.mSizeOfHeapCommit = 0x0000;
    peopt.mLoaderFlags = 0x10000;
    peopt.mNumberOfRvaAndSizes = 0;

    //fwrite(pe_standard_mz, 1, sizeof(pe_standard_mz), stdout);


}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */

