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
	Ui::equalizerDialog* ui;
	std::vector<int> customValues={0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	void setCustomValues(const std::vector<int>& values)
	{
		if(values.size()==11)
			customValues=values;
		if(ui->presetComboBox->currentIndex()==18)
			setSlidersValue(customValues[0], customValues[1], customValues[2], customValues[3], customValues[4], customValues[5], customValues[6], 
							customValues[7], customValues[8], customValues[9], customValues[10]);
	}
	equalizerDialog(QWidget* parent=nullptr, int presetIndex=0) : QDialog(parent), ui(new Ui::equalizerDialog())
	{
		ui->setupUi(this);
		ui->presetComboBox->setCurrentIndex(presetIndex);
		setWindowTitle("Equalizer Settings");
		connect(ui->okButton, &QPushButton::clicked, this, &equalizerDialog::accepted);
		connect(ui->cancelButton, &QPushButton::clicked, this, &equalizerDialog::rejected);
	}
	~equalizerDialog()
	{
		delete ui;
	}
	void setSlidersValue(int preamp, int hz60, int hz170, int hz310, int hz600, int hz1k, int hz3k, int hz6k, int hz12k, int hz14k, int hz16k)
	{
		ui->preampSlider->setValue(preamp);
		ui->hz60Slider->setValue(hz60);
		ui->hz170Slider->setValue(hz170);
		ui->hz310Slider->setValue(hz310);
		ui->hz600Slider->setValue(hz600);
		ui->hz1kSlider->setValue(hz1k);
		ui->hz3kSlider->setValue(hz3k);
		ui->hz6kSlider->setValue(hz6k);
		ui->hz12kSlider->setValue(hz12k);
		ui->hz14kSlider->setValue(hz14k);
		ui->hz16kSlider->setValue(hz16k);
	}
private:
	void setSlidersActive(bool active=true)
	{
		ui->preampSlider->setEnabled(active);
		ui->hz60Slider->setEnabled(active);
		ui->hz170Slider->setEnabled(active);
		ui->hz310Slider->setEnabled(active);
		ui->hz600Slider->setEnabled(active);
		ui->hz1kSlider->setEnabled(active);
		ui->hz3kSlider->setEnabled(active);
		ui->hz6kSlider->setEnabled(active);
		ui->hz12kSlider->setEnabled(active);
		ui->hz14kSlider->setEnabled(active);
		ui->hz16kSlider->setEnabled(active);
	}

private slots:
	void on_presetComboBox_currentIndexChanged(int index)
	{
		switch (index)
		{
		case 0: // Flat
			setSlidersActive(false);
			setSlidersValue(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
			break;
		case 1: // Classical
			setSlidersActive(false);
			setSlidersValue(120, 0, 0, 0, 0, 0, 0, -72, -72, -72, -96);
			break;
		case 2: // Club
			setSlidersActive(false);
			setSlidersValue(60, 0, 0, 80, 56, 56, 56, 32, 0, 0, 0);
			break;
		case 3: //Dance
			setSlidersActive(false);
			setSlidersValue(50, 96, 72, 24, 0, 0, -56, -72, -72, 0, 0);
			break;
		case 4: // Full Bass
			setSlidersActive(false);
			setSlidersValue(50, -80, 96, 96, 56, 16, -40, -80, -103, 112, 112);
			break;
		case 5: // Full Bass & Treble
			setSlidersActive(false);
			setSlidersValue(40, 72, 56, 0, -72, -48, 16, 80, 112, 120, 120);
			break;
		case 6: // Full Treble
			setSlidersActive(false);
			setSlidersValue(30, -96, -96, -96, -40, 24, 112, 160, 160, 160, 167);
			break;
		case 7: // Headphones
			setSlidersActive(false);
			setSlidersValue(40, 48, 112, 56, -32, -24, 16, 48, 96, 128, 144);
			break;
		case 8: // Large Hall
			setSlidersActive(false);
			setSlidersValue(50, 103, 103, 56, 56, 0, -48, -48, -48, 0, 0);
			break;
		case 9: // Live
			setSlidersActive(false);
			setSlidersValue(70, -48, 0, 40, 56, 56, 56, 40, 24, 24, 24);
			break;
		case 10: //party
			setSlidersActive(false);
			setSlidersValue(60, 72, 72, 0, 0, 0, 0, 0, 0, 72, 72);
			break;
		case 11: //pop
			setSlidersActive(false);
			setSlidersValue(50, -24, 0, 24, 48, 48, 48, 24, 0, -24, -24);
			break;
		case 12: //Reggae
			setSlidersActive(false);
			setSlidersValue(80, 0, 0, 0, -56, 0, 64, 64, 0, 0, 0);
			break;
		case 13: //Rock
			setSlidersActive(false);
			setSlidersValue(50, 80, 48, -56, -80, -32, 40, 88, 112, 112, 112);
			break;
		case 14: //Ska
			setSlidersActive(false);
			setSlidersValue(60, -24, -48, -40, 0, 40, 56, 88, 96, 112, 96);
			break;
		case 15: //Soft
			setSlidersActive(false);
			setSlidersValue(50, -48, 16, 0, -24, 0, 40, 80, 96, 112, 120);
			break;
		case 16: //Soft Rock
			setSlidersActive(false);
			setSlidersValue(70, 40, 40, 24, 0, -40, -56, -32, 0, 24, 88);
			break;
		case 17: //Techno
			setSlidersActive(false);
			setSlidersValue(50, 80, 56, 0, -56, -48, 0, 80, 96, 96, 88);
			break;
		case 18: // Custom
			setSlidersActive(true);
			setSlidersValue(customValues[0], customValues[1], customValues[2], customValues[3], customValues[4], customValues[5], customValues[6], 
							customValues[7], customValues[8], customValues[9], customValues[10]);
			break;
		default:
			setSlidersActive(false);
			setSlidersValue(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
			break;
		}
	}
	void on_preampSlider_valueChanged(int value) { ui->preampValueLabel->setText(QString::number(value/10.0, 'f', 1)+" dB"); }
	void on_hz60Slider_valueChanged(int value) { ui->hz60ValueLabel->setText(QString::number(value/10.0, 'f', 1)+" dB"); }
	void on_hz170Slider_valueChanged(int value) { ui->hz170ValueLabel->setText(QString::number(value/10.0, 'f', 1)+" dB"); }
	void on_hz310Slider_valueChanged(int value) { ui->hz310ValueLabel->setText(QString::number(value/10.0, 'f', 1)+" dB"); }
	void on_hz600Slider_valueChanged(int value) { ui->hz600ValueLabel->setText(QString::number(value/10.0, 'f', 1)+" dB"); }
	void on_hz1kSlider_valueChanged(int value) { ui->hz1kValueLabel->setText(QString::number(value/10.0, 'f', 1)+" dB"); }
	void on_hz3kSlider_valueChanged(int value) { ui->hz3kValueLabel->setText(QString::number(value/10.0, 'f', 1)+" dB"); }
	void on_hz6kSlider_valueChanged(int value) { ui->hz6kValueLabel->setText(QString::number(value/10.0, 'f', 1)+" dB"); }
	void on_hz12kSlider_valueChanged(int value) { ui->hz12kValueLabel->setText(QString::number(value/10.0, 'f', 1)+" dB"); }
	void on_hz14kSlider_valueChanged(int value) { ui->hz14kValueLabel->setText(QString::number(value/10.0, 'f', 1)+" dB"); }
	void on_hz16kSlider_valueChanged(int value) { ui->hz16kValueLabel->setText(QString::number(value/10.0, 'f', 1)+" dB"); }
};