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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define OPCODE_REXW         0x101
#define OPCODE_DIGIT_PREFIX 0x200
#define OPCODE_REGISTER     0x300
#define OPCODE_CB           0x401
#define OPCODE_CW           0x402
#define OPCODE_CD           0x404
#define OPCODE_CP           0x406
#define OPCODE_CO           0x408
#define OPCODE_CT           0x40a
#define OPCODE_IB           0x501
#define OPCODE_IW           0x502
#define OPCODE_ID           0x504
#define OPCODE_IO           0x508
#define OPCODE_RB           0x601
#define OPCODE_RW           0x602
#define OPCODE_RD           0x604
#define OPCODE_RO           0x608
#define OPCODE_ST_PREFIX    0x700

/*
 * Opcode
 */
struct opcode {
    size_t size;
    int rexw;
    int opcode[8];
};

/*
 * Rule tuple
 */
struct rule {
    struct opcode op;
};

int
_parse_opcode(const char *token)
{
    int ret;

    if ( 0 == strcasecmp("W", token) ) {
        /* REX.W */
        return OPCODE_REXW;
    } else if ( '/' == *token ) {
        token++;
        if ( 'r' == *token ) {
            ret = OPCODE_REGISTER;
        } else if ( '0' <= *token && *token <= '7' ) {
            ret = OPCODE_DIGIT_PREFIX + (*token - '0');
        }
        if ( '\0' != *token ) {
            return -1;
        }
        return ret;
    } else if ( 0 == strcasecmp("cb", token) ) {
        return OPCODE_CB;
    } else if ( 0 == strcasecmp("cw", token) ) {
        return OPCODE_CW;
    } else if ( 0 == strcasecmp("cd", token) ) {
        return OPCODE_CD;
    } else if ( 0 == strcasecmp("cp", token) ) {
        return OPCODE_CP;
    } else if ( 0 == strcasecmp("co", token) ) {
        return OPCODE_CO;
    } else if ( 0 == strcasecmp("ct", token) ) {
        return OPCODE_CT;
    } else if ( 0 == strcasecmp("ib", token) ) {
        return OPCODE_IB;
    } else if ( 0 == strcasecmp("iw", token) ) {
        return OPCODE_IW;
    } else if ( 0 == strcasecmp("id", token) ) {
        return OPCODE_ID;
    } else if ( 0 == strcasecmp("io", token) ) {
        return OPCODE_IO;
    } else if ( '+' == *token ) {
        token++;
        if ( 0 == strcasecmp("rb", token ) ) {
            return OPCODE_RB;
        } else if ( 0 == strcasecmp("rw", token ) ) {
            return OPCODE_RW;
        } else if ( 0 == strcasecmp("rd", token ) ) {
            return OPCODE_RD;
        } else if ( 0 == strcasecmp("ro", token ) ) {
            return OPCODE_RO;
        } else if ( '0' <= *token && *token <= '7' ) {
            ret = OPCODE_ST_PREFIX + (*token - '0');
            token++;
            if ( '\0' != *token ) {
                return -1;
            }
            return ret;
        } else {
            return -1;
        }
    }

    return -1;
}

int
_parse_operand(const char *token)
{
    return -1;
}

/*
 * Parse an instruction definition file
 */
int
instr_parse_file(FILE *fp)
{
    char buf[1024];
    while ( feof(fp) ) {
        if ( NULL == fgets(buf, sizeof(buf), fp) ) {
            continue;
        }
        /* Parse this line */
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
