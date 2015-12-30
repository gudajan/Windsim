#ifndef VOXEL_GRID_INPUT_DIALOG_H
#define VOXEL_GRID_INPUT_DIALOG_H

#include <QDialog>
#include "ui_voxelGridInput.h"

#include <vector>

class VoxelGridInputDialog : public QDialog
{
public:
	VoxelGridInputDialog(QWidget* parent = nullptr);

	std::vector<int> getGridResolution();
	std::vector<double> getVoxelSize();
	QString getSimulator();

private slots:
	void chooseSimulator();

private:
	Ui::gridInput ui;
};

#endif