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
	connect(ui.pbInfo, SIGNAL(clicked()), this, SLOT(showOpenCLInfo()));
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

int VoxelGridInputDialog::getClDevice()
{
	return ui.spDevice->value();
}

int VoxelGridInputDialog::getClPlatform()
{
	return ui.spPlatform->value();
}

void VoxelGridInputDialog::chooseSimulatorSettings()
{
	QString exe = QFileDialog::getOpenFileName(this, tr("Choose simulator executable"), QString(), tr("Executables (*.exe)"));

	if (exe.isEmpty())
		return;

	ui.leSim->setText(exe);
}

void VoxelGridInputDialog::showOpenCLInfo()
{
	QString info = QString::fromStdString(wtl::getOpenCLInfo());

	QMessageBox::information(this, tr("Available OpenCL platforms and devices"), info);
}