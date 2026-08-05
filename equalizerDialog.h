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
		if(ui->presetComboBox->currentIndex()==10)
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
		case 1: // Headphones
			setSlidersActive(false);
			setSlidersValue(90, 45, 30, 10, -10, -10, 10, 35, 40, 40, 35);
			break;
		case 2: // Laptop
			setSlidersActive(false);
			setSlidersValue(100, -60, -20, 10, 30, 40, 35, 25, 10, 0, -10);
			break;
		case 3: // Speakers
			setSlidersActive(false);
			setSlidersValue(100, -30, -10, 15, 25, 30, 20, 20, 10, 5, 0);
			break;
		case 4: // Bass Boost
			setSlidersActive(false);
			setSlidersValue(70, 70, 50, 30, 0, 0, 0, 0, 0, 0, 0);
			break;
		case 5: // Treble Boost
			setSlidersActive(false);
			setSlidersValue(80, 0, 0, 0, 0, 10, 30, 50, 60, 65, 70);
			break;
		case 6: // Bass & Treble Boost
			setSlidersActive(false);
			setSlidersValue(70, 60, 45, 15, -10, -20, 10, 40, 55, 60, 60);
			break;
		case 7: // pop
			setSlidersActive(false);
			setSlidersValue(90, 15, 35, 45, 30, 0, -15, -20, -15, 0, 10);
			break;
		case 8: // Rock
			setSlidersActive(false);
			setSlidersValue(80, 50, 35, -15, -25, 5, 30, 45, 40, 30, 20);
			break;
		case 9: // Classical
			setSlidersActive(false);
			setSlidersValue(120, 0, 0, 0, 0, 0, -10, -20, -25, -30, -35);
			break;
		case 10: // Custom
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