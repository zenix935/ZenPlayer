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
	addToPlaylistDialog(QList<QString> playlists,QWidget* parent=nullptr) : QDialog(parent),ui(new Ui::addToPlaylistDialog())
	{
		ui->setupUi(this);
		setWindowTitle("Choose the playlist you want to add this song to");
		connect(ui->okButton,&QPushButton::clicked,this,&addToPlaylistDialog::accepted);
		connect(ui->cancelButton,&QPushButton::clicked,this,&addToPlaylistDialog::rejected);
		ui->playlistListWidget->addItems(playlists);
	}
	~addToPlaylistDialog()
	{
		delete ui;
	}
	int ind=-1;
private:
	Ui::addToPlaylistDialog* ui;
private slots:
	void on_playlistListWidget_itemSelectionChanged() { ind=ui->playlistListWidget->currentRow(); }
};