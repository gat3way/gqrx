/*
 *      ccw.h -- ALE demodulator class
 *
 *
 *      Copyright (C) 2017 Milen Rangelov
 *      Significant portions of the code are used from xdemorse project 
 *      Which is Copyright (C) Neoklis Kyriazis (nkcyham at yahoo.com)
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
#ifndef CW_H
#define CW_H

#include <QObject>
#include <QTextCursor>
#include <math.h>
#include <gnuradio/fft/fft.h>

typedef struct
{
  int  dsp_rate;
  int   tone_freq,
        speed_wpm,
        unit_elem,
        min_unit,
        max_unit,
        max_unit_x2,
        det_squelch,
        det_threshold;

  double
        det_ratio;

} rc_data_t;




class CCw : public QObject
{
    Q_OBJECT
public:
    explicit CCw(QObject *parent = 0);
    ~CCw();

    void demod(float *buffer, int length);
    void reset();
    void incWPM();
    void decWPM();
    void change_freq(int freq);
    void change_wpm(int wpm);
    int getWPM();


signals:
    void newMessage(const QString &message);
    void prevMessage(const QTextCursor::MoveOperation &position, QTextCursor::MoveMode mode);
    void updateWPM(const QString &message);

public slots:


private:
    void verbprintf(int verb_level, const char *fmt, ...);
    bool Get_Fragment(float *sample);
    bool Get_Character(char *ch, float *sample);
    char Hex_to_Ascii(int *hex);
    void Adapt_Decoder(void);
    gr::fft::fft_real_fwd *sigfft;
    int bestfreq;
    int lastsamples;
    float *backbuf;
    int backpos;
    float *votes;
    int voted;
    struct tdetector_data
    {
        int frag_len,
            samples_buff_len,
            samples_buff_idx;
        short *samples_buff;
        double cosw, sinw, coeff;
        int *sig_level_buff;
        int sig_level_idx;
        unsigned char *state;
        int state_idx;
    } detector_data;
    int space_elem_cnt;
    int space_frag_cnt;
    int mark_elem_cnt;
    int context;
    int mark_frag_cnt;
    int mark_cnt;
    int space_cnt;
    int hex_code;
    unsigned char state;
    int Flags;
    rc_data_t rc_data;
    int period;
    float *history;
    int fftsize;
    float poweravg;
};

#endif // CLE_H
