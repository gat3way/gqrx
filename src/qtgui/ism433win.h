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
#ifndef ISM433WIN_H
#define ISM433WIN_H

#include <QMainWindow>
#include <QVarLengthArray>
#include "dsp/ism433/cism433.h"


namespace Ui {
    class Ism433Win;
}


/*! \brief CW decoder window. */
class Ism433Win : public QMainWindow
{
    Q_OBJECT

public:
    explicit Ism433Win(QWidget *parent = 0);
    ~Ism433Win();
    void process_samples(float *buffer, int length);

protected:
    void closeEvent(QCloseEvent *ev);

signals:
    void windowClosed();  /*! Signal we emit when window is closed. */

private slots:
    void on_actionClear_triggered();
    void on_actionSave_triggered();
    void on_actionInfo_triggered();
    void on_actionRight_triggered();
    void on_actionLeft_triggered();
    void none();
private:
    Ui::Ism433Win *ui;  /*! Qt Designer form. */

    CIsm433 *decoder;     /*! The CW decoder object. */

    QVarLengthArray<float, 16384> tmpbuf;   /*! Needed to remember "overlap" smples. */
};

#endif // ISM433WIN_H
