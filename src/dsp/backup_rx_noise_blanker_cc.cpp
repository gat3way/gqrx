/* -*- c++ -*- */
/*
 * Gqrx SDR: Software defined radio receiver powered by GNU Radio and Qt
 *           http://gqrx.dk/
 *
 * Copyright 2011-2012 Alexandru Csete OZ9AEC.
 * Copyright 2004-2008 by Frank Brickle, AB2KT and Bob McGwier, N4HY
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
#include <math.h>
#include <gnuradio/io_signature.h>
#include <gnuradio/gr_complex.h>
#include "dsp/rx_noise_blanker_cc.h"





rx_nb_cc_sptr make_rx_nb_cc(double sample_rate, float thld1, float thld2)
{
    return gnuradio::get_initial_sptr(new rx_nb_cc(sample_rate, thld1, thld2));
}


/*! \brief Create noise blanker object.
 *
 * Use make_rx_nb_cc() instead.
 */
rx_nb_cc::rx_nb_cc(double sample_rate, float thld1, float thld2)
    : gr::sync_block ("rx_nb_cc",
          gr::io_signature::make(1, 1, sizeof(gr_complex)),
          gr::io_signature::make(1, 1, sizeof(gr_complex))),
      d_nb1_on(false),
      d_nb2_on(false),
      d_sample_rate(sample_rate),
      d_thld_nb1(thld1),
      d_thld_nb2(thld2),
      d_avgmag_nb1(1.0),
      d_avgmag_nb2(1.0),
      d_delidx(2),
      d_sigidx(0),
      d_hangtime(0)
{
    memset(d_delay, 0, 8 * sizeof(gr_complex));

    // Init nb1 params
    d_nb1_tau = 1.0/1000;
    d_nb1_hangtime = 1.0/10000.0;
    d_nb1_advtime = 1.0/10000.0;
    d_nb1_backtau = 1.0/10000.0;
    d_nb1_threshold = 3;
    d_nb1_wave = (double *) malloc (((int)(MAX_SAMPLERATE * MAX_NB1_TAU) + 1) * sizeof(gr_complex) * (sizeof(gr_complex) / sizeof(double)));
    d_nb1_dline_size = (int)((MAX_NB1_TAU + MAX_NB1_ADVTIME) * MAX_SAMPLERATE) + 1;
    d_nb1_dline = (gr_complex *) malloc (d_nb1_dline_size * sizeof(gr_complex));
    
    d_nb1_trans_count = (int)(d_nb1_tau * sample_rate);
    if (d_nb1_trans_count < 2) d_nb1_trans_count = 2;
    d_nb1_count = 0;
    d_nb1_hang_count = (int)(d_nb1_hangtime * sample_rate);
    d_nb1_adv_count = (int)(d_nb1_advtime * sample_rate);
    d_nb1_in_idx = d_nb1_trans_count + d_nb1_adv_count;
    d_nb1_out_idx = 0;
    d_nb1_coef = 3.1415926  / d_nb1_trans_count;
    d_nb1_state = 0;
    d_nb1_avg = 1.0;
    d_nb1_power = 1.0;
    d_nb1_backmult = exp(-1.0 / (sample_rate * d_nb1_backtau));
    d_nb1_ombackmult = 1.0 - d_nb1_backmult;


    int a=((int)(MAX_SAMPLERATE * MAX_NB1_TAU) + 1) * sizeof(gr_complex) * (sizeof(gr_complex) / sizeof(double));
    int b=d_nb1_trans_count;
    printf("Sizes: %d %d\n",a,b);
    a = d_nb1_dline_size * sizeof(gr_complex);
    b = d_nb1_dline_size * sizeof(gr_complex) * (sizeof(gr_complex) / sizeof(int));
    printf("Sizes: %d %d\n",a,b);

    for (int i = 0; i <= d_nb1_trans_count; i++)
        d_nb1_wave[i] = 0.5 * cos(i * d_nb1_coef);



    // Init nb2 params
    //sample_rate = 6000000 / 16;
    int i;
    double coef;
    d_nb2_mode = 1; // should be 1-4
    d_nb2_advslewtime = 0.0001;
    d_nb2_advtime = 0.0001;
    d_nb2_hangslewtime = 0.0001;
    d_nb2_hangtime = 0.0001;
    d_nb2_max_imp_seq_time = 0.025;
    d_nb2_backtau = 0.00001;
    d_nb2_threshold = 3.5;
    d_nb2_dline_size = (int)(MAX_SAMPLERATE * (MAX_NB2_ADV_SLEW_TIME + 
                                            MAX_NB2_ADV_TIME + 
                                            MAX_NB2_HANG_SLEW_TIME + 
                                            MAX_NB2_HANG_TIME + 
                                            MAX_NB2_SEQ_TIME ) + 2);
    d_nb2_dline = (gr_complex *)malloc (d_nb2_dline_size * sizeof (gr_complex));
    d_nb2_imp = (int *)malloc (d_nb2_dline_size * sizeof (int));
    d_nb2_awave = (double *)malloc ((int)(MAX_NB2_ADV_SLEW_TIME  * MAX_SAMPLERATE + 1) * sizeof (double));
    d_nb2_hwave = (double *)malloc ((int)(MAX_NB2_HANG_SLEW_TIME * MAX_SAMPLERATE + 1) * sizeof (double));

    d_nb2_filterlen = 10;
    d_nb2_bfbuff = (gr_complex *)malloc (d_nb2_filterlen * sizeof (gr_complex));
    d_nb2_ffbuff = (gr_complex *)malloc (d_nb2_filterlen * sizeof (gr_complex));
    d_nb2_fcoefs = (gr_complex *)malloc (d_nb2_filterlen * sizeof (gr_complex));
    d_nb2_fcoefs[0] = 0.308720593;
    d_nb2_fcoefs[1] = 0.216104415;
    d_nb2_fcoefs[2] = 0.151273090;
    d_nb2_fcoefs[3] = 0.105891163;
    d_nb2_fcoefs[4] = 0.074123814;
    d_nb2_fcoefs[5] = 0.051886670;
    d_nb2_fcoefs[6] = 0.036320669;
    d_nb2_fcoefs[7] = 0.025424468;
    d_nb2_fcoefs[8] = 0.017797128;
    d_nb2_fcoefs[9] = 0.012457989;

    d_nb2_adv_slew_count = (int)(d_nb2_advslewtime * sample_rate);
    d_nb2_adv_count = (int)(d_nb2_advtime * sample_rate);
    d_nb2_hang_count = (int)(d_nb2_hangtime * sample_rate);
    d_nb2_hang_slew_count = (int)(d_nb2_hangslewtime * sample_rate);
    d_nb2_max_imp_seq = (int)(d_nb2_max_imp_seq_time * sample_rate);
    d_nb2_backmult = exp (-1.0 / (sample_rate * d_nb2_backtau));
    d_nb2_ombackmult = 1.0 - d_nb2_backmult;
    if (d_nb2_adv_slew_count > 0)
    {
        coef = 3.1415926 / (d_nb2_adv_slew_count + 1);
        for (i = 0; i < d_nb2_adv_slew_count; i++)
            d_nb2_awave[i] = 0.5 * cos ((i + 1) * coef);
    }
    if (d_nb2_hang_slew_count > 0)
    {
        coef = 3.1415926 / d_nb2_hang_slew_count;
        for (i = 0; i < d_nb2_hang_slew_count; i++)
            d_nb2_hwave[i] = 0.5 * cos (i * coef);
}


    d_nb2_out_idx = 0;
    d_nb2_scan_idx = d_nb2_out_idx + d_nb2_adv_slew_count + d_nb2_adv_count + 1;
    d_nb2_in_idx = d_nb2_scan_idx + d_nb2_max_imp_seq + d_nb2_hang_count + d_nb2_hang_slew_count + d_nb2_filterlen;
    d_nb2_state = 0;
    d_nb2_overflow = 0;
    d_nb2_avg = 1.0;
    d_nb2_bfb_in_idx = d_nb2_filterlen - 1;
    d_nb2_ffb_in_idx = d_nb2_filterlen - 1;
    memset (d_nb2_dline, 0, d_nb2_dline_size * sizeof (gr_complex));
    memset (d_nb2_imp, 0, d_nb2_dline_size * sizeof (int));
    memset (d_nb2_bfbuff, 0, d_nb2_filterlen * sizeof (gr_complex));
    memset (d_nb2_ffbuff, 0, d_nb2_filterlen * sizeof (gr_complex));


    /*
    memset(d_nb1_dline, 0, d_nb1_dline_size * sizeof(gr_complex) * (sizeof(gr_complex) / sizeof(int)));
    */

}

rx_nb_cc::~rx_nb_cc()
{
    free (d_nb1_dline);
    free (d_nb1_wave);
    free (d_nb2_fcoefs);
    free (d_nb2_ffbuff);
    free (d_nb2_bfbuff);
    free (d_nb2_hwave);
    free (d_nb2_awave);
    free (d_nb2_imp);
    free (d_nb2_dline);
}

/*! \brief Receiver noise blanker work method.
 *  \param mooutput_items
 *  \param input_items
 *  \param output_items
 */
int rx_nb_cc::work(int noutput_items,
                   gr_vector_const_void_star &input_items,
                   gr_vector_void_star &output_items)
{
    const gr_complex *in = (const gr_complex *) input_items[0];
    gr_complex *out = (gr_complex *) output_items[0];
    int i;

    boost::mutex::scoped_lock lock(d_mutex);

    // copy data into output buffer then perform the processing on that buffer
    for (i = 0; i < noutput_items; i++)
    {
        out[i] = in[i];
    }

    if (d_nb1_on)
    {
        process_nb1(out, noutput_items);
    }
    if (d_nb2_on)
    {
        process_nb2(out, noutput_items);
    }

    return noutput_items;
}

/*! \brief Perform noise blanker 1 processing.
 *  \param buf The data buffer holding gr_complex samples.
 *  \param num The number of samples in the buffer.
 *
 * Noise blanker 1 is the first noise blanker in the processing chain.
 * It is intended to reduce the effect of impulse type noise.
 *
 * FIXME: Needs different constants for higher sample rates?
 */
void rx_nb_cc::process_nb1(gr_complex *buf, int num)
{
/*
    float cmag;
    gr_complex zero(0.0, 0.0);

    for (int i = 0; i < num; i++)
    {
        cmag = abs(buf[i]);
        d_delay[d_sigidx] = buf[i];
        d_avgmag_nb1 = 0.999*d_avgmag_nb1 + 0.001*cmag;

        if ((d_hangtime == 0) && (cmag > (d_thld_nb1*d_avgmag_nb1)))
            d_hangtime = 7;

        if (d_hangtime > 0)
        {
            buf[i] = zero;
            d_hangtime--;
        }
        else
        {
            buf[i] = d_delay[d_delidx];
        }

        d_sigidx = (d_sigidx + 7) & 7;
        d_delidx = (d_delidx + 7) & 7;
    }
*/
    double mag;
    double scale;
    gr_complex zero(0.0, 0.0);


    for (int i = 0; i < num; i++)
    {
            mag = abs(buf[i]);
            d_nb1_avg = d_nb1_backmult * d_nb1_avg + d_nb1_ombackmult * mag;
            d_nb1_dline[d_nb1_in_idx] = buf[i];
            if (mag > (d_nb1_avg * d_nb1_threshold))
                d_nb1_count = d_nb1_trans_count + d_nb1_adv_count;

            switch (d_nb1_state)
            {
                case 0:
                    buf[i] = d_nb1_dline[d_nb1_out_idx];
                    if (d_nb1_count > 0)
                    {
                        d_nb1_state = 1;
                        d_nb1_dtime = 0;
                        d_nb1_power = 1.0;
                    }
                    break;
                case 1:
                    scale = d_nb1_power * (0.5 + d_nb1_wave[d_nb1_dtime]);
                    buf[i] = d_nb1_dline[d_nb1_out_idx];
                    if (++d_nb1_dtime > d_nb1_trans_count)
                    {
                        d_nb1_state = 2;
                        d_nb1_atime = 0;
                    }
                    break;
                case 2:
                    buf[i] = zero;
                    if (++d_nb1_atime > d_nb1_adv_count)
                        d_nb1_state = 3;
                    break;
                case 3:
                    if (d_nb1_count > 0)
                        d_nb1_htime = -d_nb1_count;
                                
                    buf[i] = zero;
                    if (++d_nb1_htime > d_nb1_hang_count)
                    {
                        d_nb1_state = 4;
                        d_nb1_itime = 0;
                    }
                    break;
                case 4:
                    scale = 0.5 - d_nb1_wave[d_nb1_itime];
                    buf[i] = d_nb1_dline[d_nb1_out_idx] * gr_complex(scale,scale);
                    if (d_nb1_count > 0)
                    {
                        d_nb1_state = 1;
                        d_nb1_dtime = 0;
                        d_nb1_power = scale;
                    }
                    else if (++d_nb1_itime > d_nb1_trans_count)
                        d_nb1_state = 0;
                    break;
            }
            if (d_nb1_count > 0) d_nb1_count--;
            if (++d_nb1_in_idx == d_nb1_dline_size) d_nb1_in_idx = 0; 
            if (++d_nb1_out_idx == d_nb1_dline_size) d_nb1_out_idx = 0;
        }



}

/*! \brief Perform noise blanker 2 processing.
 *  \param buf The data buffer holding gr_complex samples.
 *  \param num The number of samples in the buffer.
 *
 * Noise blanker 2 is the second noise blanker in the processing chain.
 * It is intended to reduce non-pulse type noise (i.e. longer time constants).
 *
 * FIXME: Needs different constants for higher sample rates?
 */
void rx_nb_cc::process_nb2(gr_complex *buf, int num)
{
/*
    float cmag;
    gr_complex c1(0.75);
    gr_complex c2(0.25);

    for (int i = 0; i < num; i++)
    {
        cmag = abs(buf[i]);
        d_avgsig = c1*d_avgsig + c2*buf[i];
        d_avgmag_nb2 = 0.999*d_avgmag_nb2 + 0.001*cmag;

        if (cmag > d_thld_nb2*d_avgmag_nb2)
            buf[i] = d_avgsig;
    }
*/

    double cmag;
    double scale;
    int bf_idx;
    int ff_idx;
    int lidx, tidx;
    int i, j, k;
    int bfboutidx;
    int ffboutidx;
    int hcount;
    int len;
    int ffcount;
    int staydown;

    for (i = 0; i < num; i++)
    {
        d_nb2_dline[d_nb2_in_idx] = buf[i];
        cmag = abs(d_nb2_dline[d_nb2_in_idx]);
        d_nb2_avg = d_nb2_backmult * d_nb2_avg + d_nb2_ombackmult * cmag;
        if (cmag > (d_nb2_avg * d_nb2_threshold))
            d_nb2_imp[d_nb2_in_idx] = 1;
        else
            d_nb2_imp[d_nb2_in_idx] = 0;
        // TODO
        if ((bf_idx = d_nb2_out_idx + d_nb2_adv_slew_count) >= d_nb2_dline_size) bf_idx -= d_nb2_dline_size;
        if (d_nb2_imp[bf_idx] == 0)
        {
            if (++d_nb2_bfb_in_idx == d_nb2_filterlen) d_nb2_bfb_in_idx -= d_nb2_filterlen;
            d_nb2_bfbuff[d_nb2_bfb_in_idx] = d_nb2_dline[bf_idx];
        }
        switch (d_nb2_state)
        {
            case 0:
            {
                buf[i] = d_nb2_dline[d_nb2_out_idx];
                d_nb2_last = d_nb2_dline[d_nb2_out_idx];
                if (d_nb2_imp[d_nb2_scan_idx] > 0)
                {
                    d_nb2_time = 0;
                    if (d_nb2_adv_slew_count > 0)
                        d_nb2_state = 1;
                    else if (d_nb2_adv_count > 0)
                        d_nb2_state = 2;
                    else
                        d_nb2_state = 3;
                    tidx = d_nb2_scan_idx;
                    d_nb2_blank_count = 0;
                    // TODO

                    do
                    {
                        len = 0;
                        hcount = 0;
                        while ((d_nb2_imp[tidx] > 0 || hcount > 0) && d_nb2_blank_count < d_nb2_max_imp_seq)
                        {
                            d_nb2_blank_count++;
                            if (hcount > 0) hcount--;
                            if (d_nb2_imp[tidx] > 0) hcount = d_nb2_hang_count + d_nb2_hang_slew_count;
                            if (++tidx >= d_nb2_dline_size) tidx -= d_nb2_dline_size;
                        }
                        j = 1;
                        len = 0;
                        lidx = tidx;
                        while (j <= d_nb2_adv_slew_count + d_nb2_adv_count && len == 0)
                        {
                            if (d_nb2_imp[lidx] == 1)
                            {
                                len = j;
                                tidx = lidx;
                            }
                            if (++lidx >= d_nb2_dline_size) lidx -= d_nb2_dline_size;
                            j++;
                        }
                        if((d_nb2_blank_count += len) > d_nb2_max_imp_seq)
                        {
                            d_nb2_blank_count = d_nb2_max_imp_seq;
                            d_nb2_overflow = 1;
                            break;
                        }
                    } while (len != 0);
                    if (d_nb2_overflow == 0)
                    {
                        d_nb2_blank_count -= d_nb2_hang_slew_count;
                        d_nb2_next = d_nb2_dline[tidx];

                        if (d_nb2_mode == 1 || d_nb2_mode == 2 || d_nb2_mode == 4)
                        {
                            bfboutidx = d_nb2_bfb_in_idx;
                            d_nb2_IQ = gr_complex(0.0,0.0);
                            for (k = 0; k < d_nb2_filterlen; k++)
                            {
                                d_nb2_IQ += d_nb2_fcoefs[k] * d_nb2_bfbuff[bfboutidx];
                                if (--bfboutidx < 0) bfboutidx += d_nb2_filterlen;
                            }
                        }
                        if (d_nb2_mode == 2 || d_nb2_mode == 3 || d_nb2_mode == 4)
                        {
                            if ((ff_idx = d_nb2_scan_idx + d_nb2_blank_count) >= d_nb2_dline_size) ff_idx -= d_nb2_dline_size;
                            ffcount = 0;
                            while (ffcount < d_nb2_filterlen)
                            {
                                if (d_nb2_imp[ff_idx] == 0)
                                {
                                    if (++d_nb2_ffb_in_idx == d_nb2_filterlen) d_nb2_ffb_in_idx -= d_nb2_filterlen;
                                    d_nb2_ffbuff[d_nb2_ffb_in_idx] = d_nb2_dline[ff_idx];
                                    ++ffcount;
                                }
                                if (++ff_idx >= d_nb2_dline_size) ff_idx -= d_nb2_dline_size;
                            }
                            if ((ffboutidx = d_nb2_ffb_in_idx + 1) >= d_nb2_filterlen) ffboutidx -= d_nb2_filterlen;
                            d_nb2_IQ2 = gr_complex(0.0,0.0);
                            for (k = 0; k < d_nb2_filterlen; k++)
                            {
                                d_nb2_IQ2 += d_nb2_fcoefs[k] * d_nb2_ffbuff[ffboutidx];
                                if (++ffboutidx >= d_nb2_filterlen) ffboutidx -= d_nb2_filterlen;
                            }
                        }
                        switch (d_nb2_mode)
                        {
                            case 0: // zero
                                d_nb2_deltaIQ = gr_complex(0.0,0.0);
                                d_nb2_IQ = gr_complex(0.0,0.0);
                                break;
                            case 1: // sample-hold
                                d_nb2_deltaIQ = gr_complex(0.0,0.0);
                                d_nb2_IQ = d_nb2_IQ1;
                                break;
                            case 2: // mean-hold
                                d_nb2_deltaIQ = gr_complex(0.0,0.0);
                                d_nb2_IQ = gr_complex(0.5,0.5) * (d_nb2_IQ1 + d_nb2_IQ2);
                                break;
                            case 3: // hold-sample
                                d_nb2_deltaIQ = gr_complex(0.0,0.0);
                                d_nb2_IQ = d_nb2_IQ2;
                                break;
                            case 4: // linear interpolation
                                d_nb2_deltaIQ = d_nb2_IQ2 - d_nb2_IQ1;
                                d_nb2_IQ = d_nb2_IQ1;
                                break;
                        }
                    }
                    else
                    {
                        if (d_nb2_adv_slew_count > 0)
                            d_nb2_state = 5;
                        else
                        {
                            d_nb2_state = 6;
                            d_nb2_time = 0;
                            d_nb2_blank_count += d_nb2_adv_count + d_nb2_filterlen;
                        }
                    }
                }
                break;
            }
            case 1:     // slew output in advance of blanking period
            {
                scale = 0.5 + d_nb2_awave[d_nb2_time];
                buf[i] = d_nb2_last * gr_complex(scale,scale) + gr_complex(1.0-scale,1.0-scale)*d_nb2_IQ;
                if (++d_nb2_time == d_nb2_adv_slew_count)
                {
                    d_nb2_time = 0;
                    if (d_nb2_adv_count > 0)
                        d_nb2_state = 2;
                    else
                        d_nb2_state = 3;
                }
                break;
            }
            case 2:     // initial advance period
            {
                buf[i] = d_nb2_IQ;
                d_nb2_IQ = d_nb2_deltaIQ;
                if (++d_nb2_time == d_nb2_adv_count)
                {
                    d_nb2_state = 3;
                    d_nb2_time = 0;
                }
                break;
            }
            case 3:     // impulse & hang period
            {
                buf[i] = d_nb2_IQ;
                d_nb2_IQ = d_nb2_deltaIQ;
                if (++d_nb2_time == d_nb2_blank_count)
                {
                    if (d_nb2_hang_slew_count > 0)
                    {
                        d_nb2_state = 4;
                        d_nb2_time = 0;
                    }
                    else 
                        d_nb2_state = 0;
                }
                break;
            }
            case 4:     // slew output after blanking period
            {
                scale = 0.5 - d_nb2_hwave[d_nb2_time];
                buf[i] = d_nb2_next * gr_complex(scale,scale) + gr_complex(1.0-scale,1.0-scale)*d_nb2_IQ;
                if (++d_nb2_time == d_nb2_hang_slew_count)
                    d_nb2_state = 0;
                break;
            }
            case 5:
            {
                scale = 0.5 + d_nb2_awave[d_nb2_time];
                buf[i] = d_nb2_last * gr_complex(scale,scale);
                if (++d_nb2_time == d_nb2_adv_slew_count)
                {
                    d_nb2_state = 6;
                    d_nb2_time = 0;
                    d_nb2_blank_count += d_nb2_adv_count + d_nb2_filterlen;
                }
                break;
            }
            case 6:
            {
                buf[i] = gr_complex(0.0,0.0);
                if (++d_nb2_time == d_nb2_blank_count)
                    d_nb2_state = 7;
                break;
            }
            case 7:
            {
                buf[i] = gr_complex(0.0,0.0);
                staydown = 0;
                d_nb2_time = 0;
                if ((tidx = d_nb2_scan_idx + d_nb2_hang_slew_count + d_nb2_hang_count) >= d_nb2_dline_size) tidx -= d_nb2_dline_size;
                while (d_nb2_time++ <= d_nb2_adv_count + d_nb2_adv_slew_count + d_nb2_hang_slew_count + d_nb2_hang_count)                                                                            //  CHECK EXACT COUNTS!!!!!!!!!!!!!!!!!!!!!!!
                {
                    if (d_nb2_imp[tidx] == 1) staydown = 1;
                    if (--tidx < 0) tidx += d_nb2_dline_size;
                }
                if (staydown == 0)
                {
                    if (d_nb2_hang_count > 0)
                    {
                        d_nb2_state = 8;
                        d_nb2_time = 0;
                    }
                    else if (d_nb2_hang_slew_count > 0)
                    {
                        d_nb2_state = 9;
                        d_nb2_time = 0;
                        if ((tidx = d_nb2_scan_idx + d_nb2_hang_slew_count + d_nb2_hang_count - d_nb2_adv_count - d_nb2_adv_slew_count) >= d_nb2_dline_size) tidx -= d_nb2_dline_size;
                        if (tidx < 0) tidx += d_nb2_dline_size;
                        d_nb2_IQ = d_nb2_dline[tidx];
                    }
                    else
                    {
                        d_nb2_state = 0;
                        d_nb2_overflow = 0;
                    }
                }
                break;
            }
            case 8:
            {
                buf[i] = gr_complex(0.0,0.0);
                if (++d_nb2_time == d_nb2_hang_count)
                {
                    if (d_nb2_hang_slew_count > 0)
                    {
                        d_nb2_state = 9;
                        d_nb2_time = 0;
                        if ((tidx = d_nb2_scan_idx + d_nb2_hang_slew_count - d_nb2_adv_count - d_nb2_adv_slew_count) >= d_nb2_dline_size) tidx -= d_nb2_dline_size;
                        if (tidx < 0) tidx += d_nb2_dline_size;
                        d_nb2_next = d_nb2_dline[tidx];
                    }
                    else
                    {
                        d_nb2_state = 0;
                        d_nb2_overflow = 0;
                    }
                }
                break;
            }
            case 9:
            {
                scale = 0.5 - d_nb2_hwave[d_nb2_time];
                buf[i] = d_nb2_next * gr_complex(scale,scale);
                if (++d_nb2_time >= d_nb2_hang_slew_count)
                {
                    d_nb2_state = 0;
                    d_nb2_overflow = 0;
                }
                break;
            }
        }
        if (++d_nb2_in_idx == d_nb2_dline_size) d_nb2_in_idx = 0;
        if (++d_nb2_scan_idx == d_nb2_dline_size) d_nb2_scan_idx = 0;
        if (++d_nb2_out_idx == d_nb2_dline_size) d_nb2_out_idx = 0;
    }



}

void rx_nb_cc::set_threshold1(float threshold)
{
    if ((threshold >= 1.0) && (threshold <= 20.0))
        d_thld_nb1 = threshold;
}

void rx_nb_cc::set_threshold2(float threshold)
{
    if ((threshold >= 0.0) && (threshold <= 15.0))
        d_thld_nb2 = threshold;
}
