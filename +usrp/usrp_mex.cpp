#include "mex.h"
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
#include "usrp_mex_util.hpp"
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

/* Global variable containing pointers to everything important
 * the idea is that instantiating the Matlab class will just grab a new pointer
 * to the USRP connection.  Hopefully, no one is trying to use more than one 
 * instance at a time.
 */
usrp_access *global_usrp = NULL;

/******************************************************************************
 * new_sub - mex code for initializing USRP session
 ******************************************************************************/
void new_sub(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
    // Check parameters
    if (nlhs != 1)
        mexErrMsgTxt("new: One output expected");
    if (nrhs != 7 && nrhs != 8)
        mexErrMsgTxt("new: 7-8 inputs expected");
    // Start getting parameters
    if(!mxIsScalar(prhs[1]) || mxIsComplex(prhs[1]))
        mexErrMsgTxt("new: num_channels parameter must be scalar");
    size_t num_channels = (size_t) mxGetScalar(prhs[1]);
    if(!mxIsScalar(prhs[2]) || mxIsComplex(prhs[2]))
        mexErrMsgTxt("new: fs parameter must be scalar");
    double fs = mxGetScalar(prhs[2]);
    if(!mxIsScalar(prhs[3]) || mxIsComplex(prhs[3]))
        mexErrMsgTxt("new: fc parameter must be scalar");
    double fc = mxGetScalar(prhs[3]);
    if(!mxIsScalar(prhs[4]) || mxIsComplex(prhs[4]))
        mexErrMsgTxt("new: rx gain parameter must be scalar");
    double rx_gain = mxGetScalar(prhs[4]);
    if(!mxIsScalar(prhs[5]) || mxIsComplex(prhs[5]))
        mexErrMsgTxt("new: tx gain parameter must be scalar");
    double tx_gain = mxGetScalar(prhs[5]);
    // usrp address is optional
    std::string addr;
    if(nrhs == 7) {
        if(!mxIsChar(prhs[6]))
            mexErrMsgTxt("new: addr must be string");
        addr = std::string(mxArrayToString(prhs[6]));
    } else {
        addr="";
    }
    // Check if there's already an existing instance in the global
    bool existing_inst = ( global_usrp != NULL ) ? true : false;
    // Connect
    uhd::usrp::multi_usrp::sptr tx_usrp = existing_inst ? global_usrp->usrp_tx : uhd::usrp::multi_usrp::make(addr);
    uhd::usrp::multi_usrp::sptr rx_usrp = existing_inst ? global_usrp->usrp_rx : uhd::usrp::multi_usrp::make(addr);
    // Channels 0...n_channels-1
    std::vector<size_t> channel_nums;
    for(size_t i=0; i<num_channels; i++) channel_nums.push_back(i);
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
    for(size_t ch=0; ch<num_channels; ch++) {
        uhd::tune_request_t tx_tune_request(fc);
        tx_usrp->set_tx_freq(tx_tune_request, ch);
        std::cout << boost::format("Actual TX Freq: %f MHz...")
                % (tx_usrp->get_tx_freq(ch) / 1e6) << std::endl;
        tx_usrp->set_tx_gain(tx_gain, ch);
    }
    // Per-channel rx
    for(size_t ch=0; ch<num_channels; ch++) {
        uhd::tune_request_t rx_tune_request(fc);
        rx_usrp->set_rx_freq(rx_tune_request, ch);
        std::cout << boost::format("Actual TX Freq: %f MHz...")
                % (tx_usrp->get_rx_freq(ch) / 1e6) << std::endl;
        rx_usrp->set_rx_gain(rx_gain, ch);
        std::cout << boost::format("Actual RX Gain: %f dB...")
                    % rx_usrp->get_rx_gain(ch) << std::endl;
    }
    // Create streamer objects
    // We're setting the over-the-wire sample mode to sc16, since that's the default
    // in the example file
    // fc32=float type (32-bit floating point)
    // sc16=short type (16-bit integer)
    uhd::stream_args_t stream_args("fc32", "sc16");
    stream_args.channels = channel_nums;
    uhd::tx_streamer::sptr tx_stream = (existing_inst) ? global_usrp->stream_tx : tx_usrp->get_tx_stream(stream_args);
    uhd::rx_streamer::sptr rx_stream = (existing_inst) ? global_usrp->stream_rx : rx_usrp->get_rx_stream(stream_args);
    // Ensure LO is locked
    std::vector<std::string> tx_sensor_names = tx_usrp->get_tx_sensor_names(0);
    if (std::find(tx_sensor_names.begin(), tx_sensor_names.end(), "lo_locked")
        != tx_sensor_names.end()) {
        uhd::sensor_value_t lo_locked = tx_usrp->get_tx_sensor("lo_locked", 0);
        if(!lo_locked.to_bool()){
            mexErrMsgTxt("Couldn't lock LO");
        }
    }
    // Return ptr
    if (!existing_inst) {
        global_usrp = new usrp_access(rx_usrp, tx_usrp);
        global_usrp->stream_rx = rx_stream;
        global_usrp->stream_tx = tx_stream;
    }
    plhs[0] = convertPtr2Mat(*global_usrp);
}

/******************************************************************************
 * set_gain('set_gain_rx', ptr, manual_gain, agc_rx) - set manual/AGC gain parameters
 ******************************************************************************/
void set_gain_sub_rx(usrp_access inst, int nrhs, const mxArray *prhs[]) {
    if (nrhs != 4) {
        mexErrMsgTxt("Incorrect number of inputs/outputs");
    }
    double gain_num = mxGetScalar(prhs[2]);
    bool enable_agc = (bool) mxGetScalar(prhs[3]);
    size_t num_chans = inst.usrp_rx->get_rx_num_channels();
    for (size_t ii = 0; ii < num_chans; ii++) {
        inst.usrp_rx->set_rx_gain(gain_num, ii);
        inst.usrp_rx->set_rx_agc(enable_agc, ii);
    }
}

/******************************************************************************
 * set_gain('set_gain_tx', ptr, manual_gain) - set manual tx gain parameters
 ******************************************************************************/
void set_gain_sub_tx(usrp_access inst, int nrhs, const mxArray *prhs[]) {
    if (nrhs != 3) {
        mexErrMsgTxt("Incorrect number of inputs/outputs");
    }
    double gain_num = mxGetScalar(prhs[2]);
    size_t num_chans = inst.usrp_tx->get_tx_num_channels();
    for (size_t ii = 0; ii < num_chans; ii++) {
        inst.usrp_tx->set_tx_gain(gain_num, ii);
    }
}

/******************************************************************************
 * set_gain('set_gain_tx', ptr, manual_gain) - set manual tx gain parameters
 ******************************************************************************/
void get_gain_sub_rx(usrp_access inst, mxArray *plhs[], const mxArray *prhs[]) {
    // TODO
    size_t num_chans = inst.usrp_tx->get_tx_num_channels();
    double gain_array[64];
    if(num_chans > 64) {
        mexErrMsgTxt("Too many channels!");
    }
    for (size_t ii=0; ii < num_chans; ii++) {
        gain_array[ii] = inst.usrp_rx->get_rx_gain(ii);
    }
    mxArray *gain_data = mxCreateNumericMatrix(0, 0, mxDOUBLE_CLASS, mxREAL);
    mxSetM(gain_data, num_chans);
    mxSetN(gain_data, 1);
    mxSetDoubles(gain_data, gain_array);
    plhs[0] = gain_data;
}

void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{	
    // Get the command string
    char cmd[64];
	if (nrhs < 1 || mxGetString(prhs[0], cmd, sizeof(cmd)))
		mexErrMsgTxt("First input should be a command string less than 64 characters long.");
        
    // New -> usrp_common('new', num_channels, fs, fc, rx_gain, tx_gain, [addr])
    if (!strcmp("new", cmd)) {
        new_sub(nlhs, plhs, nrhs, prhs);
        return;
    }
    
    // Check there is a second input, which should be the class instance handle
    if (nrhs < 2)
		mexErrMsgTxt("Second input should be a class instance handle.");
    
    if (nlhs > 1)
        mexErrMsgTxt("This function doesn't return anything");
    
    // Delete
    if (!strcmp("delete", cmd)) {
        // Destroy the C++ object
        destroyObject(prhs[1]);
        // Warn if other commands were ignored
        if (nlhs != 0 || nrhs != 2)
            mexWarnMsgTxt("Delete: Unexpected arguments ignored.");
        return;
    }
    
    // Get the class instance pointer from the second input
    usrp_access inst = convertMat2Ptr(prhs[1]);
    
    if (!strcmp("txrx", cmd)) {
        // txrx -> usrp_mex('txrx', ptr, input_samples)
        if (nlhs != 0 || nrhs != 6)
            mexErrMsgTxt("txrx: Unexpected arguments.");
        // Grab the appropriate data
        size_t num_samp_rx, num_chan;
        char tx_basepath[128], rx_basepath[128];
        num_samp_rx = mxGetScalar(prhs[2]);
        num_chan = mxGetScalar(prhs[3]);
        if(mxGetString(prhs[4], tx_basepath, sizeof(tx_basepath))) {
            mexErrMsgTxt("txrx: couldn't get tx base path");
        }
        if(mxGetString(prhs[5], rx_basepath, sizeof(rx_basepath))) {
            mexErrMsgTxt("txrx: couldn't get tx base path");
        }
        // Create set of channels.  For now, we're making some assumptions.
        std::vector<size_t> chans;
        for(size_t i=0; i<num_chan; i++) {
            chans.push_back(i);
        }
        // Allocate matrix for rx data
        // Should have num_samp rows and num_chan columns
        // This is for memory layout purposes, since Matlab does column-major formatting
        // This means each column is stored contiguously, so we want each column to be a buffer
        // Matlab does things this way because it was originally written in Fortran 🙃
        //mxArray *rx_data = mxCreateNumericMatrix(num_samp_rx, num_chan, mxSINGLE_CLASS, mxCOMPLEX);
        // Start tx in separate thread
        inst.usrp_rx->set_time_now(0.0);
        inst.usrp_tx->set_time_now(0.0); // Not sure if I need to do both separately
        double start_time = 0.005; // give us 0.005 seconds to fill the tx buffers
        usrp_rx_start(inst.stream_rx, num_samp_rx, start_time);
        // start transmit worker thread
        // setup the metadata flags
        uhd::tx_metadata_t md;
        md.start_of_burst = true;
        md.end_of_burst   = false;
        md.has_time_spec  = true;
        md.time_spec = uhd::time_spec_t(start_time); 
        // tx MUST be run in a thread to avoid accidentally giving the Matlab main thread realtime priority
        boost::thread_group transmit_thread;
        transmit_thread.create_thread(boost::bind(&send_from_file, inst.stream_tx, std::string(tx_basepath), 1000, chans.size(), md));
        auto spb = inst.stream_tx->get_max_num_samps() * 10;
        recv_to_file(inst.usrp_rx, inst.stream_rx, std::string(rx_basepath), spb, num_samp_rx, start_time, chans);
        transmit_thread.join_all();
        if (check_clear_underflow()) {
            mexErrMsgTxt("Underflows happened.  Please try again.");
        }
        // Return data
        //plhs[0] = rx_data;
        return;
    }

    if (!strcmp("set_gain_rx", cmd)) {
        set_gain_sub_rx(inst, nrhs, prhs);
        return;
    }

    if (!strcmp("set_gain_tx", cmd)) {
        set_gain_sub_tx(inst, nrhs, prhs);
        return;
    }

    if (!strcmp("get_gain_rx", cmd)) {
        if (nrhs != 2 || nlhs != 1) {
            mexErrMsgTxt("Invalid number of in or out parameters");
        }
        get_gain_sub_rx(inst, plhs, prhs);
        return;
    }

    if(!strcmp("gpio_spi_msg", cmd)) {
        // gpio -> usrp_mex('gpio', ptr, ctrl_data)
        if (nlhs != 0 || nrhs != 3)
            mexErrMsgTxt("gpio: Unexpected arguments");
        
        if (!mxIsUint8(prhs[2])) {
            mexErrMsgTxt("gpio: need uint8 for packet data");
        }
        if (mxIsComplex(prhs[2])) {
            mexErrMsgTxt("gpio: input packets should not be complex");
        }

        // For simplicity, we'll assume that the input data is either a 
        // row vector or a column vector
        size_t num_pkt = mxGetM(prhs[2])*mxGetN(prhs[2]);
        // The actual data ordering will be the same either way
        
        uint8_t *pkt_data = mxGetUint8s(prhs[2]);
        usrp_gpio_spi(inst.usrp_rx, pkt_data, num_pkt);
        return;
    }

    if(!strcmp("gpio_arm", cmd)) {
        usrp_gpio_arm_trigger(inst.usrp_rx);
        return;
    }
    
    if(!strcmp("gpio_disarm", cmd)) {
        usrp_gpio_disarm_trigger(inst.usrp_rx);
        return;
    }
    // Got here, so command not recognized
    mexErrMsgTxt("Command not recognized.");
}
