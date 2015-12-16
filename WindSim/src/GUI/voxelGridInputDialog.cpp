#include "voxelGridInputDialog.h"

VoxelGridInputDialog::VoxelGridInputDialog(QWidget* parent)
	: QDialog(parent),
	ui()
{
	ui.setupUi(this);
}

std::vector<int> VoxelGridInputDialog::getGridResolution()
{
	return{ ui.spResX->value(), ui.spResY->value(), ui.spResZ->value() };
}

std::vector<double> VoxelGridInputDialog::getVoxelSize()
{
	return{ ui.dspSizeX->value(), ui.dspSizeY->value(), ui.dspSizeZ->value() };
}