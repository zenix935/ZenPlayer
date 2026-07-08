#include <QMainWindow>
#include <QApplication>
#include <QFileDialog>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QUrl>
#include <QListWidgetItem>
#include <QMenu>
#include <QPoint>
#include <QCursor>
#include <QFileInfo>
#include <QMediaMetaData>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <fstream>
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
   
   void on_tracksListWidget_itemClicked(QListWidgetItem* item);
   void showTracksContextMenu(const QPoint &pos);

   void on_tabWidget_currentChanged(int index);

   void playTrack();
   void handleMetadataChanged();

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

   void setDefaultTrackPic();
   QPixmap getRoundedPixmap(const QPixmap& src, int radius);
};
