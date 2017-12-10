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
#include "ism433win.h"
#include "ui_ism433win.h"


Ism433Win::Ism433Win(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::Ism433Win)
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

    /* ISM433 decoder */
    decoder = new CIsm433(this);

    connect(decoder, SIGNAL(newMessage(QString)), ui->textView, SLOT(appendPlainText(QString)));
}

Ism433Win::~Ism433Win()
{
    qDebug() << "Ism433 decoder destroyed.";

    delete decoder;
    delete ui;
}


/*! \brief Process new set of samples. */
void Ism433Win::process_samples(float *buffer, int length)
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
void Ism433Win::closeEvent(QCloseEvent *ev)
{
    Q_UNUSED(ev);

    emit windowClosed();
}


/*! \brief User clicked on the Clear button. */
void Ism433Win::on_actionClear_triggered()
{
    ui->textView->clear();
}


/*! \brief User clicked on the Save button. */
void Ism433Win::on_actionSave_triggered()
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
void Ism433Win::on_actionInfo_triggered()
{
    QMessageBox::about(this, tr("About ISM433 Decoder"),
                       tr("<p>Gqrx ISM433 PWM/OOK Decoder %1</p>"
                          "<p>The Gqrx ISM433 decoder taps directly into the SDR signal path "
                          "eliminating the need to mess with virtual or real audio cables. "
                          "Please us AM demodulation. "
                          "It can decode ISM433 and displays the decoded characters in a text view.</p>"
                          ).arg(VERSION));

}


void Ism433Win::on_actionLeft_triggered()
{
    decoder->decLevel();
    int level = decoder->getLevel();
    if (level==0)
    {
        QString str = "Threshold Level: 0 (Auto)";
        ui->levelLabel->setText(str);
    }
    else
    {
        QString str = "Threshold Level: " + QString::number(level);
        ui->levelLabel->setText(str);
    }
}

void Ism433Win::on_actionRight_triggered()
{
    decoder->incLevel();
    int level = decoder->getLevel();
    if (level==0)
    {
        QString str = "Threshold Level: 0 (Auto)";
        ui->levelLabel->setText(str);
    }
    else
    {
        QString str = "Threshold Level: " + QString::number(level);
        ui->levelLabel->setText(str);
    }
}

void Ism433Win::none()
{}
