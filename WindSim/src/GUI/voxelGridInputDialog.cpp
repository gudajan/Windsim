#include "voxelGridInputDialog.h"

#include <QFileDialog>

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

QString VoxelGridInputDialog::getSimulator()
{
	return ui.leSim->text();
}

void VoxelGridInputDialog::chooseSimulator()
{
	QString exe = QFileDialog::getOpenFileName(this, tr("Choose simulator executable"), QString(), tr("Executables (*.exe)"));

	if (exe.isEmpty())
		return;

	ui.leSim->setText(exe);
}