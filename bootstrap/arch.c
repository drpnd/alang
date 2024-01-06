/*_
 * Copyright (c) 2023-2024 Hirochika Asai <asai@jar.jp>
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

#include "arch.h"
#include <stdlib.h>

/*
 * arch_init -- initialize the architecture-specific data structure
 */
arch_t *
arch_init(arch_cpu_t cpu, arch_loader_t loader)
{
    arch_t *arch;

    arch = malloc(sizeof(arch_t));
    if ( arch == NULL ) {
        return NULL;
    }
    arch->cpu = cpu;
    arch->loader = loader;

    switch ( cpu ) {
    case ARCH_CPU_AARCH64:
        break;
    case ARCH_CPU_X86_64:
        break;
    default:
        free(arch);
        return NULL;
    }

    switch ( loader ) {
    case ARCH_LD_ELF:
        arch->export = elf_export;
        break;
    case ARCH_LD_MACH_O:
        arch->export = mach_o_export;
        break;
    default:
        free(arch);
        return NULL;
    }

    arch->assemble = NULL;
    arch->export = NULL;

    return arch;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
