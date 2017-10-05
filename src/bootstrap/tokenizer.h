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

#ifndef _TOKENIZER_H
#define _TOKENIZER_H

#include "token.h"
#include <stdint.h>
#include <unistd.h>

/* Invalid token */
#define AL_EINVALTOK    1

/*
 * Tokenizer
 */
typedef struct _tokenizer al_tokenizer_t;
struct _tokenizer {
    char *buf;
    size_t sz;
    off_t off;
    int (*cur)(al_tokenizer_t *);
    int (*next)(al_tokenizer_t *);
    int _allocated;

    /* Tokenized list of tokens */
    al_token_list_t *tokens;
};

#ifdef __cplusplus
extern "C" {
#endif

    al_tokenizer_t * tokenizer_init(al_tokenizer_t *, char *, size_t);
    void tokenizer_release(al_tokenizer_t *);
    al_token_list_t * tokenizer_tokenize(char *);

    void token_release(al_token_t *);
    void token_list_release(al_token_list_t *);

#ifdef __cplusplus
}
#endif

#endif /* _TOKENIZER_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
