/* -*- c++ -*- */
/*
 * Gqrx SDR: Software defined radio receiver powered by GNU Radio and Qt
 *           http://gqrx.dk/
 *
 * Copyright 2011 Alexandru Csete OZ9AEC.
 *
 * Gqrx is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * Gqrx is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Gqrx; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */
#include <gnuradio/filter/firdes.h>
#include <gnuradio/io_signature.h>
#include <iostream>
#include <math.h>
#include "dsp/rx_demod_dsd.h"


/*
enum dsd_frame_mode {
  DSD_FRAME_AUTO_DETECT,
  DSD_FRAME_P25_PHASE_1,
  DSD_FRAME_DSTAR,
  DSD_FRAME_NXDN48_IDAS,
  DSD_FRAME_NXDN96,
  DSD_FRAME_PROVOICE,
  DSD_FRAME_DMR_MOTOTRBO,
  DSD_FRAME_X2_TDMA
};

enum dsd_modulation_optimizations {
  DSD_MOD_AUTO_SELECT,
  DSD_MOD_C4FM,
  DSD_MOD_GFSK,
  DSD_MOD_QPSK
};

*/

/* Create a new instance of rx_demod_dsd and return a boost shared_ptr. */
rx_demod_dsd_sptr make_rx_demod_dsd(float quad_rate)
{
    return gnuradio::get_initial_sptr(new rx_demod_dsd(quad_rate));
}

static const int MIN_IN = 1;  /* Mininum number of input streams. */
static const int MAX_IN = 1;  /* Maximum number of input streams. */
static const int MIN_OUT = 1; /* Minimum number of output streams. */
static const int MAX_OUT = 1; /* Maximum number of output streams. */

rx_demod_dsd::rx_demod_dsd(float quad_rate)
    : gr::hier_block2 ("rx_demod_dsd",
                      gr::io_signature::make (MIN_IN, MAX_IN, sizeof (gr_complex)),
                      gr::io_signature::make (MIN_OUT, MAX_OUT, sizeof (float))),
    d_quad_rate(quad_rate)
{
    float gain;

    /* demodulator gain */
    gain = (float)d_quad_rate / ((float)2.0*2.0 * (float)M_PI * (float)15000.0);
#ifndef QT_NO_DEBUG_OUTPUT
    std::cerr << "DSD demod gain: " << gain << std::endl;
#endif
    /* demodulator */
    d_quad = gr::analog::quadrature_demod_cf::make(gain);
    d_taps = gr::filter::firdes::low_pass(1.0,
                                 96000.0,
                                 3900,
                                 100,
                                 gr::filter::firdes::WIN_HAMMING,
                                 6.76
                                );
    d_taps2 = gr::filter::firdes::low_pass(1.0,
                                 96000.0,
                                 32000,
                                 12000,
                                 gr::filter::firdes::WIN_HAMMING,
                                 6.76
                                );

    //d_filter = gr::filter::fir_filter_fff::make(2, d_taps2);
    d_filter = gr::filter::rational_resampler_base_ccf::make(1, 2, d_taps2);
    d_resample = gr::filter::rational_resampler_base_fff::make(12, 1, d_taps);
    d_decoder = dsd_make_block_ff((dsd_frame_mode)0,/*DSD_FRAME_AUTODETECT*/ (dsd_modulation_optimizations)0,/*dsd_MOD_AUTOSELECT*/ 3, true, 3, false);

    /* connect block */
    connect(self(), 0, d_filter, 0);
    connect(d_filter, 0, d_quad, 0);
    connect(d_quad, 0, d_decoder, 0);
    connect(d_decoder, 0, d_resample, 0);
    connect(d_resample, 0, self(), 0);
}

rx_demod_dsd::~rx_demod_dsd ()
{
}


void rx_demod_dsd::set_frame_type(int type)
{
    d_decoder->reset_state();
    d_decoder->set_mode((dsd_frame_mode)type);
}

void rx_demod_dsd::set_optimization(int type)
{
    d_decoder->reset_state();
    d_decoder->set_optimization((dsd_modulation_optimizations)type);
}



