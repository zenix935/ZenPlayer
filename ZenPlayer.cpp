#include "ZenPlayer.h"

ZenPlayer::ZenPlayer(QWidget *parent) : QMainWindow(parent), ui(new Ui::ZenPlayerClass())
{
    mute=false;
    repeat=false;
    shuffle=false;
    isFolder=true;
    pause=true;
    ui->setupUi(this);
	setWindowIcon(QIcon(":/pics/pics/icon.png"));
    loadData();

    player = new QMediaPlayer(this);
    audioOutput = new QAudioOutput(this);
    player->setAudioOutput(audioOutput);
    
    // Set initial volume from slider
    audioOutput->setVolume(ui->volumeSlider->value() / 100.0);

    // Enable custom context menus
    ui->foldersListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->playlistListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->tracksListWidget->setContextMenuPolicy(Qt::CustomContextMenu);

    // Connect context menu signals to slots
    connect(ui->foldersListWidget, &QListWidget::customContextMenuRequested, this, &ZenPlayer::showFoldersContextMenu);
    connect(ui->playlistListWidget, &QListWidget::customContextMenuRequested, this, &ZenPlayer::showPlaylistsContextMenu);
    connect(ui->tracksListWidget, &QListWidget::customContextMenuRequested, this, &ZenPlayer::showTracksContextMenu);
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
	ui->volumeLabel->setText(QString::number(value));
    if (audioOutput)
        audioOutput->setVolume(value / 100.0);
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
    }
    else
    {
        QIcon icon(":/pics/pics/cycle.png");
        ui->shuffleButton->setIcon(icon);
        ui->shuffleButton->setToolTip("Enable shuffle");
        shuffle=false;
    }
}
void ZenPlayer::on_previousButton_clicked()
{

}
void ZenPlayer::on_nextButton_clicked()
{

}
void ZenPlayer::on_playButton_clicked()
{
    QListWidgetItem* currentItem=ui->tracksListWidget->currentItem();
    if (!currentItem) return;

    int index=ui->tracksListWidget->row(currentItem);
    if (index<0 || index>=trackPaths.size()) return;

    QString trackpath=trackPaths.at(index);
    QUrl trackUrl=QUrl::fromLocalFile(trackpath);

    // If the selected track is different from the currently loaded one, play the new one!
    if (player->source()!=trackUrl)
    {
        playTrack();
        QIcon icon(":/pics/pics/pause.png");
        ui->playButton->setIcon(icon);
        ui->playButton->setToolTip("Pause");
        pause=false;
        return;
    }

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
    }
}

//save and load
void ZenPlayer::saveData()
{
    std::vector<std::string> temp;
    for(auto& folder:folderPaths)
        temp.push_back(folder.toStdString());
    data["folders"]=temp;
    temp.clear();
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
		std::vector<std::string> temp=data["folders"];
		for(const auto& folder:temp)
		{
			QString folderpath=QString::fromStdString(folder);
			folderPaths.append(folderpath);
			QString foldername=folderpath.section('/',-1);
			ui->foldersListWidget->addItem(foldername);
		}
        temp.clear();
        for(const auto& playlist:data["playlists"])
        {
			temp.push_back(playlist["name"]);
			QString playlistname=QString::fromStdString(playlist["name"]);
			ui->playlistListWidget->addItem(playlistname);
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
    if (!item) return;
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
void ZenPlayer::on_tracksListWidget_itemClicked(QListWidgetItem* item)
{
    ui->playButton->setEnabled(true);
    int index=ui->tracksListWidget->row(item);
    QString trackpath=trackPaths.at(index);
}
void ZenPlayer::showTracksContextMenu(const QPoint &pos)
{
    QListWidgetItem* item=ui->tracksListWidget->itemAt(pos);
    if (!item) return;
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
    if (index == 0) //folders tab
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
    else if (index == 1) //playlists tab
    {
        QListWidgetItem* currentItem = ui->playlistListWidget->currentItem();
        if (currentItem)
            on_playlistListWidget_itemClicked(currentItem);
        else
        {
            ui->tracksListWidget->clear();
            trackPaths.clear();
        }
    }
}

void ZenPlayer::playTrack()
{
    if (!player) return;

    QListWidgetItem* currentItem=ui->tracksListWidget->currentItem();
    if (!currentItem) return;

    int index=ui->tracksListWidget->row(currentItem);
    if (index>=0 && index<trackPaths.size())
    {
        QString trackpath=trackPaths.at(index);
        QUrl trackUrl=QUrl::fromLocalFile(trackpath);
        
        if (player->source()!=trackUrl)
            player->setSource(trackUrl);
        player->play();
    }
}


