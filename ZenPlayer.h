#include <QDir>
#include <QUrl>
#include <QMenu>
#include <QPoint>
#include <QCursor>
#include <QPixmap>
#include <QPainter>
#include <QFileInfo>
#include <QFileDialog>
#include <QMainWindow>
#include <QApplication>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QPainterPath>
#include <QMessageBox>
#include <QMediaMetaData>
#include <QStandardPaths>
#include <QListWidgetItem>
#include <random>
#include <fstream>
#include <algorithm>
#include "json.hpp"
#include "ui_ZenPlayer.h"
#include "createPlaylistDialog.h"

using json=nlohmann::json;

QT_BEGIN_NAMESPACE
namespace Ui { class ZenPlayerClass; };
QT_END_NAMESPACE

class ZenPlayer : public QMainWindow
{
   Q_OBJECT

public:
   ZenPlayer(QWidget *parent = nullptr);
   ~ZenPlayer();

private slots:
   void on_muteButton_clicked();
   void on_volumeSlider_valueChanged(int value);

   void on_repeatButton_clicked();
   void on_shuffleButton_clicked();
   void on_previousButton_clicked();
   void on_nextButton_clicked();
   void on_playButton_clicked();

   void saveData();
   void loadData();

   void on_addFolderButton_clicked();
   void on_foldersListWidget_itemClicked(QListWidgetItem* item);
   void showFoldersContextMenu(const QPoint &pos);

   void on_addPlaylistButton_clicked();
   void on_playlistListWidget_itemClicked(QListWidgetItem* item);
   void showPlaylistsContextMenu(const QPoint &pos);
   
   void on_tracksListWidget_itemDoubleClicked(QListWidgetItem* item);
   void showTracksContextMenu(const QPoint &pos);

   void on_tabWidget_currentChanged(int index);

   void playTrack();
   void handleMetadataChanged();
   void on_positionChanged(qint64 position);
   void on_durationChanged(qint64 duration);
   void on_timeSlider_sliderMoved(int position);
   
   void on_queueListWidget_itemClicked(QListWidgetItem* item);

private:  
   Ui::ZenPlayerClass *ui;
   QMediaPlayer* player;
   QAudioOutput* audioOutput;
   bool mute,repeat,shuffle,pause,isFolder;
   int volume;
   json data;
   QList<QString> folderPaths;
   QList<QString> trackPaths;
   QList<QString> playQueue;
   QList<QString> originalQueue;
   QString currentTrackPath;
   int currentQueueIndex;

   void setDefaultTrackPic();
   QPixmap getRoundedPixmap(const QPixmap& src, int radius);
   QString formatTime(qint64 ms);
   void buildQueueFromCurrentTracks();
   void updateQueueWidget();
   void playTrackAtIndex(int index);
};
