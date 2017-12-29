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
#include "nxdn48win.h"
#include "ui_nxdn48win.h"
#include "../dsp/dsd_block_ff.h"


Nxdn48Win::Nxdn48Win(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::Nxdn48Win)
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
    CdsdProxy *inst = CdsdProxy::getInstance();
    connect(inst, SIGNAL(sendNxdnTrunkid(int)), this, SLOT(sendNxdnTrunkid(int)));
    connect(inst, SIGNAL(sendNxdnTrunkChan(int,int,int)), this, SLOT(sendNxdnTrunkChan(int,int,int)));
    connect(inst, SIGNAL(sendNxdnTrunkReg(int,int)), this, SLOT(sendNxdnTrunkReg(int,int)));
    connect(inst, SIGNAL(sendNxdnTrunkCall(int,int,int)), this, SLOT(sendNxdnTrunkCall(int,int,int)));
    connect(inst, SIGNAL(sendNxdnNodata(int)), this, SLOT(sendNxdnNodata(int)));
    trunkbase = trunksteps = trunkchan = trunkid = trunkready = -1;
    trunkcallsrc = trunkcalldst = trunkcallchan = -1;
    trunkregid = trunkreggroup = trunkstandard = -1;
    trunking = false;
    trunktraffic = false;
    controlfreq = 0;
}

Nxdn48Win::~Nxdn48Win()
{
    qDebug() << "NXDN48 decoder destroyed.";

    delete ui;
}


void Nxdn48Win::reset(qint64 freq)
{
    QString str;
    char msg[255];

    zeroframes = 0;
    trunkbase = trunksteps = trunkchan = trunkid = trunkready = -1;
    trunkcallsrc = trunkcalldst = trunkcallchan = -1;
    trunkregid = trunkreggroup = trunkstandard = -1;
    sprintf(msg,"Frequency reset: new frequency %lld Hz",freq);
    if (!trunking)
        controlfreq = freq;
    str.append(msg);
    ui->textView->appendPlainText(msg);
}




/*! \brief Catch window close events and emit signal so that main application can destroy us. */
void Nxdn48Win::closeEvent(QCloseEvent *ev)
{
    Q_UNUSED(ev);

    emit windowClosed();
}


/*! \brief User clicked on the Clear button. */
void Nxdn48Win::on_actionClear_triggered()
{
    ui->textView->clear();
}


void Nxdn48Win::on_actionTable_triggered()
{
    trunktable = new TrunkChannels(this, channelmap);
    connect(trunktable, SIGNAL(channelsEnforce(QMap<int,channel_t>)), this, SLOT(channelsEnforce(QMap<int,channel_t>)));
    connect(trunktable, SIGNAL(windowClosed()), this, SLOT(trunktable_closed()));
    trunktable->show();
}


void Nxdn48Win::on_actionTrunk_triggered()
{
    QString str;
    char msg[255];

    if (channelmap.size() > 0)
    {
        if (!trunking)
        {
            sprintf(msg,"Started following trunk calls");
            trunking = true;
        }
        else
        {
            sprintf(msg,"Stopped following trunk calls");
            trunking = false;
        }
    }
    else
    {
        sprintf(msg,"No band plan defined");
    }

    str.append(msg);
    ui->textView->appendPlainText(msg);
}



/*! \brief User clicked on the Clear button. */
void Nxdn48Win::trunktable_closed()
{
    delete trunktable;
    trunktable = 0;
}



/*! \brief User clicked on the Save button. */
void Nxdn48Win::on_actionSave_triggered()
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
void Nxdn48Win::on_actionInfo_triggered()
{
    QMessageBox::about(this, tr("About NXDN48 Trunk Decoder"),
                       tr("<p>Gqrx NXDN48 Trunk Decoder %1</p>"
                          "<p>The Gqrx NXDN48 decoder taps directly into the SDR signal path "
                          "eliminating the need to mess with virtual or real audio cables. "
                          "It can manage NXDN48 Type-C trunking. Please use DSD demod with NXDN48 frame type.</p>"
                          ).arg(VERSION));

}



void Nxdn48Win::channelsEnforce(QMap<int,channel_t> map)
{
    QString str;
    char msg[255];
    channelmap = map;
    sprintf(msg,"New band plan enforced (%d channels)",map.size());
    str.append(msg);
    ui->textView->appendPlainText(msg);
    // test
    //emit changeFreq(400000000);
}


void Nxdn48Win::trunkReady()
{
    QString str;
    char msg[255];

    if ((trunkready<0)&&(trunkready<2))
    {
        trunkready = 1;
        trunkstandard = 1;
        if (trunkbase == 0)
        {
            sprintf(msg,"NXDN48 Trunk Control Channel detected (ID: %d)\nNon-standard band plan used, please define a band plan",trunkid);
            trunkstandard = 0;
        }
        else
        {
            if (trunksteps==2)
            {
                if (trunkbase == 1)
                    sprintf(msg,"NXDN48 Trunk Control Channel detected (ID: %d, base: 100MHz, steps: 1.25 KHz)",trunkid);
                else if (trunkbase == 2)
                    sprintf(msg,"NXDN48 Trunk Control Channel detected (ID: %d, base: 330MHz, steps: 1.25 KHz)",trunkid);
                else if (trunkbase == 3)
                    sprintf(msg,"NXDN48 Trunk Control Channel detected (ID: %d, base: 400MHz, steps: 1.25 KHz)",trunkid);
                else if (trunkbase == 4)
                    sprintf(msg,"NXDN48 Trunk Control Channel detected (ID: %d, base: 750MHz, steps: 1.25 KHz)",trunkid);
                else if (channelmap.size()==0)
                {
                    sprintf(msg,"NXDN48 Trunk Control Channel detected (ID: %d)\nNon-standard band plan used, please define a band plan",trunkid);
                    trunkstandard = 0;
                }
            }
            else if (trunksteps==3)
            {
                if (trunkbase == 1)
                    sprintf(msg,"NXDN48 Trunk Control Channel detected (ID: %d, base: 100MHz, steps: 3.125 KHz)",trunkid);
                else if (trunkbase == 2)
                    sprintf(msg,"NXDN48 Trunk Control Channel detected (ID: %d, base: 330MHz, steps: 3.125 KHz)",trunkid);
                else if (trunkbase == 3)
                    sprintf(msg,"NXDN48 Trunk Control Channel detected (ID: %d, base: 400MHz, steps: 3.125 KHz)",trunkid);
                else if (trunkbase == 4)
                    sprintf(msg,"NXDN48 Trunk Control Channel detected (ID: %d, base: 750MHz, steps: 3.125 KHz)",trunkid);
                else if (channelmap.size()==0)
                    sprintf(msg,"NXDN48 Trunk Control Channel detected (ID: %d)\nNon-standard band plan used, please define a band plan",trunkid);
            }
            else
            {
                sprintf(msg,"NXDN48 Trunk Control Channel detected (ID: %d)\nNon-standard band plan used, please define a band plan",trunkid);
                trunkstandard = 0;
            }
        }
        str.append(msg);
        ui->textView->appendPlainText(msg);
    }
}



void Nxdn48Win::sendNxdnTrunkid(int id)
{
    trunkid = id;
    if ((trunkid>=0) && (trunksteps>=0) && (trunkchan>=0))
        trunkReady();
}


void Nxdn48Win::sendNxdnTrunkChan(int chan,int steps, int base)
{
    trunkchan = chan;
    trunksteps = steps;
    trunkbase = base;
    if ((trunkid>=0) && (trunksteps>=0) && (trunkchan>=0))
        trunkReady();
}

void Nxdn48Win::sendNxdnTrunkReg(int regunit,int regdst)
{
    if ((trunkregid!=regunit) && (trunkreggroup!=regdst) && (trunkready==1))
    {
        QString str;
        char msg[255];
        sprintf(msg,"Trunk registration (unit: %d, group: %d)",regunit,regdst);
        str.append(msg);
        ui->textView->appendPlainText(msg);
        trunkregid = regunit;
        trunkreggroup = regdst;
    }
}

void Nxdn48Win::sendNxdnTrunkCall(int src,int dst, int chan)
{
    int key;

    if (((trunkcallsrc!=src)||(trunkcalldst!=dst)||(trunkcallchan!=chan)) && (trunkready==1))
    {
        trunkcallsrc = src;
        trunkcalldst = dst;
        trunkcallchan = chan;
        QString str;
        char msg[255];
        sprintf(msg,"Voice Call Assignment (unit: %d, group: %d, channel: %d)",src,dst,chan);
        str.append(msg);
        ui->textView->appendPlainText(msg);

        if (trunking)
        {
            key = chan;
            if (channelmap.value(key).frequency!=0)
            {
                sprintf(msg,"Channel %d (group %d) found in bandplan, switching to frequency %lld",chan,dst,channelmap.value(key).frequency);
                str.append(msg);
                ui->textView->appendPlainText(msg);
                trunktraffic = true;
                emit changeFreq(channelmap.value(key).frequency);
            }
            else
            {
                sprintf(msg,"Channel %d not found in bandplan",chan);
                str.append(msg);
                ui->textView->appendPlainText(msg);
            }
        }
    }
}

void Nxdn48Win::sendNxdnNodata(int state)
{
    QString str;
    char msg[255];

    if (state==0)
    {
        if (trunktraffic)
            zeroframes++;

        if (zeroframes==60)
        {
            zeroframes = 0;
            trunktraffic = false;
            sprintf(msg,"No more traffic data, going back to control channel");
            str.append(msg);
            ui->textView->appendPlainText(msg);
            emit changeFreq(controlfreq);
        }
    }
    else
        zeroframes = 0;
}
