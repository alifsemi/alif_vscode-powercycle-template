/* This file was ported to work on Alif Semiconductor Ensemble family of devices. */

/* Copyright (C) 2022 Alif Semiconductor - All Rights Reserved.
 * Use, distribution and modification of this code is permitted under the
 * terms stated in the Alif Semiconductor Software License Agreement
 *
 * You should have received a copy of the Alif Semiconductor Software
 * License Agreement with this file. If not, please write to:
 * contact@alifsemi.com, or visit: https://alifsemi.com/license
 *
 */

/*
 * Copyright (c) 2021 Arm Limited. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

__STACK_SIZE    = 0x00002000;
__HEAP_SIZE     = 0x00004000;

MEMORY
{
  ITCM  (rwx) : ORIGIN = 0x00000000, LENGTH = 0x00080000
  DTCM  (rwx) : ORIGIN = 0x20000000, LENGTH = 0x00180000
  MRAM  (rx)  : ORIGIN = 0x80000000, LENGTH = 0x00200000
}

ENTRY(Reset_Handler)

SECTIONS
{
  .startup.at_itcm : ALIGN(4)
  {
    KEEP(*(.vectors))

    *startup_*.c.obj(.text .rodata*)
    *system_*.c.obj(.text .rodata*)
    *system_utils.c.obj(.text .rodata*)
    *app_init.c.obj*(.text .rodata*)
    *pm*(.text .rodata*)
    *tgu_*(.text .rodata*)
    *mpu_*(.text .rodata*)
    *tcm_partition*(.text .rodata*)

    *(startup_ro_data)
  } > ITCM

  .copy.table : ALIGN(4)
  {
    __copy_table_start__ = .;
    LONG ( LOADADDR(.data.at_dtcm) )
    LONG ( ADDR(.data.at_dtcm) )
    LONG ( SIZEOF(.data.at_dtcm)/4 )
    __copy_table_end__ = .;
  } > ITCM

  .zero.table : ALIGN(4)
  {
    __zero_table_start__ = .;
    LONG (ADDR(.bss))
    LONG (SIZEOF(.bss)/4)
    __zero_table_end__ = .;
  } > ITCM

  .data.at_dtcm : ALIGN(8)
  {
    *(vtable)
    *(.data)
    *(.data*)
    *arm_common_tables.c.obj(.data* .rodata*)

    KEEP(*(.jcr*))

    . = ALIGN(8);
    *(.ARM.extab* .gnu.linkonce.armextab.*)
    . = ALIGN(8);
    __exidx_start = .;
    *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    __exidx_end = .;

  } > DTCM AT > ITCM

  .bss.noinit (NOLOAD) : ALIGN(8)
  {
    *(.bss.noinit)
    *(.bss.noinit.*)
  } > DTCM

  .bss (NOLOAD) : ALIGN(8)
  {
    __bss_start__ = .;
    *(.bss)
    *(.bss.*)
    *(COMMON)
    __bss_end__ = .;
  } > DTCM

  .heap (NOLOAD) : ALIGN(8)
  {
    __end__ = .;
    PROVIDE(end = .);
    . = . + __HEAP_SIZE;
    __HeapLimit = .;
  } > DTCM

  .stack (ORIGIN(DTCM) + LENGTH(DTCM) - __STACK_SIZE) (NOLOAD) : ALIGN(8)
  {
    __StackLimit = .;
    . = . + __STACK_SIZE;
    __StackTop = .;
  } > DTCM
  PROVIDE(__stack = __StackTop);

  .readonly.at_itcm : ALIGN(4)
  {
    /* Use wildcards to mop up any read-only not directed to TCM */ 
    KEEP(*(.init))
    KEEP(*(.fini))

    . = ALIGN(4);
    /* preinit data */
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP(*(.preinit_array))
    PROVIDE_HIDDEN (__preinit_array_end = .);

    . = ALIGN(4);
    /* init data */
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array))
    PROVIDE_HIDDEN (__init_array_end = .);

    . = ALIGN(4);
    /* finit data */
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP(*(SORT(.fini_array.*)))
    KEEP(*(.fini_array))
    PROVIDE_HIDDEN (__fini_array_end = .);

    /* .ctors */
    *crtbegin.o(.ctors)
    *crtbegin?.o(.ctors)
    *(EXCLUDE_FILE(*crtend?.o *crtend.o) .ctors)
    *(SORT(.ctors.*))
    *(.ctors)

    /* .dtors */
    *crtbegin.o(.dtors)
    *crtbegin?.o(.dtors)
    *(EXCLUDE_FILE(*crtend?.o *crtend.o) .dtors)
    *(SORT(.dtors.*))
    *(.dtors)

    *(.text*)
    *(.rodata*)
    *(.kws_model)
    *(.kws_reference)
    *(.typical_mem_map)
    *(.typical_reference)

    KEEP(*(.eh_frame*))
  } > ITCM

  /* Check if data + heap + stack exceeds RAM limit */
  ASSERT(__StackLimit >= __HeapLimit, "region RAM overflowed with stack")
}
