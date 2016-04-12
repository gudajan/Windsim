#include "settingsDialog.h"

#include "settings.h"
#include "staticLogger.h"

#include <QMessageBox>

#include <sstream>

SettingsDialog::SettingsDialog(QWidget* parent)
	: QDialog(parent, Qt::WindowTitleHint | Qt::WindowCloseButtonHint)
	, m_openCLInfo(wtl::getOpenCLInfo())
{
	ui.setupUi(this);
	ui.cmbCalcMethod->setItemData(ui.cmbCalcMethod->findText("Pressure"), "Use pressure field from the simulation to calculate acting forces", Qt::ToolTipRole);
	ui.cmbCalcMethod->setItemData(ui.cmbCalcMethod->findText("Velocity"), "Use velocity field from the simulation to calculate faked acting forces", Qt::ToolTipRole);

	updateSettings();

	// Camera type radiobuttons
	connect(ui.rbFirstPerson, SIGNAL(toggled(bool)), this, SLOT(cameraTypeToggled(bool)));
	connect(ui.rbModelView, SIGNAL(toggled(bool)), this, SLOT(cameraTypeToggled(bool)));

	connect(ui.cbShowDynTrans, SIGNAL(toggled(bool)), this, SLOT(showDynTransToggled(bool)));
	connect(ui.cmbCalcMethod, SIGNAL(currentTextChanged(const QString&)), this, SLOT(dynCalcMethodChanged(const QString&)));

	connect(ui.cmbCLPlatform, SIGNAL(currentIndexChanged(int)), this, SLOT(plaformChanged()));
	connect(ui.cmbCLDevice, SIGNAL(currentIndexChanged(int)), this, SLOT(deviceChanged()));
	connect(ui.pbInfo, SIGNAL(clicked()), this, SLOT(showOpenCLInfo()));
	connect(ui.cbDisplayCLInfo, SIGNAL(toggled(bool)), this, SLOT(displayCLInfoToggled(bool)));
	connect(ui.cbPrintCLInfo, SIGNAL(toggled(bool)), this, SLOT(printCLInfoToggled(bool)));

	connect(ui.buttonBox, SIGNAL(clicked(QAbstractButton*)), this, SLOT(buttonClicked(QAbstractButton*)));

	setModal(false);
}

SettingsDialog::~SettingsDialog()
{
}

void SettingsDialog::setup(QObject* obj)
{
	connect(this, SIGNAL(settingsChanged()), obj, SLOT(applySettings()));
	connect(obj, SIGNAL(settingsChanged()), this, SLOT(updateSettings()));
}


void SettingsDialog::updateSettings()
{
	if (conf.cam.type == ModelView)
		ui.rbModelView->setChecked(true);
	else
		ui.rbFirstPerson->setChecked(true);

	ui.cbShowDynTrans->setChecked(conf.dyn.showDynDuringMod);
	ui.cmbCalcMethod->setCurrentIndex(conf.dyn.method);

	fillOpenCLPlatforms();
	ui.cmbCLPlatform->setCurrentIndex(conf.opencl.platform);
	fillOpenCLDevices();
	ui.cmbCLDevice->setCurrentIndex(conf.opencl.device);

	ui.cbDisplayCLInfo->setChecked(conf.opencl.showInfo);
	ui.cbPrintCLInfo->setChecked(conf.opencl.printInfo);
}

void SettingsDialog::cameraTypeToggled(bool b)
{
	if (!b) return;

	conf.cam.type = ui.rbModelView->isChecked() ? ModelView : FirstPerson;

	emit settingsChanged(); // Change camera type immediately
}

void SettingsDialog::showDynTransToggled(bool b)
{
	conf.dyn.showDynDuringMod = b;
}

void SettingsDialog::dynCalcMethodChanged(const QString& text)
{
	DynamicsMethod method = text == "Pressure" ? Pressure : Velocity;

	conf.dyn.method = method;
}

void SettingsDialog::plaformChanged()
{
	// TODO: What if no devices exist for platform
	conf.opencl.platform = ui.cmbCLPlatform->currentIndex();
	fillOpenCLDevices();
	conf.opencl.device = 0;
	ui.cmbCLDevice->setCurrentIndex(conf.opencl.device);
}

void SettingsDialog::deviceChanged()
{
	conf.opencl.device = ui.cmbCLDevice->currentIndex();
}

void SettingsDialog::displayCLInfoToggled(bool b)
{
	conf.opencl.showInfo = b;

	emit settingsChanged(); // Enable text overlay
}

void SettingsDialog::printCLInfoToggled(bool b)
{
	conf.opencl.printInfo = b;
}

void SettingsDialog::showOpenCLInfo()
{
	std::stringstream out;
	out << "Available OpenCL Platforms:" << std::endl;
	out << std::endl;
	int pi = 0;
	for (const auto& platform : m_openCLInfo)
	{
		const auto& pDesc = platform.first;
		out << "Platform " << pi << ":\n";
		out << "\tName: " << pDesc.at("name") << std::endl;
		out << "\tVendor: " << pDesc.at("vendor") << std::endl;
		out << "\tVersion: " << pDesc.at("version") << std::endl;
		out << "\tProfile: " << pDesc.at("profile") << std::endl;
		out << "\tAvailable OpenCL Devices:" << std::endl;
		int di = 0;
		for (const auto& device : platform.second)
		{
			out << "\t\tDevice " << di << ":\n";
			out << "\t\t\tName: " << device.at("name") << std::endl;
			out << "\t\t\tVendor: " << device.at("vendor") << std::endl;
			out << "\t\t\tPCIe Bus: " << device.at("PCIeBus") << std::endl;
			out << "\t\t\tGlobal Memory: " << device.at("globalMemory") << std::endl;
			out << "\t\t\tCompute Units: " << device.at("computeUnits") << std::endl;
			out << "\t\t\tVersion: " << device.at("version") << std::endl;
			out << "\t\t\tProfile: " << device.at("profile") << std::endl;
			out << std::endl;
			di++;
		}
		out << std::endl;
		pi++;
	}

	QMessageBox::information(this, tr("Available OpenCL platforms and devices"), QString::fromStdString(out.str()));
}


void SettingsDialog::buttonClicked(QAbstractButton* button)
{
	QDialogButtonBox::StandardButton sb = ui.buttonBox->standardButton(button);
	if (sb == QDialogButtonBox::Apply || sb == QDialogButtonBox::Ok)
	{
		emit settingsChanged();
	}
}

void SettingsDialog::fillOpenCLPlatforms()
{
	ui.cmbCLPlatform->blockSignals(true);
	ui.cmbCLPlatform->clear();
	ui.cmbCLPlatform->blockSignals(false);
	int i = 0;
	for (const auto& platform : m_openCLInfo)
	{
		const auto& desc = platform.first;
		ui.cmbCLPlatform->addItem(QString::fromStdString(desc.at("name"))); // Leave vendor

		std::stringstream out;
		out << "Name: " << desc.at("name") << std::endl;
		out << "Vendor: " << desc.at("vendor") << std::endl;
		out << "Version: " << desc.at("version") << std::endl;
		out << "Profile: " << desc.at("profile") << std::endl;
		ui.cmbCLPlatform->setItemData(i, QString::fromStdString(out.str()) , Qt::ToolTipRole);

		i++;
	}
}

void SettingsDialog::fillOpenCLDevices()
{
	ui.cmbCLDevice->clear();
	int i = 0;
	int index = ui.cmbCLPlatform->currentIndex();
	if (index >= m_openCLInfo.size())
	{
		StaticLogger::logit("WARNING: OpenCL Devices of Settings Dialog not filled correctly!");
		return;
	}

	for (const auto& device : m_openCLInfo[index].second)
	{
		ui.cmbCLDevice->addItem(QString::fromStdString(device.at("name") + " " + device.at("vendor")));

		std::stringstream out;
		out << "Name: " << device.at("name") << std::endl;
		out << "Vendor: " << device.at("vendor") << std::endl;
		out << "PCIe Bus: " << device.at("PCIeBus") << std::endl;
		out << "Global Memory: " << device.at("globalMemory") << std::endl;
		out << "Compute Units: " << device.at("computeUnits") << std::endl;
		out << "Version: " << device.at("version") << std::endl;
		out << "Profile: " << device.at("profile") << std::endl;
		ui.cmbCLDevice->setItemData(i, QString::fromStdString(out.str()), Qt::ToolTipRole);

		i++;
	}
}