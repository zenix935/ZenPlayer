#include "ZenPlayer.h"

ZenPlayer::ZenPlayer(QWidget *parent) : QMainWindow(parent),ui(new Ui::ZenPlayerClass())
{
    mute=false;
    repeat=false;
    shuffle=false;
    pause=true;
    ui->setupUi(this);
	setWindowIcon(QIcon("pics/play.png"));
}

ZenPlayer::~ZenPlayer()
{
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
void ZenPlayer::on_addFolderButton_clicked()
{
    QString folderpath=QFileDialog::getExistingDirectory(this,"Select a folder");
    folderPaths.append(folderpath);
	QString foldername=folderpath.section('/',-1);
	ui->foldersListWidget->addItem(foldername);
}
void ZenPlayer::on_foldersListWidget_itemClicked(QListWidgetItem* item)
{
	int index=ui->foldersListWidget->row(item);
	QString folderpath=folderPaths.at(index);
	QDir directory(folderpath);
	QStringList musicFiles=directory.entryList(QStringList()<<"*.mp3"<<"*.wav"<<"*.flac",QDir::Files);
	for(const auto& file:musicFiles)
	{
		QString fullPath=folderpath+'/'+file;
		tracksPaths.append(fullPath);
	}
	ui->tracksListWidget->clear();
	ui->tracksListWidget->addItems(musicFiles);
}
void ZenPlayer::on_foldersListWidget_itemDoubleClicked(QListWidgetItem* item)
{
	int index=ui->foldersListWidget->row(item);
    closeFolderDialog d;
    if(d.exec()==QDialog::Accepted)
    {
		folderPaths.removeAt(index);
		ui->foldersListWidget->takeItem(index);
		ui->tracksListWidget->clear();
		tracksPaths.clear();
    }
}