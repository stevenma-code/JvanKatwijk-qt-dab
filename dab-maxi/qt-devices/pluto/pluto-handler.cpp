#
/*
 *    Copyright (C) 2014 .. 2020
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of Qt-DAB
 *
 *    Qt-DAB is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation version 2 of the License.
 *
 *    Qt-DAB is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Qt-DAB if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include	<QThread>
#include	<QSettings>
#include	<QTime>
#include	<QDate>
#include	<QLabel>
#include	<QDebug>
#include	<QFileDialog>
#include	"pluto-handler.h"

#define	DEFAULT_GAIN	30

/* static scratch mem for strings */
static char tmpstr[64];

/* helper function generating channel names */
static
char*	get_ch_name(const char* type, int id) {
        snprintf (tmpstr, sizeof(tmpstr), "%s%d", type, id);
        return tmpstr;
}

/* returns ad9361 phy device */
static
struct	iio_device* get_ad9361_phy (struct iio_context *ctx) {
struct iio_device *dev =  iio_context_find_device(ctx, "ad9361-phy");
	if (dev == nullptr) {
	   fprintf (stderr, "No ad9361-phy found, fatal\n");
	   throw (21);
	}
        return dev;
}

/* finds AD9361 streaming IIO devices */
static
bool	get_ad9361_stream_dev (struct iio_context *ctx,
	                       struct iio_device **dev) {
	*dev = iio_context_find_device (ctx, "cf-ad9361-lpc");
	return *dev != nullptr;
}

static
bool	get_phy_chan (struct iio_context *ctx,
                      int chid, struct iio_channel **chn) {
	*chn = iio_device_find_channel (get_ad9361_phy (ctx),
                                        get_ch_name ("voltage", chid),
                                        false);
	return *chn != nullptr;
}

/* finds AD9361 local oscillator IIO configuration channels */
static
bool    get_lo_chan (struct iio_context *ctx, struct iio_channel **chn) {
	*chn = iio_device_find_channel (get_ad9361_phy (ctx),
                                        get_ch_name ("altvoltage", 0),
                                        true);
	return *chn != nullptr;
}

/* applies streaming configuration through IIO */
static
bool	cfg_ad9361_streaming_ch (struct iio_context *ctx,
	                         struct stream_cfg *cfg, int chid) {
struct iio_channel *chn = nullptr;

// Configure phy and lo channels
	fprintf (stderr, "* Acquiring AD9361 phy channel %d\n", chid);
	if (!get_phy_chan (ctx, chid, &chn)) {
	   return false;
	}

	if (iio_channel_attr_write (chn, "rf_port_select",
	                                                cfg -> rfport) < 0) {
	   fprintf (stderr, "cannot select port\n");
	   return false;
	}

	if (iio_channel_attr_write_longlong (chn, "rf_bandwidth",
	                                                 cfg -> bw_hz) < 0) {
	   fprintf (stderr, "cannot select bandwidth\n");
	   return false;
	}

	if (iio_channel_attr_write_longlong (chn, "sampling_frequency",
	                                                 cfg -> fs_hz) < 0) {
	   fprintf (stderr, "cannot set sampling frequency\n");
	   return false;
	}

// Configure LO channel
	fprintf (stderr, "* Acquiring AD9361 %s lo channel\n", "RX");
	chn = iio_device_find_channel (get_ad9361_phy (ctx),
                                        get_ch_name ("altvoltage", 0),
                                        true);
	if (chn == 0) {
	   fprintf (stderr, "cannot find lo for channel\n");
	   return false;
	}

	if (iio_channel_attr_write_longlong (chn, "frequency",
	                                                 cfg -> lo_hz) < 0) {
	   fprintf (stderr, "cannot set local oscillator frequency\n");
	   return false;
	}
	return true;
}

/* finds AD9361 streaming IIO channels */
static
bool get_ad9361_stream_ch (__notused struct iio_context *ctx,
	                   struct iio_device *dev,
	                   int chid, struct iio_channel **chn) {
        *chn = iio_device_find_channel (dev,
	                                get_ch_name ("voltage", chid), false);
        if (*chn == nullptr)
	   *chn = iio_device_find_channel (dev,
	                                   get_ch_name ("altvoltage", chid),
	                                   false);
        return *chn != nullptr;
}

	plutoHandler::plutoHandler  (QSettings *s,
	                             QString &recorderVersion):
	                                  myFrame (nullptr),
	                                  _I_Buffer (4 * 1024 * 1024) {
	plutoSettings			= s;
	this	-> recorderVersion	= recorderVersion;
	setupUi (&myFrame);
	myFrame. show	();
	this	-> inputRate		= Khz (2048);

	ctx				= nullptr;
	rxbuf				= nullptr;
	rx0_i				= nullptr;
	rx0_q				= nullptr;

	rxcfg. bw_hz			= 1536000;
	rxcfg. fs_hz			= 2048000;
	rxcfg. lo_hz			= 220000000;
	rxcfg. rfport			= "A_BALANCED";

	ctx = iio_create_default_context ();
	if (ctx == nullptr) {
	   fprintf (stderr, "no context found\n");
	   throw (21);
	}

	fprintf (stderr, "context created, now counting devices\n");
	if (iio_context_get_devices_count(ctx) <= 0) {
	   fprintf (stderr, "no devices found, fatal\n");
	   throw (22);
	}

	fprintf (stderr, "we have devices\n");
	fprintf (stderr, "* Acquiring AD9361 streaming devices\n");
	rx = iio_context_find_device (ctx, "cf-ad9361-lpc");
	if (rx == nullptr) {
	   fprintf (stderr, "No rx streaming device found\n");
	   throw (23);
	}

        fprintf (stderr, "* Configuring AD9361 for streaming\n");
        if (!cfg_ad9361_streaming_ch (ctx, &rxcfg, 0)) {
	   fprintf (stderr, "RX port 0 not found");
	   throw (24);
	}

	fprintf (stderr, "* Initializing AD9361 IIO streaming channels\n");
        if (!get_ad9361_stream_ch (ctx, rx, 0, &rx0_i)) {
	   fprintf (stderr,  "RX chan i not found");
	   throw (25);
	}

        if (!get_ad9361_stream_ch (ctx, rx, 1, &rx0_q)) {
	   fprintf (stderr, "RX chan q not found");
	   throw (26);
	}

        fprintf (stderr, "* Enabling IIO streaming channels\n");
        iio_channel_enable (rx0_i);
        iio_channel_enable (rx0_q);

        printf("* Creating non-cyclic IIO buffers with 1 MiS\n");
        rxbuf = iio_device_create_buffer (rx, 1024*1024, false);
	if (rxbuf == nullptr) {
	   fprintf (stderr, "could not create RX buffer, fatal\n");
	   throw (27);
	}

	iio_buffer_set_blocking_mode (rxbuf, true);
	iio_channel_attr_write (rx0_i, "gain_control_mode", "manual");
	iio_channel_attr_write (rx0_q, "gain_control_mode", "manual");
	agcMode		= false;
	
//	and be prepared for future changes in the settings
	connect (gainControl, SIGNAL (valueChanged (int)),
	         this, SLOT (set_gainControl (int)));
	connect (agcControl, SIGNAL (stateChanged (int)),
	         this, SLOT (set_agcControl (int)));
	running. store (false);
}

	plutoHandler::~plutoHandler() {
	stopReader();
	iio_buffer_destroy (rxbuf);
	iio_context_destroy (ctx);
}
//

void	plutoHandler::setVFOFrequency	(int32_t newFrequency) {
int	ret;
	rxcfg. lo_hz = newFrequency;
	ret = iio_channel_attr_write_longlong (rx0_i, "frequency",
	                                                 rxcfg. lo_hz);
	if (ret < 0) {
	   fprintf (stderr, "cannot set local oscillator frequency\n");
	}
}

int32_t	plutoHandler::getVFOFrequency() {
	return rxcfg. lo_hz;
}

void	plutoHandler::set_gainControl	(int newGain) {
int ret;
char buf [64];
	if (agcMode) {
	   ret = iio_channel_attr_write (rx0_i, "gain_control_mode", "manual");
	   if (ret < 0) {
	      fprintf (stderr, "could not change gain control to manual");
	      return;
	   }
	   agcMode	= !agcMode;
	}

	sprintf (buf, "%lu", (uint32_t)newGain);
	ret = iio_channel_attr_write_longlong (rx0_i, "hardwaregain", newGain);
	if (ret < 0) {
	   fprintf (stderr,
	           "could not set hardware gain to %s\n", buf);
	   return;
	}
}

void	plutoHandler::set_agcControl	(int dummy) {
	agcMode	= agcControl -> isChecked ();
	(void)dummy;
	if (agcMode) {
	   iio_channel_attr_write (rx0_i, "gain_control_mode", "slow_attack");
	}
	else 
	   set_gainControl (gainControl -> value ());
}

bool	plutoHandler::restartReader	(int32_t freq) {
	if (running. load())
	   return true;		// should not happen

	setVFOFrequency (freq);
	if (vfoFrequency == 0)
	   return false;

	start ();
	return true;
}

void	plutoHandler::stopReader() {
	if (!running. load())
	   return;
	running. store (false);
	while (isRunning())
	   usleep (500);
}

void	plutoHandler::run	() {
char	*p_end, *p_dat;
int	p_inc;
int	nbytes_rx;
std::complex<float> localBuf [2048];
int	localbufP	= 0;

	while (running. load ()) {
	   nbytes_rx	= iio_buffer_refill (rxbuf);
	   p_inc	= iio_buffer_step (rxbuf);
	   p_end	= (char *)(iio_buffer_end  (rxbuf));
	   for (p_dat = (char *)iio_buffer_first (rxbuf, rx0_i);
	        p_dat < p_end; p_dat += p_inc) {
	      const int16_t i_p = ((int16_t *)p_dat) [0];
	      const int16_t q_p = ((int16_t *)p_dat) [1];
	         localBuf [localbufP] = std::complex<float> (i_p / 2048.0,
	                                                     q_p / 2048.0);
	         localbufP ++;
	         if (localbufP >= 2048) {
	            _I_Buffer. putDataIntoBuffer (localBuf, 2048);
	            localbufP = 0;
	         }
	   }
	}
}

	
	

int32_t	plutoHandler::getSamples (std::complex<float> *V, int32_t size) { 
	(void)V;
	return size;
}

int32_t	plutoHandler::Samples () {
	return _I_Buffer. GetRingBufferReadAvailable();
}

void	plutoHandler::resetBuffer() {
	_I_Buffer. FlushRingBuffer();
}

int16_t	plutoHandler::bitDepth() {
	return 12;
}

QString	plutoHandler::deviceName	() {
	return "ADALM PLUTO";
}

void	plutoHandler::show	() {
	myFrame. show	();
}

void	plutoHandler::hide	() {
	myFrame. hide	();
}

bool	plutoHandler::isHidden	() {
	return myFrame. isHidden ();
}
