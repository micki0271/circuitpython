/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2014 Paul Sokolovsky
 * Copyright (c) 2019 Dan Halbert for Adafruit Industries
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

#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "py/runtime.h"

#define FLAG_DEBUG 0x1000


STATIC mp_obj_t re_pattern_match_search(size_t n_args, const mp_obj_t *args, bool is_anchored) {
    mp_obj_pattern_t *self = MP_OBJ_TO_PTR(args[0]);
    mp_obj_t str = args[1];

    mp_int_t pos = 0;
    mp_int_t endpos = 0;
    // Dealing with an end pos can be expensive for Unicode chars, so tell the common_hal routine
    // if it's given, or whether it will use the whole string.
    bool endpos_given = false;

    if (n_args > 2) {
        // Start pos given.
        pos = mp_obj_get_int(args[2]);
        // Compute true string length.
        mp_int_t str_len = MP_OBJ_SMALL_INT_VALUE(mp_obj_len_maybe(str));

        // No match possible if out of range.
        if (pos > str_len) {
            return mp_const_none;
        }
        if (pos < 0) {
            pos = 0;
        }
    }

    if (n_args > 3) {
        // End pos given.
        endpos = mp_obj_get_int(args[3]);
        endpos_given = true;

        // No match possible if out of range.
        if (endpos <= pos) {
            return mp_const_none;
        }
    }

    return common_hal_re_pattern_match_search(self, pos, endpos_given, endpos, is_anchored);
}

STATIC mp_obj_t re_pattern_match(size_t n_args, const mp_obj_t *args) {
    return common_hal_re_pattern_execute(n_args, args, true);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(re_match_obj, 2, 4, re_pattern_match);

STATIC mp_obj_t re_pattern_search(size_t n_args, const mp_obj_t *args) {
    return common_hal_re_pattern_execute(n_args, args, false);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(re_search_obj, 2, 4, re_pattern_search);

STATIC mp_obj_t re_pattern_split(size_t n_args, const mp_obj_t *args) {
    re_pattern_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    int maxsplit = 0;
    if (n_args > 2) {
        maxsplit = mp_obj_get_int(args[2]);
    }

    return common_hal_re_split(self, args[1], maxsplit);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(re_split_obj, 2, 3, re_pattern_split);

STATIC mp_obj_t re_sub_helper(mp_obj_t self_in, size_t n_args, const mp_obj_t *args) {
    mp_obj_re_t *self = MP_OBJ_TO_PTR(self_in);
    mp_obj_t replace = args[1];
    mp_obj_t where = args[2];
    mp_int_t count = 0;
    if (n_args > 3) {
        count = mp_obj_get_int(args[3]);
        // Note: flags are currently ignored
    }

    size_t where_len;
    const char *where_str = mp_obj_str_get_data(where, &where_len);
    Subject subj;
    subj.begin = where_str;
    subj.end = subj.begin + where_len;
    int caps_num = (self->re.sub + 1) * 2;

    vstr_t vstr_return;
    vstr_return.buf = NULL; // We'll init the vstr after the first match
    mp_obj_match_t *match = mp_local_alloc(sizeof(mp_obj_match_t) + caps_num * sizeof(char*));
    match->base.type = &match_type;
    match->num_matches = caps_num / 2; // caps_num counts start and end pointers
    match->str = where;

    for (;;) {
        // cast is a workaround for a bug in msvc: it treats const char** as a const pointer instead of a pointer to pointer to const char
        memset((char*)match->caps, 0, caps_num * sizeof(char*));
        int res = re1_5_recursiveloopprog(&self->re, &subj, match->caps, caps_num, false);

        // If we didn't have a match, or had an empty match, it's time to stop
        if (!res || match->caps[0] == match->caps[1]) {
            break;
        }

        // Initialise the vstr if it's not already
        if (vstr_return.buf == NULL) {
            vstr_init(&vstr_return, match->caps[0] - subj.begin);
        }

        // Add pre-match string
        vstr_add_strn(&vstr_return, subj.begin, match->caps[0] - subj.begin);

        // Get replacement string
        const char* repl = mp_obj_str_get_str((mp_obj_is_callable(replace) ? mp_call_function_1(replace, MP_OBJ_FROM_PTR(match)) : replace));

        // Append replacement string to result, substituting any regex groups
        while (*repl != '\0') {
            if (*repl == '\\') {
                ++repl;
                bool is_g_format = false;
                if (*repl == 'g' && repl[1] == '<') {
                    // Group specified with syntax "\g<number>"
                    repl += 2;
                    is_g_format = true;
                }

                if ('0' <= *repl && *repl <= '9') {
                    // Group specified with syntax "\g<number>" or "\number"
                    unsigned int match_no = 0;
                    do {
                        match_no = match_no * 10 + (*repl++ - '0');
                    } while ('0' <= *repl && *repl <= '9');
                    if (is_g_format && *repl == '>') {
                        ++repl;
                    }

                    if (match_no >= (unsigned int)match->num_matches) {
                        nlr_raise(mp_obj_new_exception_arg1(&mp_type_IndexError, MP_OBJ_NEW_SMALL_INT(match_no)));
                    }

                    const char *start_match = match->caps[match_no * 2];
                    if (start_match != NULL) {
                        // Add the substring matched by group
                        const char *end_match = match->caps[match_no * 2 + 1];
                        vstr_add_strn(&vstr_return, start_match, end_match - start_match);
                    }
                }
            } else {
                // Just add the current byte from the replacement string
                vstr_add_byte(&vstr_return, *repl++);
            }
        }

        // Move start pointer to end of last match
        subj.begin = match->caps[1];

        // Stop substitutions if count was given and gets to 0
        if (count > 0 && --count == 0) {
            break;
        }
    }

    mp_local_free(match);

    if (vstr_return.buf == NULL) {
        // Optimisation for case of no substitutions
        return where;
    }

    // Add post-match string
    vstr_add_strn(&vstr_return, subj.begin, subj.end - subj.begin);

    return mp_obj_new_str_from_vstr(mp_obj_get_type(where), &vstr_return);
}

STATIC mp_obj_t re_sub(size_t n_args, const mp_obj_t *args) {
    return re_sub_helper(args[0], n_args, args);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(re_sub_obj, 3, 5, re_sub);

STATIC const mp_rom_map_elem_t re_pattern_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_match), MP_ROM_PTR(&re_pattern_match_obj) },
    { MP_ROM_QSTR(MP_QSTR_search), MP_ROM_PTR(&re_pattern_search_obj) },
    { MP_ROM_QSTR(MP_QSTR_split), MP_ROM_PTR(&re_pattern_split_obj) },
    { MP_ROM_QSTR(MP_QSTR_sub), MP_ROM_PTR(&re_pattern_sub_obj) },
};

STATIC MP_DEFINE_CONST_DICT(re_pattern_locals_dict, re_pattern_locals_dict_table);

STATIC const mp_obj_type_t re_pattern_type = {
    { &mp_type_type },
    .name = MP_QSTR_Pattern,
    .print = re_pattern_print,
    .locals_dict = (void*)&re_pattern_locals_dict,
};

STATIC mp_obj_t re_compile(size_t n_args, const mp_obj_t *args) {
    const char *re_str = mp_obj_str_get_str(args[0]);
    int size = re1_5_sizecode(re_str);
    if (size == -1) {
        goto error;
    }
    re_pattern_obj_t *o = m_new_obj_var(mp_obj_re_t, char, size);
    o->base.type = &re_type;
    int flags = 0;
    if (n_args > 1) {
        flags = mp_obj_get_int(args[1]);
    }
    int error = re1_5_compilecode(&o->re, re_str);
    if (error != 0) {
error:
        mp_raise_ValueError(translate("Error in regex"));
    }
    if (flags & FLAG_DEBUG) {
        re1_5_dumpcode(&o->re);
    }
    return MP_OBJ_FROM_PTR(o);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(re_compile_obj, 1, 2, re_compile);

STATIC mp_obj_t re_exec(bool is_anchored, uint n_args, const mp_obj_t *args) {
    (void)n_args;
    mp_obj_t pattern = re_compile(1, args);

    const mp_obj_t args2[] = {pattern, args[1]};
    mp_obj_t match = ure_exec(is_anchored, 2, args2);
    return match;
}

STATIC mp_obj_t re_match(size_t n_args, const mp_obj_t *args) {
    return re_exec(true, n_args, args);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(re_match_obj, 2, 4, re_match);

STATIC mp_obj_t re_search(size_t n_args, const mp_obj_t *args) {
    return re_exec(false, n_args, args);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_re_search_obj, 2, 4, re_search);

STATIC mp_obj_t re_sub(size_t n_args, const mp_obj_t *args) {
    mp_obj_t pattern = re_compile(1, args);
    return re_sub_helper(pattern, n_args, args);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_re_sub_obj, 3, 5, re_sub);

STATIC const mp_rom_map_elem_t re_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_re) },
    { MP_ROM_QSTR(MP_QSTR_compile), MP_ROM_PTR(&re_compile_obj) },
    { MP_ROM_QSTR(MP_QSTR_match), MP_ROM_PTR(&re_match_obj) },
    { MP_ROM_QSTR(MP_QSTR_search), MP_ROM_PTR(&re_search_obj) },
    { MP_ROM_QSTR(MP_QSTR_sub), MP_ROM_PTR(&re_sub_obj) },
    { MP_ROM_QSTR(MP_QSTR_DEBUG), MP_ROM_INT(FLAG_DEBUG) },
};

STATIC MP_DEFINE_CONST_DICT(re_module_globals, re_module_globals_table);

const mp_obj_module_t re_module_globals = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&re_module_globals,
};
