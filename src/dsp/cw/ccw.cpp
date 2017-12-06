/*
 *      ccw.cpp -- ALE demodulator class
 *
 *      Copyright (C) 2017 Milen Rangelov
 *      Significant portions of the code are used from xdemorse project 
 *      Which is Copyright (C) Neoklis Kyriazis (nkcyham at yahoo.com)
 *
 *      Copyright (C) 2011 Alexandru Csete (oz9aec at gmail.com)
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <QDebug>
#include <QTime>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include "ccw.h"

#define CYCLES_PER_FRAG          2
#define SQUELCH_MULTIPLIER       150
#define RATIO_MULTIPLIER         20.0
#define ADAPT_SPEED              0x000001
#define MARK_TONE                0x000800
#define SPACE_TONE               0x001000

#define NUMBER_OF_CHAR           55


#define MORSE_ASCII_CHAR \
{ \
  'A','B','C','D','E','F','G','H','I','J','K','L','M', \
  'N','O','P','Q','R','S','T','U','V','W','X','Y','Z', \
  '1','2','3','4','5','6','7','8','9','0','.',',',':', \
  '?','\'','-','/','(','"',';','$','#','<','!','@',']',\
  '=','~',' ','*' \
}

#define MORSE_HEX_CODE \
{ \
  0x06,0x17,0x15,0x0b,0x03,0x1d,0x09,0x1f,0x07,0x18,0x0a,0x1b,0x04,\
  0x05,0x08,0x19,0x12,0x0d,0x0f,0x02,0x0e,0x1e,0x0c,0x16,0x14,0x13,\
  0x30,0x38,0x3c,0x3e,0x3f,0x2f,0x27,0x23,0x21,0x20,0x6a,0x4c,0x47,\
  0x73,0x61,0x5e,0x2d,0x52,0x6d,0x55,0xf6,0x35,0x7a,0x2a,0x37,0x29,\
  0x2e,0xff,0x01 \
}

#define NO_CONTEXT              0
#define MARK_SIGNAL             0x000001
#define ELEM_SPACE              0x000002
#define CHAR_SPACE              0x000004
#define WAIT_WORD_SPACE         0x000008
#define WORD_SPACE              0x000010
#define WAIT_FOR_MARK           0x000020
#define WAIT_FOR_SPACE          0x000040
#define LONG_SPACE              0x000100

#define ENTER_DOT               0x01

static int verbose_level = 10;
static bool first = true;


CCw::CCw(QObject *parent) :
    QObject(parent)
{
    reset();
}

CCw::~CCw()
{
    delete sigfft;
    free(detector_data.samples_buff);
    free(detector_data.sig_level_buff);
    free(detector_data.state);
    free(backbuf);
    free(votes);
}


void CCw::incWPM()
{
    rc_data.min_unit = 40;
    rc_data.max_unit = 6;
    if (rc_data.speed_wpm<40)
        rc_data.speed_wpm++;
    rc_data.unit_elem = (60 * rc_data.tone_freq) / (50 * CYCLES_PER_FRAG * rc_data.speed_wpm);
    rc_data.max_unit = (60 * rc_data.tone_freq) /
        (50 * CYCLES_PER_FRAG * rc_data.max_unit);
    rc_data.max_unit_x2 = rc_data.max_unit * 2;
    rc_data.min_unit = (60 * rc_data.tone_freq) /
        (50 * CYCLES_PER_FRAG * rc_data.min_unit);
}


void CCw::decWPM()
{
    rc_data.min_unit = 40;
    rc_data.max_unit = 6;
    if (rc_data.speed_wpm>6)
        rc_data.speed_wpm--;
    rc_data.unit_elem = (60 * rc_data.tone_freq) / (50 * CYCLES_PER_FRAG * rc_data.speed_wpm);
    rc_data.max_unit = (60 * rc_data.tone_freq) /
        (50 * CYCLES_PER_FRAG * rc_data.max_unit);
    rc_data.max_unit_x2 = rc_data.max_unit * 2;
    rc_data.min_unit = (60 * rc_data.tone_freq) /
        (50 * CYCLES_PER_FRAG * rc_data.min_unit);
}


int CCw::getWPM()
{
    return rc_data.speed_wpm;
}


void CCw::change_freq(int freq)
{
    int idx;

    rc_data.min_unit = 60;
    rc_data.max_unit = 6;

    rc_data.tone_freq = freq;
    rc_data.unit_elem = (60 * rc_data.tone_freq) / (50 * CYCLES_PER_FRAG * rc_data.speed_wpm);
    Flags |= ADAPT_SPEED;
    rc_data.max_unit = (60 * rc_data.tone_freq) / (50 * CYCLES_PER_FRAG * rc_data.max_unit);
    rc_data.max_unit_x2 = rc_data.max_unit * 2;
    rc_data.min_unit = (60 * rc_data.tone_freq) / (50 * CYCLES_PER_FRAG * rc_data.min_unit);

    double w = 2.0 * M_PI * (double)rc_data.tone_freq / (double)rc_data.dsp_rate;
    detector_data.cosw  = cos(w);
    detector_data.sinw  = sin(w);
    detector_data.coeff = 2.0 * detector_data.cosw;
    detector_data.frag_len = (rc_data.dsp_rate * CYCLES_PER_FRAG) / rc_data.tone_freq;
    detector_data.samples_buff_len = (CYCLES_PER_FRAG * rc_data.max_unit * rc_data.dsp_rate) / rc_data.tone_freq;



    detector_data.samples_buff_idx = 0;
    detector_data.sig_level_idx = 0;
    detector_data.state_idx = 0;

    if (detector_data.samples_buff)
        for( idx = 0; idx < detector_data.samples_buff_len; idx++ )
            detector_data.samples_buff[idx] = 0;
    if (detector_data.sig_level_buff)
        for( idx = 0; idx < rc_data.max_unit_x2; idx++ )
            detector_data.sig_level_buff[idx] = 0;
    if (detector_data.state)
        for( idx = 0; idx < rc_data.max_unit; idx++ )
            detector_data.state[idx] = 0;

    space_elem_cnt = 0;
    space_frag_cnt = 0;
    mark_elem_cnt  = 0;
    mark_frag_cnt  = 0;
    Flags = 0;
    Flags |= ADAPT_SPEED;
    mark_cnt  = 0;
    space_cnt = 0;
    hex_code  = ENTER_DOT;
    state = 0;
    context = NO_CONTEXT;
}

void CCw::change_wpm(int wpm)
{
    rc_data.speed_wpm = wpm;
    rc_data.min_unit = 40;
    rc_data.max_unit = 6;
    rc_data.unit_elem = (60 * rc_data.tone_freq) / (50 * CYCLES_PER_FRAG * rc_data.speed_wpm);
    rc_data.max_unit = (60 * rc_data.tone_freq) / (50 * CYCLES_PER_FRAG * rc_data.max_unit);
    rc_data.max_unit_x2 = rc_data.max_unit * 2;
    rc_data.min_unit = (60 * rc_data.tone_freq) / (50 * CYCLES_PER_FRAG * rc_data.min_unit);
}




/*! \brief Reset the decoder. */
void CCw::reset()
{
    size_t alloc;
    int idx;

    space_elem_cnt = 0;
    space_frag_cnt = 0;
    mark_elem_cnt  = 0;
    context   = NO_CONTEXT;
    mark_frag_cnt  = 0;
    mark_cnt  = 0;
    space_cnt = 0;
    hex_code  = ENTER_DOT;
    Flags = 0;
    period = 15;

    sigfft = new gr::fft::fft_real_fwd(12000);

    bestfreq = 700;
    lastsamples = 0;
    rc_data.dsp_rate = 24000;
    rc_data.det_squelch = 45;//50;
    rc_data.det_ratio = 1.7;//2.0;

    backbuf = (float*)malloc(24000 * sizeof(float)*4);
    memset(backbuf,0,sizeof(float)*24000*4);
    backpos = 0;

    history = (float*)malloc(24000 * sizeof(float)*2);
    memset(history,0,sizeof(float)*24000*2);
    fftsize = 0;


    votes = (float*)malloc(6000 * sizeof(float));
    memset(votes,0,sizeof(float)*6000);
    voted = 0;

    rc_data.tone_freq = bestfreq;

    detector_data.samples_buff = NULL;
    detector_data.sig_level_buff = NULL;
    detector_data.state = NULL;
    change_wpm(15);
    change_freq(bestfreq);


    alloc = (size_t)detector_data.samples_buff_len * sizeof(short) * 24000 * 100;
    detector_data.samples_buff = (short int*)malloc(alloc);
    for( idx = 0; idx < (int)(alloc/(sizeof(short))); idx++ )
        detector_data.samples_buff[idx] = 0;

    detector_data.sig_level_buff = NULL;
    alloc = (size_t)rc_data.max_unit_x2 * sizeof(int) * 24000 * 100;
    detector_data.sig_level_buff = (int*)malloc(alloc);
    for( idx = 0; idx < (int)(alloc/(sizeof(int))); idx++ )
        detector_data.sig_level_buff[idx] = 0;

    detector_data.state = NULL;
    alloc = (size_t)rc_data.max_unit * 24000 * 100;
    detector_data.state = (unsigned char*)malloc(alloc);
    for( idx = 0; idx < (int)alloc; idx++ )
        detector_data.state[idx] = 0;
    // yes, again
    change_freq(bestfreq);


}




void CCw::demod(float *buffer, int length)
{
    char sym;
    QString letter;
    float magn, best;
    int bestpos;
    int pos;
    int a,idx;
    float *in = sigfft->get_inbuf();


    if (length>0)
    {
        memcpy(&backbuf[backpos],buffer,(length)*sizeof(float));
        backpos += length;
    }

    if (backpos<(int)detector_data.frag_len)
        return;

    for (a=0;a<backpos;a+=detector_data.frag_len)
    {
        if (Get_Character(&sym, &backbuf[a]))
        {
            verbprintf(3,"DECODED - %c\n",sym);
            letter.append(sym);
            emit prevMessage(QTextCursor::End,QTextCursor::MoveAnchor);
            emit newMessage(letter);
            emit prevMessage(QTextCursor::End,QTextCursor::MoveAnchor);
        }
    }
    a-=detector_data.frag_len;
    memmove(backbuf,&backbuf[a],(backpos-a)*sizeof(float));
    backpos -= a;



    memcpy(&history[fftsize],buffer,length);
    fftsize+=length;

    if (fftsize>12000)
    {
        magn = pos = bestpos = best = (float)0;
        memcpy(in, history, sizeof(float)*std::min(12000,fftsize));
        sigfft->execute();
        memset(history,0,12000*sizeof(float));
        gr_complex *out = sigfft->get_outbuf();
        best = 0;
        for (int k = 0; k < 6000; k++)
        {
            magn = abs(out[k]);
            if ((!isnan(magn)) && (isfinite(magn)) && (magn<100000) && (magn > -100000) && (magn!=0.0))
            {
                votes[k]+=magn;
                if (magn>best)
                {
                    best = magn;
                    bestpos = k;
                }
            }
        }
        verbprintf(10,"FreqFFT: bestpos=%d best=%f freq=%d fft=%d len=%d\n",bestpos,best,(bestpos*2), std::min(12000,fftsize),fftsize);
        fftsize = 0;
        voted++;
    }


    if (voted==period)
    {
        int mb = 0;
        int mp = 0;
        for (int k=0;k<6000;k++)
        if (votes[k]>mb)
        {
                mp = k;
                mb = votes[k];
        }
        voted = 0;
        period = 50;
        printf("NEW MB=%d MP=%d tone_freq=%d\n",mb,(mp*2),rc_data.tone_freq);
        if ((mb>1)&&(((mp*2))!=rc_data.tone_freq)&&(mp!=0)&&(mp<400))
        {
            verbprintf(5,"New best freq (votes=%d) = %d\n",mb,(mp*2));
            change_freq((mp*2));
        }
        memset(votes,0,sizeof(float)*6000);
    }

    lastsamples++;




}


bool CCw::Get_Fragment(float *in)
{
    int block_size;
    int frag_timer;
    int frag_level;
    int lead_edge;
    int trail_edge;
    int level_sum;
    int ref_ratio;
    int state_cnt;
    int idx, ids;
    double q0, q1, q2, level;
    float tmp;

    block_size = detector_data.frag_len;

    for( frag_timer = 0; frag_timer < detector_data.frag_len; frag_timer++ )
    {
        tmp = in[frag_timer];

        detector_data.samples_buff[detector_data.samples_buff_idx] = (short int)((tmp*32768.0*0.99));

        detector_data.samples_buff_idx++;
        if( detector_data.samples_buff_idx >= detector_data.samples_buff_len )
        {
            detector_data.samples_buff_idx = 0;
        }
    }

    detector_data.samples_buff_idx -= block_size;
    if( detector_data.samples_buff_idx < 0 )
        detector_data.samples_buff_idx += detector_data.samples_buff_len;

    q1 = q2 = 0.0;
    for( idx = 0; idx < block_size; idx++ )
    {
        q0 = detector_data.coeff * q1 - q2 + (double)detector_data.samples_buff[detector_data.samples_buff_idx];
        q2 = q1;
        q1 = q0;
        detector_data.samples_buff_idx++;
        if( detector_data.samples_buff_idx >= detector_data.samples_buff_len )
            detector_data.samples_buff_idx = 0;

    }

    q1 /= (double)block_size;
    q2 /= (double)block_size;
    level = q1*q1 + q2*q2 - q1*q2*detector_data.coeff;
    double sq = sqrt( level );
    frag_level = (int)sq;

    detector_data.sig_level_idx++;
    if( detector_data.sig_level_idx >= rc_data.max_unit_x2 )
        detector_data.sig_level_idx = 0;

    detector_data.sig_level_buff[detector_data.sig_level_idx] = frag_level;

    ids = detector_data.sig_level_idx;
    lead_edge = 0;
    for( idx = 0; idx < rc_data.unit_elem; idx++ )
    {
        lead_edge += detector_data.sig_level_buff[ids];
        ids--;
        if( ids < 0 ) ids += rc_data.max_unit_x2;
    }

    trail_edge = 0;
    for( idx = 0; idx < rc_data.unit_elem; idx++ )
    {
        trail_edge += detector_data.sig_level_buff[ids];
        ids--;
        if( ids < 0 ) ids += rc_data.max_unit_x2;
    }

    lead_edge /= rc_data.unit_elem;
    trail_edge /= rc_data.unit_elem;

    level_sum = lead_edge + trail_edge;
    ref_ratio = (int)( rc_data.det_ratio * RATIO_MULTIPLIER );

    detector_data.state_idx++;
    if( detector_data.state_idx >= rc_data.max_unit )
        detector_data.state_idx = 0;

    if( level_sum > rc_data.det_squelch * SQUELCH_MULTIPLIER )
    {
        if( ((int)RATIO_MULTIPLIER) * lead_edge > ref_ratio * trail_edge ) {
            state = 20;
        }
        else if( ((int)RATIO_MULTIPLIER) * trail_edge > ref_ratio * lead_edge ) {
            state = 0;
        }
    }

    detector_data.state[detector_data.state_idx] = state;

    ids = detector_data.state_idx;
    state_cnt = 0;
    for( idx = 0; idx < rc_data.unit_elem; idx++ )
    {
        state_cnt += detector_data.state[ids];
        ids--;
        if( ids < 0 ) ids += rc_data.max_unit;
    }

    if( (int)state_cnt > 11 * rc_data.unit_elem )
    {
        Flags |= MARK_TONE;
        Flags &= ~SPACE_TONE;
    }
    else if( (int)state_cnt < 9 * rc_data.unit_elem )
    {
        Flags &= ~MARK_TONE;
        Flags |= SPACE_TONE;
    }

    /*
    int det_ratio = 1;
    if( (lead_edge > trail_edge) && trail_edge )
        det_ratio = ((int)RATIO_MULTIPLIER * lead_edge) / trail_edge;
    else if( lead_edge )
        det_ratio = ((int)RATIO_MULTIPLIER * trail_edge) / lead_edge;
    */

    return( true );
}



bool CCw::Get_Character(char *chr, float *sample )
{
    int unit_d2  = rc_data.unit_elem / 2;
    int unit_x2  = rc_data.unit_elem * 2;
    int unit_x5  = rc_data.unit_elem * 5;
    int unit_x7  = rc_data.unit_elem * 7;
    int unit_x8  = rc_data.unit_elem * 8;
    int unit_x16 = rc_data.unit_elem * 16;

    if( !Get_Fragment(sample) ) return( false );

    if ((Flags & MARK_TONE) == MARK_TONE)
        mark_cnt++;
    else
        space_cnt++;

    if( mark_cnt > unit_x8 )
        mark_cnt = unit_x8;

    if( space_cnt > unit_x16 )
        space_cnt = unit_x16;

    switch( context )
    {
        case MARK_SIGNAL:
            if( mark_cnt >= unit_x8 )
            {
                space_cnt = 0;
                hex_code = ENTER_DOT;
                context = WAIT_FOR_SPACE;
            }
            else if ((Flags & SPACE_TONE) == SPACE_TONE)
            {
                space_cnt = 1;
                context = ELEM_SPACE;
                verbprintf(10,"SPACE\n");
            }
            break;

        case ELEM_SPACE:
            if( space_cnt >= unit_d2 || ((Flags & MARK_TONE) == MARK_TONE) )
            {
                if( mark_cnt < unit_x2 )
                {
                    hex_code = (hex_code << 1) | ENTER_DOT;
                    mark_frag_cnt += mark_cnt;
                    mark_elem_cnt += 1;
                }
                else
                {
                    hex_code <<= 1;
                    mark_frag_cnt += mark_cnt;
                    mark_elem_cnt += 3;
                }

                if( ((Flags & SPACE_TONE) == SPACE_TONE) )
                {
                    mark_cnt = 0;
                    context = CHAR_SPACE;
                }
                else
                {
                    space_cnt = 0;
                    mark_cnt  = 1;
                    context = MARK_SIGNAL;
                    verbprintf(10,"MARK\n");
                }
            }
            break;

        case CHAR_SPACE:
            if( ((Flags & SPACE_TONE) == SPACE_TONE) )
            {
                if( space_cnt >= unit_x2 )
                {
                    context = WAIT_WORD_SPACE;
                    *chr = Hex_to_Ascii( &hex_code );
                    return( true );
                }
            }
            else
            {
                space_frag_cnt += space_cnt;
                space_elem_cnt++;
                space_cnt = 0;
                mark_cnt  = 1;
                context = MARK_SIGNAL;
                verbprintf(10,"MARK\n");
            }
            break;

        case WAIT_WORD_SPACE:
            if( ((Flags & SPACE_TONE) == SPACE_TONE) )
            {
                if( space_cnt >= unit_x5 )
                    context = WORD_SPACE;
            }
            else
            {
                Adapt_Decoder();
                space_cnt = 0;
                mark_cnt  = 1;
                context = MARK_SIGNAL;
                verbprintf(10,"MARK\n");
            }
            break;

        case WORD_SPACE:
            if( ((Flags & SPACE_TONE) == SPACE_TONE) )
            {
                if( space_cnt >= unit_x7 )
                {
                  *chr = ' ';
                  context = WAIT_FOR_MARK;
                  return( true );
                }
            }
            else
            {
                Adapt_Decoder();
                space_cnt = 0;
                mark_cnt  = 1;
                *chr = ' ';
                context = MARK_SIGNAL;
                verbprintf(10,"MARK\n");
                return( true );
            }
            break;

        case WAIT_FOR_MARK:
            if( ((Flags & MARK_TONE) == MARK_TONE) )
            {
                space_cnt = 0;
                context = MARK_SIGNAL;
                verbprintf(10,"MARK\n");
            }
            break;

        case WAIT_FOR_SPACE:
            if( ((Flags & SPACE_TONE) == SPACE_TONE) )
            {
                space_cnt = 1;
                mark_cnt  = 0;
                context = WAIT_FOR_MARK;
            }
            break;

        default:
            if( ((Flags & MARK_TONE) == MARK_TONE) )
                context = MARK_SIGNAL;
            else
                context = WAIT_FOR_MARK;
    }
    return( false );
}





void CCw::verbprintf(int verb_level, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    if (verb_level <= verbose_level) {
        vfprintf(stdout, fmt, args);
        fflush(stdout);
    }
    va_end(args);
}


char CCw::Hex_to_Ascii( int *hex_code )
{
    static char morse_ascii_char[ NUMBER_OF_CHAR + 1 ] = MORSE_ASCII_CHAR;
    static unsigned char morse_hex_char[ NUMBER_OF_CHAR ] = MORSE_HEX_CODE;
    int idx;

    for( idx = 0; idx < NUMBER_OF_CHAR; idx++ )
        if( *hex_code ==  morse_hex_char[ idx ] )
            break;
    *hex_code = ENTER_DOT;
    return( morse_ascii_char[ idx ] );
}


void CCw::Adapt_Decoder( void )
{
    if( mark_elem_cnt && mark_frag_cnt && space_elem_cnt && space_frag_cnt )
    {
        if( ((Flags & ADAPT_SPEED) == ADAPT_SPEED) )
        {
            int speed_err = (mark_frag_cnt + space_frag_cnt) /
                                  (mark_elem_cnt + space_elem_cnt)
                                  - rc_data.unit_elem;
            int speed1 = ( 60 * rc_data.tone_freq ) / ( 50 * CYCLES_PER_FRAG * rc_data.unit_elem );
            verbprintf(5,"PREV SPEED: %d\n", speed1);

            if( (rc_data.unit_elem > rc_data.min_unit) && (speed_err < 0) )
                rc_data.unit_elem--;
            if( (rc_data.unit_elem < rc_data.max_unit) && (speed_err > 0) )
                rc_data.unit_elem++;

            int speed = ( 60 * rc_data.tone_freq ) / ( 50 * CYCLES_PER_FRAG * rc_data.unit_elem );

            if (speed1!=speed)
            {
                verbprintf(5,"NEW SPEED: %d\n", speed);
                QString str = "WPM: "+QString::number(speed);
                emit updateWPM(str);
                change_wpm(speed);
            }
        }
    }
    space_elem_cnt = space_frag_cnt = 0;
    mark_elem_cnt  = mark_frag_cnt  = 0;
}
