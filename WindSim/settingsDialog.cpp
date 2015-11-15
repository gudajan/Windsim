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

	setModal(false);
}

SettingsDialog::~SettingsDialog()
{
}


void SettingsDialog::updateSettings()
{
	if (conf.cam.type == ModelView)
		ui.rbModelView->setChecked(true);
	else
		ui.rbFirstPerson->setChecked(true);
}

void SettingsDialog::cameraTypeToggled(bool b)
{
	if (!b) return;

	conf.cam.type = ui.rbModelView->isChecked() ? ModelView : FirstPerson;

	emit settingsChanged();
}
