/* -----------------------------------------------------------------------------
 * Copyright (c) 2019-2022 Arm Limited (or its affiliates). All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *
 * $Date:        4. April 2022
 * $Revision:    V1.2
 *
 * Project:      WiFi Driver Configuration for 
 *               Inventek ISM43362-M3G-L44 WiFi Module (SPI variant)
 * -------------------------------------------------------------------------- */

/*
 * MODIFIED for DISCO_L475VG_IOT01A_Thingspeak (B-L475E-IOT01A), 2026-11-12. Copy of
 * Drivers/CMSIS-Driver/WiFi/ISM43362/Config/WiFi_ISM43362_Config.h (CMSIS-Driver 2.11.0) with:
 *   - WIFI_ISM43362_SPI_DRV_NUM   0 -> 3          (Driver_SPI3, Core/Src/cmsis_driver_spi3.c, SPI3)
 *   - WIFI_ISM43362_SPI_BUS_SPEED 20000000 -> 10000000  (10 Mbit/s: SPI3 as configured in
 *                                    STM32CubeMX, and the speed ST's own driver uses on this board)
 * All other values unchanged. This file is found before the original because it is on the C
 * compiler's include path (Core/Config) and the original's folder (Config/) is not.
 */

//-------- <<< Use Configuration Wizard in Context Menu >>> --------------------

#ifndef __WIFI_ISM43362_CONFIG_H
#define __WIFI_ISM43362_CONFIG_H

// <h> WiFi Inventek ISM43362 Driver Configuration (SPI)

//   <o> WiFi Driver Number
//   <i> Configuration settings specifying driver number (default: Driver_WiFi0)
#define WIFI_ISM43362_DRV_NUM               0

//   <o> SPI Driver Number
//   <i> Configuration settings specifying SPI driver number used (if Module uses SPI interface)
#define WIFI_ISM43362_SPI_DRV_NUM           3

//   <o> Memory pool size
//   <i> Configuration settings specifying memory pool size for receive packets buffering
#define WIFI_ISM43362_MEM_POOL_SIZE         16384

// </h>

// Number of sockets supported by Module (default and maximum: 4)
#define WIFI_ISM43362_SOCKETS_NUM          (4)

// SPI bus speed (default: 20 Mbps)
#define WIFI_ISM43362_SPI_BUS_SPEED        (10000000)

// SPI mutex acquire timeout in ms (default: 1000)
#define WIFI_ISM43362_SPI_TIMEOUT          (1000)

// SPI receive transfer size (default: 32)
// This setting specifies chunk size in which SPI read is done
#define WIFI_ISM43362_SPI_RECEIVE_SIZE     (32)

// SPI command timeout in ms (default: 30000)
// Maximum time that command can keep the SPI DATARDY line busy
#define WIFI_ISM43362_CMD_TIMEOUT          (30000)

// Asynchronous thread polling time interval in ms (default: 8)
// Interval in which asynchronous events are polled and also interval 
// in which long blocking socket receive is split
#define WIFI_ISM43362_ASYNC_INTERVAL       (8)

// Asynchronous thread stack size (default: 1024)
#define WIFI_ISM43362_ASYNC_THREAD_STACK_SIZE  (1024)

// Asynchronous thread priority (default: osPriorityAboveNormal)
// This priority should be above user threads
#define WIFI_ISM43362_ASYNC_PRIORITY       (osPriorityAboveNormal)

#endif // __WIFI_ISM43362_CONFIG_H
