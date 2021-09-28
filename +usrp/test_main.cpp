#include <uhd/exception.hpp>
#include <uhd/types/tune_request.hpp>
#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/utils/static.hpp>
#include <uhd/utils/thread.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/math/special_functions/round.hpp>
#include <boost/thread/thread.hpp>
#include <csignal>
#include <fstream>
#include <iostream>
#include <chrono>
#include "usrp_gpio.hpp"
#include "usrp_io.hpp"

void usrp_rx_start(uhd::rx_streamer::sptr stream, size_t num_samps, double start_time)
{
    uhd::stream_cmd_t stream_cmd(uhd::stream_cmd_t::STREAM_MODE_NUM_SAMPS_AND_DONE);
    stream_cmd.num_samps = num_samps;
    stream_cmd.time_spec = uhd::time_spec_t(start_time);
    stream_cmd.stream_now = false;
    stream->issue_stream_cmd(stream_cmd);
}

int main() {
    std::string addr = "";
    int num_channels = 2;
    double fs = 125000000;
    double fc = 2000000000;
    int tx_gain = 0;
    int rx_gain = 0;

    // Check if there's already an existing instance in the global
    bool existing_inst = false; //(global_usrp != NULL) ? true : false;
    // Connect
    uhd::usrp::multi_usrp::sptr tx_usrp = uhd::usrp::multi_usrp::make(addr);
    uhd::usrp::multi_usrp::sptr rx_usrp = uhd::usrp::multi_usrp::make(addr);
    // Channels 0...n_channels-1
    std::vector<size_t> channel_nums;
    for (size_t i = 0; i < num_channels; i++)
        channel_nums.push_back(i);
    // Clocks
    tx_usrp->set_clock_source("internal");
    rx_usrp->set_clock_source("internal");
    // DBG from example file
    std::cout << boost::format("Using TX Device: %s") % tx_usrp->get_pp_string()
              << std::endl;
    std::cout << boost::format("Using RX Device: %s") % rx_usrp->get_pp_string()
              << std::endl;
    // Sample rate
    tx_usrp->set_tx_rate(fs);
    rx_usrp->set_rx_rate(fs);
    std::cout << boost::format("Actual TX Rate: %f Msps") % (tx_usrp->get_tx_rate() / 1e6)
              << std::endl;
    std::cout << boost::format("Actual RX Rate: %f Msps") % (rx_usrp->get_rx_rate() / 1e6)
              << std::endl;
    // Per-channel tx
    for (size_t ch = 0; ch < num_channels; ch++)
    {
        uhd::tune_request_t tx_tune_request(fc);
        tx_usrp->set_tx_freq(tx_tune_request, ch);
        std::cout << boost::format("Actual TX Freq: %f MHz...") % (tx_usrp->get_tx_freq(ch) / 1e6) << std::endl;
        tx_usrp->set_tx_gain(tx_gain, ch);
    }
    // Per-channel rx
    for (size_t ch = 0; ch < num_channels; ch++)
    {
        uhd::tune_request_t rx_tune_request(fc);
        rx_usrp->set_rx_freq(rx_tune_request, ch);
        std::cout << boost::format("Actual TX Freq: %f MHz...") % (tx_usrp->get_rx_freq(ch) / 1e6) << std::endl;
        rx_usrp->set_rx_gain(rx_gain, ch);
        std::cout << boost::format("Actual RX Gain: %f dB...") % rx_usrp->get_rx_gain(ch) << std::endl;
    }
    // Create streamer objects
    // We're setting the over-the-wire sample mode to sc16, since that's the default
    // in the example file
    // fc32=float type (32-bit floating point)
    // sc16=short type (16-bit integer)
    uhd::stream_args_t stream_args("fc32", "sc16");
    stream_args.channels = channel_nums;
    uhd::tx_streamer::sptr tx_stream = tx_usrp->get_tx_stream(stream_args);
    uhd::rx_streamer::sptr rx_stream = rx_usrp->get_rx_stream(stream_args);
    // Ensure LO is locked
    std::vector<std::string> tx_sensor_names = tx_usrp->get_tx_sensor_names(0);
    if (std::find(tx_sensor_names.begin(), tx_sensor_names.end(), "lo_locked") != tx_sensor_names.end())
    {
        uhd::sensor_value_t lo_locked = tx_usrp->get_tx_sensor("lo_locked", 0);
        if (!lo_locked.to_bool())
        {
            std::cerr << ("Couldn't lock LO") << std::endl;
        }
    }

    size_t num_samp_rx, num_chan;
    //char tx_basepath[128], rx_basepath[128];
    num_samp_rx = 100;
    num_chan = 2;
    char *tx_basepath = "/run/user/1000/tp036d5ce0_b6ed_496b_a841_716b92ba3e73.dat";
    char *rx_basepath = "/run/user/1000/tp3cb104d6_f198_4371_8884_fb94ec7967cd.dat";

    // Create set of channels.  For now, we're making some assumptions.
    std::vector<size_t> chans;
    for (size_t i = 0; i < num_chan; i++)
    {
        chans.push_back(i);
    }
    // Allocate matrix for rx data
    // Should have num_samp rows and num_chan columns
    // This is for memory layout purposes, since Matlab does column-major formatting
    // This means each column is stored contiguously, so we want each column to be a buffer
    // Matlab does things this way because it was originally written in Fortran 🙃
    //mxArray *rx_data = mxCreateNumericMatrix(num_samp_rx, num_chan, mxSINGLE_CLASS, mxCOMPLEX);
    // Start tx in separate thread
    rx_usrp->set_time_now(0.0);
    tx_usrp->set_time_now(0.0); // Not sure if I need to do both separately
    double start_time = 0.005;       // give us 0.005 seconds to fill the tx buffers
    usrp_rx_start(rx_stream, num_samp_rx, start_time);
    // start transmit worker thread
    // setup the metadata flags
    uhd::tx_metadata_t md;
    md.start_of_burst = true;
    md.end_of_burst = false;
    md.has_time_spec = true;
    md.time_spec = uhd::time_spec_t(start_time);
    // tx MUST be run in a thread to avoid accidentally giving the Matlab main thread realtime priority
    boost::thread_group transmit_thread;
    transmit_thread.create_thread(boost::bind(&send_from_file, tx_stream, std::string(tx_basepath), 1000, chans.size(), md));
    auto spb = tx_stream->get_max_num_samps() * 10;
    recv_to_file_fc(rx_usrp, rx_stream, std::string(rx_basepath), spb, num_samp_rx, start_time, chans);
    std::cout << "?1\n";
    transmit_thread.join_all();
    std::cout << "?1\n";
    if (check_clear_underflow())
    {
        std::cerr << "Underflow" << std::endl;
    }
    std::cout << "x\n";
}