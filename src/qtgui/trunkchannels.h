#ifndef TRUNKCHANNELS_H
#define TRUNKCHANNELS_H

#include <QMainWindow>
#include <QMap>
#include <QFileDialog>
#include <QFile>
#include <QDataStream>

namespace Ui {
class TrunkChannels;
}

typedef struct {
    int channel;
    qint64 frequency;
} channel_t;

class TrunkChannels : public QMainWindow
{
    Q_OBJECT

public:
    explicit TrunkChannels(QWidget *parent = 0);
    TrunkChannels(QWidget *parent = 0,QMap<int,channel_t> map=QMap<int,channel_t>());
    ~TrunkChannels();

signals:
    void channelsEnforce(QMap<int,channel_t> channels);
    void windowClosed();


protected:
    void closeEvent(QCloseEvent *ev);


private slots:
    void buttonNewRow();
    void buttonDeleteRow();
    void on_actionEnforce_triggered();
    void on_actionLoad_triggered();
    void on_actionSave_triggered();
    void enforce();
    void load();
    void save();

private:
    Ui::TrunkChannels *ui;
    QMap<int,channel_t> channelmap;
};

#endif // TRUNKCHANNELS_H
