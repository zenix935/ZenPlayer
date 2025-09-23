#pragma once
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QApplication>
#include <QFileDialog>
#include <fstream>
#include "json.hpp"
#include "ui_ZenPlayer.h"
#include "closeFolderDialog.h"

using json=nlohmann::json;

QT_BEGIN_NAMESPACE
namespace Ui { class ZenPlayerClass; };
QT_END_NAMESPACE

class ZenPlayer : public QMainWindow
{
    Q_OBJECT

public:
    ZenPlayer(QWidget *parent = nullptr);
    ~ZenPlayer();

private slots:
    void on_muteButton_clicked();
	void on_volumeSlider_valueChanged(int value);

    void on_repeatButton_clicked();
    void on_shuffleButton_clicked();
    void on_previousButton_clicked();
    void on_nextButton_clicked();
    void on_playButton_clicked();

    void saveData();
	void loadData();
    void on_addFolderButton_clicked();
	void on_foldersListWidget_itemClicked(QListWidgetItem* item);
    void on_foldersListWidget_itemDoubleClicked(QListWidgetItem* item);
    void on_tracksListWidget_itemDoubleClicked(QListWidgetItem* item);

	void on_tracksListWidget_itemClicked(QListWidgetItem* item);

private:
    Ui::ZenPlayerClass *ui;
    bool mute,repeat,shuffle,pause;
	json data;
    QList<QString> folderPaths;
    QList<QString> tracksPaths;
};
