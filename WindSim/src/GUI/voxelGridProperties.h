#ifndef VOXEL_GRID_PROPERTIES_H
#define VOXEL_GRID_PROPERTIES_H

#include "ui_voxelGridProperties.h"
#include "common.h"

#include <QDialog>
#include <QJsonObject>

class VoxelGridProperties : public QDialog
{
	Q_OBJECT
public:
	VoxelGridProperties(QJsonObject properties, QWidget* parent = nullptr);

	void setup(QObject* obj);
	int getCurrentID() { return m_properties["id"].toInt(); };

	void updateProperties(const QJsonObject& properties);

private slots:
	void nameChanged(const QString & text);
	void disabledChanged(int state);

	// Voxel Grid Properties
	void showVoxelChanged(int state);
	void positionChanged();
	void resolutionChanged();
	void voxelSizeChanged();
	void glyphSettingsChanged();
	void volumeSettingsChanged();

	// WindTunnel Properties
	void simulatorSettingsChanged();
	void chooseSimulatorSettings(); // Open Filedialog to choose settings file
	void runSimulationChanged(int state);
	void smokeSettingsChanged();
	void lineSettingsChanged();
	void restartSimulation();

	void buttonClicked(QAbstractButton* button);

signals:
	void propertiesChanged(const QJsonObject& data, Modifications mod);
	void triggerFunction(const QJsonObject& data); // Triggers a function for the given object and provides necessary data

private:
	void blockSignals(bool b);

	float sliderFloatValue(const QSlider* slider) const { return static_cast<float>(slider->value()) / static_cast<float>(slider->maximum()); };
	void setSliderValue(QSlider* slider, float value) { slider->setValue(value * slider->maximum()); };

	Ui::voxelGridProperties ui;

	QJsonObject m_properties;
};
#endif