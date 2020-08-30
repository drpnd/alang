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

#ifndef _ARCH_X86_64_REG_H
#define _ARCH_X86_64_REG_H

#define REG_GENERIC     0
#define REG_SEGMENT     1
#define REG_DEBUG       2

#define REG_CODE(r)     ((r) & 0x7)
#define REG_REX(r)      (((r) >> 3) & 0x1)
#define REG_REX0(r)     (((r) >> 4) & 0x1)
#define REG_NE(r)       (((r) >> 5) & 0x1)
#define REG_TYPE(r)      (((r) >> 8) & 0xff)
#define REG_SIZE(r)     (((r) >> 16) & 0xffff)

#define REG_ENCODE(code, rex, rex0, ne, size)                           \
    ((code) | ((rex) << 3) | ((rex0) << 4) | ((ne) << 5) | ((size) << 16))

#define SEG_ENCODE(code, size)  ((code) | (1 << 8))
#define DB_ENCODE(code, size)   ((code) | (2 << 8))

/*
 * x86-64 registers
 */
typedef enum {
    REG_NONE = 0,
    /* IP */
    //REG_EIP,
    //REG_RIP,
    /* AX */
    REG_AL = REG_ENCODE(0, 0, 0, 0, 8),
    REG_AH = REG_ENCODE(4, 0, 0, 1, 8),
    REG_AX = REG_ENCODE(0, 0, 0, 0, 16),
    REG_EAX = REG_ENCODE(0, 0, 0, 0, 32),
    REG_RAX = REG_ENCODE(0, 0, 0, 0, 64),
    /* CX */
    REG_CL = REG_ENCODE(1, 0, 0, 0, 8),
    REG_CH = REG_ENCODE(5, 0, 0, 1, 8),
    REG_CX = REG_ENCODE(1, 0, 0, 0, 16),
    REG_ECX = REG_ENCODE(1, 0, 0, 0, 32),
    REG_RCX = REG_ENCODE(1, 0, 0, 0, 64),
    /* DX */
    REG_DL = REG_ENCODE(2, 0, 0, 0, 8),
    REG_DH = REG_ENCODE(6, 0, 0, 1, 8),
    REG_DX = REG_ENCODE(2, 0, 0, 0, 16),
    REG_EDX = REG_ENCODE(2, 0, 0, 0, 32),
    REG_RDX = REG_ENCODE(2, 0, 0, 0, 64),
    /* BX */
    REG_BL = REG_ENCODE(3, 0, 0, 0, 8),
    REG_BH = REG_ENCODE(7, 0, 0, 1, 8),
    REG_BX = REG_ENCODE(3, 0, 0, 0, 16),
    REG_EBX = REG_ENCODE(3, 0, 0, 0, 32),
    REG_RBX = REG_ENCODE(3, 0, 0, 0, 64),
    /* SP */
    REG_SPL = REG_ENCODE(4, 0, 1, 0, 8),
    REG_SP = REG_ENCODE(4, 0, 0, 0, 16),
    REG_ESP = REG_ENCODE(4, 0, 0, 0, 32),
    REG_RSP = REG_ENCODE(4, 0, 0, 0, 64),
    /* BP */
    REG_BPL = REG_ENCODE(5, 0, 1, 0, 8),
    REG_BP = REG_ENCODE(5, 0, 0, 0, 16),
    REG_EBP = REG_ENCODE(5, 0, 0, 0, 32),
    REG_RBP = REG_ENCODE(5, 0, 0, 0, 64),
    /* SI */
    REG_SIL = REG_ENCODE(6, 0, 1, 0, 8),
    REG_SI = REG_ENCODE(6, 0, 0, 0, 16),
    REG_ESI = REG_ENCODE(6, 0, 0, 0, 32),
    REG_RSI = REG_ENCODE(6, 0, 0, 0, 64),
    /* DI */
    REG_DIL = REG_ENCODE(7, 0, 1, 0, 8),
    REG_DI = REG_ENCODE(7, 0, 0, 0, 16),
    REG_EDI = REG_ENCODE(7, 0, 0, 0, 32),
    REG_RDI = REG_ENCODE(7, 0, 0, 0, 64),
    /* R8 */
    REG_R8L = REG_ENCODE(0, 1, 0, 0, 8),
    REG_R8W = REG_ENCODE(0, 1, 0, 0, 16),
    REG_R8D = REG_ENCODE(0, 1, 0, 0, 32),
    REG_R8 = REG_ENCODE(0, 1, 0, 0, 64),
    /* R9 */
    REG_R9L = REG_ENCODE(1, 1, 0, 0, 8),
    REG_R9W = REG_ENCODE(1, 1, 0, 0, 16),
    REG_R9D = REG_ENCODE(1, 1, 0, 0, 32),
    REG_R9 = REG_ENCODE(1, 1, 0, 0, 64),
    /* R10 */
    REG_R10L = REG_ENCODE(2, 1, 0, 0, 8),
    REG_R10W = REG_ENCODE(2, 1, 0, 0, 16),
    REG_R10D = REG_ENCODE(2, 1, 0, 0, 32),
    REG_R10 = REG_ENCODE(2, 1, 0, 0, 64),
    /* R11 */
    REG_R11L = REG_ENCODE(3, 1, 0, 0, 8),
    REG_R11W = REG_ENCODE(3, 1, 0, 0, 16),
    REG_R11D = REG_ENCODE(3, 1, 0, 0, 32),
    REG_R11 = REG_ENCODE(3, 1, 0, 0, 64),
    /* R12 */
    REG_R12L = REG_ENCODE(4, 1, 0, 0, 8),
    REG_R12W = REG_ENCODE(4, 1, 0, 0, 16),
    REG_R12D = REG_ENCODE(4, 1, 0, 0, 32),
    REG_R12 = REG_ENCODE(4, 1, 0, 0, 64),
    /* R13 */
    REG_R13L = REG_ENCODE(5, 1, 0, 0, 8),
    REG_R13W = REG_ENCODE(5, 1, 0, 0, 16),
    REG_R13D = REG_ENCODE(5, 1, 0, 0, 32),
    REG_R13 = REG_ENCODE(5, 1, 0, 0, 64),
    /* R14 */
    REG_R14L = REG_ENCODE(6, 1, 0, 0, 8),
    REG_R14W = REG_ENCODE(6, 1, 0, 0, 16),
    REG_R14D = REG_ENCODE(6, 1, 0, 0, 32),
    REG_R14 = REG_ENCODE(6, 1, 0, 0, 64),
    /* R15 */
    REG_R15L = REG_ENCODE(7, 1, 0, 0, 8),
    REG_R15W = REG_ENCODE(7, 1, 0, 0, 16),
    REG_R15D = REG_ENCODE(7, 1, 0, 0, 32),
    REG_R15 = REG_ENCODE(7, 1, 0, 0, 64),
    /* Segment registers */
    REG_CS = SEG_ENCODE(1, 16),
    REG_DS = SEG_ENCODE(3, 16),
    REG_ES = SEG_ENCODE(0, 16),
    REG_FS = SEG_ENCODE(4, 16),
    REG_GS = SEG_ENCODE(5, 16),
    REG_SS = SEG_ENCODE(2, 16),
    /* Flags */
    //REG_FLAGS,
    //REG_EFLAGS,
    //REG_RFLAGS,
    /* ST */
    REG_ST0 = REG_ENCODE(0, 0, 0, 0, 80),
    REG_ST1 = REG_ENCODE(1, 0, 0, 0, 80),
    REG_ST2 = REG_ENCODE(2, 0, 0, 0, 80),
    REG_ST3 = REG_ENCODE(3, 0, 0, 0, 80),
    REG_ST4 = REG_ENCODE(4, 0, 0, 0, 80),
    REG_ST5 = REG_ENCODE(5, 0, 0, 0, 80),
    REG_ST6 = REG_ENCODE(6, 0, 0, 0, 80),
    REG_ST7 = REG_ENCODE(7, 0, 0, 0, 80),
    /* MM */
    REG_MM0 = REG_ENCODE(0, 0, 0, 0, 64),
    REG_MM1 = REG_ENCODE(1, 0, 0, 0, 64),
    REG_MM2 = REG_ENCODE(2, 0, 0, 0, 64),
    REG_MM3 = REG_ENCODE(3, 0, 0, 0, 64),
    REG_MM4 = REG_ENCODE(4, 0, 0, 0, 64),
    REG_MM5 = REG_ENCODE(5, 0, 0, 0, 64),
    REG_MM6 = REG_ENCODE(6, 0, 0, 0, 64),
    REG_MM7 = REG_ENCODE(7, 0, 0, 0, 64),
    /* XMM */
    REG_XMM0 = REG_ENCODE(0, 0, 0, 0, 128),
    REG_XMM1 = REG_ENCODE(1, 0, 0, 0, 128),
    REG_XMM2 = REG_ENCODE(2, 0, 0, 0, 128),
    REG_XMM3 = REG_ENCODE(3, 0, 0, 0, 128),
    REG_XMM4 = REG_ENCODE(4, 0, 0, 0, 128),
    REG_XMM5 = REG_ENCODE(5, 0, 0, 0, 128),
    REG_XMM6 = REG_ENCODE(6, 0, 0, 0, 128),
    REG_XMM7 = REG_ENCODE(7, 0, 0, 0, 128),
    REG_XMM8 = REG_ENCODE(0, 1, 0, 0, 128),
    REG_XMM9 = REG_ENCODE(1, 1, 0, 0, 128),
    REG_XMM10 = REG_ENCODE(2, 1, 0, 0, 128),
    REG_XMM11 = REG_ENCODE(3, 1, 0, 0, 128),
    REG_XMM12 = REG_ENCODE(4, 1, 0, 0, 128),
    REG_XMM13 = REG_ENCODE(5, 1, 0, 0, 128),
    REG_XMM14 = REG_ENCODE(6, 1, 0, 0, 128),
    REG_XMM15 = REG_ENCODE(7, 1, 0, 0, 128),
    /* YMM */
    REG_YMM0 = REG_ENCODE(0, 0, 0, 0, 256),
    REG_YMM1 = REG_ENCODE(1, 0, 0, 0, 256),
    REG_YMM2 = REG_ENCODE(2, 0, 0, 0, 256),
    REG_YMM3 = REG_ENCODE(3, 0, 0, 0, 256),
    REG_YMM4 = REG_ENCODE(4, 0, 0, 0, 256),
    REG_YMM5 = REG_ENCODE(5, 0, 0, 0, 256),
    REG_YMM6 = REG_ENCODE(6, 0, 0, 0, 256),
    REG_YMM7 = REG_ENCODE(7, 0, 0, 0, 256),
    REG_YMM8 = REG_ENCODE(0, 1, 0, 0, 256),
    REG_YMM9 = REG_ENCODE(1, 1, 0, 0, 256),
    REG_YMM10 = REG_ENCODE(2, 1, 0, 0, 256),
    REG_YMM11 = REG_ENCODE(3, 1, 0, 0, 256),
    REG_YMM12 = REG_ENCODE(4, 1, 0, 0, 256),
    REG_YMM13 = REG_ENCODE(5, 1, 0, 0, 256),
    REG_YMM14 = REG_ENCODE(6, 1, 0, 0, 256),
    REG_YMM15 = REG_ENCODE(7, 1, 0, 0, 256),
} x86_64_reg_t;

#endif /* _ARCH_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
