/*
 *      cale.h -- ALE demodulator class
 *
 *      Copyright (C) 1996
 *          Thomas Sailer (sailer@ife.ee.ethz.ch, hb9jnx@hb9w.che.eu)
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
#ifndef CALE_H
#define CALE_H

#include <QObject>


typedef struct
{
    double real;
    double imag;
} Complex;


class CAle : public QObject
{
    Q_OBJECT
public:
    explicit CAle(QObject *parent = 0);
    ~CAle();

    void demod(float *buffer, int length);
    void reset();

signals:
    void newMessage(const QString &message);

public slots:

private:
    void verbprintf(int verb_level, const char *fmt, ...);
    unsigned long golay_encode(unsigned int data);
    unsigned int golay_decode(unsigned long code, int *errors);
    int decode_word (unsigned long word, int nr, int berw);
    unsigned long modem_de_interleave_and_fec(int *input, int *errors);
    void modem_new_symbol(int sym, int nr);
    void modem_init(void);
    void log(char *current, char *current2, int state, int ber);
    void do_modem(float *sample, int length);

    int left;

    double  fft_cs_twiddle[64];
    double  fft_ss_twiddle[64];
    double  fft_history[64];
    Complex fft_out[64];
    double  fft_mag[64];
    int     fft_history_offset;
    /*
     * sync information
     */
    double mag_sum[17][64];
    int    mag_history_offset;
    int    word_sync[17];

    // worker data
    int started[17]; /* if other than DATA has arrived */
    int bits[17][49*3];
    int input_buffer_pos[17];
    int word_sync_position[17];

    // protocol data
    char to[4];
    char from[4];
    char data[4];
    char rep[4];
    char tis[4];
    char tws[4];
    char current[64];
    char current2[64];
    int ber[17];
    int lastber;
    int bestpos;

    int inew;
    int ito;
    int ifrom;
    int idata;
    int irep;
    int itis;
    int itws;
    int state;
    int state_count;
    int stage;
    int last_symbol[17];
    int last_sync_position[17];
    int sample_count;

};

#endif // CLE_H
