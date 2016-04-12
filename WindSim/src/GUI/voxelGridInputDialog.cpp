#include "voxelGridInputDialog.h"

#include <WindTunnel.h>

#include <QFileDialog>
#include <QMessageBox>

VoxelGridInputDialog::VoxelGridInputDialog(QWidget* parent)
	: QDialog(parent, Qt::WindowTitleHint | Qt::WindowCloseButtonHint),
	ui()
{
	ui.setupUi(this);

	connect(ui.pbSim, SIGNAL(clicked()), this, SLOT(chooseSimulatorSettings()));
}

std::vector<int> VoxelGridInputDialog::getGridResolution()
{
	return{ ui.spResX->value(), ui.spResY->value(), ui.spResZ->value() };
}

std::vector<double> VoxelGridInputDialog::getGridSize()
{
	return{ ui.dspSizeX->value() , ui.dspSizeY->value(), ui.dspSizeZ->value()};
}

QString VoxelGridInputDialog::getSimulatorSettings()
{
	return ui.leSim->text();
}

void VoxelGridInputDialog::chooseSimulatorSettings()
{
	QString settings = QFileDialog::getOpenFileName(this, tr("Choose WindTunnel settings file"), QString(), tr("Json-files (*.json *.txt)"));

	if (settings.isEmpty())
		return;

	ui.leSim->setText(settings);
}
