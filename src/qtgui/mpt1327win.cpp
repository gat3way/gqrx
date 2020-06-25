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
#include "mpt1327win.h"
#include "ui_mpt1327win.h"
#include "../dsp/dsd_block_ff.h"


#define MPT1327_SYNC 0xc4d7
#define MPT1327_SYNT 0x3b28
#define MPT1327_POLYNOM 0x6815
#define BASEFREQ 229
#define STEP 0.0125
#define CC 229.087500
#define TARGET_PREFIX 0xffff
#define TARGET_IDENT 0xffff


Mpt1327Win::Mpt1327Win(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::Mpt1327Win)
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
    /*
    CdsdProxy *inst = CdsdProxy::getInstance();
    connect(inst, SIGNAL(sendMpt1327Trunkid(int)), this, SLOT(sendMpt1327Trunkid(int)));
    connect(inst, SIGNAL(sendMpt1327TrunkChan(int,int,int)), this, SLOT(sendMpt1327TrunkChan(int,int,int)));
    connect(inst, SIGNAL(sendMpt1327TrunkReg(int,int)), this, SLOT(sendMpt1327TrunkReg(int,int)));
    connect(inst, SIGNAL(sendMpt1327TrunkCall(int,int,int)), this, SLOT(sendMpt1327TrunkCall(int,int,int)));
    connect(inst, SIGNAL(sendMpt1327Nodata(int)), this, SLOT(sendMpt1327Nodata(int)));
    */
    trunkbase = trunksteps = trunkchan = trunkid = trunkready = -1;
    trunkcallsrc = trunkcalldst = trunkcallchan = -1;
    trunkregid = trunkreggroup = trunkstandard = -1;
    trunking = false;
    trunktraffic = false;
    controlfreq = 0;

    // Init MPT1327 stuff
    curr_sample = 0;
    sum[0] = sum[1] = sum[2] = sum[3] = 0.0;
    nuf_ones = 0;
    pll = 0;
    samplespersym = (int)floor(10800.0/1200.0);
    i0 = (double *)malloc(samplespersym * 2 * sizeof(double));
    q0 = (double *)malloc(samplespersym * 2 * sizeof(double));
    i1 = (double *)malloc(samplespersym * 2 * sizeof(double));
    q1 = (double *)malloc(samplespersym * 2 * sizeof(double));

    corr_i0 = (double *)malloc(samplespersym * sizeof(double));
    corr_q0 = (double *)malloc(samplespersym * sizeof(double));
    corr_i1 = (double *)malloc(samplespersym * sizeof(double));
    corr_q1 = (double *)malloc(samplespersym * sizeof(double));

    memset((void *)corr_i0, 0, samplespersym * sizeof(double));
    memset((void *)corr_q0, 0, samplespersym * sizeof(double));
    memset((void *)corr_i1, 0, samplespersym * sizeof(double));
    memset((void *)corr_q1, 0, samplespersym * sizeof(double));

    for (i = 0; i < samplespersym * 2 ; i++) 
    {
        i0[i] = sin(2.0*M_PI*((double)i/(double)samplespersym) *
                    ((double)1800.0/(double)1200.0));
        q0[i] = cos(2.0*M_PI*((double)i/(double)samplespersym) *
                    ((double)1800.0/(double)1200.0));
        i1[i] = sin(2.0*M_PI*((double)i/(double)samplespersym) *
                    ((double)1200.0/(double)1200.0));
        q1[i] = cos(2.0*M_PI*((double)i/(double)samplespersym) *
                    ((double)1200.0/(double)1200.0));
    }
    pll = samplespersym;
    ones_calc_mask = (1<<(samplespersym - 1));
    memset(mpt1327_state.codeword,0,4*sizeof(uint16_t));
    mpt1327_state.fcs = 0;
    mpt1327_state.cnt = 0;
    mpt1327_state.state = 0;
    corr = 0;
    sysid = 0;
}

Mpt1327Win::~Mpt1327Win()
{
    qDebug() << "MPT1327 decoder destroyed.";

    free(i0);
    free(q0);
    free(i1);
    free(q1);
    free(corr_i0);
    free(corr_q0);
    free(corr_i1);
    free(corr_q1);

    delete ui;
}


void Mpt1327Win::reset(qint64 freq)
{
    QString str;
    char msg[255];

    zeroframes = 0;
    trunkbase = trunksteps = trunkchan = trunkid = trunkready = -1;
    trunkcallsrc = trunkcalldst = trunkcallchan = -1;
    trunkregid = trunkreggroup = trunkstandard = -1;
    if (!trunking)
    {
        sprintf(msg,"Frequency reset: new frequency %lld Hz",freq);
        str.append(msg);
        ui->textView->appendPlainText(msg);
        controlfreq = freq;
    }
}




/*! \brief Catch window close events and emit signal so that main application can destroy us. */
void Mpt1327Win::closeEvent(QCloseEvent *ev)
{
    Q_UNUSED(ev);

    emit windowClosed();
}


/*! \brief User clicked on the Clear button. */
void Mpt1327Win::on_actionClear_triggered()
{
    ui->textView->clear();
}


void Mpt1327Win::on_actionTable_triggered()
{
    trunktable = new TrunkChannels(this, channelmap);
    connect(trunktable, SIGNAL(channelsEnforce(QMap<int,channel_t>)), this, SLOT(channelsEnforce(QMap<int,channel_t>)));
    connect(trunktable, SIGNAL(windowClosed()), this, SLOT(trunktable_closed()));
    trunktable->show();
}


void Mpt1327Win::on_actionTrunk_triggered()
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
void Mpt1327Win::trunktable_closed()
{
    delete trunktable;
    trunktable = 0;
}



/*! \brief User clicked on the Save button. */
void Mpt1327Win::on_actionSave_triggered()
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
void Mpt1327Win::on_actionInfo_triggered()
{
    QMessageBox::about(this, tr("About MPT1327 Trunk Decoder"),
                       tr("<p>Gqrx MPT1327 Trunk Decoder %1</p>"
                          "<p>The Gqrx MPT1327 decoder taps directly into the SDR signal path "
                          "eliminating the need to mess with virtual or real audio cables. "
                          "It can manage MPT1327 trunking. </p>"
                          ).arg(VERSION));

}



void Mpt1327Win::channelsEnforce(QMap<int,channel_t> map)
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


void Mpt1327Win::trunkReady()
{
    QString str;
    char msg[255];
    msg[0] = 0;

    if ((trunkready<0)&&(trunkready<2))
    {
        trunkready = 1;
        trunkstandard = 1;
        if (!trunking)
            sprintf(msg,"MPT1327 Trunk Control Channel detected (ID: %d)\nNon-standard band plan used, please define a band plan",trunkid);
        trunkstandard = 0;
        if (msg[0])
        {
            str.append(msg);
            ui->textView->appendPlainText(msg);
        }
    }
}



void Mpt1327Win::sendMpt1327Trunkid(int id)
{
    trunkid = id;
    if ((trunkid>=0) && (trunksteps>=0) && (trunkchan>=0))
        trunkReady();
}


void Mpt1327Win::sendMpt1327TrunkChan(int chan,int steps, int base)
{
    trunkchan = chan;
    trunksteps = steps;
    trunkbase = base;
    if ((trunkid>=0) && (trunksteps>=0) && (trunkchan>=0))
        trunkReady();
}

void Mpt1327Win::sendMpt1327TrunkReg(int regunit,int regdst)
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

void Mpt1327Win::sendMpt1327TrunkCall(int src,int dst, int chan)
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

void Mpt1327Win::sendMpt1327Nodata(int state)
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




int Mpt1327Win::getbit(int n, uint16_t *codewords)
{
    uint16_t mask;
    uint16_t codeword;

    codeword = codewords[n/16];
    mask = 1<<(15-(n%16));

    return (codeword & mask)?1:0;
}



uint16_t Mpt1327Win::mpt1327_fcs(mpt1327_t *m)
{
    int n, bit;
    uint16_t syndrome = 0, parity = 0;

    for (n = 0; n < 64; n++) {
        bit = getbit(n, m->codeword);
        parity ^= bit;
        if (n == 62) bit ^= 1;
        if (n < 63) {
            syndrome <<= 1;
            if (bit ^ ((syndrome & 0x8000)?1:0))
                syndrome ^= 0x6815;
        }
    }

    syndrome &= 0x7fff;
    if (parity)
        syndrome |= 0x8000;
    
    return syndrome;
}





void Mpt1327Win::mpt1327(unsigned char bit, mpt1327_t *m)
{
    uint16_t para;
    uint16_t channel;
    double channel2;
    uint16_t prefix;
    uint16_t ident1;
    uint16_t cat;
    uint16_t type;
    uint16_t func;
    QString str;
    char msg[255];
 

    /*
     * Shift in the least significant bit into the 64 bit frame.
     */
    m->codeword[0] <<= 1;
    m->codeword[0] += (m->codeword[1] & 0x8000)?1:0;
    m->codeword[1] <<= 1;
    m->codeword[1] += (m->codeword[2] & 0x8000)?1:0;
    m->codeword[2] <<= 1;
    m->codeword[2] += (m->codeword[3] & 0x8000)?1:0;
    m->codeword[3] <<= 1;
    if (bit) m->codeword[3] += 1;
    m->cnt++;

    switch (m->state) 
    {

        case 0:
            if (((m->codeword[3] & 0xffff) == MPT1327_SYNC) ||
                ((m->codeword[3] & 0xffff) == MPT1327_SYNT)) 
            {
                if (mpt1327_fcs(m) == 0) 
                {
                    //printf("SYS 0x%4.4x ", m->codeword[0] & 0x7fff);
                    //fflush(stdout);

                    if (sysid != (m->codeword[0] & 0x7fff))
                    {
                        sysid = (m->codeword[0] & 0x7fff);
                        sprintf(msg,"MPT1327 Trunk channel detected, system ID 0x%4.4x",sysid);
                        str.append(msg);
                        ui->textView->appendPlainText(msg);
                    }

                }
                m->state = 1;
                m->cnt = 0;
            }
            break;

        case 1:
            if (m->cnt == 64) 
            {
                if (mpt1327_fcs(m) == 0) 
                {
                    cat = ((m->codeword[1]>>7) & 0x7);
                    type = ((m->codeword[1]>>5) & 0x3);
                    func = ((m->codeword[1]>>2) & 0x7);

                    if (cat == 0x0) 
                    {
                        //if (type == 0x0) printf("ALOHA ");
                        if (type == 0x1) 
                        {
                            printf("ACK");
                            if (func == 0x0) printf("K General ");
                            if (func == 0x1) printf("I Intermediate ");
                            if (func == 0x2) printf("Q Call Queued");
                            if (func == 0x3) printf("X Mensage rejected ");
                            if (func == 0x4) printf("V Called unit unavailable ");
                            if (func == 0x5) printf("E ** EMERGENCY CALL **");
                            if (func == 0x6) printf("T Try on given address ");
                            if (func == 0x7) printf("B Call Back or Negative ");

                            prefix = ((m->codeword[0] & 0x7f00) >> 8);
                            printf(" Prefix: 0x%x",prefix);

                            ident1 = ((m->codeword[0] << 5 | m->codeword[1] >> 11) & 0x1fff);
                            printf(" Ident1: 0x%x ",ident1);
                        }
                        if (type == 0x2) printf("REQ / AHOY ");
                        if (type == 0x3) printf("MISC ");
                        if (type!=0) printf("\n");
                    }

                    if (cat == 0x1 && type == 0x0) 
                    {
                        printf ("Single Address Message \n");
                    }

                    if (cat == 0x1 && type == 0x1) 
                    {
                        printf ("Sort Data Message \n");
                    }

                    if (cat == 0x0 && type == 0x3 && func == 0x1) 
                    {
                        printf ("Call Maintenance Message - MAINT \n");
                    }
                    if (cat == 0x0 && type == 0x3 && func == 0x3) 
                    {
                        printf ("Move to Control Channel - MOVE \n");
                    }

                    if (m->codeword[1] & (1<<10)) 
                    {
                        para = m->codeword[2];
                        para += (m->codeword[1] & 0x03) << 16;

                        /* Endast nedkopplingen */
                        if ((para & 0xaaa) == 0xaaa) 
                        {
                            time_t t = time(NULL);
                            channel = (m->codeword[0]>>5 & 0x2ff);
                            channel2 = (BASEFREQ+(channel*STEP));

                        /* Clear Down Allocated Traffic Channel */
                            printf("CLEAR CHAN: 0x%x Freq:%f %s", channel, channel2, ctime(&t));
                            printf("[END ");
                            //setfreq(CC);
                            printf("\n");
                        }
                    }
                    else 
                    {
                        printf("GTC ");
                        channel = ( (m->codeword[1] << 1 | m->codeword[2] >> 15) & 0x3ff);
                        printf("Channel 0x%x ", channel);
                        channel2 = (BASEFREQ+(channel*STEP));
                        printf ("Freq: %f Mhz ", channel2);

                        prefix = ((m->codeword[0] & 0x7f00) >> 8);
                        printf(" Prefix: 0x%x",prefix);
                        ident1 = ((m->codeword[0] << 5 | m->codeword[1] >> 11) & 0x1fff);
                        printf(" Ident1: 0x%x ",ident1);

                        if (m->codeword[1] & 0x200) 
                        {
                            printf("Data ");
                        }
                        else
                        {
                            printf("Voice ");
                            if ((prefix & TARGET_PREFIX) == prefix) 
                            {
                                if ((ident1 & TARGET_IDENT) == ident1) 
                                {
                                    printf("[");
                                    //setfreq(channel2);
                                }
                            }
                        }
                        printf("\n");
                    }
                }
                m->state = 0;
                m->cnt = 0;
            }
            break;
        default:
            m->state = 0;
    }
}



int Mpt1327Win::sym_recovery_1200(uint16_t correc, unsigned char num_ones, int samplespersymbol)
{
    int diff = 0;

    if (num_ones > 4) 
    {
        mpt1327(1, &mpt1327_state);
    } 
    else {
        mpt1327(0, &mpt1327_state);
    }

    if (num_ones > 4) 
    {
        if ((correc & 0x0028) != 0x0028) 
        {
            if (correc & 0x0008)
                diff = 3;
            else
                diff = -3;
        } 
        else if ((correc & 0x0044) != 0x0044) 
        {
            if (correc & 0x0004)
                diff = 2;
            else
                diff = -2;
        }
        else if ((correc & 0x0082) != 0x0082) 
        {
            if (correc & 0x0002)
                diff = 1;
            else
                diff = -1;
        }
        else if ((correc & 0x0101) != 0x0101) 
        {
            diff = 0;
        }
        else 
        {
            diff = 0;
        }
    }

    return(samplespersymbol + diff); 

}



void Mpt1327Win::demod(float *buffer, int length)
{
    for (i = 0; i < length; i++) 
    {
        sample = (double)buffer[i];
        sum[0] -= corr_i0[curr_sample%samplespersym];
        sum[1] -= corr_q0[curr_sample%samplespersym];
        sum[2] -= corr_i1[curr_sample%samplespersym];
        sum[3] -= corr_q1[curr_sample%samplespersym];

        corr_i0[curr_sample%samplespersym] = i0[curr_sample] * sample;
        corr_q0[curr_sample%samplespersym] = q0[curr_sample] * sample;
        corr_i1[curr_sample%samplespersym] = i1[curr_sample] * sample;
        corr_q1[curr_sample%samplespersym] = q1[curr_sample] * sample;

        sum[0] += corr_i0[curr_sample%samplespersym];
        sum[1] += corr_q0[curr_sample%samplespersym];
        sum[2] += corr_i1[curr_sample%samplespersym];
        sum[3] += corr_q1[curr_sample%samplespersym];

        curr_sample++;
        curr_sample %= (samplespersym * 2);

        if (corr & ones_calc_mask)
            nuf_ones--;

        corr <<= 1;
        abs_sum = fabs(sum[2]) + fabs(sum[3]) - fabs(sum[0]) - fabs(sum[1]);
        if (abs_sum > 0.0){
            corr += 1;
            nuf_ones++;
        }
        pll--;      
        if (pll == 0) 
        {
            pll = sym_recovery_1200(corr, nuf_ones, samplespersym);
        }
    }
}

void Mpt1327Win::process_samples(float *buffer, int length)
{
/*
    int overlap = 18;
    int i;

    for (i = 0; i < length; i++) {
        tmpbuf.append(buffer[i]);
    }

    demod(tmpbuf.data(), length);

    tmpbuf.clear();
    for (i = length-overlap; i < length; i++) {
        tmpbuf.append(buffer[i]);
    }
*/
    demod(buffer, length);
}

