% This script is useful for measuring key parameters that determine how
% well the phased array is working

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

CB_ENTRIES=[6 8];

Fs=125e6;
Fc=2.4e9;
TX_GAIN=50;
RX_GAIN=5;
TX_PA = 4;
RX_PA = 8;
PA_GAIN=5;
TX_SCALE=0.8;

pa_clk_count = 100; % hardcoded in processing code - don't change


uartfh = serial('/dev/ttyUSB1','BaudRate', 115200);
fopen(uartfh);
pa_ctl(CB_ENTRIES, PA_gain, TX_PA, 1, round(pa_clk_count), uartfh);
pa_ctl(zeros(1,2), PA_gain, RX_PA, 0, round(pa_clk_count), uartfh);
fclose(uartfh);

usrp = USRPHandle(N_PA, Fs, Fc, RX_GAIN, TX_GAIN);
usrp.pa_arm_trigger();

Ts = 1/Fs;

t = (0:Ts:((txLength-1))*Ts).'; % Create time vector(Sample Frequency is Ts (Hz))

payload = exp(t*1i*2*pi*20e6);%+exp(t*1i*pi*10e6); %5 and 10 MHz sinusoid as our "payload"

txData = [payload]; %,payload*exp(1i*0.5*pi)
txData = TX_SCALE .* txData ./ max(abs(txData));

rx_IQ = usrp.txrx_data(txData);

% Process data
rfrx.pa_ctl_tone_test(rx_IQ);

% Save data
clear usrp uartfh Ts t payload txData
SRC_SCRIPT=mfilename;
save([SRC_SCRIPT '_' datestr(datetime('now'), 'yymmdd-HHMMSS') '.mat']);
