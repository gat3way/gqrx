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
#ifndef MPT1327WIN_H
#define MPT1327WIN_H

#include <QMainWindow>
#include <QVarLengthArray>
#include <QMap>
#include "qtgui/trunkchannels.h"
#include <stdint.h>

namespace Ui {
    class Mpt1327Win;
}


typedef struct {
    uint16_t codeword[4];
    uint16_t fcs;
    unsigned char cnt;
    unsigned char state;
} mpt1327_t;





/*! \brief MPT1327 trunk decoder window. */
class Mpt1327Win : public QMainWindow
{
    Q_OBJECT

public:
    explicit Mpt1327Win(QWidget *parent = 0);
    ~Mpt1327Win();
    void process_samples(float *buffer, int length);
    void reset(qint64 freq);
    void demod(float *buffer, int length);

protected:
    void closeEvent(QCloseEvent *ev);

signals:
    void windowClosed();  /*! Signal we emit when window is closed. */
    void changeFreq(qint64 freq);

private slots:
    void sendMpt1327Trunkid(int id);
    void sendMpt1327TrunkChan(int chan,int steps, int base);
    void sendMpt1327TrunkReg(int regunit,int regdst);
    void sendMpt1327TrunkCall(int src,int dst, int chan);
    void sendMpt1327Nodata(int state);
    void on_actionClear_triggered();
    void on_actionSave_triggered();
    void on_actionInfo_triggered();
    void on_actionTable_triggered();
    void on_actionTrunk_triggered();
    void trunktable_closed();
    void channelsEnforce(QMap<int,channel_t> channels);

private:
    int sym_recovery_1200(uint16_t corr, unsigned char nuf_ones, int samplespersym);
    void mpt1327(unsigned char bit, mpt1327_t *state);
    uint16_t mpt1327_fcs(mpt1327_t *m);
    int getbit(int n, uint16_t *codewords);
    Ui::Mpt1327Win *ui;  /*! Qt Designer form. */
    TrunkChannels *trunktable;
    void trunkReady();
    int trunkid;
    int trunksteps;
    int trunkbase;
    int trunkchan;
    int trunkready;
    int trunkcallsrc;
    int trunkcalldst;
    int trunkcallchan;
    int trunkregid;
    int trunkreggroup;
    int trunkstandard;
    QMap<int,channel_t> channelmap;
    bool trunking;
    bool trunktraffic;
    qint64 controlfreq;
    int zeroframes;
    // MPT1327 data
    int sysid;

    int samplespersym, i, nuf_read, curr_sample;
    double *i0, *q0, *i1, *q1;
    double *corr_i0, *corr_q0, *corr_i1, *corr_q1;
    double sample;
    unsigned char *buffer;
    double sum[4];
    uint16_t corr, ones_calc_mask;
    unsigned char nuf_ones, pll;
    double abs_sum;
    mpt1327_t mpt1327_state;
    QVarLengthArray<float, 21600> tmpbuf;   /*! Needed to remember "overlap" smples. */
};

#endif // MPT1327WIN_H
