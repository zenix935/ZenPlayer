#include <QDir>
#include <QUrl>
#include <QSet>
#include <QMenu>
#include <QPoint>
#include <QCursor>
#include <QPixmap>
#include <QPainter>
#include <QFileInfo>
#include <QFileDialog>
#include <QMainWindow>
#include <QMessageBox>
#include <QApplication>
#include <QMediaPlayer>
#include <QPainterPath>
#include <QMediaMetaData>
#include <QStandardPaths>
#include <QListWidgetItem>
#include <random>
#include <fstream>
#include <algorithm>
#include "json.hpp"
#include "VlcEngine.h"
#include "ui_ZenPlayer.h"
#include "ClickableSlider.h"
#include "equalizerDialog.h"
#include "ListItemDelegate.h"
#include "TrackItemDelegate.h"
#include "createPlaylistDialog.h"

using json=nlohmann::json;

QT_BEGIN_NAMESPACE
namespace Ui { class ZenPlayerClass; };
QT_END_NAMESPACE

class ZenPlayer : public QMainWindow
{
   Q_OBJECT

public:
   ZenPlayer(QWidget* parent = nullptr);
   ~ZenPlayer();

protected:
   bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
   void on_muteButton_clicked();
   void on_volumeSlider_valueChanged(int value);

   void on_repeatButton_clicked();
   void on_shuffleButton_clicked();
   void on_previousButton_clicked();
   void on_nextButton_clicked();
   void on_playButton_clicked();
   void on_equalizerButton_clicked();
   void on_crossfadeSlider_valueChanged(int value);

   void on_sortComboBox_currentIndexChanged();
   void on_orderComboBox_currentIndexChanged();

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
   void sortTracks();
   
   void on_queueListWidget_itemClicked(QListWidgetItem* item);
   void showQueueContextMenu(const QPoint &pos);
   void onVlcStateChanged(VlcEngine::State newState);
   void on_searchPushButton_clicked();
   void onCrossfadeTimerTimeout();

private:  
   Ui::ZenPlayerClass* ui;
   VlcEngine* vlcEngine;
   VlcEngine* vlcEngine2;
   VlcEngine* activeEngine;
   VlcEngine* fadingEngine;

   QTimer* crossfadeTimer;
   bool isCrossfading=false;
   int crossfadeElapsedMs=0;
   int crossfadeDurationMs=0;
   bool crossfadeTriggered=false;

   QMediaPlayer* metaPlayer;
   bool mute,repeat,shuffle,pause,isFolder;
   bool showRemainingTime=false;
   int volume;
   json data;
   QList<QString> folderPaths;
   QList<QString> trackPaths;
   QList<QString> searchTrackPaths;
   QList<QString> playQueue;
   QList<QString> originalQueue;
   QString currentTrackPath;
   int currentQueueIndex;
   int equalizerPresetIndex=0;
   std::vector<int> equalizerCustomValues; 
   std::vector<int> equalizerCurrentValues;

   void applyEqualizerToVlc(VlcEngine* targetEngine = nullptr);
   void checkTriggerCrossfade(qint64 positionMs);
   void startCrossfadeTo(int nextIndex, qint64 fadeMs);
   void cancelCrossfade();

   void setDefaultTrackPic();
   QPixmap getRoundedPixmap(const QPixmap& src, int radius);
   QString formatTime(qint64 ms);
   void updateMaxTimeLabel();
   void buildQueueFromCurrentTracks();
   void updateQueueWidget();
   void playTrackAtIndex(int index);
};