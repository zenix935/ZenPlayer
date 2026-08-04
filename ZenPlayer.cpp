#include "ZenPlayer.h"

ZenPlayer::ZenPlayer(QWidget *parent) : QMainWindow(parent), ui(new Ui::ZenPlayerClass())
{
    mute=false;
    repeat=false;
    shuffle=false;
    isFolder=true;
    pause=true;
    currentQueueIndex=-1;
    volume=50;
    ui->setupUi(this);
	setWindowIcon(QIcon(":/pics/pics/icon.ico"));

    vlcEngine=new VlcEngine(this);
    if (!vlcEngine->init(""))
        qWarning()<<"[ZenPlayer] Failed to initialize vlcEngine";

    vlcEngine2=new VlcEngine(this);
    if (!vlcEngine2->init(""))
        qWarning()<<"[ZenPlayer] Failed to initialize vlcEngine2";

    activeEngine=vlcEngine;
    fadingEngine=nullptr;

    crossfadeTimer=new QTimer(this);
    crossfadeTimer->setInterval(20);
    connect(crossfadeTimer, &QTimer::timeout, this, &ZenPlayer::onCrossfadeTimerTimeout);

    metaPlayer=new QMediaPlayer(this);
    
    loadData();
    
    //Set initial volume from slider
    vlcEngine->setVolume(ui->volumeSlider->value());
    vlcEngine2->setVolume(ui->volumeSlider->value());

    //Set initial track info and picture states
    setDefaultTrackPic();
    ui->trackInfoLabel->setText("No Song Playing");

    //Install event filter for maxTimeLabel click toggle
    ui->maxTimeLabel->installEventFilter(this);
    ui->maxTimeLabel->setCursor(Qt::PointingHandCursor);

    //Enable custom context menus
    ui->foldersListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->playlistListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->tracksListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->queueListWidget->setContextMenuPolicy(Qt::CustomContextMenu);

    //Connect context menu signals to slots
    connect(ui->foldersListWidget, &QListWidget::customContextMenuRequested, this, &ZenPlayer::showFoldersContextMenu);
    connect(ui->playlistListWidget, &QListWidget::customContextMenuRequested, this, &ZenPlayer::showPlaylistsContextMenu);
    connect(ui->tracksListWidget, &QListWidget::customContextMenuRequested, this, &ZenPlayer::showTracksContextMenu);
    connect(ui->queueListWidget, &QListWidget::customContextMenuRequested, this, &ZenPlayer::showQueueContextMenu);

    //Connect QMediaPlayer metadata signals
    connect(metaPlayer, &QMediaPlayer::metaDataChanged, this, &ZenPlayer::handleMetadataChanged);
    connect(metaPlayer, &QMediaPlayer::mediaStatusChanged, this, &ZenPlayer::handleMetadataChanged);

    //Connect VlcEngine playback signals for both engines
    connect(vlcEngine, &VlcEngine::positionChanged, this, &ZenPlayer::on_positionChanged);
    connect(vlcEngine, &VlcEngine::durationChanged, this, &ZenPlayer::on_durationChanged);
    connect(vlcEngine, &VlcEngine::stateChanged, this, &ZenPlayer::onVlcStateChanged);

    connect(vlcEngine2, &VlcEngine::positionChanged, this, &ZenPlayer::on_positionChanged);
    connect(vlcEngine2, &VlcEngine::durationChanged, this, &ZenPlayer::on_durationChanged);
    connect(vlcEngine2, &VlcEngine::stateChanged, this, &ZenPlayer::onVlcStateChanged);

    connect(ui->crossfadeSlider, &QSlider::valueChanged, this, &ZenPlayer::on_crossfadeSlider_valueChanged);
    on_crossfadeSlider_valueChanged(ui->crossfadeSlider->value());

    connect(ui->timeSlider, &QSlider::sliderMoved, this, &ZenPlayer::on_timeSlider_sliderMoved);

    //Connect queueListWidget signal
    connect(ui->tracksListWidget, &QListWidget::itemDoubleClicked, this, &ZenPlayer::on_tracksListWidget_itemDoubleClicked);
    connect(ui->sortComboBox, &QComboBox::currentIndexChanged, this, &ZenPlayer::on_sortComboBox_currentIndexChanged);
    connect(ui->orderComboBox, &QComboBox::currentIndexChanged, this, &ZenPlayer::on_orderComboBox_currentIndexChanged);
    connect(ui->searchPushButton, &QPushButton::clicked, this, &ZenPlayer::on_searchPushButton_clicked);
    connect(ui->searchLineEdit, &QLineEdit::returnPressed, this, &ZenPlayer::on_searchPushButton_clicked);

    //Setup custom delegate for tracksListWidget with play button on hover
    TrackItemDelegate* trackDelegate=new TrackItemDelegate(ui->tracksListWidget);
    ui->tracksListWidget->setItemDelegate(trackDelegate);
    ui->tracksListWidget->setMouseTracking(true);
    ui->tracksListWidget->viewport()->setAttribute(Qt::WA_Hover, true);
    ui->tracksListWidget->viewport()->installEventFilter(this);

    connect(trackDelegate, &TrackItemDelegate::playClicked, this, [this](const QModelIndex &index) 
    {
        QListWidgetItem* item=ui->tracksListWidget->item(index.row());
        if (item) 
        {
            ui->tracksListWidget->setCurrentItem(item);
            on_tracksListWidget_itemDoubleClicked(item);
        }
    });

    connect(trackDelegate, &TrackItemDelegate::menuClicked, this, [this](const QModelIndex &index) 
    {
        QListWidgetItem* item=ui->tracksListWidget->item(index.row());
        if (item) 
        {
            // Pass the center of the item rect so itemAt() resolves correctly
            QRect visualRect=ui->tracksListWidget->visualRect(index);
            showTracksContextMenu(visualRect.center());
        }
    });

    //Setup custom delegate for playlistListWidget with 3-dot menu on hover
    ListItemDelegate* playlistDelegate=new ListItemDelegate(ui->playlistListWidget);
    ui->playlistListWidget->setItemDelegate(playlistDelegate);
    ui->playlistListWidget->setMouseTracking(true);
    ui->playlistListWidget->viewport()->setAttribute(Qt::WA_Hover, true);
    ui->playlistListWidget->viewport()->installEventFilter(this);

    connect(playlistDelegate, &ListItemDelegate::menuClicked, this, [this](const QModelIndex &index) 
    {
        QListWidgetItem* item=ui->playlistListWidget->item(index.row());
        if (item) 
        {
            QRect visualRect=ui->playlistListWidget->visualRect(index);
            showPlaylistsContextMenu(visualRect.center());
        }
    });

    //Setup custom delegate for foldersListWidget with 3-dot menu on hover
    ListItemDelegate* folderDelegate=new ListItemDelegate(ui->foldersListWidget);
    ui->foldersListWidget->setItemDelegate(folderDelegate);
    ui->foldersListWidget->setMouseTracking(true);
    ui->foldersListWidget->viewport()->setAttribute(Qt::WA_Hover, true);
    ui->foldersListWidget->viewport()->installEventFilter(this);

    connect(folderDelegate, &ListItemDelegate::menuClicked, this, [this](const QModelIndex &index) 
    {
        QListWidgetItem* item=ui->foldersListWidget->item(index.row());
        if (item) 
        {
            QRect visualRect=ui->foldersListWidget->visualRect(index);
            showFoldersContextMenu(visualRect.center());
        }
    });

    //Setup custom delegate for queueListWidget with 3-dot menu on hover
    ListItemDelegate* queueDelegate=new ListItemDelegate(ui->queueListWidget);
    ui->queueListWidget->setItemDelegate(queueDelegate);
    ui->queueListWidget->setMouseTracking(true);
    ui->queueListWidget->viewport()->setAttribute(Qt::WA_Hover, true);
    ui->queueListWidget->viewport()->installEventFilter(this);

    connect(queueDelegate, &ListItemDelegate::menuClicked, this, [this](const QModelIndex &index) 
    {
        QListWidgetItem* item=ui->queueListWidget->item(index.row());
        if (item) 
        {
            QRect visualRect=ui->queueListWidget->visualRect(index);
            showQueueContextMenu(visualRect.center());
        }
    });
}

bool ZenPlayer::eventFilter(QObject* watched, QEvent* event)
{
    if (ui && watched==ui->tracksListWidget->viewport()) 
    {
        if (event->type()==QEvent::MouseMove || event->type()==QEvent::HoverMove) 
        {
            QMouseEvent* me=static_cast<QMouseEvent*>(event);
            QModelIndex index=ui->tracksListWidget->indexAt(me->pos());
            bool isHand=false;
            if (index.isValid()) 
            {
                QRect rect=ui->tracksListWidget->visualRect(index);
                int btnWidth=22;
                int btnHeight=22;
                int leftPadding=8;
                QRect btnRect(rect.left()+leftPadding, rect.top()+(rect.height()-btnHeight)/2, btnWidth, btnHeight);
                QRect menuRect=TrackItemDelegate::menuBtnRect(rect, ui->tracksListWidget->viewport()->width());
                if (btnRect.contains(me->pos()) || menuRect.contains(me->pos()))
                    isHand=true;
            }
            ui->tracksListWidget->viewport()->setCursor(isHand ? Qt::PointingHandCursor : Qt::ArrowCursor);
            ui->tracksListWidget->viewport()->update();
        }
        else if (event->type()==QEvent::Leave)
        {
            ui->tracksListWidget->viewport()->setCursor(Qt::ArrowCursor);
            ui->tracksListWidget->viewport()->update();
        }
    }
    if (ui && watched==ui->playlistListWidget->viewport()) 
    {
        if (event->type()==QEvent::MouseMove || event->type()==QEvent::HoverMove) 
        {
            QMouseEvent* me=static_cast<QMouseEvent*>(event);
            QModelIndex index=ui->playlistListWidget->indexAt(me->pos());
            bool isHand=false;
            if (index.isValid()) 
            {
                QRect rect=ui->playlistListWidget->visualRect(index);
                QRect menuRect=ListItemDelegate::menuBtnRect(rect, ui->playlistListWidget->viewport()->width());
                if (menuRect.contains(me->pos()))
                    isHand=true;
            }
            ui->playlistListWidget->viewport()->setCursor(isHand ? Qt::PointingHandCursor : Qt::ArrowCursor);
            ui->playlistListWidget->viewport()->update();
        }
        else if (event->type()==QEvent::Leave)
        {
            ui->playlistListWidget->viewport()->setCursor(Qt::ArrowCursor);
            ui->playlistListWidget->viewport()->update();
        }
    }
    if (ui && watched==ui->foldersListWidget->viewport()) 
    {
        if (event->type()==QEvent::MouseMove || event->type()==QEvent::HoverMove) 
        {
            QMouseEvent* me=static_cast<QMouseEvent*>(event);
            QModelIndex index=ui->foldersListWidget->indexAt(me->pos());
            bool isHand=false;
            if (index.isValid()) 
            {
                QRect rect=ui->foldersListWidget->visualRect(index);
                QRect menuRect=ListItemDelegate::menuBtnRect(rect, ui->foldersListWidget->viewport()->width());
                if (menuRect.contains(me->pos()))
                    isHand=true;
            }
            ui->foldersListWidget->viewport()->setCursor(isHand ? Qt::PointingHandCursor : Qt::ArrowCursor);
            ui->foldersListWidget->viewport()->update();
        }
        else if (event->type()==QEvent::Leave)
        {
            ui->foldersListWidget->viewport()->setCursor(Qt::ArrowCursor);
            ui->foldersListWidget->viewport()->update();
        }
    }
    if (ui && watched==ui->queueListWidget->viewport()) 
    {
        if (event->type()==QEvent::MouseMove || event->type()==QEvent::HoverMove) 
        {
            QMouseEvent* me=static_cast<QMouseEvent*>(event);
            QModelIndex index=ui->queueListWidget->indexAt(me->pos());
            bool isHand=false;
            if (index.isValid()) 
            {
                QRect rect=ui->queueListWidget->visualRect(index);
                QRect menuRect=ListItemDelegate::menuBtnRect(rect, ui->queueListWidget->viewport()->width());
                if (menuRect.contains(me->pos()))
                    isHand=true;
            }
            ui->queueListWidget->viewport()->setCursor(isHand ? Qt::PointingHandCursor : Qt::ArrowCursor);
            ui->queueListWidget->viewport()->update();
        }
        else if (event->type()==QEvent::Leave)
        {
            ui->queueListWidget->viewport()->setCursor(Qt::ArrowCursor);
            ui->queueListWidget->viewport()->update();
        }
    }
    if (ui && watched==ui->maxTimeLabel)
    {
        if (event->type()==QEvent::MouseButtonRelease)
        {
            QMouseEvent* me=static_cast<QMouseEvent*>(event);
            if (me->button()==Qt::LeftButton)
            {
                showRemainingTime=!showRemainingTime;
                updateMaxTimeLabel();
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

ZenPlayer::~ZenPlayer()
{
    saveData();
    delete ui;
}

// Sound/volume
void ZenPlayer::on_muteButton_clicked()
{
    if(!mute)
    {
        QIcon icon(":/pics/pics/mute.png");
        ui->muteButton->setIcon(icon);
        ui->muteButton->setToolTip("Unmute");
        mute=true;

        ui->volumeSlider->setEnabled(false);
        if (vlcEngine) 
            vlcEngine->setMuted(true);
        if (vlcEngine2) 
            vlcEngine2->setMuted(true);
    }
    else
    {
        QIcon icon(":/pics/pics/sound.png");
        ui->muteButton->setIcon(icon);
        ui->muteButton->setToolTip("Mute");
        mute=false;

        ui->volumeSlider->setEnabled(true);
        if (vlcEngine) 
            vlcEngine->setMuted(false);
        if (vlcEngine2) 
            vlcEngine2->setMuted(false);
    }
}
void ZenPlayer::on_volumeSlider_valueChanged(int value)
{
    volume=value;
	ui->volumeLabel->setText(QString::number(value));
    if (vlcEngine) 
        vlcEngine->setVolume(value);
    if (vlcEngine2) 
        vlcEngine2->setVolume(value);
}

// Controls
void ZenPlayer::on_repeatButton_clicked()
{
	if(!repeat)
	{
		QIcon icon(":/pics/pics/repeat-one.png");
		ui->repeatButton->setIcon(icon);
        ui->repeatButton->setToolTip("Enable repeat all");
		repeat=true;
	}
	else
	{
		QIcon icon(":/pics/pics/repeat-all.png");
		ui->repeatButton->setIcon(icon);
        ui->repeatButton->setToolTip("Enable repeat one");
		repeat=false;
	}
}
void ZenPlayer::on_shuffleButton_clicked()
{
    if(!shuffle)
    {
        QIcon icon(":/pics/pics/shuffle.png");
        ui->shuffleButton->setIcon(icon);
        ui->shuffleButton->setToolTip("Disable shuffle");
        shuffle=true;

        if (!playQueue.isEmpty())
        {
            QString activeTrack=currentTrackPath;
            playQueue.removeAll(activeTrack);

            std::random_device rd;
            std::mt19937 g(rd());
            std::shuffle(playQueue.begin(), playQueue.end(), g);

            if (!activeTrack.isEmpty()) 
            {
                playQueue.prepend(activeTrack);
                currentQueueIndex=0;
            } 
            else
                currentQueueIndex=-1;
        }
        updateQueueWidget();
    }
    else
    {
        QIcon icon(":/pics/pics/cycle.png");
        ui->shuffleButton->setIcon(icon);
        ui->shuffleButton->setToolTip("Enable shuffle");
        shuffle=false;

        playQueue=originalQueue;
        if (!currentTrackPath.isEmpty()) 
            currentQueueIndex=playQueue.indexOf(currentTrackPath);
        else
            currentQueueIndex=-1;
        updateQueueWidget();
    }
}
void ZenPlayer::on_previousButton_clicked()
{
    if (playQueue.isEmpty()) 
        return;

    qint64 currentPos=ui->timeSlider->value();
    bool hasNotPlayedOverTwoSecs=(currentPos <= 2000);

    cancelCrossfade();

    if (hasNotPlayedOverTwoSecs)
    {
        int prevIndex=currentQueueIndex-1;
        if (prevIndex<0)
            prevIndex=playQueue.size()-1;
        playTrackAtIndex(prevIndex);
    }
    else
    {
        if (activeEngine)
            activeEngine->setPosition(0);
        ui->timeSlider->setValue(0);
        ui->currentTimeLabel->setText(formatTime(0));
        if (showRemainingTime)
            updateMaxTimeLabel();
    }
}
void ZenPlayer::on_nextButton_clicked()
{
    if (playQueue.isEmpty())
        return;
    cancelCrossfade();
    int nextIndex=currentQueueIndex+1;
    if (nextIndex>=playQueue.size())
        nextIndex=0;
    playTrackAtIndex(nextIndex);
}
void ZenPlayer::on_playButton_clicked()
{
    if (currentTrackPath.isEmpty())
    {
        QListWidgetItem* currentItem=ui->tracksListWidget->currentItem();
        if (currentItem)
        {
            int index=ui->tracksListWidget->row(currentItem);
            if (index>=0 && index<trackPaths.size())
            {
                currentTrackPath=trackPaths.at(index);
                currentQueueIndex=playQueue.indexOf(currentTrackPath);
            }
        }
        else if (!trackPaths.isEmpty())
        {
            currentTrackPath=trackPaths.at(0);
            currentQueueIndex=0;
        }
    }

    if (!currentTrackPath.isEmpty())
    {
        if(!pause)
        {
            QIcon icon(":/pics/pics/play.png");
            ui->playButton->setIcon(icon);
            ui->playButton->setToolTip("Play");
            pause=true;
            if (activeEngine)
                activeEngine->pause();
            if (fadingEngine)
                fadingEngine->pause();
        }
        else
        {
            QIcon icon(":/pics/pics/pause.png");
            ui->playButton->setIcon(icon);
            ui->playButton->setToolTip("Pause");
            pause=false;
            if (activeEngine && activeEngine->state()==VlcEngine::Paused)
                activeEngine->resume();
            else
                playTrack();
            updateQueueWidget();
        }
    }
}

void ZenPlayer::on_equalizerButton_clicked()
{
    equalizerDialog d(this,equalizerPresetIndex);
    d.setCustomValues(equalizerCustomValues);
    if(d.exec()==QDialog::Accepted)
    {
        equalizerPresetIndex=d.ui->presetComboBox->currentIndex();
        if(equalizerPresetIndex==10)
        {
            equalizerCustomValues.clear();
            equalizerCustomValues.push_back(d.ui->preampSlider->value());
            equalizerCustomValues.push_back(d.ui->hz60Slider->value());
            equalizerCustomValues.push_back(d.ui->hz170Slider->value());
            equalizerCustomValues.push_back(d.ui->hz310Slider->value());
            equalizerCustomValues.push_back(d.ui->hz600Slider->value());
            equalizerCustomValues.push_back(d.ui->hz1kSlider->value());
            equalizerCustomValues.push_back(d.ui->hz3kSlider->value());
            equalizerCustomValues.push_back(d.ui->hz6kSlider->value());
            equalizerCustomValues.push_back(d.ui->hz12kSlider->value());
            equalizerCustomValues.push_back(d.ui->hz14kSlider->value());
            equalizerCustomValues.push_back(d.ui->hz16kSlider->value());
        }
        switch (equalizerPresetIndex)
        {
        case 0: ui->equalizerlabel->setText("Flat"); break;
        case 1: ui->equalizerlabel->setText("Headphones"); break;
        case 2: ui->equalizerlabel->setText("Laptop"); break;
        case 3: ui->equalizerlabel->setText("Speakers"); break;
        case 4: ui->equalizerlabel->setText("Bass Boost"); break;
        case 5: ui->equalizerlabel->setText("Treble Boost"); break;
        case 6: ui->equalizerlabel->setText("Bass & Treble Boost"); break;
        case 7: ui->equalizerlabel->setText("Pop"); break;
        case 8: ui->equalizerlabel->setText("Rock"); break;
        case 9: ui->equalizerlabel->setText("Classical"); break;
        case 10: ui->equalizerlabel->setText("Custom"); break;
        }
        equalizerCurrentValues.clear();
        equalizerCurrentValues.push_back(d.ui->preampSlider->value());
        equalizerCurrentValues.push_back(d.ui->hz60Slider->value());
        equalizerCurrentValues.push_back(d.ui->hz170Slider->value());
        equalizerCurrentValues.push_back(d.ui->hz310Slider->value());
        equalizerCurrentValues.push_back(d.ui->hz600Slider->value());
        equalizerCurrentValues.push_back(d.ui->hz1kSlider->value());
        equalizerCurrentValues.push_back(d.ui->hz3kSlider->value());
        equalizerCurrentValues.push_back(d.ui->hz6kSlider->value());
        equalizerCurrentValues.push_back(d.ui->hz12kSlider->value());
        equalizerCurrentValues.push_back(d.ui->hz14kSlider->value());
        equalizerCurrentValues.push_back(d.ui->hz16kSlider->value());
        applyEqualizerToVlc();
    }
}
void ZenPlayer::on_crossfadeSlider_valueChanged(int value)
{
    ui->crossfadeValueLabel->setText(QString::number(value/10.0, 'f', 1)+"s");
}

//save and load
void ZenPlayer::saveData()
{
    std::vector<std::string> temp;
    for(const auto& folder:folderPaths)
        temp.push_back(folder.toStdString());
    data["folders"]=temp;
    data["volume"]=std::to_string(volume);
    data["equalizerPresetIndex"]=std::to_string(equalizerPresetIndex);
    temp.clear();
    for(const auto &v : equalizerCustomValues)
        temp.push_back(std::to_string(v));
    data["equalizerCustomValues"]=temp;
    data["crossfadeValue"]=ui->crossfadeSlider->value();
    data["showRemainingTime"]=showRemainingTime;
    
    // Save current queue and index
    temp.clear();
    for(const auto& track:playQueue)
        temp.push_back(track.toStdString());
    data["queue"]=temp;
    data["queueIndex"]=currentQueueIndex;

    // Save active folder/playlist context
    data["activeTab"]=ui->tabWidget->currentIndex();
    data["sortIndex"]=ui->sortComboBox->currentIndex();
    data["orderIndex"]=ui->orderComboBox->currentIndex();
    if (ui->tabWidget->currentIndex()==0)
    {
        data["activeFolderRow"]=ui->foldersListWidget->currentRow();
        data["activePlaylistRow"]=-1;
    }
    else
    {
        data["activeFolderRow"]=-1;
        data["activePlaylistRow"]=ui->playlistListWidget->currentRow();
    }

    QString appDataPath=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appDataPath);
    QString dataJsonPath=appDataPath+"/data.json";

	std::ofstream file(dataJsonPath.toStdString());
    if(file.is_open())
        file<<data.dump(4);
    else
        qDebug()<<"There was a problem in saving data";
}
void ZenPlayer::loadData()
{
    QString appDataPath=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString dataJsonPath=appDataPath+"/data.json";
    std::ifstream file(dataJsonPath.toStdString());
    if(file.is_open())
    {
        try 
        {
            file>>data;
            file.close();

            if (data.contains("volume")) 
            {
                std::string tempVolume=data["volume"];
                volume=std::stoi(tempVolume);
            }
            ui->volumeLabel->setText(QString::number(volume));
            QSignalBlocker blocker(ui->volumeSlider);
            ui->volumeSlider->setValue(volume);
            if(data.contains("equalizerPresetIndex"))
            {
                std::string temp=data["equalizerPresetIndex"];
                equalizerPresetIndex=std::stoi(temp);
                switch (equalizerPresetIndex)
                {
                case 0: ui->equalizerlabel->setText("Flat"); break;
                case 1: ui->equalizerlabel->setText("Headphones"); break;
                case 2: ui->equalizerlabel->setText("Laptop"); break;
                case 3: ui->equalizerlabel->setText("Speakers"); break;
                case 4: ui->equalizerlabel->setText("Bass Boost"); break;
                case 5: ui->equalizerlabel->setText("Treble Boost"); break;
                case 6: ui->equalizerlabel->setText("Bass & Treble Boost"); break;
                case 7: ui->equalizerlabel->setText("Pop"); break;
                case 8: ui->equalizerlabel->setText("Rock"); break;
                case 9: ui->equalizerlabel->setText("Classical"); break;
                case 10: ui->equalizerlabel->setText("Custom"); break;
                }
            }
            if(data.contains("equalizerCustomValues") && data["equalizerCustomValues"].is_array())
            {
                equalizerCustomValues.clear();
                for(const std::string& value:data["equalizerCustomValues"])
                    equalizerCustomValues.push_back(std::stoi(value));
            }
            if (data.contains("showRemainingTime"))
            {
                showRemainingTime=data["showRemainingTime"];
                updateMaxTimeLabel();
            }
            if (data.contains("crossfadeValue"))
            {
                int val=data["crossfadeValue"];
                QSignalBlocker blocker(ui->crossfadeSlider);
                ui->crossfadeSlider->setValue(val);
                on_crossfadeSlider_valueChanged(val);
            }

            if (data.contains("folders") && data["folders"].is_array()) 
            {
                std::vector<std::string> temp=data["folders"];
                for(const auto& folder:temp)
                {
                    QString folderpath=QString::fromStdString(folder);
                    folderPaths.append(folderpath);
                    QString foldername=folderpath.section('/',-1);
                    ui->foldersListWidget->addItem(foldername);
                }
            }
            
            if (data.contains("queue") && data["queue"].is_array())
            {
                playQueue.clear();
                for(const auto& track:data["queue"])
                    playQueue.append(QString::fromStdString(track));
                originalQueue=playQueue;
            }
            if (data.contains("queueIndex"))
            {
                currentQueueIndex=data["queueIndex"];
                if (currentQueueIndex>=0 && currentQueueIndex<playQueue.size())
                {
                    currentTrackPath=playQueue.at(currentQueueIndex);
                    updateQueueWidget();
                    if (metaPlayer)
                    {
                        metaPlayer->setSource(QUrl::fromLocalFile(currentTrackPath));
                        handleMetadataChanged();
                    }
                }
            }
            applyEqualizerToVlc();

            if (data.contains("sortIndex"))
            {
                QSignalBlocker blocker(ui->sortComboBox);
                ui->sortComboBox->setCurrentIndex(data["sortIndex"]);
            }
            if (data.contains("orderIndex"))
            {
                QSignalBlocker blocker(ui->orderComboBox);
                ui->orderComboBox->setCurrentIndex(data["orderIndex"]);
            }
            if (data.contains("playlists") && data["playlists"].is_array()) {
                for(const auto& playlist:data["playlists"])
                {
                    QString playlistname=QString::fromStdString(playlist["name"]);
                    ui->playlistListWidget->addItem(playlistname);
                }
            }

            // Restore active folder/playlist and set lists
            if (data.contains("activeTab"))
            {
                int tab=data["activeTab"];
                ui->tabWidget->setCurrentIndex(tab);
                if (tab==0 && data.contains("activeFolderRow"))
                {
                    int row=data["activeFolderRow"];
                    if (row>=0 && row<ui->foldersListWidget->count())
                    {
                        ui->foldersListWidget->setCurrentRow(row);
                        on_foldersListWidget_itemClicked(ui->foldersListWidget->item(row));
                    }
                }
                else if (tab==1 && data.contains("activePlaylistRow"))
                {
                    int row=data["activePlaylistRow"];
                    if (row>=0 && row<ui->playlistListWidget->count())
                    {
                        ui->playlistListWidget->setCurrentRow(row);
                        on_playlistListWidget_itemClicked(ui->playlistListWidget->item(row));
                    }
                }
            }
        } 
        catch (...) 
        {
            qDebug() << "Error parsing data.json";
        }
	}
	else
		qDebug()<<"No existing configuration found (normal for first run)";
}

//folders functions
void ZenPlayer::on_addFolderButton_clicked()
{
    QString folderpath=QFileDialog::getExistingDirectory(this,"Select a folder");
    if (folderpath.isEmpty() || folderPaths.contains(folderpath))
    {
        QMessageBox msgBox(this);
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.setText("<b>Error:</b> Duplicated folder");
        msgBox.setInformativeText("You can't add an existing folder");
        msgBox.exec();
        return;
    }
    folderPaths.append(folderpath);
	QString foldername=folderpath.section('/',-1);
	ui->foldersListWidget->addItem(foldername);
}
void ZenPlayer::on_foldersListWidget_itemClicked(QListWidgetItem* item)
{
    isFolder=true;
	int index=ui->foldersListWidget->row(item);
	QString folderpath=folderPaths.at(index);
	QDir directory(folderpath);
	QStringList musicFiles=directory.entryList(QStringList()<<"*.mp3"<<"*.wav"<<"*.flac",QDir::Files);

    trackPaths.clear();
	for(const auto& file:musicFiles)
	{
		QString fullPath=folderpath+'/'+file;
		trackPaths.append(fullPath);
	}
	ui->tracksListWidget->clear();
	for(const auto& file:musicFiles)
	{
		QFileInfo fileInfo(file);
		ui->tracksListWidget->addItem(fileInfo.completeBaseName());
	}
	sortTracks();
}
void ZenPlayer::showFoldersContextMenu(const QPoint &pos)
{
    QListWidgetItem* item=ui->foldersListWidget->itemAt(pos);
    if (!item) 
        return;
    ui->foldersListWidget->setCurrentItem(item);

    QMenu menu(this);
    QAction* removeAction=menu.addAction("Remove Folder");
    QAction* selectedAction=menu.exec(QCursor::pos());
    if (selectedAction==removeAction)
    {
        int index=ui->foldersListWidget->row(item);
        folderPaths.removeAt(index);
        ui->foldersListWidget->takeItem(index);
        ui->tracksListWidget->clear();
        trackPaths.clear();
    }
}

//playlist functions
void ZenPlayer::on_addPlaylistButton_clicked()
{
    createPlaylistDialog d;
    if(d.exec()==QDialog::Accepted) 
    {
        std::string name=d.getPlaylistName().toStdString();
        if (name.empty())
        {
            QMessageBox msgBox(this);
            msgBox.setIcon(QMessageBox::Critical);
            msgBox.setText("<b>Error:</b> Empty name");
            msgBox.setInformativeText("You can't add a playlist with empty name");
            msgBox.exec();
            return;
        }
        if(!data.contains("playlists")||!data["playlists"].is_array())
            data["playlists"]=json::array();
        for (const auto& p : data["playlists"]) 
        {
            if (p.contains("name") && p["name"] == name)
            {
                QMessageBox msgBox(this);
                msgBox.setIcon(QMessageBox::Critical);
                msgBox.setText("<b>Error:</b> Duplicated playlist");
                msgBox.setInformativeText("You can't add an existing playlist");
                msgBox.exec();
                return;
            }
        }
        json newPlaylist;
        newPlaylist["name"]=name;
        newPlaylist["tracks"]=json::array();
        data["playlists"].push_back(newPlaylist);

        ui->playlistListWidget->clear();
        for(const auto& playlist:data["playlists"])
        {
            QString playlistname=QString::fromStdString(playlist["name"]);
            ui->playlistListWidget->addItem(playlistname);
        }
    }
}
void ZenPlayer::on_playlistListWidget_itemClicked(QListWidgetItem* item)
{
    isFolder=false;
    int index=ui->playlistListWidget->row(item);
	ui->tracksListWidget->clear();
	trackPaths.clear();
	for(const auto& track:data["playlists"][index]["tracks"])
	{
		QString trackpath=QString::fromStdString(track);
		trackPaths.append(trackpath);
		QString trackname=trackpath.section('/',-1);
		QFileInfo fileInfo(trackname);
		ui->tracksListWidget->addItem(fileInfo.completeBaseName());
	}
	sortTracks();
}
void ZenPlayer::showPlaylistsContextMenu(const QPoint &pos)
{
    QListWidgetItem* item=ui->playlistListWidget->itemAt(pos);
    if (!item) 
        return;
    ui->playlistListWidget->setCurrentItem(item);
    QMenu menu(this);
    QAction* removeAction=menu.addAction("Remove Playlist");

    QAction* selectedAction=menu.exec(QCursor::pos());
    if (selectedAction==removeAction)
    {
        auto& playlists=data["playlists"];
        auto it=std::remove_if(playlists.begin(),playlists.end(),[&](const json& playlist) {return playlist["name"]==item->text().toStdString();});
        playlists.erase(it,playlists.end());
        ui->playlistListWidget->clear();
        for(const auto& playlist:data["playlists"])
        {
            QString playlistname=QString::fromStdString(playlist["name"]);
            ui->playlistListWidget->addItem(playlistname);
        }
    }
}

//tracks functions
void ZenPlayer::on_tracksListWidget_itemDoubleClicked(QListWidgetItem* item)
{
    int index=ui->tracksListWidget->row(item);
    if (index>=0 && index<trackPaths.size())
    {
        buildQueueFromCurrentTracks();
        
        currentTrackPath=trackPaths.at(index);
        currentQueueIndex=playQueue.indexOf(currentTrackPath);
        playTrack();
        updateQueueWidget();
        
        QIcon icon(":/pics/pics/pause.png");
        ui->playButton->setIcon(icon);
        ui->playButton->setToolTip("Pause");
        pause=false;
    }
}
void ZenPlayer::showTracksContextMenu(const QPoint &pos)
{
    QListWidgetItem* item=ui->tracksListWidget->itemAt(pos);
    if (!item) 
        return;
    ui->tracksListWidget->setCurrentItem(item);
    QMenu menu(this);
    if (isFolder)
    {
        QMenu* addToPlaylistMenu=menu.addMenu("Add to Playlist");
        QList<QAction*> playlistActions;
        for (const auto& p : data["playlists"])
        {
            QString name=QString::fromStdString(p["name"]);
            QAction* action=addToPlaylistMenu->addAction(name);
            playlistActions.append(action);
        }

        QAction* selectedAction=menu.exec(QCursor::pos());
        if (selectedAction)
        {
            int playlistIndex=playlistActions.indexOf(selectedAction);
            if (playlistIndex!=-1)
            {
                int trackIndex=ui->tracksListWidget->row(item);
                QString trackpath=trackPaths.at(trackIndex);
                auto& tracks=data["playlists"][playlistIndex]["tracks"];
                if (std::find(tracks.begin(), tracks.end(), trackpath.toStdString())==tracks.end())
                    tracks.push_back(trackpath.toStdString());
            }
        }
    }
    else
    {
        QAction* removeAction=menu.addAction("Remove Track");
        QAction* selectedAction=menu.exec(QCursor::pos());
        if (selectedAction==removeAction)
        {
            int index=ui->tracksListWidget->row(item);
            auto& playlists=data["playlists"];
            auto& tracks=playlists[ui->playlistListWidget->currentRow()]["tracks"];
            auto it=std::remove_if(tracks.begin(), tracks.end(), [&](const json& track) {return track==trackPaths.at(index).toStdString(); });
            tracks.erase(it, tracks.end());
            ui->tracksListWidget->takeItem(index);
            trackPaths.removeAt(index);
        }
    }
}

// search function
void ZenPlayer::on_searchPushButton_clicked()
{
    QString query=ui->searchLineEdit->text().trimmed();
    searchTrackPaths.clear();

    QSet<QString> candidateSet;
    
    for (const auto& folderpath : folderPaths)
    {
        QDir directory(folderpath);
        QStringList musicFiles=directory.entryList(QStringList()<<"*.mp3"<<"*.wav"<<"*.flac", QDir::Files);
        for (const auto& file : musicFiles)
        {
            QString fullPath=folderpath+'/'+file;
            candidateSet.insert(fullPath);
        }
    }

    if (data.contains("playlists") && data["playlists"].is_array())
        for (const auto& playlist : data["playlists"])
            if (playlist.contains("tracks") && playlist["tracks"].is_array())
                for (const auto& track : playlist["tracks"])
                    candidateSet.insert(QString::fromStdString(track));

    for (const auto& path : trackPaths)
        candidateSet.insert(path);

    if (!query.isEmpty())
    {
        for (const auto& trackPath : candidateSet)
        {
            QFileInfo fileInfo(trackPath);
            if (fileInfo.completeBaseName().contains(query, Qt::CaseInsensitive) || trackPath.contains(query, Qt::CaseInsensitive))
                searchTrackPaths.append(trackPath);
        }
        trackPaths=searchTrackPaths;
    }
    else
    {
        if (ui->tabWidget->currentIndex()==0) //folders tab
        {
            QListWidgetItem* currentItem=ui->foldersListWidget->currentItem();
            if (currentItem)
            {
                int index=ui->foldersListWidget->row(currentItem);
                QString folderpath=folderPaths.at(index);
                QDir directory(folderpath);
                QStringList musicFiles=directory.entryList(QStringList()<<"*.mp3"<<"*.wav"<<"*.flac",QDir::Files);
                trackPaths.clear();
                for(const auto& file:musicFiles)
                {
                    QString fullPath=folderpath+'/'+file;
                    trackPaths.append(fullPath);
                }
            }
        }
        else if (ui->tabWidget->currentIndex()==1) //playlists tab
        {
            QListWidgetItem* currentItem=ui->playlistListWidget->currentItem();
            if (currentItem)
            {
                int index=ui->playlistListWidget->row(currentItem);
                trackPaths.clear();
                for(const auto& track:data["playlists"][index]["tracks"])
                    trackPaths.append(QString::fromStdString(track));
            }
        }
    }

    ui->tracksListWidget->clear();
    for (const auto& path : trackPaths)
    {
        QFileInfo fileInfo(path);
        ui->tracksListWidget->addItem(fileInfo.completeBaseName());
    }
    sortTracks();
}

//tab function
void ZenPlayer::on_tabWidget_currentChanged(int index)
{
    if (index==0) //folders tab
    {
        QListWidgetItem* currentItem=ui->foldersListWidget->currentItem();
        if (currentItem)
            on_foldersListWidget_itemClicked(currentItem);
        else
        {
            ui->tracksListWidget->clear();
            trackPaths.clear();
        }
    }
    else if (index==1) //playlists tab
    {
        QListWidgetItem* currentItem=ui->playlistListWidget->currentItem();
        if (currentItem)
            on_playlistListWidget_itemClicked(currentItem);
        else
        {
            ui->tracksListWidget->clear();
            trackPaths.clear();
        }
    }
}

//playing functions
void ZenPlayer::cancelCrossfade()
{
    if (crossfadeTimer && crossfadeTimer->isActive())
        crossfadeTimer->stop();

    if (fadingEngine)
    {
        fadingEngine->stop();
        fadingEngine->setSoftwareVolume(0.0f);
        fadingEngine=nullptr;
    }

    if (activeEngine)
        activeEngine->setSoftwareVolume(1.0f);

    isCrossfading=false;
    crossfadeTriggered=false;
}

void ZenPlayer::playTrack()
{
    if (!activeEngine) 
        return;

    cancelCrossfade();
    if (!currentTrackPath.isEmpty())
    {
        if (metaPlayer)
            metaPlayer->setSource(QUrl::fromLocalFile(currentTrackPath));
        activeEngine->setSoftwareVolume(1.0f);
        activeEngine->play(currentTrackPath);
        applyEqualizerToVlc(activeEngine);
    }
}
void ZenPlayer::handleMetadataChanged()
{
    if (!metaPlayer)
        return;

    QMediaMetaData metadata=metaPlayer->metaData();
    QString title=metadata.value(QMediaMetaData::Title).toString();
    QString artist=metadata.value(QMediaMetaData::Author).toString();
    if (artist.isEmpty())
        artist=metadata.value(QMediaMetaData::ContributingArtist).toString();

    if (title.isEmpty())
        title=QFileInfo(metaPlayer->source().toLocalFile()).completeBaseName();
    
    QString displayText=title;
    if (!artist.isEmpty())
        displayText+=" - "+artist;
        
    ui->trackInfoLabel->setText(displayText);

    QVariant coverArtVariant=metadata.value(QMediaMetaData::CoverArtImage);
    if (!coverArtVariant.isValid() || coverArtVariant.isNull())
        coverArtVariant=metadata.value(QMediaMetaData::ThumbnailImage);
        
    if (coverArtVariant.isValid())
    {
        QImage coverImg;
        if (coverArtVariant.canConvert<QImage>())
            coverImg=coverArtVariant.value<QImage>();
        else if (coverArtVariant.canConvert<QPixmap>())
            coverImg=coverArtVariant.value<QPixmap>().toImage();

        if (!coverImg.isNull())
        {
            ui->trackPicLabel->setStyleSheet("");
            QPixmap pm=QPixmap::fromImage(coverImg);
            QPixmap roundedPm=getRoundedPixmap(pm, 15);
            ui->trackPicLabel->setPixmap(roundedPm);
            ui->trackPicLabel->setAlignment(Qt::AlignCenter);
            return;
        }
    }
    
    setDefaultTrackPic();
}

void ZenPlayer::onVlcStateChanged(VlcEngine::State newState)
{
    VlcEngine* senderEng=qobject_cast<VlcEngine*>(sender());
    if (senderEng && senderEng != activeEngine)
        return;

    if (newState==VlcEngine::Ended)
    {
        if (isCrossfading)
            return;

        if (repeat)
            playTrack();
        else
            on_nextButton_clicked();
    }
}

void ZenPlayer::applyEqualizerToVlc(VlcEngine* targetEngine)
{
    auto applyTo=[this](VlcEngine* eng) 
    {
        if (!eng) 
            return;
        if (equalizerPresetIndex==0) 
        {
            eng->resetEqualizer();
            return;
        }
        if (equalizerCurrentValues.size() >= 11) 
        {
            float preampDb=equalizerCurrentValues[0]/10.0f;
            float bandDbs[10];
            for (int i=0; i<10; i++)
                bandDbs[i]=equalizerCurrentValues[i+1]/10.0f;
            eng->applyEqualizer(preampDb, bandDbs, 10);
        }
    };

    if (targetEngine)
        applyTo(targetEngine);
    else 
    {
        applyTo(vlcEngine);
        applyTo(vlcEngine2);
    }
}

void ZenPlayer::checkTriggerCrossfade(qint64 positionMs)
{
    if (crossfadeTriggered || isCrossfading)
        return;

    int sliderVal=ui->crossfadeSlider->value();
    if (sliderVal <= 0)
        return;

    if (!activeEngine || !activeEngine->isPlaying())
        return;

    qint64 durMs=activeEngine->duration();
    if (durMs <= 1000)
        return;

    qint64 requestedFadeMs=sliderVal*100;
    qint64 fadeMs=qMin(requestedFadeMs, durMs/2);
    qint64 remainingMs=durMs-positionMs;

    if (remainingMs>0 && remainingMs<=fadeMs)
    {
        if (playQueue.isEmpty())
            return;

        int nextIndex=-1;
        if (repeat)
            nextIndex=currentQueueIndex;
        else
        {
            if (currentQueueIndex+1 < playQueue.size())
                nextIndex=currentQueueIndex+1;
            else
                nextIndex=0;
        }

        if (nextIndex<0 || nextIndex>=playQueue.size())
            return;

        startCrossfadeTo(nextIndex, fadeMs);
    }
}

void ZenPlayer::startCrossfadeTo(int nextIndex, qint64 fadeMs)
{
    crossfadeTriggered=true;
    isCrossfading=true;
    crossfadeDurationMs=static_cast<int>(fadeMs);
    crossfadeElapsedMs=0;
    fadingEngine=activeEngine;
    activeEngine=(activeEngine==vlcEngine) ? vlcEngine2 : vlcEngine;
    fadingEngine->setSoftwareVolume(1.0f);
    activeEngine->setSoftwareVolume(0.0f);
    activeEngine->setVolume(volume);
    applyEqualizerToVlc(activeEngine);
    currentQueueIndex=nextIndex;
    currentTrackPath=playQueue[currentQueueIndex];
    updateQueueWidget();
    if (currentQueueIndex>=0 && currentQueueIndex < ui->queueListWidget->count())
        ui->queueListWidget->setCurrentRow(currentQueueIndex);

    if (metaPlayer)
        metaPlayer->setSource(QUrl::fromLocalFile(currentTrackPath));

    activeEngine->play(currentTrackPath);
    crossfadeTimer->start(20);
}

void ZenPlayer::onCrossfadeTimerTimeout()
{
    if (!isCrossfading || !fadingEngine || !activeEngine)
    {
        crossfadeTimer->stop();
        isCrossfading=false;
        return;
    }

    crossfadeElapsedMs+=20;
    float progress=static_cast<float>(crossfadeElapsedMs)/crossfadeDurationMs;

    if (progress >= 1.0f)
    {
        progress=1.0f;
        fadingEngine->setSoftwareVolume(0.0f);
        fadingEngine->stop();
        activeEngine->setSoftwareVolume(1.0f);
        crossfadeTimer->stop();
        isCrossfading=false;
        fadingEngine=nullptr;
        crossfadeTriggered=false;
    }
    else
    {
        fadingEngine->setSoftwareVolume(1.0f - progress);
        activeEngine->setSoftwareVolume(progress);
    }
}

void ZenPlayer::setDefaultTrackPic()
{
    ui->trackPicLabel->setStyleSheet(
        "QLabel "
        "{"
        "  border: 2px #555555;"
        "  border-radius: 15px;"
        "  background-color: #2b2b2b;"
        "}"
    );
    ui->trackPicLabel->setPixmap(QPixmap());
}
QPixmap ZenPlayer::getRoundedPixmap(const QPixmap& src, int radius)
{
    if (src.isNull()) 
        return src;
    QSize labelSize=ui->trackPicLabel->size();
    if (labelSize.width()<=0 || labelSize.height()<=0)
        labelSize=QSize(200, 200);
    QPixmap scaledSrc=src.scaled(labelSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    
    QPixmap cropped(labelSize);
    cropped.fill(Qt::transparent);
    QPainter cropper(&cropped);
    int xOffset=(scaledSrc.width()-labelSize.width())/2;
    int yOffset=(scaledSrc.height()-labelSize.height())/2;
    cropper.drawPixmap(0, 0, scaledSrc, xOffset, yOffset, labelSize.width(), labelSize.height());
    cropper.end();

    QPixmap target(labelSize);
    target.fill(Qt::transparent);

    QPainter painter(&target);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QPainterPath path;
    path.addRoundedRect(QRectF(0, 0, labelSize.width(), labelSize.height()), radius, radius);
    painter.setClipPath(path);
    painter.drawPixmap(0, 0, cropped);
    painter.end();

    return target;
}
void ZenPlayer::on_positionChanged(qint64 position)
{
    VlcEngine* senderEng=qobject_cast<VlcEngine*>(sender());
    if (senderEng && senderEng != activeEngine)
        return;

    if (!ui->timeSlider->isSliderDown())
        ui->timeSlider->setValue(position);
    ui->currentTimeLabel->setText(formatTime(position));
    if (showRemainingTime)
        updateMaxTimeLabel();

    checkTriggerCrossfade(position);
}
void ZenPlayer::on_durationChanged(qint64 duration)
{
    VlcEngine* senderEng=qobject_cast<VlcEngine*>(sender());
    if (senderEng && senderEng != activeEngine)
        return;

    ui->timeSlider->setRange(0, duration);
    updateMaxTimeLabel();
}
void ZenPlayer::on_timeSlider_sliderMoved(int position)
{
    if (activeEngine)
        activeEngine->setPosition(position);
    ui->currentTimeLabel->setText(formatTime(position));
    if (showRemainingTime)
        updateMaxTimeLabel();
}

void ZenPlayer::on_sortComboBox_currentIndexChanged() { sortTracks(); }
void ZenPlayer::on_orderComboBox_currentIndexChanged() { sortTracks(); }

void ZenPlayer::sortTracks()
{
    int index=ui->sortComboBox->currentIndex();
    int Oindex=ui->orderComboBox->currentIndex();
    if (index<0 || trackPaths.isEmpty())
        return;

    QList<QPair<QString, QString>> pairs; // <sortKey, trackPath>
    for (const auto &path : trackPaths)
    {
        QString key;
        QFileInfo fi(path);
        if (index==0) // A-Z (Title)
            key=fi.completeBaseName();
        else if (index==1) // Date added (File modification time)
            key=fi.lastModified().toString(Qt::ISODateWithMs);
        pairs.append({key.toLower(), path});
    }

    if(Oindex==0)
        std::sort(pairs.begin(), pairs.end(), [](const QPair<QString, QString> &a, const QPair<QString, QString> &b) {return a.first<b.first;});
    else if(Oindex==1)
        std::sort(pairs.begin(), pairs.end(), [](const QPair<QString, QString> &a, const QPair<QString, QString> &b) {return a.first>b.first;});

    trackPaths.clear();
    ui->tracksListWidget->clear();
    for (const auto &p : pairs)
    {
        trackPaths.append(p.second);
        QFileInfo fi(p.second);
        ui->tracksListWidget->addItem(fi.completeBaseName());
    }
}
QString ZenPlayer::formatTime(qint64 ms)
{
    qint64 totalSeconds=ms/1000;
    qint64 seconds=totalSeconds%60;
    qint64 minutes=(totalSeconds/60)%60;
    qint64 hours=totalSeconds/3600;

    if (hours>0)
    {
        return QString("%1:%2:%3")
            .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    }
    else
    {
        return QString("%1:%2")
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    }
}

//queue functions
void ZenPlayer::buildQueueFromCurrentTracks()
{
    originalQueue=trackPaths;
    playQueue=originalQueue;

    if (shuffle && !playQueue.isEmpty())
    {
        QString activeTrack=currentTrackPath;
        playQueue.removeAll(activeTrack);

        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(playQueue.begin(), playQueue.end(), g);

        if (!activeTrack.isEmpty())
        {
            playQueue.prepend(activeTrack);
            currentQueueIndex=0;
        }
    }
    else
    {
        if (!currentTrackPath.isEmpty())
            currentQueueIndex=playQueue.indexOf(currentTrackPath);
        else
            currentQueueIndex=-1;
    }
    updateQueueWidget();
}
void ZenPlayer::updateQueueWidget()
{
    QSignalBlocker blocker(ui->queueListWidget);
    ui->queueListWidget->clear();
    for (int i=0; i<playQueue.size(); ++i)
    {
        QString trackPath=playQueue.at(i);
        QString trackName=QFileInfo(trackPath).completeBaseName();
        
        QListWidgetItem* item=new QListWidgetItem(trackName);
        if (i==currentQueueIndex)
        {
            QFont font=item->font();
            font.setBold(true);
            item->setFont(font);
            item->setText("▶ "+trackName);
            item->setForeground(QBrush(QColor("#1DB954")));
        }
        else if (i<currentQueueIndex)
            item->setForeground(QBrush(QColor("#777777"))); // History
        else
            item->setForeground(QBrush(QColor("#FFFFFF"))); // Upcoming
        ui->queueListWidget->addItem(item);
    }

    if (currentQueueIndex>=0 && currentQueueIndex<ui->queueListWidget->count())
        ui->queueListWidget->setCurrentRow(currentQueueIndex);

    bool canNavigate=(playQueue.size()>1);
    ui->nextButton->setEnabled(canNavigate);
    ui->previousButton->setEnabled(canNavigate);
}
void ZenPlayer::playTrackAtIndex(int index)
{
    if (index>=0 && index<playQueue.size())
    {
        cancelCrossfade();
        currentQueueIndex=index;
        currentTrackPath=playQueue.at(index);
        playTrack();
        updateQueueWidget();
        QIcon icon(":/pics/pics/pause.png");
        ui->playButton->setIcon(icon);
        ui->playButton->setToolTip("Pause");
        pause=false;
    }
}
void ZenPlayer::on_queueListWidget_itemClicked(QListWidgetItem* item)
{
    int index=ui->queueListWidget->row(item);
    qDebug()<<index;
    playTrackAtIndex(index);
}

void ZenPlayer::showQueueContextMenu(const QPoint &pos)
{
    QListWidgetItem* item=ui->queueListWidget->itemAt(pos);
    if (!item) 
        return;
    ui->queueListWidget->setCurrentItem(item);
    QMenu menu(this);
    QAction* removeAction=menu.addAction("Remove from Queue");
    QAction* selectedAction=menu.exec(QCursor::pos());
    if (selectedAction==removeAction)
    {
        int index=ui->queueListWidget->row(item);
        if (index>=0 && index<playQueue.size())
        {
            QString removedTrack=playQueue.at(index);
            playQueue.removeAt(index);
            originalQueue.removeOne(removedTrack);
            if (playQueue.isEmpty())
            {
                currentQueueIndex=-1;
                currentTrackPath="";
            }
            else
            {
                if (index<currentQueueIndex)
                    currentQueueIndex--;
                else if (index==currentQueueIndex)
                {
                    if (currentQueueIndex>=playQueue.size())
                        currentQueueIndex=playQueue.size()-1;
                    currentTrackPath=playQueue.at(currentQueueIndex);
                }
            }
            updateQueueWidget();
        }
    }
}

void ZenPlayer::updateMaxTimeLabel()
{
    qint64 duration=ui->timeSlider->maximum();
    if (showRemainingTime)
    {
        qint64 position=ui->timeSlider->value();
        qint64 remaining=qMax<qint64>(0, duration-position);
        ui->maxTimeLabel->setText("-" + formatTime(remaining));
    }
    else
        ui->maxTimeLabel->setText(formatTime(duration));
}