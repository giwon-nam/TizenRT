/****************************************************************************
 *
 * Copyright 2023 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/

#define MEM_VAR_REGION_COUNT 4

extern uint32_t _sbss;
extern uint32_t _ebss;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t __psram_bss_start__;
extern uint32_t __psram_bss_end__;
extern uint32_t __sdata_ext;
extern uint32_t __edata_ext;

void *variable_region_start_addr[MEM_VAR_REGION_COUNT] = {&_sbss, &_sdata, &__psram_bss_start__, &__sdata_ext};
void *variable_region_end_addr[MEM_VAR_REGION_COUNT] = {&_ebss, &_edata, &__psram_bss_end__, &__edata_ext};
