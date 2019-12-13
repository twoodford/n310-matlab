//
// Copyright 2014-15 Ettus Research LLC
// Copyright 2018 Ettus Research, a National Instruments Company
// Copyright 2019 Tim Woodford
//
// SPDX-License-Identifier: GPL-3.0-or-later
//#include "usrp_io.hpp"
#include <boost/filesystem.hpp>
#include <chrono>
#include <uhd/utils/thread.hpp>

static bool stop_signal_called = false;
static bool tx_underflowed = false;

/***********************************************************************
 * Utilities
 **********************************************************************/
//! Change to filename, e.g. from usrp_samples.dat to usrp_samples.00.dat,
//  but only if multiple names are to be generated.
std::string generate_out_filename(
    const std::string& base_fn, size_t n_names, size_t this_name)
{
    if (n_names == 1) {
        // Matlab side assumes we always create multiple files
        // So uncommenting this line may cause lots of unhappiness
        //return base_fn;
    }

    boost::filesystem::path base_fn_fp(base_fn);
    base_fn_fp.replace_extension(boost::filesystem::path(
        str(boost::format("%02d%s") % this_name % base_fn_fp.extension().string())));
    return base_fn_fp.string();
}

//! Check if an underflow occurred, and clear the error variable
bool check_clear_underflow() {
    bool val = tx_underflowed;
    tx_underflowed = false;
    return val;
}

/*******************************************************
 * send_from_file
 ******************************************************/
void send_from_file(
    uhd::tx_streamer::sptr tx_stream,
    const std::string &file,
    size_t samps_per_buff,
    size_t num_channels,
    uhd::tx_metadata_t md
){
    // Give this thread realtime
    uhd::set_thread_priority_safe();

    // Buffers
    std::vector<std::vector<std::complex<float>>> buffs(
        num_channels, std::vector<std::complex<float>>(samps_per_buff));
    std::vector<std::complex<float>*> buff_ptrs;
    for (size_t i = 0; i < buffs.size(); i++) {
        buff_ptrs.push_back(&buffs[i].front());
    }

    // Open files
    std::vector<boost::shared_ptr<std::ifstream>> infiles;
    for (size_t i = 0; i < buffs.size(); i++) {
        const std::string this_filename = generate_out_filename(file, buffs.size(), i);
        std::cout << "Opening tx data: " << this_filename<< std::endl;
        infiles.push_back(boost::shared_ptr<std::ifstream>(
            new std::ifstream(this_filename.c_str(), std::ofstream::binary)));
        if(infiles[i]->fail()) {
            std::cerr << "Failed to open file (" << strerror(errno) << "!  Will not transmit" << std::endl;
        }
    }
    UHD_ASSERT_THROW(infiles.size() == buffs.size());

    //loop until the entire file has been read
    size_t underflows = 0;
    auto start = std::chrono::system_clock::now();
    while(not md.end_of_burst and not stop_signal_called){

        // Fill all tx buffers
        size_t num_tx_samps;
        for(size_t ch = 0; ch < num_channels; ch++) {
            infiles[ch]->read((char*) buff_ptrs[ch], samps_per_buff*sizeof(std::complex<float>));
            num_tx_samps = size_t(infiles[ch]->gcount()/sizeof(std::complex<float>));

            md.end_of_burst = infiles[ch]->eof();

            if(infiles[ch]->fail() && !(infiles[ch]->eof())) { // Ignore any errors if we're at EOF anyways, I guess
                if (errno==EAGAIN) {
                    // It is very puzzling that I would get this error, since the file shouldn't be opened in non-blocking mode (?)
                    // It seems to occur at EOF, so we might never actually reach this point
                    continue;
                }
                std::cout << "tx read failed: " << strerror(errno) << std::endl;
                md.end_of_burst = true;
            }
        }
        // Send all tx buffers
        tx_stream->send(buff_ptrs, num_tx_samps, md);

        // Check for async messages (underflow)
        uhd::async_metadata_t async_msg;
        if(tx_stream->recv_async_msg(async_msg)) {
            switch (async_msg.event_code) {
                case uhd::async_metadata_t::EVENT_CODE_SEQ_ERROR:
                case uhd::async_metadata_t::EVENT_CODE_TIME_ERROR:
                    std::cout << "Sequence or time error" << std::endl;
                    break;
                case uhd::async_metadata_t::EVENT_CODE_UNDERFLOW_IN_PACKET:
                    std::cout << "Underflow in packet" << std::endl;
                case uhd::async_metadata_t::EVENT_CODE_UNDERFLOW:
                    underflows++;
                    tx_underflowed = true;
                    break;
                default:
                    break;
            }
        }

        md.start_of_burst = false; // Corrected from example file
        md.has_time_spec  = false;
    }
    auto end = std::chrono::system_clock::now();
    auto duration = (end-start);
    std::cout << "Elapsed time for tx: " << ((double) duration.count())/1000000000 << std::endl;

    if (underflows > 0) {
        std::cout << "Warning: " << underflows << " underflows!" << std::endl;
        tx_underflowed = true;
    }

    for(size_t ch = 0; ch < num_channels; ch++) {
        infiles[ch]->close();
    }
}

/***********************************************************************
 * recv_to_file function
 **********************************************************************/
template <typename samp_type>
samp_type *recv_to_file(uhd::usrp::multi_usrp::sptr usrp,
    uhd::rx_streamer::sptr rx_stream,
    const std::string& file,
    size_t samps_per_buff,
    int num_requested_samples,
    double settling_time,
    std::vector<size_t> rx_channel_nums)
{
    int num_total_samps = 0;

    // Prepare buffers for received samples and metadata
    uhd::rx_metadata_t md;
    std::vector<std::vector<samp_type>> buffs(
        rx_channel_nums.size(), std::vector<samp_type>(samps_per_buff));
    // create a vector of pointers to point to each of the channel buffers
    std::vector<samp_type*> buff_ptrs;
    for (size_t i = 0; i < buffs.size(); i++) {
        buff_ptrs.push_back(&buffs[i].front());
    }

    // Create one ofstream object per channel
    // (use shared_ptr because ofstream is non-copyable)
    std::vector<boost::shared_ptr<std::ofstream>> outfiles;
    for (size_t i = 0; i < buffs.size(); i++) {
        const std::string this_filename = generate_out_filename(file, buffs.size(), i);
        outfiles.push_back(boost::shared_ptr<std::ofstream>(
            new std::ofstream(this_filename.c_str(), std::ofstream::binary)));
    }
    UHD_ASSERT_THROW(outfiles.size() == buffs.size());
    UHD_ASSERT_THROW(buffs.size() == rx_channel_nums.size());
    bool overflow_message = true;
    double timeout =
        settling_time + 0.1f; // expected settling time + padding for first recv

    while (not stop_signal_called
           and (num_requested_samples > num_total_samps or num_requested_samples == 0)) {
        size_t num_rx_samps = rx_stream->recv(buff_ptrs, samps_per_buff, md, timeout);
        timeout             = 0.1f; // small timeout for subsequent recv

        if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_TIMEOUT) {
            std::cout << boost::format("Timeout while streaming") << std::endl;
            break;
        }
        if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_OVERFLOW) {
            if (overflow_message) {
                overflow_message = false;
                std::cerr
                    << boost::format(
                           "Got an overflow indication. Please consider the following:\n"
                           "  Your write medium must sustain a rate of %fMB/s.\n"
                           "  Dropped samples will not be written to the file.\n"
                           "  Please modify this example for your purposes.\n"
                           "  This message will not appear again.\n")
                           % (usrp->get_rx_rate() * sizeof(samp_type) / 1e6);
            }
            continue;
        }
        if (md.error_code != uhd::rx_metadata_t::ERROR_CODE_NONE) {
            throw std::runtime_error(
                str(boost::format("Receiver error %s") % md.strerror()));
        }

        num_total_samps += num_rx_samps;

        for (size_t i = 0; i < outfiles.size(); i++) {
            outfiles[i]->write(
                (const char*)buff_ptrs[i], num_rx_samps * sizeof(samp_type));
        }
    }

    // Shut down receiver
    uhd::stream_cmd_t stream_cmd(uhd::stream_cmd_t::STREAM_MODE_NUM_SAMPS_AND_DONE);
    stream_cmd.stream_mode = uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS;
    rx_stream->issue_stream_cmd(stream_cmd);

    // Close files
    for (size_t i = 0; i < outfiles.size(); i++) {
        outfiles[i]->close();
    }
}

std::complex<float> *recv_to_file(uhd::usrp::multi_usrp::sptr usrp,
    uhd::rx_streamer::sptr rx_stream,
    const std::string& file,
    size_t samps_per_buff,
    int num_requested_samples,
    double settling_time,
    std::vector<size_t> rx_channel_nums) {
        return recv_to_file(usrp,rx_stream,file,samps_per_buff,num_requested_samples,settling_time,rx_channel_nums);
}