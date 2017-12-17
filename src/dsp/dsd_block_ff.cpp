/* -*- c++ -*- */
/*
 * Copyright 2004,2010 Free Software Foundation, Inc.
 *
 * This file is part of GNU Radio
 *
 * GNU Radio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * GNU Radio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Radio; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

/*
 * config.h is generated by configure.  It contains the results
 * of probing for features, options etc.  It should be the first
 * file included in your .cc file.
 */

#include "dsp/dsd_block_ff.h"
#include <gnuradio/io_signature.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>



CdsdProxy *CdsdProxy::instance = 0;


/*
 * Create a new instance of dsd_block_ff and return
 * a boost shared_ptr.  This is effectively the public constructor.
 */
dsd_block_ff_sptr
dsd_make_block_ff (dsd_frame_mode frame, dsd_modulation_optimizations mod, int uvquality, bool errorbars, int verbosity, bool empty, int num)
{
    return gnuradio::get_initial_sptr(new dsd_block_ff (frame, mod, uvquality, errorbars, verbosity, empty, num));
}

/*
 * Specify constraints on number of input and output streams.
 * This info is used to construct the input and output signatures
 * (2nd & 3rd args to gr_block's constructor).  The input and
 * output signatures are used by the runtime system to
 * check that a valid number and type of inputs and outputs
 * are connected to this block.  In this case, we accept
 * only 1 input and 1 output.
 */
static const int MIN_IN = 1;	// mininum number of input streams
static const int MAX_IN = 1;	// maximum number of input streams
static const int MIN_OUT = 1;	// minimum number of output streams
static const int MAX_OUT = 1;	// maximum number of output streams



void cleanupHandler(void *arg) {
    dsd_params *params = (dsd_params *) arg;

    pthread_mutex_destroy(&params->state.output_mutex);
    pthread_mutex_destroy(&params->state.input_mutex);
    pthread_cond_destroy(&params->state.input_ready);
    pthread_cond_destroy(&params->state.output_ready);
}



void* run_dsd (void *arg)
{
    dsd_params *params = (dsd_params *) arg;
    pthread_cleanup_push(cleanupHandler, arg);
    liveScanner (&params->opts, &params->state);
    pthread_cleanup_pop(0);
    return NULL;
}



void dsd_block_ff::reset_state(){
    dsd_state *state = &params.state;
    memset (state->src_list, 0, sizeof (long) * 50);
    memset (state->xv, 0, sizeof (float) * (NZEROS+1));
    memset (state->nxv, 0, sizeof (float) * (NXZEROS+1));
    state->debug_audio_errors = 0;
    state->debug_header_errors = 0;
    state->debug_header_critical_errors = 0;
    state->symbolcnt = 0;
    initialize_p25_heuristics(&state->p25_heuristics);
}



dsd_state *dsd_block_ff::get_state()
{
  return &params.state;
}



/*
 * The private constructor
 */

dsd_block_ff::dsd_block_ff (dsd_frame_mode frame, dsd_modulation_optimizations mod, int uvquality, bool errorbars, int verbosity, bool empty, int num)
  : gr::sync_decimator ("dsd_block_ff",
	      gr::io_signature::make(MIN_IN, MAX_IN, sizeof (float)),
	      gr::io_signature::make(MIN_OUT, MAX_OUT, sizeof (float)), 6)
{
    initOpts (&params.opts);
    initState (&params.state);
    pthread_attr_t tattr;

    proxy = CdsdProxy::getInstance();

    params.num = num;
    params.opts.split = 1;
    params.opts.playoffset = 0;
    params.opts.delay = 0;

    //params.opts.symboltiming = 1;
    //params.opts.errorbars = 1;

    set_mode(frame);

    params.opts.uvquality = uvquality;
    params.opts.verbose = verbosity;
    params.opts.errorbars = errorbars;

    empty = 0;
    empty_frames = empty;

    set_optimization(mod);


    // Initialize the mutexes
    if(pthread_mutex_init(&params.state.input_mutex, NULL))
    {
        printf("Unable to initialize input mutex\n");
    }
    if(pthread_mutex_init(&params.state.output_mutex, NULL))
    {
        printf("Unable to initialize output mutex\n");
    }
    if(pthread_mutex_init(&params.state.quit_mutex, NULL))
    {
        printf("Unable to initialize quit mutex\n");
    }

    // Initialize the conditions
    if(pthread_cond_init(&params.state.input_ready, NULL))
    {
        printf("Unable to initialize input condition\n");
    }
    if(pthread_cond_init(&params.state.output_ready, NULL))
    {
        printf("Unable to initialize output condition\n");
    }
    // Lock output mutex
    if (pthread_mutex_lock(&params.state.output_mutex))
    {
        printf("Unable to lock mutex\n");
    }

    if (!empty_frames) {
        set_output_multiple(120);
    }
    params.state.msgbuf = (char*)malloc(4096);
    memset(params.state.msgbuf,0,4096);
    params.state.input_length = 0;
    params.state.output_buffer = (short *) malloc(4 * 80000); // TODO: Make this variable size.
    params.state.output_offset = 0;
    if (params.state.output_buffer == NULL)
    {
        printf("Unable to allocate output buffer.\n");
    }

    int error;
    if ((error=pthread_attr_init(&tattr)))
    {
        fprintf(stderr,"Attribute initialization failed with error %s\n",strerror(error));
    }


    pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);

    if(pthread_create(&dsd_thread, &tattr, &run_dsd, &params))
    {
        printf("Unable to spawn thread\n");
    }
    pthread_attr_destroy(&tattr);

}




/*
 * Our virtual destructor.
 */
dsd_block_ff::~dsd_block_ff ()
{
    pthread_cancel(dsd_thread);
    usleep(1000*1000);

    free(params.state.output_buffer);
    free(params.state.dibit_buf);
    free(params.state.audio_out_buf);
    free(params.state.audio_out_float_buf);
    free(params.state.cur_mp);
    free(params.state.prev_mp);
    free(params.state.prev_mp_enhanced);
    free(params.state.msgbuf);
}


void dsd_block_ff::set_optimization(dsd_modulation_optimizations optimizations)
{
    optimization = optimizations;

    if (optimizations == dsd_MOD_AUTO_SELECT)
    {
        params.opts.mod_c4fm = 1;
        params.opts.mod_qpsk = 1;
        params.opts.mod_gfsk = 1;
        params.state.rf_mod = 0;
    }
    else if (optimizations == dsd_MOD_C4FM)
    {
        params.opts.mod_c4fm = 1;
        params.opts.mod_qpsk = 0;
        params.opts.mod_gfsk = 0;
        params.state.rf_mod = 0;
        //printf ("Enabling only C4FM modulation optimizations.\n");
    }
    else if (optimizations == dsd_MOD_GFSK)
    {
        params.opts.mod_c4fm = 0;
        params.opts.mod_qpsk = 0;
        params.opts.mod_gfsk = 1;
        params.state.rf_mod = 2;
        //printf ("Enabling only GFSK modulation optimizations.\n");
    }
    else if (optimizations == dsd_MOD_QPSK)
    {
        params.opts.mod_c4fm = 0;
        params.opts.mod_qpsk = 1;
        params.opts.mod_gfsk = 0;
        params.state.rf_mod = 1;
        //printf ("Enabling only QPSK modulation optimizations.\n");
    }
}

void dsd_block_ff::set_mode(dsd_frame_mode frame)
{
    frame_mode = frame;
    if (frame == dsd_FRAME_AUTO_DETECT)
    {
        params.state.samplesPerSymbol = 10;
        params.state.symbolCenter = 4;
        params.opts.frame_dstar = 0;
        params.opts.frame_x2tdma = 1;
        params.opts.frame_p25p1 = 1;
        params.opts.frame_nxdn48 = 0;
        params.opts.frame_nxdn96 = 1;
        params.opts.frame_dmr = 1;
        params.opts.frame_provoice = 0;
    }
    else if (frame == dsd_FRAME_DSTAR)
    {
        params.state.samplesPerSymbol = 10;
        params.state.symbolCenter = 4;
        params.opts.frame_dstar = 1;
        params.opts.frame_x2tdma = 0;
        params.opts.frame_p25p1 = 0;
        params.opts.frame_nxdn48 = 0;
        params.opts.frame_nxdn96 = 0;
        params.opts.frame_dmr = 0;
        params.opts.frame_provoice = 0;
        //printf ("Decoding only D-STAR frames.\n");
    }
    else if (frame == dsd_FRAME_X2_TDMA)
    {
        params.state.samplesPerSymbol = 10;
        params.state.symbolCenter = 4;
        params.opts.frame_dstar = 0;
        params.opts.frame_x2tdma = 1;
        params.opts.frame_p25p1 = 0;
        params.opts.frame_nxdn48 = 0;
        params.opts.frame_nxdn96 = 0;
        params.opts.frame_dmr = 0;
        params.opts.frame_provoice = 0;
        //printf ("Decoding only X2-TDMA frames.\n");
    }
    else if (frame == dsd_FRAME_PROVOICE)
    {
        params.opts.frame_dstar = 0;
        params.opts.frame_x2tdma = 0;
        params.opts.frame_p25p1 = 0;
        params.opts.frame_nxdn48 = 0;
        params.opts.frame_nxdn96 = 0;
        params.opts.frame_dmr = 0;
        params.opts.frame_provoice = 1;
        params.state.samplesPerSymbol = 5;
        params.state.symbolCenter = 2;
        params.opts.mod_c4fm = 0;
        params.opts.mod_qpsk = 0;
        params.opts.mod_gfsk = 1;
        params.state.rf_mod = 2;
        //printf ("Setting symbol rate to 9600 / second\n");
        //printf ("Enabling only GFSK modulation optimizations.\n");
        //printf ("Decoding only ProVoice frames.\n");
    }
    else if (frame == dsd_FRAME_P25_PHASE_1)
    {
        params.state.samplesPerSymbol = 10;
        params.state.symbolCenter = 4;
        params.opts.frame_dstar = 0;
        params.opts.frame_x2tdma = 0;
        params.opts.frame_p25p1 = 1;
        params.opts.frame_nxdn48 = 0;
        params.opts.frame_nxdn96 = 0;
        params.opts.frame_dmr = 0;
        params.opts.frame_provoice = 0;
        //printf ("Decoding only P25 Phase 1 frames.\n");
    }
    else if (frame == dsd_FRAME_NXDN48_IDAS)
    {
        params.opts.frame_dstar = 0;
        params.opts.frame_x2tdma = 0;
        params.opts.frame_p25p1 = 0;
        params.opts.frame_nxdn48 = 1;
        params.opts.frame_nxdn96 = 0;
        params.opts.frame_dmr = 0;
        params.opts.frame_provoice = 0;
        params.state.samplesPerSymbol = 20;
        params.state.symbolCenter = 10;
        params.opts.mod_c4fm = 1;
        params.opts.mod_qpsk = 0;
        params.opts.mod_gfsk = 0;
        params.state.rf_mod = 0;
        //printf ("Setting symbol rate to 2400 / second\n");
        //printf ("Enabling only GFSK modulation optimizations.\n");
        //printf ("Decoding only NXDN 4800 baud frames.\n");
    }
    else if (frame == dsd_FRAME_NXDN96)
    {
        params.state.samplesPerSymbol = 10;
        params.state.symbolCenter = 4;
        params.opts.frame_dstar = 0;
        params.opts.frame_x2tdma = 0;
        params.opts.frame_p25p1 = 0;
        params.opts.frame_nxdn48 = 0;
        params.opts.frame_nxdn96 = 1;
        params.opts.frame_dmr = 0;
        params.opts.frame_provoice = 0;
        params.opts.mod_c4fm = 1;
        params.opts.mod_qpsk = 0;
        params.opts.mod_gfsk = 0;
        params.state.rf_mod = 0;
        //printf ("Enabling only GFSK modulation optimizations.\n");
        //printf ("Decoding only NXDN 9600 baud frames.\n");
    }
    else if (frame == dsd_FRAME_DMR_MOTOTRBO)
    {
        params.state.samplesPerSymbol = 10;
        params.state.symbolCenter = 4;
        params.opts.frame_dstar = 0;
        params.opts.frame_x2tdma = 0;
        params.opts.frame_p25p1 = 0;
        params.opts.frame_nxdn48 = 0;
        params.opts.frame_nxdn96 = 0;
        params.opts.frame_dmr = 1;
        params.opts.frame_provoice = 0;
        params.opts.mod_c4fm = 1;
        params.opts.mod_qpsk = 0;
        params.opts.mod_gfsk = 0;
        params.state.rf_mod = 0;
        //printf ("Decoding only DMR/MOTOTRBO frames.\n");
    }
}


dsd_frame_mode dsd_block_ff::get_mode()
{
    return frame_mode;
}



int dsd_block_ff::work(int noutput_items,
        gr_vector_const_void_star &input_items,
        gr_vector_void_star &output_items)
{
  int i;
  int send_to_dsd = 0;
  const float *in = (const float *) input_items[0];
  float *out = (float *) output_items[0];

  for (i = 0; i < noutput_items * 6; i++) {
    if (in[i] != 0) {
      send_to_dsd = 1;
      break;
    }
  }
  if (!send_to_dsd) {
    // All samples are zero, so skip DSD processing.
    for (i = 0; i < noutput_items; i++) {
      out[i] = 0;
    }
    return noutput_items;
  }

  params.state.output_samples = out;
  params.state.output_num_samples = 0;
  params.state.output_length = noutput_items;
  params.state.output_finished = 0;

  if (pthread_mutex_lock(&params.state.input_mutex))
  {
    printf("Unable to lock mutex\n");
  }

  params.state.input_samples = in;
  params.state.input_length = noutput_items * 6;
  params.state.input_offset = 0;

  if (pthread_cond_signal(&params.state.input_ready))
  {
    printf("Unable to signal\n");
  }

  if (pthread_mutex_unlock(&params.state.input_mutex))
  {
    printf("Unable to unlock mutex\n");
  }

  while (params.state.output_finished == 0)
  {
    if (pthread_cond_wait(&params.state.output_ready, &params.state.output_mutex))
    {
      printf("general_work -> Error waiting for condition\n");
    }
  }
  if (params.state.msgbuf[0]!=0)
  {
      params.state.msgbuf[1024] = 0;
      proxy->send(params.state.msgbuf);
      memset(params.state.msgbuf,0,1024);
  }

  // Tell runtime system how many output items we produced.
  return noutput_items;
}

