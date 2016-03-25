#include "voxelGridInputDialog.h"

#include <WindTunnelLib/WindTunnel.h>

#include <QFileDialog>
#include <QMessageBox>

VoxelGridInputDialog::VoxelGridInputDialog(QWidget* parent)
	: QDialog(parent),
	ui()
{
	ui.setupUi(this);

	connect(ui.pbSim, SIGNAL(clicked()), this, SLOT(chooseSimulatorSettings()));
}

std::vector<int> VoxelGridInputDialog::getGridResolution()
{
	return{ ui.spResX->value(), ui.spResY->value(), ui.spResZ->value() };
}

std::vector<double> VoxelGridInputDialog::getVoxelSize()
{
	return{ ui.dspSizeX->value(), ui.dspSizeY->value(), ui.dspSizeZ->value() };
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

void VoxelGridInputDialog::showOpenCLInfo()
{
	QString info = QString::fromStdString(wtl::getOpenCLInfo());

	QMessageBox::information(this, tr("Available OpenCL platforms and devices"), info);
}