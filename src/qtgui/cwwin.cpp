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
#include <QMessageBox>
#include <QFileDialog>
#include <QFile>
#include <QDir>
#include <QDebug>
#include "cwwin.h"
#include "ui_cwwin.h"


CwWin::CwWin(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::CwWin)
{
    ui->setupUi(this);

    /* select font for text viewer */
#ifdef Q_OS_MAC
    ui->textView->setFont(QFont("Monaco", 12));
#else
    ui->textView->setFont(QFont("Monospace", 11));
#endif

    /* Add right-aligned info button */
    QWidget *spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->toolBar->addWidget(spacer);
    ui->toolBar->addAction(ui->actionInfo);

    /* CW decoder */
    decoder = new CCw(this);

    connect(decoder, SIGNAL(newMessage(QString)), ui->textView, SLOT(insertPlainText(QString)));
    //connect(decoder, SIGNAL(prevMessage(QTextCursor::MoveOperation,QTextCursor::MoveMode)), ui->textView, SLOT(moveCursor(QTextCursor::MoveOperation,QTextCursor::MoveMode)));
    connect(decoder, SIGNAL(updateWPM(QString)), ui->wpmLabel, SLOT(setText(QString)));
}

CwWin::~CwWin()
{
    qDebug() << "CW decoder destroyed.";

    delete decoder;
    delete ui;
}


/*! \brief Process new set of samples. */
void CwWin::process_samples(float *buffer, int length)
{
    int overlap = 18;
    int i;

    for (i = 0; i < length; i++) {
        tmpbuf.append(buffer[i]);
    }

    decoder->demod(tmpbuf.data(), length);

    /* clear tmpbuf and store "overlap" */
    tmpbuf.clear();
    for (i = length-overlap; i < length; i++) {
        tmpbuf.append(buffer[i]);
    }

}


/*! \brief Catch window close events and emit signal so that main application can destroy us. */
void CwWin::closeEvent(QCloseEvent *ev)
{
    Q_UNUSED(ev);

    emit windowClosed();
}


/*! \brief User clicked on the Clear button. */
void CwWin::on_actionClear_triggered()
{
    ui->textView->clear();
}


/*! \brief User clicked on the Save button. */
void CwWin::on_actionSave_triggered()
{
    /* empty text view has blockCount = 1 */
    if (ui->textView->blockCount() < 2) {
        QMessageBox::warning(this, tr("Gqrx error"), tr("Nothing to save."),
                             QMessageBox::Ok, QMessageBox::Ok);
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(this, tr("Save File"),
                                                    QDir::homePath(),
                                                    tr("Text Files (*.txt)"));

    if (fileName.isEmpty()) {
        qDebug() << "Save cancelled by user";
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "Error creating file: " << fileName;
        return;
    }

    QTextStream out(&file);
    out << ui->textView->toPlainText();
    file.close();
}


/*! \brief User clicked Info button. */
void CwWin::on_actionInfo_triggered()
{
    QMessageBox::about(this, tr("About CW Decoder"),
                       tr("<p>Gqrx CW Decoder %1</p>"
                          "<p>The Gqrx CW decoder taps directly into the SDR signal path "
                          "eliminating the need to mess with virtual or real audio cables. "
                          "It can decode CW (Morse Code)and displays the decoded characters in a text view.</p>"
                          ).arg(VERSION));

}


void CwWin::on_actionLeft_triggered()
{
    decoder->decWPM();
    int wpm = decoder->getWPM();
    QString str = "WPM: " + QString::number(wpm);
    ui->wpmLabel->setText(str);
}

void CwWin::on_actionRight_triggered()
{
    decoder->incWPM();
    int wpm = decoder->getWPM();
    QString str = "WPM: " + QString::number(wpm);
    ui->wpmLabel->setText(str);
}

void CwWin::none()
{}
