#include "ZenPlayer.h"

ZenPlayer::ZenPlayer(QWidget *parent) : QMainWindow(parent),ui(new Ui::ZenPlayerClass())
{
    mute=false;
    repeat=false;
    shuffle=false;
    isFolder=true;
    pause=true;
    ui->setupUi(this);
	setWindowIcon(QIcon("pics/play.png"));
    loadData();
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
        QIcon icon("pics/mute.png");
        ui->muteButton->setIcon(icon);
        ui->muteButton->setToolTip("Unmute");
        mute=true;
        ui->volumeSlider->setEnabled(false);
    }
    else
    {
        QIcon icon("pics/sound.png");
        ui->muteButton->setIcon(icon);
        ui->muteButton->setToolTip("Mute");
        mute=false;
        ui->volumeSlider->setEnabled(true);
    }
}
void ZenPlayer::on_volumeSlider_valueChanged(int value)
{
	ui->volumeLabel->setText(QString::number(value));
}

//controls
void ZenPlayer::on_repeatButton_clicked()
{
	if(!repeat)
	{
		QIcon icon("pics/repeat-one.png");
		ui->repeatButton->setIcon(icon);
        ui->repeatButton->setToolTip("Enable repeat all");
		repeat=true;
	}
	else
	{
		QIcon icon("pics/repeat-all.png");
		ui->repeatButton->setIcon(icon);
        ui->repeatButton->setToolTip("Enable repeat one");
		repeat=false;
	}
}
void ZenPlayer::on_shuffleButton_clicked()
{
    if(!shuffle)
    {
        QIcon icon("pics/shuffle.png");
        ui->shuffleButton->setIcon(icon);
        ui->shuffleButton->setToolTip("Disable shuffle");
        shuffle=true;
    }
    else
    {
        QIcon icon("pics/cycle.png");
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
    if(!pause)
    {
        QIcon icon("pics/play.png");
        ui->playButton->setIcon(icon);
        ui->playButton->setToolTip("Play");
        pause=true;
    }
    else
    {
        QIcon icon("pics/pause.png");
        ui->playButton->setIcon(icon);
        ui->playButton->setToolTip("Pause");
        pause=false;
    }
}

//opening folders and making playlists
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
	for(const auto& file:musicFiles)
	{
		QString fullPath=folderpath+'/'+file;
		trackPaths.append(fullPath);
	}
	ui->tracksListWidget->clear();
	ui->tracksListWidget->addItems(musicFiles);
}
void ZenPlayer::on_foldersListWidget_itemDoubleClicked(QListWidgetItem* item)
{
	int index=ui->foldersListWidget->row(item);
    removeDialog d("Do you want to remove this folder ("+item->text()+")?","Remove Folder");
    if(d.exec()==QDialog::Accepted)
    {
		folderPaths.removeAt(index);
		ui->foldersListWidget->takeItem(index);
		ui->tracksListWidget->clear();
		trackPaths.clear();
    }
}
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

}
void ZenPlayer::on_playlistListWidget_itemDoubleClicked(QListWidgetItem* item)  
{  
	removeDialog d("Do you want to remove this playlist ("+item->text()+")?","Remove Playlist");
    if(d.exec()==QDialog::Accepted)
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
void ZenPlayer::on_tracksListWidget_itemDoubleClicked(QListWidgetItem* item)
{
    if(isFolder)
    {
        addToPlaylistDialog d;
        //d.addPlaylists(playlistPaths);
        if(d.exec()==QDialog::Accepted)
        {

        }
    }
}

//playing songs
void ZenPlayer::on_tracksListWidget_itemClicked(QListWidgetItem* item)
{
	int index=ui->tracksListWidget->row(item);
	QString trackpath=trackPaths.at(index);
    /*
    needs to be replaced with player functions
	QDesktopServices::openUrl(QUrl::fromLocalFile(trackpath));
    */
}