/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Damien P. George
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

#include <stdint.h>
#include <string.h>

#include "py/runtime.h"
#include "py/objstr.h"
#include "py/mperrno.h"
#include "supervisor/shared/vfs.h"

#if MICROPY_VFS

#if MICROPY_VFS_FAT
#include "extmod/vfs_fat.h"
#endif

// For mp_vfs_proxy_call, the maximum number of additional args that can be passed.
// A fixed maximum size is used to avoid the need for a costly variable array.
#define PROXY_MAX_ARGS (2)

// path is the path to lookup and *path_out holds the path within the VFS
// object (starts with / if an absolute path).
// Returns MP_VFS_ROOT for root dir (and then path_out is undefined) and
// MP_VFS_NONE for path not found.
mp_vfs_mount_t *mp_vfs_lookup_path(const char *path, const char **path_out) {
    if (*path == '/' || MP_STATE_VM(vfs_cur) == MP_VFS_ROOT) {
        // an absolute path, or the current volume is root, so search root dir
        bool is_abs = 0;
        if (*path == '/') {
            ++path;
            is_abs = 1;
        }
        if (*path == '\0') {
            // path is "" or "/" so return virtual root
            return MP_VFS_ROOT;
        }
        for (mp_vfs_mount_t *vfs = MP_STATE_VM(vfs_mount_table); vfs != NULL; vfs = vfs->next) {
            size_t len = vfs->len - 1;
            if (len == 0) {
                *path_out = path - is_abs;
                return vfs;
            }
            if (strncmp(path, vfs->str + 1, len) == 0) {
                if (path[len] == '/') {
                    *path_out = path + len;
                    return vfs;
                } else if (path[len] == '\0') {
                    *path_out = "/";
                    return vfs;
                }
            }
        }

        // if we get here then there's nothing mounted on /

        if (is_abs) {
            // path began with / and was not found
            return MP_VFS_NONE;
        }
    }

    // a relative path within a mounted device
    *path_out = path;
    return MP_STATE_VM(vfs_cur);
}

// Version of mp_vfs_lookup_path that takes and returns uPy string objects.
mp_vfs_mount_t *mp_vfs_lookup_path_str_in_out(const mp_obj_t path_in, mp_obj_t *path_out) {
    const char *path = mp_obj_str_get_str(path_in);
    const char *p_out;
    mp_vfs_mount_t *vfs = mp_vfs_lookup_path(path, &p_out);
    if (vfs != MP_VFS_NONE && vfs != MP_VFS_ROOT) {
        *path_out = mp_obj_new_str_of_type(mp_obj_get_type(path_in),
            (const byte*)p_out, strlen(p_out));
    }
    return vfs;
}

// Version of mp_vfs_lookup_path that returns a uPy string object.
mp_vfs_mount_t *mp_vfs_lookup_path_str_out(const char* path, mp_obj_t *path_out) {
    const char *p_out;
    mp_vfs_mount_t *vfs = mp_vfs_lookup_path(path, &p_out);
    if (vfs != MP_VFS_NONE && vfs != MP_VFS_ROOT) {
        *path_out = mp_obj_new_str_of_type(&mp_type_str,
            (const byte*)p_out, strlen(p_out));
    }
    return vfs;
}

// Strip off trailing slashes to please underlying libraries
mp_vfs_mount_t *mp_vfs_lookup_dir_path_str_out(const char* path, mp_obj_t *path_out) {
    const char *p_out;
    mp_vfs_mount_t *vfs = mp_vfs_lookup_path(path, &p_out);
    if (vfs != MP_VFS_NONE && vfs != MP_VFS_ROOT) {
        size_t len = strlen(p_out);
        while (len > 1 && p_out[len - 1] == '/') {
            len--;
        }
        *path_out = mp_obj_new_str_of_type(&mp_type_str, (const byte*)p_out, len);
    }
    return vfs;
}

mp_obj_t mp_vfs_proxy_call(mp_vfs_mount_t *vfs, qstr meth_name, size_t n_args, const mp_obj_t *args) {
    assert(n_args <= PROXY_MAX_ARGS);
    if (vfs == MP_VFS_NONE) {
        // mount point not found
        mp_raise_OSError(MP_ENODEV);
    }
    if (vfs == MP_VFS_ROOT) {
        // can't do operation on root dir
        mp_raise_OSError(MP_EPERM);
    }
    mp_obj_t meth[2 + PROXY_MAX_ARGS];
    mp_load_method(vfs->obj, meth_name, meth);
    if (args != NULL) {
        memcpy(meth + 2, args, n_args * sizeof(*args));
    }
    return mp_call_method_n_kw(n_args, 0, meth);
}

mp_import_stat_t mp_vfs_import_stat(const char *path) {
    const char *path_out;
    mp_vfs_mount_t *vfs = mp_vfs_lookup_path(path, &path_out);
    if (vfs == MP_VFS_NONE || vfs == MP_VFS_ROOT) {
        return MP_IMPORT_STAT_NO_EXIST;
    }

    // If the mounted object has the VFS protocol, call its import_stat helper
    const mp_vfs_proto_t *proto = mp_obj_get_type(vfs->obj)->protocol;
    if (proto != NULL) {
        return proto->import_stat(MP_OBJ_TO_PTR(vfs->obj), path_out);
    }

    // delegate to vfs.stat() method
    mp_obj_t path_o = mp_obj_new_str(path_out, strlen(path_out));
    mp_obj_t stat;
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        stat = mp_vfs_proxy_call(vfs, MP_QSTR_stat, 1, &path_o);
        nlr_pop();
    } else {
        // assume an exception means that the path is not found
        return MP_IMPORT_STAT_NO_EXIST;
    }
    mp_obj_t *items;
    mp_obj_get_array_fixed_n(stat, 10, &items);
    mp_int_t st_mode = mp_obj_get_int(items[0]);
    if (st_mode & MP_S_IFDIR) {
        return MP_IMPORT_STAT_DIR;
    } else {
        return MP_IMPORT_STAT_FILE;
    }
}

// Note: buffering and encoding args are currently ignored
mp_obj_t mp_vfs_open(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_file, ARG_mode, ARG_encoding };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_file, MP_ARG_OBJ | MP_ARG_REQUIRED, {.u_rom_obj = MP_ROM_PTR(&mp_const_none_obj)} },
        { MP_QSTR_mode, MP_ARG_OBJ, {.u_rom_obj = MP_ROM_QSTR(MP_QSTR_r)} },
        { MP_QSTR_buffering, MP_ARG_INT, {.u_int = -1} },
        { MP_QSTR_encoding, MP_ARG_OBJ, {.u_rom_obj = MP_ROM_PTR(&mp_const_none_obj)} },
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    mp_vfs_mount_t *vfs = mp_vfs_lookup_path_str_in_out(args[ARG_file].u_obj, &args[ARG_file].u_obj);
    return mp_vfs_proxy_call(vfs, MP_QSTR_open, 2, (mp_obj_t*)&args);
}
MP_DEFINE_CONST_FUN_OBJ_KW(mp_vfs_open_obj, 0, mp_vfs_open);

#endif // MICROPY_VFS
