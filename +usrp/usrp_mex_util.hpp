#ifndef _USRP_MEX_UTIL_H_
#define _USRP_MEX_UTIL_H_

#include "mex.h"
#include <ctype.h>
#include <vector>

class usrp_access
{
public:
    usrp_access(uhd::usrp::multi_usrp::sptr rx, uhd::usrp::multi_usrp::sptr tx) :
            usrp_rx(rx), usrp_tx(tx) { }
    uhd::usrp::multi_usrp::sptr usrp_rx;
    uhd::usrp::multi_usrp::sptr usrp_tx;
    uhd::rx_streamer::sptr stream_rx;
    uhd::tx_streamer::sptr stream_tx;
};

#define CLASS_HANDLE_SIGNATURE 0xFF00F0A5
template<class base> class class_handle
{
public:
    class_handle(base ptr) : signature_m(CLASS_HANDLE_SIGNATURE), name_m(typeid(base).name()), ptr_m(ptr) {}
    ~class_handle() { signature_m = 0; delete ptr_m; }
    bool isValid() { return ((signature_m == CLASS_HANDLE_SIGNATURE) && !strcmp(name_m.c_str(), typeid(base).name())); }
    base ptr() { return ptr_m; }

private:
    uint32_t signature_m;
    const std::string name_m;
    base const ptr_m;
};

inline mxArray *convertPtr2Mat(usrp_access ptr)
{
    mexLock();
    mxArray *out = mxCreateNumericMatrix(1, 1, mxUINT64_CLASS, mxREAL);
    *((uint64_t *)mxGetData(out)) = reinterpret_cast<uint64_t>(new class_handle<usrp_access>(ptr));
    return out;
}

inline usrp_access convertMat2Ptr(const mxArray *in)
{
    if (mxGetNumberOfElements(in) != 1 || mxGetClassID(in) != mxUINT64_CLASS || mxIsComplex(in))
        mexErrMsgTxt("Input must be a real uint64 scalar.");
    class_handle<usrp_access> *ptr = reinterpret_cast<class_handle<usrp_access> *>(*((uint64_t *)mxGetData(in)));
    if (!ptr->isValid())
        mexErrMsgTxt("Handle not valid.");
    return ptr->ptr();
}

inline void destroyObject(const mxArray *in)
{
	// Shared pointers can be destroyed by calling reset() on the sptr object
	// http://lists.ettus.com/pipermail/usrp-users_lists.ettus.com/2015-December/045291.html
	// Note that the attached thread says we can only do this 256 times
    convertMat2Ptr(in).usrp_tx.reset();
    convertMat2Ptr(in).usrp_rx.reset();
    // TODO will cause problems if we have more than 1 instance of the class
    // Either do reference counting or just remove 
    //mexUnlock();
}

inline std::vector<void*> get_buffers_for_matrix(const mxArray *data, size_t num_chan, size_t num_samp) {
    float *headptr = (float *) mxGetData(data);
    // By the way, Matlab now (thankfully) interleaves real/imag, which makes things
    // much easier when you're interfacing with C/C++
    std::vector<void*> bufs;
    // Staring too long into the void...
    for (size_t ch = 0; ch < num_chan; ch++) {
        // Pointer arithmatic for the sole purpose of creating compiler warnings
        bufs.push_back(headptr+ch*num_samp);
        printf("Ptr %lu: %p\n", ch, headptr+ch*num_samp);
    }
    return bufs;
}

#endif