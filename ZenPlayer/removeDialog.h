#pragma once
#include <ui_removeDialog.h>
#include <QDialog>	

QT_BEGIN_NAMESPACE
namespace Ui { class removeDialog; };
QT_END_NAMESPACE

class removeDialog : public QDialog
{
	Q_OBJECT
public:
	removeDialog(QString text,QString title,QWidget* parent=nullptr) : QDialog(parent),ui(new Ui::removeDialog())
	{
		ui->setupUi(this);
		setWindowTitle(title);
		ui->label->setText(text);
		connect(ui->yesButton,&QPushButton::clicked,this,&removeDialog::accepted);
		connect(ui->NoButton,&QPushButton::clicked,this,&removeDialog::rejected);
	}
	~removeDialog()
	{
		delete ui;
	}

private:
	Ui::removeDialog* ui;
};