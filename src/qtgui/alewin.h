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
#ifndef ALEWIN_H
#define ALEWIN_H

#include <QMainWindow>
#include <QVarLengthArray>
#include "dsp/ale/cale.h"


namespace Ui {
    class AleWin;
}


/*! \brief ALE decoder window. */
class AleWin : public QMainWindow
{
    Q_OBJECT

public:
    explicit AleWin(QWidget *parent = 0);
    ~AleWin();
    void process_samples(float *buffer, int length);

protected:
    void closeEvent(QCloseEvent *ev);

signals:
    void windowClosed();  /*! Signal we emit when window is closed. */

private slots:
    void on_actionClear_triggered();
    void on_actionSave_triggered();
    void on_actionInfo_triggered();

private:
    Ui::AleWin *ui;  /*! Qt Designer form. */

    CAle *decoder;     /*! The ALE decoder object. */

    QVarLengthArray<float, 16384> tmpbuf;   /*! Needed to remember "overlap" smples. */
};

#endif // ALEWIN_H
