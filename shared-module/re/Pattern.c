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

#include "shared-bindings/re/Pattern.h"

#include "re1.5/re1.5.h"

mp_obj_t common_hal_re_pattern_match_search(re_pattern_obj_t *self, mp_obj_t str, mp_int_t pos, bool endpos_given, mp_int_t endpos, bool is_anchored) {
    Subject subj;
    size_t len;

    // Assume whole string to start.
    subj.begin = mp_obj_str_get_data(args[1], &len);
    subj.end = subj.begin + len;

    const mp_obj_type_t *str_type = mp_obj_get_type(str);
    if (pos > 0) {
        subj.begin = (const char *) str_index_to_ptr(str_type, (const byte *) subj.begin, len,
                                                     MP_OBJ_NEW_SMALL_INT(pos), true);
    }

    if (endpos_given) {
        subj.end = (const char *) str_index_to_ptr(str_type, (const byte *) subj.begin, len,
                                                   MP_OBJ_NEW_SMALL_INT(endpos), true);
    }

    int caps_num = (self->re.sub + 1) * 2;
    mp_obj_match_t *match = m_new_obj_var(mp_obj_match_t, char*, caps_num);
    // cast is a workaround for a bug in msvc: it treats const char** as a const pointer instead of a pointer to pointer to const char
    memset((char*)match->caps, 0, caps_num * sizeof(char*));
    int res = re1_5_recursiveloopprog(&self->re, &subj, match->caps, caps_num, is_anchored);
    if (res == 0) {
        m_del_var(mp_obj_match_t, char*, caps_num, match);
        return mp_const_none;
    }

    match->base.type = &match_type;
    match->num_matches = caps_num / 2; // caps_num counts start and end pointers
    match->str = args[1];
    return MP_OBJ_FROM_PTR(match);
}

mp_obj_t common_hal_re_pattern_split(re_pattern_obj_t *self, mp_obj_t str, mp_int_t maxsplit) {
    Subject subj;
    size_t len;
    const mp_obj_type_t *str_type = mp_obj_get_type(str);
    subj.begin = mp_obj_str_get_data(str, &len);
    subj.end = subj.begin + len;
    int caps_num = (self->re.sub + 1) * 2;

    mp_obj_t retval = mp_obj_new_list(0, NULL);
    const char **caps = mp_local_alloc(caps_num * sizeof(char*));
    while (true) {
        // cast is a workaround for a bug in msvc: it treats const char** as a const pointer instead of a pointer to pointer to const char
        memset((char**)caps, 0, caps_num * sizeof(char*));
        int res = re1_5_recursiveloopprog(&self->re, &subj, caps, caps_num, false);

        // if we didn't have a match, or had an empty match, it's time to stop
        if (!res || caps[0] == caps[1]) {
            break;
        }

        mp_obj_t s = mp_obj_new_str_of_type(str_type, (const byte*)subj.begin, caps[0] - subj.begin);
        mp_obj_list_append(retval, s);
        if (self->re.sub > 0) {
            mp_raise_NotImplementedError(translate("Splitting with sub-captures"));
        }
        subj.begin = caps[1];
        if (maxsplit > 0 && --maxsplit == 0) {
            break;
        }
    }
    // cast is a workaround for a bug in msvc (see above)
    mp_local_free((char**)caps);

    mp_obj_t s = mp_obj_new_str_of_type(str_type, (const byte*)subj.begin, subj.end - subj.begin);
    mp_obj_list_append(retval, s);
    return retval;
}

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

STATIC MP_DEFINE_CONST_DICT(re_locals_dict, re_locals_dict_table);

STATIC mp_obj_t mod_re_compile(size_t n_args, const mp_obj_t *args) {
    const char *re_str = mp_obj_str_get_str(args[0]);
    int size = re1_5_sizecode(re_str);
    if (size == -1) {
        goto error;
    }
    mp_obj_re_t *o = m_new_obj_var(mp_obj_re_t, char, size);
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
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_re_compile_obj, 1, 2, mod_re_compile);

STATIC mp_obj_t mod_re_exec(bool is_anchored, uint n_args, const mp_obj_t *args) {
    (void)n_args;
    mp_obj_t self = mod_re_compile(1, args);

    const mp_obj_t args2[] = {self, args[1]};
    mp_obj_t match = ure_exec(is_anchored, 2, args2);
    return match;
}

STATIC mp_obj_t mod_re_match(size_t n_args, const mp_obj_t *args) {
    return mod_re_exec(true, n_args, args);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_re_match_obj, 2, 4, mod_re_match);

STATIC mp_obj_t mod_re_search(size_t n_args, const mp_obj_t *args) {
    return mod_re_exec(false, n_args, args);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_re_search_obj, 2, 4, mod_re_search);

#if MICROPY_PY_URE_SUB
STATIC mp_obj_t mod_re_sub(size_t n_args, const mp_obj_t *args) {
    mp_obj_t self = mod_re_compile(1, args);
    return re_sub_helper(self, n_args, args);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_re_sub_obj, 3, 5, mod_re_sub);
#endif

// Source files #include'd here to make sure they're compiled in
// only if module is enabled by config setting.

#define re1_5_fatal(x) assert(!x)
#include "re1.5/compilecode.c"
#include "re1.5/dumpcode.c"
#include "re1.5/recursiveloop.c"
#include "re1.5/charclass.c"

#endif //MICROPY_PY_URE
