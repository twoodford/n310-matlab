BUILDDIR:=build

MEX=${shell which mex}
ifeq ("$(MEX)", "")
	MEX:=${shell which matlab-mex}
endif

all: $(BUILDDIR)/gpio_test usrp_mex.mex

usrp_mex.mex: usrp_mex.cpp usrp_gpio.cpp usrp_io.cpp
	$(MEX) CFLAGS='-fpic -std=c++17' -R2018a $^ -lboost_filesystem -lboost_thread `pkg-config --libs --cflags uhd` 

$(BUILDDIR)/gpio_test: gpio_test.cpp
	g++ -std=c++17 -o $@ $^ `pkg-config --libs --cflags uhd` 
