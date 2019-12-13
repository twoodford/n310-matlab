//
// Copyright 2014-15 Ettus Research LLC
// Copyright 2018 Ettus Research, a National Instruments Company
// Copyright 2019 Tim Woodford
//
// SPDX-License-Identifier: GPL-3.0-or-later
//#include "usrp_gpio.hpp"
#include <unistd.h>

// General definitions
#define GPIO_PANEL "FP0"
#define ALL_BITS 0xfff

// SPI definitions - not currently in use, but might be someday
#define SCK_BIT (0x1 << 1)
#define SS_BIT (0x1 << 2)
#define MOSI_BITS (0xff << 4)
#define SPI_BITS ( SCK_BIT | SS_BIT | MOSI_BITS )

// ATR configuration
#define TRIGGER_BIT (0x001 << 3)
#define TX_BIT (0x001 << 1)
#define RX_BIT (0x001 << 2)
#define ATR_MASK ( TRIGGER_BIT | TX_BIT | RX_BIT )

void usrp_gpio_arm_trigger(uhd::usrp::multi_usrp::sptr usrp)
{
    // Set bit to automatic control mode
    usrp->set_gpio_attr(GPIO_PANEL, "CTRL", ATR_MASK, ATR_MASK);
    // Set trigger as output
    usrp->set_gpio_attr(GPIO_PANEL, "DDR", ATR_MASK, ATR_MASK);
    // Set up trigger
    usrp->set_gpio_attr(GPIO_PANEL, "ATR_TX", TRIGGER_BIT | TX_BIT, ATR_MASK);
    usrp->set_gpio_attr(GPIO_PANEL, "ATR_RX", TRIGGER_BIT | RX_BIT, ATR_MASK);
    usrp->set_gpio_attr(GPIO_PANEL, "ATR_XX", ATR_MASK, ATR_MASK);
    usrp->set_gpio_attr(GPIO_PANEL, "ATR_0X", 0, ATR_MASK);
}

void usrp_gpio_disarm_trigger(uhd::usrp::multi_usrp::sptr usrp)
{
    usrp->set_gpio_attr(GPIO_PANEL, "DDR", ATR_MASK, ATR_MASK);
    // Should stay 0 in all circumstances
    usrp->set_gpio_attr(GPIO_PANEL, "OUT", 0, ATR_MASK);
    usrp->set_gpio_attr(GPIO_PANEL, "ATR_TX", 0, ATR_MASK);
    usrp->set_gpio_attr(GPIO_PANEL, "ATR_RX", 0, ATR_MASK);
    usrp->set_gpio_attr(GPIO_PANEL, "ATR_XX", 0, ATR_MASK);
    usrp->set_gpio_attr(GPIO_PANEL, "ATR_0X", 0, ATR_MASK);
    // Resume manual mode
    usrp->set_gpio_attr(GPIO_PANEL, "CTRL", 0, ATR_MASK);

}

void usrp_gpio_spi(uhd::usrp::multi_usrp::sptr usrp, uint8_t *pkt, size_t num_pkt)
{
    // Manual control
    usrp->set_gpio_attr(GPIO_PANEL, "CTRL", 0, SPI_BITS);
    // Output mode
    usrp->set_gpio_attr(GPIO_PANEL, "DDR", SPI_BITS, SPI_BITS);
    // Reset values
    usrp->set_gpio_attr(GPIO_PANEL, "OUT", SS_BIT, SS_BIT); // First ensure ~SS is high
    // CLK starts at 0, slave reads on rising edge
    usrp->set_gpio_attr(GPIO_PANEL, "OUT", 0, SCK_BIT); 
    usrp->set_gpio_attr(GPIO_PANEL, "OUT", 0, MOSI_BITS);
    // Delay for some amount of time to make sure values are read
    usleep(10000); // 10 ms
    // SS goes low to start transmission
    usrp->set_gpio_attr(GPIO_PANEL, "OUT", 0, SS_BIT);

    // Transmission
    uint8_t *limit = pkt + num_pkt;
    for( ; pkt < limit; pkt++) {
        // CLK low, update data
        usrp->set_gpio_attr(GPIO_PANEL, "OUT", 0, SCK_BIT);
        usrp->set_gpio_attr(GPIO_PANEL, "OUT", *pkt, MOSI_BITS);
        // Delay
        usleep(10000); // 10 ms
        // CLK high, data constant
        usrp->set_gpio_attr(GPIO_PANEL, "OUT", SCK_BIT, SCK_BIT);
        usleep(10000); // 10 ms
    }
    // Reset all values
    usrp->set_gpio_attr(GPIO_PANEL, "OUT", 0, SCK_BIT);
    usrp->set_gpio_attr(GPIO_PANEL, "OUT", 0, MOSI_BITS);
    usrp->set_gpio_attr(GPIO_PANEL, "OUT", SS_BIT, SS_BIT); 
}