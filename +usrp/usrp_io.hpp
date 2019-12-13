
#include <uhd/usrp/multi_usrp.hpp>

extern std::complex<float> *recv_to_file(uhd::usrp::multi_usrp::sptr usrp,
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