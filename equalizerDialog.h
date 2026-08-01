#pragma once
#include <QDialog>
#include <ui_equalizerDialog.h>

QT_BEGIN_NAMESPACE
namespace Ui { class equalizerDialog; };
QT_END_NAMESPACE

class equalizerDialog : public QDialog
{
	Q_OBJECT
public:
	equalizerDialog(QWidget* parent=nullptr) : QDialog(parent), ui(new Ui::equalizerDialog())
	{
		ui->setupUi(this);
		setWindowTitle("Equalizer Settings");
		connect(ui->okButton, &QPushButton::clicked, this, &equalizerDialog::accepted);
		connect(ui->cancelButton, &QPushButton::clicked, this, &equalizerDialog::rejected);
	}
	~equalizerDialog()
	{
		delete ui;
	}
private:
	Ui::equalizerDialog* ui;
};