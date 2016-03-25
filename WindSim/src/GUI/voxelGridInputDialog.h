#ifndef VOXEL_GRID_INPUT_DIALOG_H
#define VOXEL_GRID_INPUT_DIALOG_H

#include <QDialog>
#include "ui_voxelGridInput.h"

#include <vector>

class VoxelGridInputDialog : public QDialog
{
	Q_OBJECT
public:
	VoxelGridInputDialog(QWidget* parent = nullptr);

	std::vector<int> getGridResolution();
	std::vector<double> getVoxelSize();
	QString getSimulatorSettings();

private slots:
	void chooseSimulatorSettings();

private:
	Ui::gridInput ui;
};

#endif