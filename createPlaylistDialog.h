#pragma once
#include <ui_createPlaylistDialog.h>
#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui { class createPlaylistDialog; };
QT_END_NAMESPACE

class createPlaylistDialog : public QDialog
{
	Q_OBJECT
public:
	createPlaylistDialog(QWidget* parent=nullptr) : QDialog(parent),ui(new Ui::createPlaylistDialog())
	{
		ui->setupUi(this);
		setWindowTitle("Choose a name for your playlist");
		connect(ui->okButton,&QPushButton::clicked,this,&createPlaylistDialog::accepted);
		connect(ui->cancelButton,&QPushButton::clicked,this,&createPlaylistDialog::rejected);
	}
	~createPlaylistDialog()
	{
		delete ui;
	}
	QString getPlaylistName()
	{
		return ui->nameLineEdit->text();
	}
private:
	Ui::createPlaylistDialog* ui;
};