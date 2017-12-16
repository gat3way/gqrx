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
#ifndef RX_DEMOD_DSD_H
#define RX_DEMOD_DSD_H

#include <gnuradio/analog/quadrature_demod_cf.h>
#include <gnuradio/hier_block2.h>
#include <gnuradio/filter/firdes.h>
#include <gnuradio/filter/rational_resampler_base_fff.h>
#include <gnuradio/filter/rational_resampler_base_ccf.h>
#include <gnuradio/filter/fir_filter_fff.h>
#include <vector>
#include "dsp/rx_agc_xx.h"
#include "dsp/dsd_block_ff.h"


class rx_demod_dsd;
typedef boost::shared_ptr<rx_demod_dsd> rx_demod_dsd_sptr;

/*! \brief Return a shared_ptr to a new instance of rx_demod_dsd.
 *  \param quad_rate The input sample rate.
 *  \param max_dev Maximum deviation in Hz
 *  \param tau De-emphasis time constant in seconds (75us in US, 50us in EUR, 0.0 disables).
 *
 * This is effectively the public constructor. To avoid accidental use
 * of raw pointers, rx_demod_dsd's constructor is private.
 * make_rx_dmod_dsd is the public interface for creating new instances.
 */
rx_demod_dsd_sptr make_rx_demod_dsd(float quad_rate);

/*! \brief DSD demodulator.
 *  \ingroup DSP
 *
 * This class implements the DSD demodulator using the gr_quadrature_demod block.
 * It also provides de-emphasis with variable time constant (use 0.0 to disable).
 *
 */
class rx_demod_dsd : public gr::hier_block2
{

public:
    rx_demod_dsd(float quad_rate); // FIXME: should be private
    ~rx_demod_dsd();
    void set_frame_type(int type);
    void set_optimization(int type);
    //void set_tau(double tau);

private:
    /* GR blocks */
    gr::analog::quadrature_demod_cf::sptr               d_quad;      /*! The quadrature demodulator block. */
    dsd_block_ff_sptr                                   d_decoder;   /*! DSD decoder */
    std::vector<float>                                  d_taps;      /*! Taps for the rational resampler. */
    std::vector<float>                                  d_taps2;     /*! Taps for the lowpass. */
    gr::filter::rational_resampler_base_fff::sptr       d_resample;
    gr::filter::rational_resampler_base_ccf::sptr       d_filter;

    /* other parameters */
    float       d_quad_rate;     /*! Quadrature rate. */
};

#endif