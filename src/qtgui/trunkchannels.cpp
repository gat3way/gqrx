#include "trunkchannels.h"
#include "ui_trunkchannels.h"


QDataStream & operator<<(QDataStream & out, const channel_t &channel)
{
    out << channel.channel << channel.frequency;
    return out;
}

QDataStream & operator>>(QDataStream & in, channel_t &channel)
{
    in >> channel.channel >> channel.frequency;
    return in;
}




TrunkChannels::TrunkChannels(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::TrunkChannels)
{
    ui->setupUi(this);
    ui->tableWidget->horizontalHeader()->setStretchLastSection(true);
    connect(ui->pushButton, SIGNAL (pressed()), this, SLOT (buttonNewRow()));
    connect(ui->pushButton_2, SIGNAL (pressed()), this, SLOT (buttonDeleteRow()));
}


TrunkChannels::TrunkChannels(QWidget *parent, QMap<int,channel_t> map) :
    QMainWindow(parent),
    ui(new Ui::TrunkChannels)
{
    channelmap = map;
    ui->setupUi(this);
    ui->tableWidget->horizontalHeader()->setStretchLastSection(true);
    connect(ui->pushButton, SIGNAL (pressed()), this, SLOT (buttonNewRow()));
    connect(ui->pushButton_2, SIGNAL (pressed()), this, SLOT (buttonDeleteRow()));

    foreach(int key, channelmap.keys())
    {
        QString str1 = QString::number(channelmap.value(key).channel);
        QString str2 = QString::number(channelmap.value(key).frequency);

        QTableWidgetItem *item1 = new QTableWidgetItem(str1);
        QTableWidgetItem *item2 = new QTableWidgetItem(str2);
        ui->tableWidget->insertRow(ui->tableWidget->rowCount());
        ui->tableWidget->setItem(ui->tableWidget->rowCount()-1,0,item1);
        ui->tableWidget->setItem(ui->tableWidget->rowCount()-1,1,item2);
    }
}




TrunkChannels::~TrunkChannels()
{
    delete ui;
}


void TrunkChannels::buttonNewRow()
{
    ui->tableWidget->insertRow(ui->tableWidget->rowCount());
}


void TrunkChannels::buttonDeleteRow()
{
    ui->tableWidget->removeRow(ui->tableWidget->currentRow());
}


void TrunkChannels::on_actionEnforce_triggered()
{
    channel_t chandata;
    int key;
    channelmap.clear();
    for( int row = 0; row < ui->tableWidget->rowCount(); ++row ) 
    {
        QTableWidgetItem *itab1 = ui->tableWidget->item(row,0);
        QTableWidgetItem *itab2 = ui->tableWidget->item(row,1);
        if ((itab1 != 0) && (itab2 != 0))
        {
            chandata.channel = itab1->text().toInt();
            chandata.frequency = itab2->text().toLongLong();
            key = chandata.channel;
            channelmap.insert(key,chandata);
        }
    }
    emit channelsEnforce(channelmap);
}

void TrunkChannels::enforce()
{
    //TODO
}

void TrunkChannels::load()
{
    //TODO
}

void TrunkChannels::save()
{
    //TODO
}

void TrunkChannels::closeEvent(QCloseEvent *ev)
{
    Q_UNUSED(ev);

    emit windowClosed();
}


void TrunkChannels::on_actionSave_triggered()
{
    channel_t chandata;
    int key;
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save File"),
                                                    QDir::homePath(),
                                                    tr("Trunk bandplan files (*.tch)"));

    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return;
    }


    channelmap.clear();
    for( int row = 0; row < ui->tableWidget->rowCount(); ++row ) 
    {
        QTableWidgetItem *itab1 = ui->tableWidget->item(row,0);
        QTableWidgetItem *itab2 = ui->tableWidget->item(row,1);
        if ((itab1 != 0) && (itab2 != 0))
        {
            chandata.channel = itab1->text().toInt();
            chandata.frequency = itab2->text().toLongLong();
            key = chandata.channel;
            channelmap.insert(key,chandata);
        }
    }

    QDataStream out(&file);
    out << channelmap;
}


void TrunkChannels::on_actionLoad_triggered()
{
    QString file;

    file = QFileDialog::getOpenFileName(this, tr("Load settings"),
                                           QDir::homePath(),
                                           tr("Trunk bandplan (*.tch)"));

    if (file.isEmpty())
        return;

    if (!file.endsWith(".tch", Qt::CaseSensitive))
        file.append(".tch");

    QFile cfile(file);
    QDataStream in(&cfile);

    if (!cfile.open(QIODevice::ReadOnly))
    {
        return;
    }
    channelmap.clear();
    in >> channelmap;
    ui->tableWidget->setRowCount(0);
    foreach(int key, channelmap.keys())
    {
        QString str1 = QString::number(channelmap.value(key).channel);
        QString str2 = QString::number(channelmap.value(key).frequency);

        QTableWidgetItem *item1 = new QTableWidgetItem(str1);
        QTableWidgetItem *item2 = new QTableWidgetItem(str2);
        ui->tableWidget->insertRow(ui->tableWidget->rowCount());
        ui->tableWidget->setItem(ui->tableWidget->rowCount()-1,0,item1);
        ui->tableWidget->setItem(ui->tableWidget->rowCount()-1,1,item2);
    }


}

