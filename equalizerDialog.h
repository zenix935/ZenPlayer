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
	std::vector<int> customValues={0, 0, 0, 0, 0, 0, 0, 0, 0};
	void setCustomValues(const std::vector<int>& values)
	{
		if(!values.empty())
			customValues=values;
		if(ui->presetComboBox->currentIndex()==9)
			setSlidersValue(customValues[0], customValues[1], customValues[2], customValues[3], customValues[4], customValues[5], customValues[6], 
							customValues[7], customValues[8]);
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
	void setSlidersValue(int hz62, int hz125, int hz250, int hz500, int hz1k, int hz2k, int hz4k, int hz8k, int hz16k)
	{
		ui->hz62Slider->setValue(hz62);
		ui->hz125Slider->setValue(hz125);
		ui->hz250Slider->setValue(hz250);
		ui->hz500Slider->setValue(hz500);
		ui->hz1kSlider->setValue(hz1k);
		ui->hz2kSlider->setValue(hz2k);
		ui->hz4kSlider->setValue(hz4k);
		ui->hz8kSlider->setValue(hz8k);
		ui->hz16kSlider->setValue(hz16k);
	}
private:
	void setSlidersActive()
	{
		ui->hz62Slider->setEnabled(true);
		ui->hz125Slider->setEnabled(true);
		ui->hz250Slider->setEnabled(true);
		ui->hz500Slider->setEnabled(true);
		ui->hz1kSlider->setEnabled(true);
		ui->hz2kSlider->setEnabled(true);
		ui->hz4kSlider->setEnabled(true);
		ui->hz8kSlider->setEnabled(true);
		ui->hz16kSlider->setEnabled(true);
	}
	void setSlidersDeactive()
	{
		ui->hz62Slider->setEnabled(false);
		ui->hz125Slider->setEnabled(false);
		ui->hz250Slider->setEnabled(false);
		ui->hz500Slider->setEnabled(false);
		ui->hz1kSlider->setEnabled(false);
		ui->hz2kSlider->setEnabled(false);
		ui->hz4kSlider->setEnabled(false);
		ui->hz8kSlider->setEnabled(false);
		ui->hz16kSlider->setEnabled(false);
	}

private slots:
	void on_presetComboBox_currentIndexChanged(int index)
	{
		switch (index)
		{
		case 0: // Flat
			setSlidersDeactive();
			setSlidersValue(0, 0, 0, 0, 0, 0, 0, 0, 0);
			break;
		case 1: // Treble Boost
			setSlidersDeactive();
			setSlidersValue(0, 0, 0, 0, 0, 10, 40, 45, 70);
			break;
		case 2: // Bass Boost
			setSlidersDeactive();
			setSlidersValue(70, 48, 40, 10, 0, 0, 0, 0, 0);
			break;
		case 3: // Headphones
			setSlidersDeactive();
			setSlidersValue(70, 44, 30, 2, 0, 5, 30, 29, 40);
			break;
		case 4: // Laptop
			setSlidersDeactive();
			setSlidersValue(60, 50, 60, 26, 20, 25, 60, 55, 70);
			break;
		case 5: // Speakers
			setSlidersDeactive();
			setSlidersValue(80, 54, 50, 27, 30, 23, 40, 36, 50);
			break;
		case 6: // Home Stereo
			setSlidersDeactive();
			setSlidersValue(60, 41, 40, 17, 20, 17, 40, 41, 60);
			break;
		case 7: // TV
			setSlidersDeactive();
			setSlidersValue(30, 45, 80, 28, 0, 13, 60, 61, 80);
			break;
		case 8: // Car
			setSlidersDeactive();
			setSlidersValue(80, 48, 30, 1, 0, 7, 40, 48, 70);
			break;
		case 9: // Custom
			setSlidersActive();
			setSlidersValue(customValues[0], customValues[1], customValues[2], customValues[3], customValues[4], customValues[5], customValues[6], 
							customValues[7], customValues[8]);
			break;
		}
	}
};