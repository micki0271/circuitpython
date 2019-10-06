/*
 * This file is part of the Micro Python project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013, 2014 Damien P. George
 * Copyright (c) 2015 Josef Gajdusek
 * Copyright (c) 2017 Scott Shawcroft for Adafruit Industries
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

#include <string.h>

#include "py/mperrno.h"
#include "py/mpstate.h"
#include "py/obj.h"
#include "py/objstr.h"
#include "py/runtime.h"
#include "supervisor/shared/vfs.h"
#include "shared-bindings/os/__init__.h"

// This provides all VFS related OS functions so that ports can share the code
// as needed. It does not provide uname.

typedef struct _ilistdir_it_t {
    mp_obj_base_t base;
    mp_fun_1_t iternext;
    union {
        mp_vfs_mount_t *vfs;
        mp_obj_t iter;
    } cur;
    bool is_str;
    bool is_iter;
} ilistdir_it_t;

STATIC mp_obj_t ilistdir_it_iternext(mp_obj_t self_in) {
    ilistdir_it_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->is_iter) {
        // continue delegating to root dir
        return mp_iternext(self->cur.iter);
    } else if (self->cur.vfs == NULL) {
        // finished iterating mount points and no root dir is mounted
        return MP_OBJ_STOP_ITERATION;
    } else {
        // continue iterating mount points
        mp_vfs_mount_t *vfs = self->cur.vfs;
        self->cur.vfs = vfs->next;
        if (vfs->len == 1) {
            // vfs is mounted at root dir, delegate to it
            mp_obj_t root = MP_OBJ_NEW_QSTR(MP_QSTR__slash_);
            self->is_iter = true;
            self->cur.iter = mp_vfs_proxy_call(vfs, MP_QSTR_ilistdir, 1, &root);
            return mp_iternext(self->cur.iter);
        } else {
            // a mounted directory
            mp_obj_tuple_t *t = MP_OBJ_TO_PTR(mp_obj_new_tuple(3, NULL));
            t->items[0] = mp_obj_new_str_of_type(
                self->is_str ? &mp_type_str : &mp_type_bytes,
                (const byte*)vfs->str + 1, vfs->len - 1);
            t->items[1] = MP_OBJ_NEW_SMALL_INT(MP_S_IFDIR);
            t->items[2] = MP_OBJ_NEW_SMALL_INT(0); // no inode number
            return MP_OBJ_FROM_PTR(t);
        }
    }
}

void common_hal_os_chdir(const char* path) {
    mp_obj_t path_out;
    mp_vfs_mount_t *vfs = mp_vfs_lookup_dir_path_str_out(path, &path_out);
    MP_STATE_VM(vfs_cur) = vfs;
    if (vfs == MP_VFS_ROOT) {
        // If we change to the root dir and a VFS is mounted at the root then
        // we must change that VFS's current dir to the root dir so that any
        // subsequent relative paths begin at the root of that VFS.
        for (vfs = MP_STATE_VM(vfs_mount_table); vfs != NULL; vfs = vfs->next) {
            if (vfs->len == 1) {
                mp_obj_t root = MP_OBJ_NEW_QSTR(MP_QSTR__slash_);
                mp_vfs_proxy_call(vfs, MP_QSTR_chdir, 1, &root);
                break;
            }
        }
    } else {
        mp_vfs_proxy_call(vfs, MP_QSTR_chdir, 1, &path_out);
    }
}

mp_obj_t common_hal_os_getcwd(void) {
    if (MP_STATE_VM(vfs_cur) == MP_VFS_ROOT) {
        return MP_OBJ_NEW_QSTR(MP_QSTR__slash_);
    }
    mp_obj_t cwd_o = mp_vfs_proxy_call(MP_STATE_VM(vfs_cur), MP_QSTR_getcwd, 0, NULL);
    if (MP_STATE_VM(vfs_cur)->len == 1) {
        // don't prepend "/" for vfs mounted at root
        return cwd_o;
    }
    const char *cwd = mp_obj_str_get_str(cwd_o);
    vstr_t vstr;
    vstr_init(&vstr, MP_STATE_VM(vfs_cur)->len + strlen(cwd) + 1);
    vstr_add_strn(&vstr, MP_STATE_VM(vfs_cur)->str, MP_STATE_VM(vfs_cur)->len);
    if (!(cwd[0] == '/' && cwd[1] == 0)) {
        vstr_add_str(&vstr, cwd);
    }
    return mp_obj_new_str_from_vstr(&mp_type_str, &vstr);
}

mp_obj_t common_hal_os_listdir(const char* path) {
    mp_obj_t path_out;
    mp_vfs_mount_t *vfs = mp_vfs_lookup_dir_path_str_out(path, &path_out);

    ilistdir_it_t iter;
    mp_obj_t iter_obj = MP_OBJ_FROM_PTR(&iter);

    if (vfs == MP_VFS_ROOT) {
        // list the root directory
        iter.base.type = &mp_type_polymorph_iter;
        iter.iternext = ilistdir_it_iternext;
        iter.cur.vfs = MP_STATE_VM(vfs_mount_table);
        iter.is_str = true;
        iter.is_iter = false;
    } else {
        iter_obj = mp_vfs_proxy_call(vfs, MP_QSTR_ilistdir, 1, &path_out);
    }

    mp_obj_t dir_list = mp_obj_new_list(0, NULL);
    mp_obj_t next;
    while ((next = mp_iternext(iter_obj)) != MP_OBJ_STOP_ITERATION) {
        // next[0] is the filename.
        mp_obj_list_append(dir_list, mp_obj_subscr(next, MP_OBJ_NEW_SMALL_INT(0), MP_OBJ_SENTINEL));
        RUN_BACKGROUND_TASKS;
    }
    return dir_list;
}

void common_hal_os_mkdir(const char* path) {
    mp_obj_t path_out;
    mp_vfs_mount_t *vfs = mp_vfs_lookup_dir_path_str_out(path, &path_out);
    if (vfs == MP_VFS_ROOT || (vfs != MP_VFS_NONE && !strcmp(mp_obj_str_get_str(path_out), "/"))) {
        mp_raise_OSError(MP_EEXIST);
    }
    mp_vfs_proxy_call(vfs, MP_QSTR_mkdir, 1, &path_out);
}

void common_hal_os_remove(const char* path) {
    mp_obj_t path_out;
    mp_vfs_mount_t *vfs = mp_vfs_lookup_path_str_out(path, &path_out);
    mp_vfs_proxy_call(vfs, MP_QSTR_remove, 1, &path_out);
}

void common_hal_os_rename(const char* old_path, const char* new_path) {
    mp_obj_t args[2];
    mp_vfs_mount_t *old_vfs = mp_vfs_lookup_path_str_out(old_path, &args[0]);
    mp_vfs_mount_t *new_vfs = mp_vfs_lookup_path_str_out(new_path, &args[1]);
    if (old_vfs != new_vfs) {
        // can't rename across filesystems
        mp_raise_OSError(MP_EPERM);
    }
    mp_vfs_proxy_call(old_vfs, MP_QSTR_rename, 2, args);
}

void common_hal_os_rmdir(const char* path) {
    mp_obj_t path_out;
    mp_vfs_mount_t *vfs = mp_vfs_lookup_dir_path_str_out(path, &path_out);
    mp_vfs_proxy_call(vfs, MP_QSTR_rmdir, 1, &path_out);
}

mp_obj_t common_hal_os_stat(const char* path) {
    mp_obj_t path_out;
    mp_vfs_mount_t *vfs = mp_vfs_lookup_path_str_out(path, &path_out);
    if (vfs == MP_VFS_ROOT) {
        mp_obj_tuple_t *t = MP_OBJ_TO_PTR(mp_obj_new_tuple(10, NULL));
        t->items[0] = MP_OBJ_NEW_SMALL_INT(MP_S_IFDIR); // st_mode
        for (int i = 1; i <= 9; ++i) {
            t->items[i] = MP_OBJ_NEW_SMALL_INT(0); // dev, nlink, uid, gid, size, atime, mtime, ctime
        }
        return MP_OBJ_FROM_PTR(t);
    }
    return mp_vfs_proxy_call(vfs, MP_QSTR_stat, 1, &path_out);
}

mp_obj_t common_hal_os_statvfs(const char* path) {
    mp_obj_t path_out;
    mp_vfs_mount_t *vfs = mp_vfs_lookup_path_str_out(path, &path_out);
    if (vfs == MP_VFS_ROOT) {
        // statvfs called on the root directory, see if there's anything mounted there
        for (vfs = MP_STATE_VM(vfs_mount_table); vfs != NULL; vfs = vfs->next) {
            if (vfs->len == 1) {
                break;
            }
        }

        // If there's nothing mounted at root then return a mostly-empty tuple
        if (vfs == NULL) {
            mp_obj_tuple_t *t = MP_OBJ_TO_PTR(mp_obj_new_tuple(10, NULL));

            // fill in: bsize, frsize, blocks, bfree, bavail, files, ffree, favail, flags
            for (int i = 0; i <= 8; ++i) {
                t->items[i] = MP_OBJ_NEW_SMALL_INT(0);
            }

            // Put something sensible in f_namemax
            t->items[9] = MP_OBJ_NEW_SMALL_INT(MICROPY_ALLOC_PATH_MAX);

            return MP_OBJ_FROM_PTR(t);
        }

        // VFS mounted at root so delegate the call to it
        path_out = MP_OBJ_NEW_QSTR(MP_QSTR__slash_);
    }
    return mp_vfs_proxy_call(vfs, MP_QSTR_statvfs, 1, &path_out);
}
