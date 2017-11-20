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

#ifndef _ITYPE_H
#define _ITYPE_H

#include <stdint.h>
#include <inttypes.h>

/* Internal type: 64 bit integer */
typedef uint64_t int_t;
#define AL_PRId         PRId64
#define AL_PRIu         PRIu64
#define AL_ITYPE_INT 64
/* Internal type: FP80 */
typedef long double fp_t;
#define AL_ITYPE_FP     80
#define AL_PRIf         "Lf"

#if __LP64__ || __LLP64__ || __ILP64__
#define AL_ITYPE_PTR    64
#elif __LP32__
#define AL_ITYPE_PTR    32
#else
#error "Unsupported pointer type."
#endif

/*
 * Literals
 */
typedef struct {
    unsigned char *s;
    int_t len;
} al_string_t;

#endif /* _ITYPE_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
