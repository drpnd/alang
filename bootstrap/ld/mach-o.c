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

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
