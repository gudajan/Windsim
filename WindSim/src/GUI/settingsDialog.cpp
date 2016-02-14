#include "settingsDialog.h"

#include "settings.h"

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