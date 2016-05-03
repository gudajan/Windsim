#include "meshProperties.h"
#include "settings.h"

#include <QColorDialog>

MeshProperties::MeshProperties(QJsonObject properties, QWidget* parent)
	: QDialog(parent, Qt::WindowTitleHint | Qt::WindowCloseButtonHint),
	m_properties(properties)
{
	ui.setupUi(this);

	updateProperties(m_properties);

	// Name
	connect(ui.nameEdit, SIGNAL(textChanged(const QString &)), this, SLOT(nameChanged(const QString &)));

	// Enabled
	connect(ui.cbEnabled, SIGNAL(stateChanged(int)), this, SLOT(enabledChanged(int)));

	// Voxelize
	connect(ui.cbVoxelize, SIGNAL(stateChanged(int)), this, SLOT(voxelizeChanged(int)));

	// Dynamics
	connect(ui.cbDynamics, SIGNAL(stateChanged(int)), this, SLOT(dynamicsChanged(int)));

	// Position
	connect(ui.xP, SIGNAL(valueChanged(double)), this, SLOT(positionChanged()));
	connect(ui.yP, SIGNAL(valueChanged(double)), this, SLOT(positionChanged()));
	connect(ui.zP, SIGNAL(valueChanged(double)), this, SLOT(positionChanged()));

	// Scaling
	connect(ui.xS, SIGNAL(valueChanged(double)), this, SLOT(scalingChanged()));
	connect(ui.yS, SIGNAL(valueChanged(double)), this, SLOT(scalingChanged()));
	connect(ui.zS, SIGNAL(valueChanged(double)), this, SLOT(scalingChanged()));

	// Rotation
	connect(ui.ax, SIGNAL(valueChanged(double)), this, SLOT(rotationChanged()));
	connect(ui.ay, SIGNAL(valueChanged(double)), this, SLOT(rotationChanged()));
	connect(ui.az, SIGNAL(valueChanged(double)), this, SLOT(rotationChanged()));
	connect(ui.angle, SIGNAL(valueChanged(double)), this, SLOT(rotationChanged()));

	// Shading
	connect(ui.rbFlat, SIGNAL(toggled(bool)), this, SLOT(shadingToggled(bool)));
	connect(ui.rbSmooth, SIGNAL(toggled(bool)), this, SLOT(shadingToggled(bool)));

	//Color
	connect(ui.pbCol, SIGNAL(clicked()), this, SLOT(pickColor()));

	// Dynamics settings
	connect(ui.dspDensity, SIGNAL(editingFinished()), this, SLOT(densityChanged()));
	connect(ui.gbDynAxis , SIGNAL(toggled(bool)), this, SLOT(localRotAxisChanged()));
	connect(ui.dynAxisX, SIGNAL(valueChanged(double)), this, SLOT(localRotAxisChanged()));
	connect(ui.dynAxisY, SIGNAL(valueChanged(double)), this, SLOT(localRotAxisChanged()));
	connect(ui.dynAxisZ, SIGNAL(valueChanged(double)), this, SLOT(localRotAxisChanged()));
	connect(ui.cbShowAccel, SIGNAL(stateChanged(int)), this, SLOT(showAccelArrowChanged(int)));

	setModal(false);
}

void MeshProperties::setup(QObject* obj)
{
	connect(obj, SIGNAL(updatePropertiesDialog(const QJsonObject&)), this, SLOT(updateProperties(const QJsonObject&)));
	// Create modifyCmd if properties changed in dialog
	connect(this, SIGNAL(propertiesChanged(const QJsonObject&, Modifications)), obj, SLOT(modifyCmd(const QJsonObject&, Modifications)));
}

void MeshProperties::updateProperties(const QJsonObject& properties)
{
	ObjectType type = stringToObjectType(properties["type"].toString().toStdString());
	if (type != ObjectType::Mesh)
		return;

	// We just update the dialog with the changes, made outside of it. So we do not want to create any signals because of these changes
	// Doing so, would create annother invalid modify command
	blockSignals(true);
	{
		// Name
		ui.nameEdit->setText(properties["name"].toString());
		setWindowTitle("Mesh Properties: " + ui.nameEdit->text());

		// Visibility
		ui.cbEnabled->setChecked(properties["enabled"].toBool());
		ui.cbVoxelize->setChecked(properties["voxelize"].toBool());
		ui.cbDynamics->setChecked(properties["dynamics"].toBool());

		// Position
		const QJsonObject& pos = properties.find("position")->toObject();
		ui.xP->setValue(pos.find("x")->toDouble()); // right: x in DX1
		ui.yP->setValue(pos.find("y")->toDouble()); // up: y in DX11
		ui.zP->setValue(pos.find("z")->toDouble()); // forward: z in DX11

		// Scaling
		const QJsonObject& scale = properties.find("scaling")->toObject();
		ui.xS->setValue(scale.find("x")->toDouble());
		ui.yS->setValue(scale.find("y")->toDouble());
		ui.zS->setValue(scale.find("z")->toDouble());

		// Rotation
		const QJsonObject& rot = properties.find("rotation")->toObject();
		ui.ax->setValue(rot.find("ax")->toDouble());
		ui.ay->setValue(rot.find("ay")->toDouble());
		ui.az->setValue(rot.find("az")->toDouble());
		ui.angle->setValue(rot.find("angle")->toDouble());

		// Shading
		if (properties.find("shading")->toString() == "Smooth")
			ui.rbSmooth->setChecked(true);
		else // Flat is default
			ui.rbFlat->setChecked(true);

		// Color
		const QJsonObject& col = properties.find("color")->toObject();
		setColorButton(col.find("r")->toInt(), col.find("g")->toInt(), col.find("b")->toInt());

		// Dynamics
		ui.dspDensity->setValue(properties["density"].toDouble());
		const QJsonObject& dynamics = properties.find("localRotAxis")->toObject();
		ui.dynAxisX->setValue(dynamics.find("x")->toDouble());
		ui.dynAxisY->setValue(dynamics.find("y")->toDouble());
		ui.dynAxisZ->setValue(dynamics.find("z")->toDouble());
		ui.gbDynAxis->setChecked(dynamics.find("enabled")->toBool());
		int d = properties.find("showAccelArrow")->toInt();
		ui.cbShowAccel->setChecked(properties.find("showAccelArrow")->toBool());

		m_properties = properties; // Copy properties
	}
	blockSignals(false);
}

// QJson currently is implemented for read-only. So it is not possible to get a reference to nested Json values
// In order to modify nested values, one has to copy the object, modify it and copy it back.

void MeshProperties::nameChanged(const QString& text)
{
	m_properties["name"] = text;
	emit propertiesChanged(m_properties, Name);
}

void MeshProperties::enabledChanged(int state)
{
	m_properties["enabled"] = state == Qt::Checked ? true : false;
	emit propertiesChanged(m_properties, Visibility);
}

void MeshProperties::voxelizeChanged(int state)
{
	m_properties["voxelize"] = state == Qt::Checked ? true : false;
	emit propertiesChanged(m_properties, Voxelization);
}


void MeshProperties::dynamicsChanged(int state)
{
	m_properties["dynamics"] = state == Qt::Checked ? true : false;
	emit propertiesChanged(m_properties, DynamicsSettings);
}


void MeshProperties::positionChanged()
{
	QJsonObject pos
	{
		{ "x", ui.xP->value() },
		{ "y", ui.yP->value() },
		{ "z", ui.zP->value() }
	};
	m_properties["position"] = pos;
	emit propertiesChanged(m_properties, Position);
}

void MeshProperties::scalingChanged()
{
	QJsonObject scale
	{
		{ "x", ui.xS->value() },
		{ "y", ui.yS->value() },
		{ "z", ui.zS->value() }
	};
	m_properties["scaling"] = scale;

	Modifications mod = Scaling;

	emit propertiesChanged(m_properties, mod);
}

void MeshProperties::rotationChanged()
{
	QJsonObject rot
	{
		{ "ax", ui.ax->value() },
		{ "ay", ui.ay->value() },
		{ "az", ui.az->value() },
		{ "angle", ui.angle->value() }
	};

	m_properties["rotation"] = rot;
	emit propertiesChanged(m_properties, Rotation);
}

void MeshProperties::shadingToggled(bool b)
{
	if (!b) return;

	m_properties["shading"] = ui.rbSmooth->isChecked() ? "Smooth" : "Flat";
	emit propertiesChanged(m_properties, Shading);
}

void MeshProperties::pickColor()
{
	QJsonObject json = m_properties["color"].toObject();
	QColor col = QColorDialog::getColor(QColor(json["r"].toInt(), json["g"].toInt(), json["b"].toInt()), this);
	if (!col.isValid()) // Color dialog canceled
		return;

	json["r"] = col.red();
	json["g"] = col.green();
	json["b"] = col.blue();

	m_properties["color"] = json;

	setColorButton(col.red(), col.green(), col.blue());

	emit propertiesChanged(m_properties, Color);
}

void MeshProperties::densityChanged()
{
	m_properties["density"] = ui.dspDensity->value();
	emit propertiesChanged(m_properties, DynamicsSettings);
}
void MeshProperties::localRotAxisChanged()
{
	QJsonObject json
	{
		{ "enabled", ui.gbDynAxis->isChecked() },
		{ "x", ui.dynAxisX->value() },
		{ "y", ui.dynAxisY->value() },
		{ "z", ui.dynAxisZ->value() }
	};
	m_properties["localRotAxis"] = json;
	emit propertiesChanged(m_properties, DynamicsSettings);
}

void MeshProperties::showAccelArrowChanged(int state)
{
	m_properties["showAccelArrow"] = state == Qt::Checked ? true : false;
	emit propertiesChanged(m_properties, ShowAccelArrow);
}

void MeshProperties::buttonClicked(QAbstractButton* button)
{
	// Apply or Ok button was clicked
	QDialogButtonBox::StandardButton sb = ui.buttonBox->standardButton(button);
	if (sb == QDialogButtonBox::Apply || sb == QDialogButtonBox::Ok)
	{
		emit propertiesChanged(m_properties, All);
	}
}

void MeshProperties::blockSignals(bool b)
{
	ui.nameEdit->blockSignals(b);

	// Visibility
	ui.cbEnabled->blockSignals(b);
	ui.cbVoxelize->blockSignals(b);
	ui.cbDynamics->blockSignals(b);

	// Position
	ui.xP->blockSignals(b);
	ui.yP->blockSignals(b);
	ui.zP->blockSignals(b);

	// Scaling
	ui.xS->blockSignals(b);
	ui.yS->blockSignals(b);
	ui.zS->blockSignals(b);

	// Rotation
	ui.ax->blockSignals(b);
	ui.ay->blockSignals(b);
	ui.az->blockSignals(b);
	ui.angle->blockSignals(b);

	//Shading
	ui.rbSmooth->blockSignals(b);
	ui.rbFlat->blockSignals(b);

	// Dynamics settings
	ui.dspDensity->blockSignals(b);
	ui.gbDynAxis->blockSignals(b);
	ui.dynAxisX->blockSignals(b);
	ui.dynAxisY->blockSignals(b);
	ui.dynAxisZ->blockSignals(b);
	ui.cbShowAccel->blockSignals(b);

}

void MeshProperties::setColorButton(int r, int g, int b)
{
	ui.pbCol->setStyleSheet("QPushButton {background-color: rgb(" + QString::number(r) + "," + QString::number(g) + "," + QString::number(b) + "); border-style: outset}");


}