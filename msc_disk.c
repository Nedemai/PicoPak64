/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
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
 *
 */
#include "hardware/gpio.h"
#include "hardware/flash.h"
#include "bsp/board.h"
#include "tusb.h"
#include <stdio.h>

#define FLASH_TARGET_OFFSET (1048576)                                             // Location in Flash Rom of the controller pak data.
const uint8_t *flash_loc_usb = (const uint8_t *)(XIP_BASE + FLASH_TARGET_OFFSET); // flash_loc[i] for reading

#if CFG_TUD_MSC

// whether host does safe-eject
static bool ejected = false;

// Some MCU doesn't have enough 8KB SRAM to store the whole disk
// We will use Flash as read-only disk with board that has
// CFG_EXAMPLE_MSC_READONLY defined
int count = 0;

enum
{
  DISK_BLOCK_NUM = 132, // 8KB is the smallest size that windows allow to mount (be sure to accomidate for all sectors)
  DISK_BLOCK_SIZE = 512 // this is also the sector and cluster
  // 16 * 512 = 8kb
  // 100 * 512 = 51,200 bytes
};

#ifdef CFG_EXAMPLE_MSC_READONLY
const
#endif
    uint8_t msc_disk[DISK_BLOCK_NUM][DISK_BLOCK_SIZE] =
        {
            //------------- Block0: Boot Sector -------------//
            // byte_per_sector    = DISK_BLOCK_SIZE; fat12_sector_num_16  = DISK_BLOCK_NUM;
            // sector_per_cluster = 1; reserved_sectors = 1;
            // fat_num            = 1; fat12_root_entry_num = 16;
            // sector_per_fat     = 1; sector_per_track = 1; head_num = 1; hidden_sectors = 0;
            // drive_number       = 0x80; media_type = 0xf8; extended_boot_signature = 0x29;
            // filesystem_type    = "FAT12   "; volume_serial_number = 0x1234; volume_label = "TinyUSB MSC";
            // FAT magic code at offset 510-511
            {
                // 0,0
                0xEB, 0x3C, 0x90,                               // 0 -2 jump code
                0x4D, 0x53, 0x44, 0x4F, 0x53, 0x35, 0x2E, 0x30, // 3- 10 MSDOS5.0
                0x00, 0x02,                                     // 11-12 BYTES PER SECTOR
                0x01,                                           // 13 SECTORS PER CLUSTER
                0x01, 0x00,                                     // 14-15 Number of reserved sectors
                0x01,                                           // 16 Number of FATs
                0x10, 0x00,                                     // 17-18 max number of root directory enteries
                DISK_BLOCK_NUM + 3, 0x00,                       // 19-20 total number of sectors (set 0 and use 32-35 if large)
                0xF8,                                           // 21 IGNORE
                0x01, 0x00,                                     // 22-23 Sectors per fat
                0x01, 0x00,                                     // 24-25 sectors per track
                0x01, 0x00,                                     // 26-27 number of heads
                0x00, 0x00, 0x00, 0x00,                         // 28-31 IGNORE
                0x00, 0x00, 0x00, 0x00,                         // 32-35 total number of sectors for FAT32, fat 12 and 16 ignore
                0x80,                                           // 36 drive number, IGNORE
                0x00,                                           // 37 IGNORE
                0x29,                                           // 38 This inidicates the next few identifying fields are pressent
                0x34, 0x12, 0x00, 0x00,                         // 39-42 serial number
                'T', 'i', 'n', 'y', 'U',
                'S', 'B', ' ', 'M', 'S', 'C', // 43-53 volume ID

                0x46, 0x41, 0x54, 0x31, 0x32, 0x20, 0x20, 0x20, // 54-61 FILE SYSTEM TYPE (FAT12, FAT16 etc)
                0x00, 0x00,                                     // 62-509 "unused"

                // Zero up to 2 last bytes of FAT magic code
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x55, 0xAA // signature at end
            },                                                                                                 // 0x1FF is 512

            //------------- Block1: FAT12 Table -------------// 1,0
            {
                0xF8, 0xFF, 0xFF, // This is the first two, 12 bit enteries. THIS IS RESERVED
                                  // 11    21    22
                                  // LL    LH    HL
                0x03, 0x40, 0x00, // 2
                0x05, 0x60, 0x00, // 4
                0x07, 0x80, 0x00, // 6
                0x09, 0xA0, 0x00,
                0x0B, 0xC0, 0x00,
                0x0D, 0xE0, 0x00,
                0x0F, 0x00, 0x01,
                0x11, 0x20, 0x01,
                0x13, 0x40, 0x01,
                0x15, 0x60, 0x01, // 20
                0x17, 0x80, 0x01,
                0x19, 0xA0, 0x01,
                0x1B, 0xC0, 0x01,
                0x1D, 0xE0, 0x01,
                0x1F, 0x00, 0x02,
                0x21, 0x20, 0x02,
                0x23, 0x40, 0x02,
                0x25, 0x60, 0x02,
                0x27, 0x80, 0x02,
                0x29, 0xA0, 0x02,
                0x2B, 0xC0, 0x02,
                0x2D, 0xE0, 0x02,
                0x2F, 0x00, 0x03,
                0x31, 0x20, 0x03,
                0x33, 0x40, 0x03,
                0x35, 0x60, 0x03,
                0x37, 0x80, 0x03,
                0x39, 0xA0, 0x03,
                0x3B, 0xC0, 0x03,
                0x3D, 0xE0, 0x03,
                0x3F, 0x00, 0x04,
                0x41, 0xF0, 0xFF // 32 Pairs 0xFFF signifies last cluster
                                 // 32 PAIRS 64 Entries of FAT12 makes up a 32kb file

            },
            //------------- Block2: Root Directory -------------// 2,0
            {
                // first entry is volume label
                'T', 'i', 'n', 'y', 'U', 'S', 'B', ' ', 'M', 'S', 'C', 0x08, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4F, 0x6D, 0x65, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                // second entry is readme file
                'M', 'E', 'M', 'P', 'A', 'K', ' ', ' ', 'M', 'P', 'K', 0x20, 0x00, 0xC6, 0x52, 0x6D,
                0x65, 0x43, 0x65, 0x43, 0x00, 0x00, 0x88, 0x6D, 0x65, 0x43, 0x02, 0x00,
                0x00, 0x80, 0x00, 0x00 // readme's files size (4 Bytes)

            },
            //------------- Block3: -------------// 3,0
};

// Invoked when received SCSI_CMD_INQUIRY
// Application fill vendor id, product id and revision with string up to 8, 16, 4 characters respectively
void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4])
{
  (void)lun;

  const char vid[] = "TinyUSB";
  const char pid[] = "Mass Storage";
  const char rev[] = "1.0";

  memcpy(vendor_id, vid, strlen(vid));
  memcpy(product_id, pid, strlen(pid));
  memcpy(product_rev, rev, strlen(rev));
}

// Invoked when received Test Unit Ready command.
// return true allowing host to read/write this LUN e.g SD card inserted
bool tud_msc_test_unit_ready_cb(uint8_t lun)
{
  (void)lun;
count = 0;
  // This loads FLASH ROM data into RAM to be accessed from USB, and probably is bad placing here.
  for (int i = 0; i < 64; i++)
  {
    for (int j = 0; j < DISK_BLOCK_SIZE; j++) // DISK_BLOCK_SIZE
    {
      msc_disk[i + 3][j] = flash_loc_usb[(i * 512) + j];
    }
  }

  // RAM disk is ready until ejected
  if (ejected)
  {
    tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
    return false;
  }

  return true;
}

// Invoked when received SCSI_CMD_READ_CAPACITY_10 and SCSI_CMD_READ_FORMAT_CAPACITY to determine the disk size
// Application update block count and block size
void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size)
{
  (void)lun;

  *block_count = DISK_BLOCK_NUM;
  *block_size = DISK_BLOCK_SIZE;
}

// Invoked when received Start Stop Unit command
// - Start = 0 : stopped power mode, if load_eject = 1 : unload disk storage
// - Start = 1 : active mode, if load_eject = 1 : load disk storage
bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject)
{
  (void)lun;
  (void)power_condition;

  if (load_eject)
  {
    if (start)
    {
      // load disk storage
    }
    else
    {
      // unload disk storage
      ejected = true;
    }
  }

  return true;
}

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and return number of copied bytes.
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize)
{
  (void)lun;

  // out of ramdisk
  if (lba >= DISK_BLOCK_NUM)
    return -1;

  uint8_t const *addr = msc_disk[lba] + offset;
  memcpy(buffer, addr, bufsize);

  return bufsize;
}

bool tud_msc_is_writable_cb(uint8_t lun)
{
  (void)lun;

#ifdef CFG_EXAMPLE_MSC_READONLY
  return false;
#else
  return true;
#endif
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and return number of written bytes
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize)
{
  (void)lun;

  // out of ramdisk
  if (lba >= DISK_BLOCK_NUM)
    return -1;

 count = count + 1;
    if(count >= 66)//64*512 is 32kb + 1024 bytes for FAT and root directory. = 66
    {
      count = 0;
      gpio_put(25,1);
      uint32_t ints = save_and_disable_interrupts();
      flash_range_erase(FLASH_TARGET_OFFSET, (32768UL + 1)); // 4kb * 7 = 32kb must be multiple of 4kb
      sleep_us(1);
      flash_range_program(FLASH_TARGET_OFFSET, &msc_disk[3][0], (32768UL + 1)); // 256 * 36 = 32kb must be multiple of 256
      restore_interrupts(ints);
      sleep_ms(500);
      gpio_put(25, 0);
    }

#ifndef CFG_EXAMPLE_MSC_READONLY
  uint8_t *addr = msc_disk[lba] + offset;
  memcpy(addr, buffer, bufsize);
#else
  (void)lba;
  (void)offset;
  (void)buffer;
#endif

  return bufsize;
}

// Callback invoked when received an SCSI command not in built-in list below
// - READ_CAPACITY10, READ_FORMAT_CAPACITY, INQUIRY, MODE_SENSE6, REQUEST_SENSE
// - READ10 and WRITE10 has their own callbacks
int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void *buffer, uint16_t bufsize)
{
  // read10 & write10 has their own callback and MUST not be handled here

  void const *response = NULL;
  int32_t resplen = 0;

  // most scsi handled is input
  bool in_xfer = true;

  switch (scsi_cmd[0])
  {
  case SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL:
    // Host is about to read/write etc ... better not to disconnect disk
    resplen = 0;
    break;

  default:
    // Set Sense = Invalid Command Operation
    tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);

    // negative means error -> tinyusb could stall and/or response with failed status
    resplen = -1;
    break;
  }

  // return resplen must not larger than bufsize
  if (resplen > bufsize)
    resplen = bufsize;

  if (response && (resplen > 0))
  {
    if (in_xfer)
    {
      memcpy(buffer, response, resplen);
    }
    else
    {
      // SCSI output
    }
  }

  return resplen;
}

#endif
