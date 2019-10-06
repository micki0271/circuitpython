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

#include "shared-bindings/re/Match.h"

STATIC void re_match_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    (void)kind;
    mp_obj_match_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, "<match num=%d>", common_hal_re_match_get_num_matches(self));
}

STATIC void check_group_num(mp_obj_match_t *self, mp_int_t num) {
    if (num < 0 || num >= common_hal_re_match_get_num_matches(self)) {
        mp_raise_IndexError(translate("no such group"));
    }
}

STATIC mp_obj_t re_match_group(mp_obj_t self_in, mp_obj_t num_in) {
    mp_obj_match_t *self = MP_OBJ_TO_PTR(self_in);
    mp_int_t num = mp_obj_get_int(num_in);
    check_group_num(self, num);
    return common_hal_re_match_group(self, check_group_num(num_in));
}
MP_DEFINE_CONST_FUN_OBJ_2(re_match_group_obj, re_match_group);

STATIC mp_obj_t re_match_groups(mp_obj_t self_in) {
    mp_obj_match_t *self = MP_OBJ_TO_PTR(self_in);
    return MP_OBJ_FROM_PTR(common_hal_re_match_groups(self));
}
MP_DEFINE_CONST_FUN_OBJ_1(re_match_groups_obj, re_match_groups);


// Shared validation and call to common_hal. Values returned in span[].
STATIC void match_span_common(size_t n_args, const mp_obj_t *args, mp_int_t span[2]) {
    mp_obj_match_t *self = MP_OBJ_TO_PTR(args[0]);
    mp_int_t num = 0;
    if (n_args == 2) {
        num = mp_obj_get_int(args[1]);
        check_group_num(self, num);
    }
    common_hal_re_match_span(self, num, span);
}

STATIC mp_obj_t re_match_span(size_t n_args, const mp_obj_t *args) {
    mp_int_t span[2];
    match_span_common(n_args, *args, span);
    return mp_obj_new_tuple(2, span);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(re_match_span_obj, 1, 2, re_match_span);

STATIC mp_obj_t re_match_start(size_t n_args, const mp_obj_t *args) {
    mp_int_t span[2];
    match_span_common(n_args, *args, span);
    return mp_obj_new_int(span[0]);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(re_match_start_obj, 1, 2, re_match_start);

STATIC mp_obj_t match_end(size_t n_args, const mp_obj_t *args) {
    mp_int_t span[2];
    match_span_common(n_args, *args, span);
    return mp_obj_new_int(span[1]);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(match_end_obj, 1, 2, match_end);

STATIC const mp_rom_map_elem_t re_match_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_group), MP_ROM_PTR(&re_match_group_obj) },
    { MP_ROM_QSTR(MP_QSTR_groups), MP_ROM_PTR(&re_match_groups_obj) },
    { MP_ROM_QSTR(MP_QSTR_span), MP_ROM_PTR(&re_match_span_obj) },
    { MP_ROM_QSTR(MP_QSTR_start), MP_ROM_PTR(&re_match_start_obj) },
    { MP_ROM_QSTR(MP_QSTR_end), MP_ROM_PTR(&re_match_end_obj) },
};

STATIC MP_DEFINE_CONST_DICT(re_match_locals_dict, re_match_locals_dict_table);

STATIC const mp_obj_type_t re_match_type = {
    { &mp_type_type },
    .name = MP_QSTR_Match,
    .print = re_match_print,
    .locals_dict = (void*)&re_match_locals_dict,
};
