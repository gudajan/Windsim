#include "settingsDialog.h"

#include "settings.h"

#include <WindTunnelLib/WindTunnel.h>

#include <QMessageBox>

SettingsDialog::SettingsDialog(QWidget* parent)
	: QDialog(parent)
{
	ui.setupUi(this);

	updateSettings();

	// Camera type radiobuttons
	connect(ui.rbFirstPerson, SIGNAL(toggled(bool)), this, SLOT(cameraTypeToggled(bool)));
	connect(ui.rbModelView, SIGNAL(toggled(bool)), this, SLOT(cameraTypeToggled(bool)));

	connect(ui.cbUseDynWorld, SIGNAL(toggled(bool)), this, SLOT(useDynWorldToggled(bool)));
	connect(ui.cbShowDynTrans, SIGNAL(toggled(bool)), this, SLOT(showDynTransToggled(bool)));

	connect(ui.sbDevice, SIGNAL(valueChanged(int)), this, SLOT(simulatorSettingsChanged()));
	connect(ui.sbPlatform, SIGNAL(valueChanged(int)), this, SLOT(simulatorSettingsChanged()));
	connect(ui.pbInfo, SIGNAL(clicked()), this, SLOT(showOpenCLInfo()));

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

	ui.cbUseDynWorld->setChecked(conf.dyn.useDynWorldForCalc);
	ui.cbShowDynTrans->setChecked(conf.dyn.showDynDuringMod);
}

void SettingsDialog::cameraTypeToggled(bool b)
{
	if (!b) return;

	conf.cam.type = ui.rbModelView->isChecked() ? ModelView : FirstPerson;

	emit settingsChanged();
}

void SettingsDialog::useDynWorldToggled(bool b)
{
	conf.dyn.useDynWorldForCalc = b;

	emit settingsChanged();
}

void SettingsDialog::showDynTransToggled(bool b)
{
	conf.dyn.showDynDuringMod = b;

	emit settingsChanged();
}

void SettingsDialog::simulatorSettingsChanged()
{
	// TODO: Either give possibility to choose from available or check if input is valid
	int device = ui.sbDevice->value();
	int platform = ui.sbPlatform->value();

	conf.opencl.device = device;
	conf.opencl.platform = platform;

	// Do only emit settingsChanged if apply button is clicked
}

void SettingsDialog::showOpenCLInfo()
{
	QString info = QString::fromStdString(wtl::getOpenCLInfo());

	QMessageBox::information(this, tr("Available OpenCL platforms and devices"), info);
}


void SettingsDialog::buttonClicked(QAbstractButton* button)
{
	QDialogButtonBox::StandardButton sb = ui.buttonBox->standardButton(button);
	if (sb == QDialogButtonBox::Apply || sb == QDialogButtonBox::Ok)
	{
		emit settingsChanged();
	}
}