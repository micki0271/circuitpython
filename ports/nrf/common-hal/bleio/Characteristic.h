/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Artur Pacholec
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

#ifndef MICROPY_INCLUDED_COMMON_HAL_BLEIO_CHARACTERISTIC_H
#define MICROPY_INCLUDED_COMMON_HAL_BLEIO_CHARACTERISTIC_H

#include "shared-module/bleio/Characteristic.h"
#include "shared-module/bleio/Service.h"
#include "common-hal/bleio/UUID.h"

typedef struct {
    mp_obj_base_t base;
    // An instance of bleio.Service or one of its subclasses.
    mp_obj_t service;
    // An instance of bleio.UUID or one of its subclasses.
    mp_obj_t uuid;
    mp_obj_t value_data;
    uint16_t handle;
    bleio_characteristic_properties_t props;
    uint16_t user_desc_handle;
    uint16_t cccd_handle;
    uint16_t sccd_handle;
} bleio_characteristic_obj_t;

#endif // MICROPY_INCLUDED_COMMON_HAL_BLEIO_CHARACTERISTIC_H
