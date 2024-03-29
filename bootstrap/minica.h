/*_
 * Copyright (c) 2022 Hirochika Asai <asai@jar.jp>
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

#ifndef _MINICA_H
#define _MINICA_H

/*
 * Compiler
 */
typedef struct {
    int (*compile)(void *);
} minica_compiler_t;

/*
 * Assembler operations
 */
typedef struct {
    int (*assemble)(void *);
} assembler_ops_t;

/*
 * Assembler
 */
typedef struct {
    void *spec;
    assembler_ops_t ops;
} assembler_t;

/*
 * Loader operations
 */
typedef struct {
    int (*export)(void *);
} ld_ops_t;

/*
 * Linker
 */
typedef struct {
    void *spec;
    ld_ops_t ops;
} linker_t;

#ifdef __cplusplus
extern "C" {
#endif

    st_t * minica_parse(FILE *);

#ifdef __cplusplus
}
#endif

#endif /* _MINICA_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
