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

#include "shared-bindings/re/Match.h"

#include "re1.5/re1.5.h"

#define FLAG_DEBUG 0x1000

mp_int_t common_hal_re_match_get_num_matches(re_match_obj_t *self) {
    return self->num_matches;
}

mp_obj_t common_hal_re_match_group(re_match_obj_t *self, mp_int_t num) {
    const char *start = self->caps[num * 2];
    if (start == NULL) {
        // no match for this group
        return mp_const_none;
    }
    return mp_obj_new_str_of_type(mp_obj_get_type(self->str),
        (const byte*)start, self->caps[num * 2 + 1] - start);
}

mp_obj_tuple_t *common_hal_re_match_groups(mp_obj_match_t self) {
    if (self->num_matches <= 1) {
        return mp_const_empty_tuple;
    }
    mp_obj_tuple_t *groups = MP_OBJ_TO_PTR(mp_obj_new_tuple(self->num_matches - 1, NULL));
    for (int i = 1; i < self->num_matches; ++i) {
        groups->items[i - 1] = match_group(self_in, MP_OBJ_NEW_SMALL_INT(i));
    }
    return groups;
}

void common_hal_re_match_span(re_match_obj_t *self, mp_int_t num, mp_int_t span[2]) {
    mp_int_t s = -1;
    mp_int_t e = -1;
    const char *start = self->caps[num * 2];
    if (start != NULL) {
        // have a match for this group
        const char *begin = mp_obj_str_get_str(self->str);
        s = start - begin;
        e = self->caps[num * 2 + 1] - begin;
    }

    span[0] = s;
    span[1] = e;
}
