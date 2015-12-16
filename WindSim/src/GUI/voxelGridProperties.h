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
	void positionChanged();
	void resolutionChanged();
	void voxelSizeChanged();

	void buttonClicked(QAbstractButton* button);

signals:
	void propertiesChanged(const QJsonObject& data, Modifications mod);

private:

	void blockSignals();
	void enableSignals();

	Ui::voxelGridProperties ui;

	QJsonObject m_properties;
};
#endif