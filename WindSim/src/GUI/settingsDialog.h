#ifndef SETTINGS_DIALOG_H
#define SETTINGS_DIALOG_H

#include <QDialog>
#include "ui_settings.h"

#include <WindTunnel.h>

class SettingsDialog : public QDialog
{
	Q_OBJECT
public:
	SettingsDialog(QWidget* parent = nullptr);
	~SettingsDialog();

	void setup(QObject* obj);

public slots:
	void updateSettings(); // Call if settings were changed outside the dialog (e.g reload via keyboard short cut) to update values in the GUI of the dialog

private slots:
	void cameraTypeToggled(bool b);
	void showDynTransToggled(bool b);
	void dynCalcMethodChanged(const QString& text);
	void plaformChanged();
	void deviceChanged();
	void displayCLInfoToggled(bool b);
	void printCLInfoToggled(bool b);

	void showOpenCLInfo();

	void buttonClicked(QAbstractButton* button);

signals:
	void settingsChanged(); // Emitted if settings are changed in the dialog at any time

private:
	void fillOpenCLPlatforms();
	void fillOpenCLDevices();
	Ui::settingsDialog ui;

	std::vector<std::pair<wtl::description, std::vector<wtl::description>>> m_openCLInfo;

};

#endif