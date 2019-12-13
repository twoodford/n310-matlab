// Utilities for dealing with GPIO interface for PA control
#include<uhd/usrp/multi_usrp.hpp>

extern void usrp_gpio_arm_trigger(uhd::usrp::multi_usrp::sptr usrp);
extern void usrp_gpio_disarm_trigger(uhd::usrp::multi_usrp::sptr usrp);
extern void usrp_gpio_spi(uhd::usrp::multi_usrp::sptr usrp, uint8_t *pkt, size_t num_pkt);