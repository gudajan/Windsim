#ifndef MESH_PROPERTIES_H
#define MESH_PROPERTIES_H

#include "ui_meshProperties.h"
#include "common.h"

#include <QDialog>
#include <QJsonObject>

class MeshProperties : public QDialog
{
	Q_OBJECT
public:
	MeshProperties(QJsonObject properties, QWidget* parent = nullptr);

	void setup(QObject* obj);
	int getCurrentID() { return m_properties["id"].toInt(); };

public slots:
	void updateProperties(const QJsonObject& properties); // If properties changed outsite of the dialog (e.g. via interactions in the 3D view)

private slots:
	void nameChanged(const QString & text);
	void disabledChanged(int state);
	void voxelizeChanged(int state);
	void dynamicsChanged(int state);
	void positionChanged();
	void scalingChanged();
	void rotationChanged();
	void shadingToggled(bool b);
	void pickColor();
	void densityChanged();
	void localRotAxisChanged();

	void buttonClicked(QAbstractButton* button);

signals:
	void propertiesChanged(const QJsonObject& data, Modifications mod);

private:
	void blockSignals();
	void enableSignals();
	void setColorButton(int r, int g, int b);

	Ui::meshProperties ui;

	QJsonObject m_properties;

};

#endif