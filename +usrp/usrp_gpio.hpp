//
// Copyright 2014-15 Ettus Research LLC
// Copyright 2018 Ettus Research, a National Instruments Company
// Copyright 2019 Tim Woodford
//
// SPDX-License-Identifier: GPL-3.0-or-later
//// Utilities for dealing with GPIO interface for PA control
#include<uhd/usrp/multi_usrp.hpp>

extern void usrp_gpio_arm_trigger(uhd::usrp::multi_usrp::sptr usrp);
extern void usrp_gpio_disarm_trigger(uhd::usrp::multi_usrp::sptr usrp);
extern void usrp_gpio_spi(uhd::usrp::multi_usrp::sptr usrp, uint8_t *pkt, size_t num_pkt);