//
// Copyright 2014-15 Ettus Research LLC
// Copyright 2018 Ettus Research, a National Instruments Company
// Copyright 2019 Tim Woodford
//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#pragma once

#include <uhd/usrp/multi_usrp.hpp>

extern void recv_to_file_fc(uhd::usrp::multi_usrp::sptr usrp,
    uhd::rx_streamer::sptr rx_stream,
    const std::string& file,
    size_t samps_per_buff,
    int num_requested_samples,
    double settling_time,
    std::vector<size_t> rx_channel_nums);

extern void send_from_file(
    uhd::tx_streamer::sptr tx_stream,
    const std::string &file,
    size_t samps_per_buff,
    size_t num_channels,
    uhd::tx_metadata_t md);

extern bool check_clear_underflow();
