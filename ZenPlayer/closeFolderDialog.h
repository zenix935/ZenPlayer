#pragma once
#include <ui_closeFolderDialog.h>
#include <QDialog>	

QT_BEGIN_NAMESPACE
namespace Ui { class closeFolderDialog; };
QT_END_NAMESPACE

class closeFolderDialog : public QDialog
{
	Q_OBJECT
public:
	closeFolderDialog(QWidget* parent=nullptr) : QDialog(parent),ui(new Ui::closeFolderDialog())
	{
		ui->setupUi(this);
		setWindowTitle("Close Folder");
		connect(ui->yesButton,&QPushButton::clicked,this,&closeFolderDialog::accepted);
		connect(ui->NoButton,&QPushButton::clicked,this,&closeFolderDialog::rejected);
	}
	~closeFolderDialog()
	{
		delete ui;
	}

private:
	Ui::closeFolderDialog* ui;
};