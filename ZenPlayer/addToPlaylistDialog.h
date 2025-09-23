#pragma once
#include <ui_addToPlaylistDialog.h>
#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui { class addToPlaylistDialog; };
QT_END_NAMESPACE

class addToPlaylistDialog : public QDialog
{
	Q_OBJECT
public:
	addToPlaylistDialog(QWidget* parent=nullptr) : QDialog(parent),ui(new Ui::addToPlaylistDialog())
	{
		ui->setupUi(this);
		setWindowTitle("Choose the playlist you want to add this song to");
		connect(ui->okButton,&QPushButton::clicked,this,&addToPlaylistDialog::accepted);
		connect(ui->cancelButton,&QPushButton::clicked,this,&addToPlaylistDialog::rejected);
	}
	~addToPlaylistDialog()
	{
		delete ui;
	}
	void addPlaylists(QList<QString> playlists)
	{
		ui->playlistListWidget->addItems(playlists);
	}
private:
	Ui::addToPlaylistDialog* ui;
};