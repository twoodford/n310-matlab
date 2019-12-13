# n310-matlab
Matlab interface for USRP N310.  Matlab's standard USRP interface does not currently support the USRP N310, and it is not clear whether its current SDR framework would be able to keep up with the N310's sampling rate.  This is a simple interface between Matlab and the uhd tx/rx code.

Note that this interface currently writes a file to what should be a RAM-backed filesystem.  This may incur a slight performance hit.  Future versions will (hopefully) transfer data directly between Matlab and the C++ code.
