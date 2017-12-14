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
#ifndef DSDWIN_H
#define DSDWIN_H

#include <QMainWindow>
#include <QObject>
#include <QVarLengthArray>
#include "dsp/dsd_block_ff.h"

namespace Ui {
    class DsdWin;
}



/*! \brief DSD decoder window. */
class DsdWin : public QMainWindow
{
    Q_OBJECT

public:
    explicit DsdWin(QWidget *parent = 0);
    ~DsdWin();

private slots:
    void sendDecoder(QString msg);


protected:
    void closeEvent(QCloseEvent *ev);

signals:
    void windowClosed();  /*! Signal we emit when window is closed. */

private slots:
    void on_actionClear_triggered();
    void on_actionSave_triggered();
    void on_actionInfo_triggered();
private:
    Ui::DsdWin *ui;  /*! Qt Designer form. */
};

#endif // DSDWIN_H
