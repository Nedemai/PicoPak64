
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
 */

#include "pin_layout.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "strings.h"

#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "hardware/flash.h"

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"
#include "pico/multicore.h"

#include "bsp/board.h"
#include "tusb.h"

// RAM IS 264KB, and we are running all code from ram.
// if code compiles to be larger than 1MB, I've got other issues
#define FLASH_TARGET_OFFSET (1048576)                                         // Location in Flash Rom of the controller pak data.
const uint8_t *flash_loc = (const uint8_t *)(XIP_BASE + FLASH_TARGET_OFFSET); // flash_loc[i] for reading

uint8_t Mempak[32768UL + 1];

void setup_gpio_pins();
void cdc_task(void);

void set_d_input()
{
  gpio_set_dir(D0, GPIO_IN);
  gpio_set_dir(D1, GPIO_IN);
  gpio_set_dir(D2, GPIO_IN);
  gpio_set_dir(D3, GPIO_IN);
  gpio_set_dir(D4, GPIO_IN);
  gpio_set_dir(D5, GPIO_IN);
  gpio_set_dir(D6, GPIO_IN);
  gpio_set_dir(D7, GPIO_IN);
}

void set_d_output()
{
  gpio_set_dir(D0, GPIO_OUT);
  gpio_set_dir(D1, GPIO_OUT);
  gpio_set_dir(D2, GPIO_OUT);
  gpio_set_dir(D3, GPIO_OUT);
  gpio_set_dir(D4, GPIO_OUT);
  gpio_set_dir(D5, GPIO_OUT);
  gpio_set_dir(D6, GPIO_OUT);
  gpio_set_dir(D7, GPIO_OUT);
}

//------ CORE 1 ----------------------------------------------
// A FEW NOTES
// ALL CORE NEEDS TO RUN IN RAM FOR THIS TO SUCCEED. SINCE WE NEED TO WRITE TO THE FLASH ROM
// WHILE BOTH CORES ARE EXECUTING CODE, THIS CODE __MUST__ BE IN RAM. SINCE IT CANNOT ACCESS
// THE ROM MEMORY
void __no_inline_not_in_flash_func(core1_entry())
{

  uint32_t data_in = 0;
  sleep_ms(10);
  while (true) // will run indefinitely, is seperate from the other processor, uses own ram.
  {

    if (!multicore_fifo_rvalid())
    {
      data_in = 0;
    }
    else
    {
      data_in = sio_hw->fifo_rd;
    }
    // data_in = multicore_fifo_pop_blocking(); // grabs data from other core

    switch (data_in)
    {
    case 0x01:
      gpio_put(25, 1);
      uint32_t ints = save_and_disable_interrupts();
      flash_range_erase(FLASH_TARGET_OFFSET, (32768UL + 1)); // 4kb * 7 = 32kb must be multiple of 4kb
      sleep_us(1);
      flash_range_program(FLASH_TARGET_OFFSET, Mempak, (32768UL + 1)); // 256 * 36 = 32kb must be multiple of 256
      restore_interrupts(ints);
      sleep_ms(2500);
      gpio_put(25, 0);
      break;
    default:
      tud_task(); // tinyusb device task
      cdc_task();
      break;
    }
  }
}
//------ CORE 1 ----------------------------------------------

//--- CORE 0 -----------------------------------------------------------

int main() // START OF SRAM "EMULATION" but this is just using picos SRAM with extra steps
{
  board_init();
  tusb_init();

  multicore_launch_core1(core1_entry);
  int delay = 0;
  int address = 0;
  // int data_in = 0;
  uint8_t write = 0;

  setup_gpio_pins();

  for (int i = 0; i <= (32768UL); i++)
  {
    Mempak[i] = flash_loc[i]; // Load Flash Memory into Ram
  }

  while (true)
  {

    address = ((sio_hw->gpio_in) & 0x7FFF);

    if (gpio_get(CE)) // Checks state of Chip Enable
    {
      delay = 0;
      if (gpio_get(WE)) // Checks state of Write Enable
      {
        set_d_output();
        gpio_put_masked(0x7F8000, Mempak[address] << 15); // Give Data to the Controller
      }
      else
      {
        set_d_input();
        Mempak[address] = (((sio_hw->gpio_in) & 0x7F8000) >> 15); // optimized for speed

        if (address > 768) // Most carts seem to dink around with the first 3 pages continuously. If the address is above we know we are saving the game.
        {
          write = 1;
        }
      }
    }

    if (!gpio_get(CE))
    {
      delay = delay + 1;
      if (delay > 75000000) // this wait is a little insane but needs to be sure the memory card is settled before writing to the rom
      {
        delay = 0;
        multicore_fifo_push_blocking(write); // command
                                             //  multicore_fifo_push_timeout_us(write,10);
        write = 0;
      }
    }
  }
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void)remote_wakeup_en;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
}

//--------------------------------------------------------------------+
// USB CDC
//--------------------------------------------------------------------+
void cdc_task(void)
{
  // connected() check for DTR bit
  // Most but not all terminal client set this when making connection
  // if ( tud_cdc_connected() )
  {
    // connected and there are data available
    if (tud_cdc_available())
    {
      // read datas
      char buf[64];
      uint32_t count = tud_cdc_read(buf, sizeof(buf));
      (void)count;

      // Echo back
      // Note: Skip echo by commenting out write() and write_flush()
      // for throughput test e.g
      //    $ dd if=/dev/zero of=/dev/ttyACM0 count=10000
      tud_cdc_write(buf, count);
      tud_cdc_write_flush();
    }
  }
}

// Invoked when cdc when line state changed e.g connected/disconnected
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
  (void)itf;
  (void)rts;

  // TODO set some indicator
  if (dtr)
  {
    // Terminal connected
  }
  else
  {
    // Terminal disconnected
  }
}

// Invoked when CDC interface received data from host
void tud_cdc_rx_cb(uint8_t itf)
{
  (void)itf;
}
// ----------- USB END ------------------------------------------------

void setup_gpio_pins()
{
  // Address pins.
  gpio_init(A0);
  gpio_set_dir(A0, GPIO_IN);
  gpio_init(A1);
  gpio_set_dir(A1, GPIO_IN);
  gpio_init(A2);
  gpio_set_dir(A2, GPIO_IN);
  gpio_init(A3);
  gpio_set_dir(A3, GPIO_IN);
  gpio_init(A4);
  gpio_set_dir(A4, GPIO_IN);
  gpio_init(A5);
  gpio_set_dir(A5, GPIO_IN);
  gpio_init(A6);
  gpio_set_dir(A6, GPIO_IN);
  gpio_init(A7);
  gpio_set_dir(A7, GPIO_IN);
  gpio_init(A8);
  gpio_set_dir(A8, GPIO_IN);
  gpio_init(A9);
  gpio_set_dir(A9, GPIO_IN);
  gpio_init(A10);
  gpio_set_dir(A10, GPIO_IN);
  gpio_init(A11);
  gpio_set_dir(A11, GPIO_IN);
  gpio_init(A12);
  gpio_set_dir(A12, GPIO_IN);
  gpio_init(A13);
  gpio_set_dir(A13, GPIO_IN);
  gpio_init(A14);
  gpio_set_dir(A14, GPIO_IN);

  // Data pins.
  gpio_init(D0);
  gpio_set_dir(D0, GPIO_OUT);
  gpio_set_slew_rate(D0, GPIO_SLEW_RATE_FAST);
  gpio_init(D1);
  gpio_set_dir(D1, GPIO_OUT);
  gpio_set_slew_rate(D1, GPIO_SLEW_RATE_FAST);
  gpio_init(D2);
  gpio_set_dir(D2, GPIO_OUT);
  gpio_set_slew_rate(D2, GPIO_SLEW_RATE_FAST);
  gpio_init(D3);
  gpio_set_dir(D3, GPIO_OUT);
  gpio_set_slew_rate(D3, GPIO_SLEW_RATE_FAST);
  gpio_init(D4);
  gpio_set_dir(D4, GPIO_OUT);
  gpio_set_slew_rate(D4, GPIO_SLEW_RATE_FAST);
  gpio_init(D5);
  gpio_set_dir(D5, GPIO_OUT);
  gpio_set_slew_rate(D5, GPIO_SLEW_RATE_FAST);
  gpio_init(D6);
  gpio_set_dir(D6, GPIO_OUT);
  gpio_set_slew_rate(D6, GPIO_SLEW_RATE_FAST);
  gpio_init(D7);
  gpio_set_dir(D7, GPIO_OUT);
  gpio_set_slew_rate(D7, GPIO_SLEW_RATE_FAST);

  // Control Pins.
  gpio_init(CE); // CHIP ENABLE
  gpio_set_dir(CE, GPIO_IN);
  gpio_set_slew_rate(CE, GPIO_SLEW_RATE_FAST);

  gpio_init(WE); // WRITE ENABLE
  gpio_set_dir(WE, GPIO_IN);
  gpio_set_slew_rate(WE, GPIO_SLEW_RATE_FAST);

  gpio_init(OE); // OUTPUT ENABLE
  gpio_set_dir(OE, GPIO_OUT);
  gpio_set_slew_rate(OE, GPIO_SLEW_RATE_FAST);

  gpio_init(25); // LED
  gpio_set_dir(25, GPIO_OUT);
}
