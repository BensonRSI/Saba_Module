/**
 * Copyright (c) 2023 SvOlli
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "pico/stdlib.h"

#define DATABUS_MASK 0x00ff
#define DATABUS_SHIFT 0

#define ROMC_MASK 0x1f00
#define ROMC_SHIFT 8

#define WRITE_SHIFT 13
#define WRITE_MASK 1<<WRITE_SHIFT

#define PHI_SHIFT 14
#define PHI_MASK 1<<PHI_SHIFT

#define IRQ_IN_SHIFT 15
#define IRQ_IN_MASK 1<<IRQ_IN_SHIFT

#define IRQ_OUT_SHIFT 28
#define IRQ_OUT_MASK 1<<IRQ_OUT_SHIFT


#define DB_OE_SHIFT 16
#define DB_OE_MASK 1<<DB_OE_SHIFT

#define DB_OE_ENABLED 0
#define DB_OE_DISABLED 1

#define DB_DIR_SHIFT 17
#define DB_DIR_MASK 1<<DB_OE_SHIFT

#define DB_DIR_IN 1
#define DB_DIR_OUT 0


void bus_run();

