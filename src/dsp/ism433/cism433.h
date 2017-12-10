/*
 *      cism433.h -- ISM433 decoder class
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
#ifndef CISM433_H
#define CISM433_H

#include <QObject>
#include <QTextCursor>
#include <math.h>
#include <stdint.h>


// C APIs
extern "C" {
    #include "includes/data.h"
    #include "includes/rtl_433_devices.h"
    #include "includes/rtl_433.h"
    #include "includes/baseband.h"
    #include "includes/pulse_demod.h"

    void data_acquired_handler(data_t *data);
}








struct dm_state {
    int32_t level_limit;
    int16_t am_buf[MAXIMAL_BUF_LENGTH];
    union {
        // These buffers aren't used at the same time, so let's use a union to save some memory
        int16_t fm_buf[MAXIMAL_BUF_LENGTH];  // FM demodulated signal (for FSK decoding)
        uint16_t temp_buf[MAXIMAL_BUF_LENGTH];  // Temporary buffer (to be optimized out..)
    };
    FilterState lowpass_filter_state;
    int analyze;
    int analyze_pulses;
    int debug_mode;
    int hop_time;
    int signal_grabber;
    int8_t* sg_buf;
    int sg_index;
    int sg_len;
    int enable_FM_demod;

    uint16_t r_dev_num;
    struct protocol_state *r_devs[MAX_PROTOCOLS];

    pulse_data_t    pulse_data;
    pulse_data_t    fsk_pulse_data;
};



class CIsm433 : public QObject
{
    Q_OBJECT
public:
    explicit CIsm433(QObject *parent = 0);
    ~CIsm433();

    void demod(float *buffer, int length);
    void reset();
    void incLevel();
    void decLevel();
    void change_level(int freq);
    int getLevel();
    void newmsg(char *str);


signals:
    void newMessage(const QString &message);
    void prevMessage(const QTextCursor::MoveOperation &position, QTextCursor::MoveMode mode);
    void updateLevel(const QString &message);

public slots:


private:
    void verbprintf(int verb_level, const char *fmt, ...);
    void register_protocol(struct dm_state *demodst, r_device *t_dev);
    void classify_signal();
    void pwm_analyze(struct dm_state *demod, int16_t *buf, uint32_t len);

    int level;
    float gain;
    unsigned int counter;
    unsigned int print;
    unsigned int print2;
    unsigned int pulses_found;
    unsigned int prev_pulse_start;
    unsigned int pulse_start;
    unsigned int pulse_end;
    unsigned int pulse_avg;
    unsigned int signal_start;
    unsigned int signal_end;
    unsigned int signal_pulse_data[4000][3];
    unsigned int signal_pulse_counter;
    int register_all = 1;
    //struct dm_state* demodstate;
    int duration;
    int flag;
    uint32_t samp_rate;
    int debug_output;
    int quiet_mode;
    struct dm_state *demodst;
    int overwrite_mode;
    int override_short;
    int override_long;
    uint16_t num_r_devices;
    int bufsize;
    float *sbuf;
    struct protocol_state zerostate;
};


#endif // CISM433_H
