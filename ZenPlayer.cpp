#include "ZenPlayer.h"

ZenPlayer::ZenPlayer(QWidget *parent) : QMainWindow(parent), ui(new Ui::ZenPlayerClass())
{
    mute=false;
    repeat=false;
    shuffle=false;
    isFolder=true;
    pause=true;
    currentQueueIndex=-1;
    ui->setupUi(this);
	setWindowIcon(QIcon(":/pics/pics/icon.png"));

    player=new QMediaPlayer(this);
    audioOutput=new QAudioOutput(this);
    player->setAudioOutput(audioOutput);
    
    loadData();
    
    //Set initial volume from slider
    audioOutput->setVolume(ui->volumeSlider->value() / 100.0);

    //Set initial track info and picture states
    setDefaultTrackPic();
    ui->trackInfoLabel->setText("No Song Playing");

    //Enable custom context menus
    ui->foldersListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->playlistListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->tracksListWidget->setContextMenuPolicy(Qt::CustomContextMenu);

    //Connect context menu signals to slots
    connect(ui->foldersListWidget, &QListWidget::customContextMenuRequested, this, &ZenPlayer::showFoldersContextMenu);
    connect(ui->playlistListWidget, &QListWidget::customContextMenuRequested, this, &ZenPlayer::showPlaylistsContextMenu);
    connect(ui->tracksListWidget, &QListWidget::customContextMenuRequested, this, &ZenPlayer::showTracksContextMenu);

    //Connect QMediaPlayer signals
    connect(player, &QMediaPlayer::metaDataChanged, this, &ZenPlayer::handleMetadataChanged);
    connect(player, &QMediaPlayer::mediaStatusChanged, this, &ZenPlayer::handleMetadataChanged);
    connect(player, &QMediaPlayer::positionChanged, this, &ZenPlayer::on_positionChanged);
    connect(player, &QMediaPlayer::durationChanged, this, &ZenPlayer::on_durationChanged);
    connect(ui->timeSlider, &QSlider::sliderMoved, this, &ZenPlayer::on_timeSlider_sliderMoved);

    //Connect queueListWidget signal
    connect(ui->tracksListWidget, &QListWidget::itemDoubleClicked, this, &ZenPlayer::on_tracksListWidget_itemDoubleClicked);
}

ZenPlayer::~ZenPlayer()
{
    saveData();
    delete ui;
}

//sound/volume
void ZenPlayer::on_muteButton_clicked()
{
    if(!mute)
    {
        QIcon icon(":/pics/pics/mute.png");
        ui->muteButton->setIcon(icon);
        ui->muteButton->setToolTip("Unmute");
        mute=true;

        ui->volumeSlider->setEnabled(false);
        if (audioOutput)
            audioOutput->setMuted(true);
    }
    else
    {
        QIcon icon(":/pics/pics/sound.png");
        ui->muteButton->setIcon(icon);
        ui->muteButton->setToolTip("Mute");
        mute=false;

        ui->volumeSlider->setEnabled(true);
        if (audioOutput)
            audioOutput->setMuted(false);
    }
}
void ZenPlayer::on_volumeSlider_valueChanged(int value)
{
    volume=value;
	ui->volumeLabel->setText(QString::number(value));
    if (audioOutput)
        audioOutput->setVolume(value/100.0);
}

//controls
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
            QString activeTrack = currentTrackPath;
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

    int prevIndex=currentQueueIndex-1;
    if (prevIndex<0)
        prevIndex=playQueue.size()-1;
    playTrackAtIndex(prevIndex);
}
void ZenPlayer::on_nextButton_clicked()
{
    if (playQueue.isEmpty())
        return;

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
            if (player)
                player->pause();
        }
        else
        {
            QIcon icon(":/pics/pics/pause.png");
            ui->playButton->setIcon(icon);
            ui->playButton->setToolTip("Pause");
            pause=false;
            playTrack();
            updateQueueWidget();
        }
    }
}

//save and load
void ZenPlayer::saveData()
{
    std::vector<std::string> temp;
    for(auto& folder:folderPaths)
        temp.push_back(folder.toStdString());
    data["folders"]=temp;
    data["volume"]=std::to_string(volume);
    
    // Save current queue and index
    temp.clear();
    for(auto& track:playQueue)
        temp.push_back(track.toStdString());
    data["queue"]=temp;
    data["queueIndex"]=currentQueueIndex;

    // Save active folder/playlist context
    data["activeTab"]=ui->tabWidget->currentIndex();
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

	std::ofstream file("data.json");
    if(file.is_open())
        file<<data.dump(4);
    else
        qDebug()<<"There was a problem in saving data";
}
void ZenPlayer::loadData()
{
    std::ifstream file("data.json");
    if(file.is_open())
    {
		file>>data;
		file.close();

        std::string tempVolume=data["volume"];
        volume=std::stoi(tempVolume);
        ui->volumeLabel->setText(QString::number(volume));
        QSignalBlocker blocker(ui->volumeSlider);
        ui->volumeSlider->setValue(volume);

		std::vector<std::string> temp=data["folders"];
		for(const auto& folder:temp)
		{
			QString folderpath=QString::fromStdString(folder);
			folderPaths.append(folderpath);
			QString foldername=folderpath.section('/',-1);
			ui->foldersListWidget->addItem(foldername);
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
                if (player)
                {
                    player->setSource(QUrl::fromLocalFile(currentTrackPath));
                    handleMetadataChanged();
                }
            }
        }

        temp.clear();
        for(const auto& playlist:data["playlists"])
        {
			temp.push_back(playlist["name"]);
			QString playlistname=QString::fromStdString(playlist["name"]);
			ui->playlistListWidget->addItem(playlistname);
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
	else
		qDebug()<<"There was a problem in loading data";
}

//folders functions
void ZenPlayer::on_addFolderButton_clicked()
{
    QString folderpath=QFileDialog::getExistingDirectory(this,"Select a folder");
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
        if(!data.contains("playlists")||!data["playlists"].is_array())
            data["playlists"]=json::array();
        json newPlaylist;
        newPlaylist["name"]=d.getPlaylistName().toStdString();
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
}
void ZenPlayer::showPlaylistsContextMenu(const QPoint &pos)
{
    QListWidgetItem* item=ui->playlistListWidget->itemAt(pos);
    if (!item) return;
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
void ZenPlayer::playTrack()
{
    if (!player) 
        return;

    if (!currentTrackPath.isEmpty())
    {
        QUrl trackUrl=QUrl::fromLocalFile(currentTrackPath);
        if (player->source()!=trackUrl)
            player->setSource(trackUrl);
        player->play();
    }
}
void ZenPlayer::handleMetadataChanged()
{
    if (player->mediaStatus() == QMediaPlayer::EndOfMedia)
    {
        if (repeat)
            playTrack();
        else
            on_nextButton_clicked();
        return;
    }

    QMediaMetaData metadata=player->metaData();
    QString title=metadata.value(QMediaMetaData::Title).toString();
    QString artist=metadata.value(QMediaMetaData::Author).toString();
    if (artist.isEmpty())
        artist=metadata.value(QMediaMetaData::ContributingArtist).toString();

    if (title.isEmpty())
        title=QFileInfo(player->source().toLocalFile()).completeBaseName();
    
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
void ZenPlayer::setDefaultTrackPic()
{
    ui->trackPicLabel->setStyleSheet(
        "QLabel {"
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
    if (!ui->timeSlider->isSliderDown())
        ui->timeSlider->setValue(position);
    ui->currentTimeLabel->setText(formatTime(position));
}
void ZenPlayer::on_durationChanged(qint64 duration)
{
    ui->timeSlider->setRange(0, duration);
    ui->maxTimeLabel->setText(formatTime(duration));
}
void ZenPlayer::on_timeSlider_sliderMoved(int position)
{
    player->setPosition(position);
    ui->currentTimeLabel->setText(formatTime(position));
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
            currentQueueIndex = 0;
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


