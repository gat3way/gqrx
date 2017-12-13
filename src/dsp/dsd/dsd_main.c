/*
 * Copyright (C) 2010 DSD Author
 * GPG Key ID: 0x3F1D7FD0 (74EF 430D F7F2 0A48 FCE6  F630 FAA2 635D 3F1D 7FD0)
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#define _MAIN

#include "dsd.h"
#include "p25p1_const.h"
#include "x2tdma_const.h"
#include "dstar_const.h"
#include "nxdn_const.h"
#include "dmr_const.h"
#include "provoice_const.h"
//#include "git_ver.h"
#include "p25p1_heuristics.h"


#ifndef MBE_FOUND
// dummy mbe params - FIX THAT 10k is too much probably
char mbe_parms[10000];
#endif


int
comp (const void *a, const void *b)
{
  if (*((const int *) a) == *((const int *) b))
    return 0;
  else if (*((const int *) a) < *((const int *) b))
    return -1;
  else
    return 1;
}

void
noCarrier (dsd_opts * opts, dsd_state * state)
{
  state->dibit_buf_p = state->dibit_buf + 200;
  memset (state->dibit_buf, 0, sizeof (int) * 200);
  state->jitter = -1;
  state->lastsynctype = -1;
  state->carrier = 0;
  state->max = 15000;
  state->min = -15000;
  state->center = 0;
  state->err_str[0] = 0;
  sprintf (state->fsubtype, "              ");
  sprintf (state->ftype, "             ");
  state->errs = 0;
  state->errs2 = 0;
  state->lasttg = 0;
  state->lastsrc = 0;
  state->last_dibit = 0;
  state->lastp25type = 0;
  state->repeat = 0;
  state->nac = 0;
  state->numtdulc = 0;
  sprintf (state->slot0light, " slot0 ");
  sprintf (state->slot1light, " slot1 ");
  state->firstframe = 0;
  if (opts->audio_gain == (float) 0)
    {
      state->aout_gain = 25;
    }
  memset (state->aout_max_buf, 0, sizeof (float) * 200);

  state->aout_max_buf_p = state->aout_max_buf;
  state->aout_max_buf_idx = 0;
  sprintf (state->algid, "________");
  sprintf (state->keyid, "________________");
#ifdef MBE_FOUND
  mbe_initMbeParms (state->cur_mp, state->prev_mp, state->prev_mp_enhanced);
#endif
}

void
initOpts (dsd_opts * opts)
{

  opts->onesymbol = 10;
  opts->errorbars = 1;
  opts->datascope = 0;
  opts->symboltiming = 0;
  opts->verbose = 2;
  opts->p25enc = 0;
  opts->p25lc = 0;
  opts->p25status = 0;
  opts->p25tg = 0;
  opts->scoperate = 15;
  opts->audio_in_fd = -1;
  opts->audio_out_fd = -1;
  opts->split = 0;
  opts->playoffset = 0;
  opts->audio_gain = 0;
  opts->audio_out = 1;
  opts->resume = 0;
  opts->frame_dstar = 0;
  opts->frame_x2tdma = 1;
  opts->frame_p25p1 = 1;
  opts->frame_nxdn48 = 0;
  opts->frame_nxdn96 = 1;
  opts->frame_dmr = 1;
  opts->frame_provoice = 0;
  opts->mod_c4fm = 1;
  opts->mod_qpsk = 1;
  opts->mod_gfsk = 1;
  opts->uvquality = 3;
  opts->inverted_x2tdma = 1;    // most transmitter + scanner + sound card combinations show inverted signals for this
  opts->inverted_dmr = 0;       // most transmitter + scanner + sound card combinations show non-inverted signals for this
  opts->mod_threshold = 26;
  opts->ssize = 36;
  opts->msize = 15;
  opts->playfiles = 0;
  opts->delay = 0;
  opts->use_cosine_filter = 1;
  opts->unmute_encrypted_p25 = 0;
}

void
initState (dsd_state * state)
{

  int i, j;

  state->dibit_buf = malloc (sizeof (int) * 1000000);
  state->dibit_buf_p = state->dibit_buf + 200;
  memset (state->dibit_buf, 0, sizeof (int) * 200);
  state->repeat = 0;
  state->audio_out_buf = malloc (sizeof (short) * 1000000);
  memset (state->audio_out_buf, 0, 100 * sizeof (short));
  state->audio_out_buf_p = state->audio_out_buf + 100;
  state->audio_out_float_buf = malloc (sizeof (float) * 1000000);
  memset (state->audio_out_float_buf, 0, 100 * sizeof (float));
  memset (state->src_list, 0, sizeof (long) * 50);
  state->audio_out_float_buf_p = state->audio_out_float_buf + 100;
  state->audio_out_idx = 0;
  state->audio_out_idx2 = 0;
  state->audio_out_temp_buf_p = state->audio_out_temp_buf;
  //state->wav_out_bytes = 0;
  state->center = 0;
  state->jitter = -1;
  state->synctype = -1;
  state->min = -15000;
  state->max = 15000;
  state->lmid = 0;
  state->umid = 0;
  state->minref = -12000;
  state->maxref = 12000;
  state->lastsample = 0;
  for (i = 0; i < 128; i++)
    {
      state->sbuf[i] = 0;
    }
  state->sidx = 0;
  for (i = 0; i < 1024; i++)
    {
      state->maxbuf[i] = 15000;
    }
  for (i = 0; i < 1024; i++)
    {
      state->minbuf[i] = -15000;
    }
  state->midx = 0;
  state->err_str[0] = 0;
  sprintf (state->fsubtype, "              ");
  sprintf (state->ftype, "             ");
  state->symbolcnt = 0;
  state->rf_mod = 0;
  state->numflips = 0;
  state->lastsynctype = -1;
  state->lastp25type = 0;
  state->offset = 0;
  state->carrier = 0;
  for (i = 0; i < 25; i++)
    {
      for (j = 0; j < 16; j++)
        {
          state->tg[i][j] = 48;
        }
    }
  state->tgcount = 0;
  state->lasttg = 0;
  state->lastsrc = 0;
  state->nac = 0;
  state->errs = 0;
  state->errs2 = 0;
  state->mbe_file_type = -1;
  state->optind = 0;
  state->numtdulc = 0;
  state->firstframe = 0;
  sprintf (state->slot0light, " slot0 ");
  sprintf (state->slot1light, " slot1 ");
  state->aout_gain = 25;
  memset (state->aout_max_buf, 0, sizeof (float) * 200);
  state->aout_max_buf_p = state->aout_max_buf;
  state->aout_max_buf_idx = 0;
  state->samplesPerSymbol = 10;
  state->symbolCenter = 4;
  sprintf (state->algid, "________");
  sprintf (state->keyid, "________________");
  state->currentslot = 0;
  state->cur_mp = malloc (sizeof (mbe_parms));
  state->prev_mp = malloc (sizeof (mbe_parms));
  state->prev_mp_enhanced = malloc (sizeof (mbe_parms));
#ifdef MBE_FOUND
  mbe_initMbeParms (state->cur_mp, state->prev_mp, state->prev_mp_enhanced);
#endif
  state->p25kid = 0;
  state->exitflag = 0;

  state->debug_audio_errors = 0;
  state->debug_header_errors = 0;
  state->debug_header_critical_errors = 0;

#ifdef TRACE_DSD
  state->debug_sample_index = 0;
  state->debug_label_file = NULL;
  state->debug_label_dibit_file = NULL;
  state->debug_label_imbe_file = NULL;
#endif
  state->last_dibit = 0;
  initialize_p25_heuristics(&state->p25_heuristics);
}


void puts_thread_scheduling(char *who)
{
    struct sched_param thread_param;
    pthread_attr_t thread_attr;
    int thread_policy = 0;

    pthread_attr_init(&thread_attr);
    pthread_attr_getschedparam(&thread_attr, &thread_param);
    pthread_attr_getschedpolicy(&thread_attr, &thread_policy);
    printf("[%s] priority: %d\n", who, thread_param.sched_priority);
    printf("[%s] schedule: ", who);
    switch(thread_policy){
    case SCHED_FIFO: printf("FIFO"); break;
    case SCHED_RR: printf("RR"); break;
    case SCHED_OTHER: printf("OTHER"); break;
    default: printf("UNKONW"); break;
}
printf("\n");

}

int needQuit(dsd_state * state)
{

  switch(pthread_mutex_trylock(&state->quit_mutex)) {
    case 0: /* if we got the lock, unlock and return 1 (true) */
      pthread_mutex_unlock(&state->quit_mutex);
      return 1;
    case EBUSY: /* return 0 (false) if the mutex was locked */
      return 0;
  }
  return 1;
}



void
liveScanner (dsd_opts * opts, dsd_state * state)
{
/*
	long tid;
	long pid;
	tid = syscall(SYS_gettid);
	pid = pthread_self();

	printf("[ Pthread: %lu - PID: %ld ]\n",pid,tid);
	puts_thread_scheduling("Thread");*/

  state->exitflag = 0;

  if (opts->audio_in_fd == -1)
    {
      if (pthread_mutex_lock(&state->input_mutex))
        {
          printf("liveScanner -> Unable to lock mutex\n");
        }
    }

  while (!state->exitflag) //!needQuit(state))
    {
      pthread_testcancel();
      noCarrier (opts, state);
      state->synctype = getFrameSync (opts, state);
      // recalibrate center/umid/lmid
      state->center = ((state->max) + (state->min)) / 2;
      state->umid = (((state->max) - state->center) * 5 / 8) + state->center;
      state->lmid = (((state->min) - state->center) * 5 / 8) + state->center;
      while (state->synctype != -1)
        {
	         if(state->exitflag) {
		           break;
             }
          processFrame (opts, state);

#ifdef TRACE_DSD
          state->debug_prefix = 'S';
#endif

          state->synctype = getFrameSync (opts, state);

#ifdef TRACE_DSD
          state->debug_prefix = '\0';
#endif

          // recalibrate center/umid/lmid
          state->center = ((state->max) + (state->min)) / 2;
          state->umid = (((state->max) - state->center) * 5 / 8) + state->center;
          state->lmid = (((state->min) - state->center) * 5 / 8) + state->center;
        }
    }
 printf("dsd_main.c: Recieved Thread Canceling - Exiting \n");
}

