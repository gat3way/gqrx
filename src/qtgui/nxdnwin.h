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
#ifndef NXDNWIN_H
#define NXDNWIN_H

#include <QMainWindow>
#include <QVarLengthArray>
#include <QMap>
#include "qtgui/trunkchannels.h"


namespace Ui {
    class NxdnWin;
}


/*! \brief NXDN trunk decoder window. */
class NxdnWin : public QMainWindow
{
    Q_OBJECT

public:
    explicit NxdnWin(QWidget *parent = 0);
    ~NxdnWin();
    void process_samples(float *buffer, int length);
    void reset(qint64 freq);

protected:
    void closeEvent(QCloseEvent *ev);

signals:
    void windowClosed();  /*! Signal we emit when window is closed. */
    void changeFreq(qint64 freq);

private slots:
    void sendNxdnTrunkid(int id);
    void sendNxdnTrunkChan(int chan,int steps, int base);
    void sendNxdnTrunkReg(int regunit,int regdst);
    void sendNxdnTrunkCall(int src,int dst, int chan);
    void sendNxdnNodata(int state);
    void on_actionClear_triggered();
    void on_actionSave_triggered();
    void on_actionInfo_triggered();
    void on_actionTable_triggered();
    void on_actionTrunk_triggered();
    void trunktable_closed();
    void channelsEnforce(QMap<int,channel_t> channels);

private:
    Ui::NxdnWin *ui;  /*! Qt Designer form. */
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
};

#endif // NXDNWIN_H
