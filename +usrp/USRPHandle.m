% Copyright 2019 Tim Woodford
% This program is free software: you can redistribute it and/or modify
% it under the terms of the GNU General Public License as published by
% the Free Software Foundation, either version 3 of the License, or
% (at your option) any later version.
% 
% This program is distributed in the hope that it will be useful,
% but WITHOUT ANY WARRANTY; without even the implied warranty of
% MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
% GNU General Public License for more details.
%
% You should have received a copy of the GNU General Public License
% along with this program.  If not, see <https://www.gnu.org/licenses/>.

classdef USRPHandle < handle
    %USRPHANDLE Interface to control USRPs with UHD
    
    properties
        usrpPtr
        num_chan
    end
    
    methods
        function obj = USRPHandle(num_chan, fs, fc, rx_gain, tx_gain, addr)
            %USRPHANDLE Construct a USRP instance
            if nargin < 6
                addr='';
            end
            if nargin < 5
                tx_gain = rx_gain;
            end
            obj.usrpPtr = usrp.usrp_mex('new', uint32(num_chan), fs, fc, rx_gain, tx_gain, addr);
            obj.num_chan = num_chan;
        end
        
        % Destructor - Destroy the C++ class instance
        function delete(this)
            if ~isempty(this.usrpPtr)
                usrp.usrp_mex('delete', this.usrpPtr);
            end
            this.usrpPtr = [];
        end
        
        function rx_dat = txrx_data(this, input_samples)
            if size(input_samples, 2) == this.num_chan
                input_samples = input_samples.';
            end
            num_samp_rx = size(input_samples, 2);
            nchan = size(input_samples, 1);
            if nchan ~= this.num_chan
                error('Invalid matrix dimensions: one dimension must match the number of channels')
            end
            % In Ubuntu, this points to a tmpfs location, so it should be RAM-backed
            tx_basename = tempname(getenv('XDG_RUNTIME_DIR'));
            rx_basename = tempname(getenv('XDG_RUNTIME_DIR'));
            % Write out tx data to files
            for ch=1:nchan
                % C++ side expects that we index from 0
                fh=fopen(sprintf('%s.%02d.dat', tx_basename, ch-1), 'w');
                fwrite(fh, reshape([real(input_samples(ch,:)); imag(input_samples(ch,:))], 2*num_samp_rx,1), 'single');
                fclose(fh);
            end
            % Emulate a try/finally for this block to make sure that we clean up any temp files
            % https://www.mathworks.com/matlabcentral/answers/300062-finally-clause-in-try-catch
            txrx_err = [];
            try
                % Do tx/rx
                usrp.usrp_mex('txrx', this.usrpPtr, num_samp_rx, nchan, ...
                    sprintf('%s.dat', tx_basename), sprintf('%s.dat', rx_basename));
                % Read rx data from files
                rx_dat = zeros(nchan, num_samp_rx, 'single');
                for ch=1:nchan
                    fname = sprintf('%s.%02d.dat', rx_basename, ch-1);
                    fh=fopen(fname, 'r');
                    sample_mat=fread(fh, 'single');
                    fclose(fh);
                    sample_mat=reshape(sample_mat, 2, numel(sample_mat)/2);
                    rx_dat(ch, :) = sample_mat(1,:) + 1j*sample_mat(2,:);
                end
            catch txrx_err
            end
            cleanup_err = [];
            for ch=1:nchan
                % Clean up
                try
                    delete(sprintf('%s.%02d.dat', rx_basename, ch-1));
                catch cleanup_err
                end
                try
                    delete(sprintf('%s.%02d.dat', tx_basename, ch-1));
                catch cleanup_err
                end
            end
             system('python /home/node1/Desktop/mmSDR_sparrow+/brige_test/disable_tx_mode.py');
             system('python /home/node1/Desktop/mmSDR_sparrow+/brige_test/disable_rx_mode.py');
            if ~isempty(txrx_err)
                rethrow(txrx_err)
            elseif ~isempty(cleanup_err)
                rethrow(cleanup_err)
            end
        end

        function set_rx_gain(this, manual_gain, agc)
            usrp.usrp_mex('set_gain_rx', this.usrpPtr, manual_gain, agc);
        end
        
        function set_tx_gain(this, manual_gain)
            usrp.usrp_mex('set_gain_tx', this.usrpPtr, manual_gain);
        end
        
        function gains = get_rx_gain(this)
            gains = usrp.usrp_mex('get_gain_rx', this.usrpPtr);
        end
        
        function pa_arm_trigger(this)
            usrp.usrp_mex('gpio_arm', this.usrpPtr);
        end
        
        function pa_disarm_trigger(this)
            usrp.usrp_mex('gpio_disarm', this.usrpPtr);
        end
        
        function pa_spi(this, spi_bits)
            spi_mat = reshape(spi_bits, numel(spi_bits)/8, 8);
            input_uints = uint8(bin2dec(num2str(spi_mat)));
            usrp.usrp_mex('gpio_spi_msg', this.usrpPtr, input_uints);
        end
    end
end
