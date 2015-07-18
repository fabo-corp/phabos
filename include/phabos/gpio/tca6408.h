/**
 * Copyright (c) 2014-2015 Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @brief TCA6408 GPIO Expander Driver
 */

#ifndef _TSB_TCA6408_H_
#define _TSB_TCA6408_H_

#include <stdint.h>
#include <stdbool.h>

int tca6408_reset(uint8_t gpio, bool en);
int tca6408_set_direction_in(uint8_t bus, uint8_t addr, uint8_t which);
int tca6408_set_default_outputs(uint8_t bus, uint8_t addr, uint8_t dflt);
int tca6408_set_direction_out(uint8_t bus, uint8_t addr, uint8_t which);
int tca6408_get_direction(uint8_t bus, uint8_t addr, uint8_t which);
int tca6408_set_polarity_inverted(uint8_t bus, uint8_t addr,
                                  uint8_t which, uint8_t inverted);
int tca6408_get_polarity_inverted(uint8_t bus, uint8_t addr, uint8_t which);
int tca6408_set(uint8_t bus, uint8_t addr, uint8_t which, uint8_t val);
int tca6408_get(uint8_t bus, uint8_t addr, uint8_t which);

#endif
