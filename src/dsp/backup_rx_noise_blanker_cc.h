/* -*- c++ -*- */
/*
 * Gqrx SDR: Software defined radio receiver powered by GNU Radio and Qt
 *           http://gqrx.dk/
 *
 * Copyright 2012 Alexandru Csete OZ9AEC.
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
#ifndef RX_NB_CC_H
#define RX_NB_CC_H

#include <gnuradio/sync_block.h>
#include <gnuradio/gr_complex.h>
#include <boost/thread/mutex.hpp>

class rx_nb_cc;

typedef boost::shared_ptr<rx_nb_cc> rx_nb_cc_sptr;


/*! \brief Return a shared_ptr to a new instance of rx_nb_cc.
 *  \param sample_rate The samle rate (default = 96000).
 *  \param threshold Noise blanker threshold. Range 0.0 to 1.0 (TBC)
 *
 * This is effectively the public constructor for a new noise blanker block.
 * To avoid accidental use of raw pointers, the rx_nb_cc constructor is private.
 * make_rx_nb_cc is the public interface for creating new instances.
 */
rx_nb_cc_sptr make_rx_nb_cc(double sample_rate=96000.0, float thld1=3.3, float thld2=2.5);


/*! \brief Noise blanker block.
 *  \ingroup DSP
 *
 * This block implements noise blanking filters based on the noise blanker code
 * from DTTSP.

 *
 */
class rx_nb_cc : public gr::sync_block
{
    friend rx_nb_cc_sptr make_rx_nb_cc(double sample_rate, float thld1, float thld2);

protected:
    rx_nb_cc(double sample_rate, float thld1, float thld2);

public:
    ~rx_nb_cc();

    int work(int noutput_items,
             gr_vector_const_void_star &input_items,
             gr_vector_void_star &output_items);

    void set_sample_rate(double sample_rate) { d_sample_rate = sample_rate; }
    void set_nb1_on(bool nb1_on) { d_nb1_on = nb1_on; }
    void set_nb2_on(bool nb2_on) { d_nb2_on = nb2_on; }
    bool get_nb1_on() { return d_nb1_on; }
    bool get_nb2_on() { return d_nb2_on; }
    void set_threshold1(float threshold);
    void set_threshold2(float threshold);

private:
    void process_nb1(gr_complex *buf, int num);
    void process_nb2(gr_complex *buf, int num);

private:
    boost::mutex  d_mutex;  /*! Used to lock internal data while processing or setting parameters. */

    bool   d_nb1_on;        /*! Current NB1 status (true/false). */
    bool   d_nb2_on;        /*! Current NB2 status (true/false). */
    double d_sample_rate;   /*! Current sample rate. */
    float  d_thld_nb1;      /*! Current threshold for noise blanker 1 (1.0 to 20.0 TBC). */
    float  d_thld_nb2;      /*! Current threshold for noise blanker 2 (0.0 to 15.0 TBC). */
    float  d_avgmag_nb1;    /*! Average magnitude. */
    float  d_avgmag_nb2;    /*! Average magnitude. */
    gr_complex d_avgsig, d_delay[8];
    int    d_delidx, d_sigidx, d_hangtime;   // FIXME: need longer buffer for higher sampel rates?
    int    d_nb1_state;
    int    d_nb2_state;


    // NB1 parameters
    double d_nb1_avg;
    double d_nb1_power;
    double d_nb1_backmult;
    double d_nb1_ombackmult;
    double d_nb1_backtau;
    double d_nb1_threshold;
    double d_nb1_tau;
    gr_complex *d_nb1_dline;
    double *d_nb1_wave;
    int    d_nb1_dline_size;
    int    d_nb1_trans_count;
    int    d_nb1_adv_count;
    int    d_nb1_count;
    int    d_nb1_dtime;
    int    d_nb1_atime;
    int    d_nb1_htime;
    int    d_nb1_itime;
    int    d_nb1_hang_count;
    int    d_nb1_advtime;
    int    d_nb1_hangtime;
    double d_nb1_coef;
    int    d_nb1_in_idx;
    int    d_nb1_out_idx;

    // NB2 parameters
    double d_nb2_advslewtime;
    double d_nb2_hangslewtime;
    double d_nb2_max_imp_seq_time;
    int *  d_nb2_imp;
    double d_nb2_avg;
    double d_nb2_power;
    double d_nb2_backmult;
    double d_nb2_ombackmult;
    double d_nb2_backtau;
    double d_nb2_threshold;
    double d_nb2_tau;
    gr_complex *d_nb2_dline;
    double *d_nb2_awave;
    double *d_nb2_hwave;
    gr_complex *d_nb2_bfbuff;
    gr_complex *d_nb2_ffbuff;
    gr_complex *d_nb2_fcoefs;
    gr_complex d_nb2_last;
    gr_complex d_nb2_next;
    gr_complex d_nb2_deltaIQ;
    gr_complex d_nb2_IQ;
    gr_complex d_nb2_IQ1;
    gr_complex d_nb2_IQ2;


    int    d_nb2_mode;
    int    d_nb2_dline_size;
    int    d_nb2_trans_count;
    int    d_nb2_adv_count;
    int    d_nb2_count;
    int    d_nb2_filterlen;
    int    d_nb2_bfb_in_idx;
    int    d_nb2_ffb_in_idx;
    int    d_nb2_overflow;
    int    d_nb2_hang_slew_count;
    int    d_nb2_max_imp_seq;
    int    d_nb2_adv_slew_count;
    int    d_nb2_scan_idx;
    int    d_nb2_dtime;
    int    d_nb2_atime;
    int    d_nb2_htime;
    int    d_nb2_itime;
    int    d_nb2_hang_count;
    int    d_nb2_advtime;
    int    d_nb2_hangtime;
    double d_nb2_coef;
    int    d_nb2_in_idx;
    int    d_nb2_out_idx;
    int    d_nb2_time;
    int    d_nb2_blank_count;


};

#define MAX_NB1_TAU                                 (0.002)
#define MAX_NB1_ADVTIME                             (0.002)
#define MAX_NB1_ADV_SLEW_TIME                       (0.002)
#define MAX_NB1_HANG_SLEW_TIME                      (0.002)
#define MAX_NB1_HANG_TIME                           (0.002)
#define MAX_NB1_SEQ_TIME                            (0.025)

#define MAX_NB2_ADV_SLEW_TIME                       (0.002)
#define MAX_NB2_ADV_TIME                            (0.002)
#define MAX_NB2_HANG_SLEW_TIME                      (0.002)
#define MAX_NB2_HANG_TIME                           (0.002)
#define MAX_NB2_SEQ_TIME                            (0.025)

#define MAX_SAMPLERATE                              (20000000)





#endif /* RX_NB_CC_H */
