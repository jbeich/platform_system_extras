// From http://people.freebsd.org/~gonzo/arm/patches/fdt-overlays-20150723.diff
/*-
 * Copyright (c) 2015 Oleksandr Tymoshenko <gonzo@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef FDT_OVERLAY_H
#define FDT_OVERLAY_H

#include "libdtoverlay_sysdeps.h"
#include "libufdt.h"

/*
 * Performs apply_overlay with more extra memory via libufdt, which is faster
 * than using libfdt.
 */
struct fdt_header *apply_overlay_libufdt(struct fdt_header *main_fdt_header,
                                         size_t main_fdt_size,
                                         void *overlay_fdtp,
                                         size_t overlay_size);

/*
 * Performs apply_overlay with less memory via libfdt, which is slower than
 * using libufdt.
 */
struct fdt_header *apply_overlay_libfdt(struct fdt_header *main_fdt_header,
                                        size_t main_fdt_size,
                                        void *overlay_fdtp,
                                        size_t overlay_size);

/* Given a buffer in RAM containing the contents of a .dtb file,
 * it initializes an FDT in-place and returns a pointer to the
 * given buffer, or NULL in case of error.
 * In case of error, it may printf() diagnostic messages.
 */
struct fdt_header *fdt_install_blob(void *blob, size_t blob_size);

/* Given a main_fdt_header buffer and an overlay_fdtp buffer containing the
 * contents of a .dtbo file, it creates a new FDT containing the applied
 * overlay_fdtp in a dto_malloc'd buffer and returns it, or NULL in case of
 * error.
 * It is allowed to modify the buffers (both main_fdt_header and overlay_fdtp
 * buffer) passed in.
 * It does not dto_free main_fdt_header and overlay_fdtp buffer passed in.
 */

static struct fdt_header *apply_overlay(struct fdt_header *main_fdt_header,
                                        size_t main_fdt_size,
                                        void *overlay_fdtp,
                                        size_t overlay_size) {
#ifdef LOW_MEMORY_USE
  return apply_overlay_libfdt(main_fdt_header, main_fdt_size, overlay_fdtp,
                              overlay_size);
#else
  /* Default */
  return apply_overlay_libufdt(main_fdt_header, main_fdt_size, overlay_fdtp,
                               overlay_size);
#endif
}

#endif /* FDT_OVERLAY_H */
