/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Damien P. George
 * Copyright (c) 2019 by Dan Halbert for Adafruit Industries
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
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef MICROPY_INCLUDED_SUPERVISOR_SHARED_VFS_H
#define MICROPY_INCLUDED_SUPERVISOR_SHARED_VFS_H

#include "py/lexer.h"
#include "py/obj.h"

// return values of mp_vfs_lookup_path
// ROOT is 0 so that the default current directory is the root directory
#define MP_VFS_NONE ((mp_vfs_mount_t*)1)
#define MP_VFS_ROOT ((mp_vfs_mount_t*)0)

// MicroPython's port-standardized versions of stat constants
#define MP_S_IFDIR (0x4000)
#define MP_S_IFREG (0x8000)

// constants for block protocol ioctl
#define BP_IOCTL_INIT           (1)
#define BP_IOCTL_DEINIT         (2)
#define BP_IOCTL_SYNC           (3)
#define BP_IOCTL_SEC_COUNT      (4)
#define BP_IOCTL_SEC_SIZE       (5)

// At the moment the VFS protocol just has import_stat, but could be extended to other methods
typedef struct _mp_vfs_proto_t {
    mp_import_stat_t (*import_stat)(void *self, const char *path);
} mp_vfs_proto_t;

typedef struct _mp_vfs_mount_t {
    const char *str; // mount point with leading /
    size_t len;
    mp_obj_t obj;
    struct _mp_vfs_mount_t *next;
} mp_vfs_mount_t;

mp_vfs_mount_t *mp_vfs_lookup_path(const char *path, const char **path_out);
mp_vfs_mount_t *mp_vfs_lookup_path_str_in_out(const mp_obj_t path_in, mp_obj_t *path_out);
mp_vfs_mount_t *mp_vfs_lookup_path_str_out(const char* path, mp_obj_t *path_out);
mp_vfs_mount_t *mp_vfs_lookup_dir_path_str_out(const char* path, mp_obj_t *path_out);

mp_obj_t mp_vfs_proxy_call(mp_vfs_mount_t *vfs, qstr meth_name, size_t n_args, const mp_obj_t *args);
mp_import_stat_t mp_vfs_import_stat(const char *path);
mp_obj_t mp_vfs_open(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args);

#endif // MICROPY_INCLUDED_SUPERVISOR_SHARED_VFS_H
