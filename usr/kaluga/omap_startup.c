/**
 * \file
 * \brief Provides a generic startup function for the ARM OMAP platform
 */
/*
 * Copyright (c) 2013, ETH Zurich.
 * Copyright (c) 2015, Hewlett Packard Enterprise Development LP.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetstr. 6, CH-8092 Zurich. Attn: Systems Group.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <barrelfish/barrelfish.h>
#include <barrelfish/spawn_client.h>

#include <arch/arm/omap44xx/device_registers.h>
#include <omap44xx_map.h>

#include "kaluga.h"

struct allowed_registers
{
    char* binary;
    lpaddr_t registers[][2];
};

static struct allowed_registers usb = {
    .binary = "hw.arm.omap44xx.usb",
    .registers =
    {
        {OMAP44XX_MAP_L4_CFG_HSUSBHOST, OMAP44XX_MAP_L4_CFG_HSUSBHOST_SIZE},
        {0x0, 0x0}
    }
};

static struct allowed_registers fdif = {
    .binary = "hw.arm.omap44xx.fdif",
    .registers =
    {
        {OMAP44XX_CAM_CM2, 0x1000},
        {OMAP44XX_DEVICE_PRM, 0x1000},
        {OMAP44XX_CAM_PRM, 0x1000},
        {OMAP44XX_MAP_L4_CFG_FACE_DETECT,OMAP44XX_MAP_L4_CFG_FACE_DETECT_SIZE},
        {0x0, 0x0}
    }
};

static struct allowed_registers mmchs = {
    .binary = "hw.arm.omap44xx.mmchs",
    .registers =
    {
        {OMAP44XX_CM2,        0x1000},
        {OMAP44XX_CLKGEN_CM2, 0x1000},
        {OMAP44XX_L4PER_CM2,  0x1000},
        // i2c
        {OMAP44XX_MAP_L4_PER_I2C1, OMAP44XX_MAP_L4_PER_I2C1_SIZE},
        {OMAP44XX_MAP_L4_PER_I2C2, OMAP44XX_MAP_L4_PER_I2C2_SIZE},
        {OMAP44XX_MAP_L4_PER_I2C3, OMAP44XX_MAP_L4_PER_I2C3_SIZE},
        {OMAP44XX_MAP_L4_PER_I2C4, OMAP44XX_MAP_L4_PER_I2C4_SIZE},
        // ctrlmodules
        {OMAP44XX_MAP_L4_CFG_SYSCTRL_GENERAL_CORE, OMAP44XX_MAP_L4_CFG_SYSCTRL_GENERAL_CORE_SIZE},
        {OMAP44XX_MAP_L4_WKUP_SYSCTRL_GENERAL_WKUP, OMAP44XX_MAP_L4_WKUP_SYSCTRL_GENERAL_WKUP_SIZE},
        {OMAP44XX_MAP_L4_CFG_SYSCTRL_PADCONF_CORE, OMAP44XX_MAP_L4_CFG_SYSCTRL_PADCONF_CORE_SIZE},
        {OMAP44XX_MAP_L4_WKUP_SYSCTRL_PADCONF_WKUP, OMAP44XX_MAP_L4_WKUP_SYSCTRL_PADCONF_WKUP_SIZE},
        // MMCHS
        {OMAP44XX_MAP_L4_PER_HSMMC1, OMAP44XX_MAP_L4_PER_HSMMC1_SIZE},
        {OMAP44XX_MAP_L4_PER_HSMMC2, OMAP44XX_MAP_L4_PER_HSMMC2_SIZE},
        {OMAP44XX_MAP_L4_PER_MMC_SD3, OMAP44XX_MAP_L4_PER_MMC_SD3_SIZE},
        {OMAP44XX_MAP_L4_PER_MMC_SD4, OMAP44XX_MAP_L4_PER_MMC_SD4_SIZE},
        {OMAP44XX_MAP_L4_PER_MMC_SD5, OMAP44XX_MAP_L4_PER_MMC_SD5_SIZE},
        {0x0, 0x0}
    }
};

static struct allowed_registers prcm = {
    .binary = "hw.arm.omap44xx.prcm",
    .registers =
    {
        {OMAP44XX_MAP_L4_WKUP_PRM, OMAP44XX_MAP_L4_WKUP_PRM_SIZE},
        {OMAP44XX_DEVICE_PRM, 0x1000},
        {OMAP44XX_MAP_L4_PER_I2C1, OMAP44XX_MAP_L4_PER_I2C1_SIZE},
        {OMAP44XX_MAP_L4_PER_I2C2, OMAP44XX_MAP_L4_PER_I2C2_SIZE},
        {OMAP44XX_MAP_L4_PER_I2C3, OMAP44XX_MAP_L4_PER_I2C3_SIZE},
        {OMAP44XX_MAP_L4_PER_I2C4, OMAP44XX_MAP_L4_PER_I2C4_SIZE},
        {0x0, 0x0}
    }
};

static struct allowed_registers uart = {
    .binary = "hw.arm.omap44xx.uart",
    .registers =
    {
        {OMAP44XX_MAP_L4_PER_UART1,OMAP44XX_MAP_L4_PER_UART1_SIZE},
        {OMAP44XX_MAP_L4_PER_UART2,OMAP44XX_MAP_L4_PER_UART2_SIZE},
        {OMAP44XX_MAP_L4_PER_UART3,OMAP44XX_MAP_L4_PER_UART3_SIZE},
        {OMAP44XX_MAP_L4_PER_UART4,OMAP44XX_MAP_L4_PER_UART4_SIZE},
        {0x0, 0x0}
    }
};

static struct allowed_registers sdma = {
    .binary = "hw.arm.omap44xx.sdma",
    .registers =
    {
        {OMAP44XX_MAP_L4_CFG_SDMA, OMAP44XX_MAP_L4_CFG_SDMA_SIZE},
        {0x0, 0x0}
    }
};

static struct allowed_registers* omap44xx[10] = {
    &usb,
    &fdif,
    &mmchs,
    &prcm,
    &uart,
    &sdma,
    NULL,
};

/**
 * \brief Startup function for OMAP drivers.
 *
 * Makes sure we get the device register capabilities.
 */
errval_t default_start_function(coreid_t where, struct module_info* driver,
        char* record)
{
    assert(driver != NULL);
    assert(record != NULL);

    errval_t err;

    // TODO Request the right set of caps and put in device_range_cap
    struct cnoderef dev_cnode;
    struct capref dev_cnode_cap;
    cslot_t retslots;
    err = cnode_create(&dev_cnode_cap, &dev_cnode, 255, &retslots);
    assert(err_is_ok(err));

    struct capref device_cap;
    device_cap.cnode = dev_cnode;
    device_cap.slot = 0;

    char* name;
    err = oct_read(record, "%s", &name);
    assert(err_is_ok(err));
    KALUGA_DEBUG("%s:%d: Starting driver for %s\n", __FUNCTION__, __LINE__, name);
    for (size_t i=0; omap44xx[i] != NULL; i++) {

        if(strcmp(name, omap44xx[i]->binary) != 0) {
            continue;
        }

        // Get the device cap from the managed capability tree
        // put them all in a single cnode
        for (size_t j=0; omap44xx[i]->registers[j][0] != 0x0; j++) {
            struct capref device_frame;
            KALUGA_DEBUG("%s:%d: mapping 0x%"PRIxLPADDR" %"PRIuLPADDR"\n", __FUNCTION__, __LINE__,
                   omap44xx[i]->registers[j][0], omap44xx[i]->registers[j][1]);

            lpaddr_t base = omap44xx[i]->registers[j][0] & ~(BASE_PAGE_SIZE-1);
            err = get_device_cap(base,
                                 omap44xx[i]->registers[j][1],
                                 &device_frame);
            assert(err_is_ok(err));

            err = cap_copy(device_cap, device_frame);
            assert(err_is_ok(err));
            device_cap.slot++;
        }
    }
    free(name);

    err = spawn_program_with_caps(0, driver->path, driver->argv, environ,
            NULL_CAP, dev_cnode_cap, 0, driver->did);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "Spawning %s failed.", driver->path);
        return err;
    }

    return SYS_ERR_OK;
}
