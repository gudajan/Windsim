#include "meshProperties.h"
#include "settings.h"

#include <QColorDialog>

MeshProperties::MeshProperties(QJsonObject properties, QWidget* parent)
	: QDialog(parent),
	m_properties(properties)
{
	ui.setupUi(this);

	updateProperties(m_properties);

	// Name
	connect(ui.nameEdit, SIGNAL(textChanged(const QString &)), this, SLOT(nameChanged(const QString &)));

	// Disabled
	connect(ui.cbDisabled, SIGNAL(stateChanged(int)), this, SLOT(disabledChanged(int)));

	// Voxelize
	connect(ui.cbVoxelize, SIGNAL(stateChanged(int)), this, SLOT(voxelizeChanged(int)));

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
	blockSignals();
	{
		// Name
		ui.nameEdit->setText(properties["name"].toString());
		setWindowTitle("Mesh Properties: " + ui.nameEdit->text());

		// Visibility
		ui.cbDisabled->setChecked(properties["disabled"].toInt() != Qt::Unchecked);
		ui.cbVoxelize->setChecked(properties["voxelize"].toInt() == Qt::Checked);

		// Position
		const QJsonObject& pos = properties.find("Position")->toObject();
		ui.xP->setValue(pos.find("x")->toDouble()); // right: x in DX1
		ui.yP->setValue(pos.find("y")->toDouble()); // up: y in DX11
		ui.zP->setValue(pos.find("z")->toDouble()); // forward: z in DX11

		// Scaling
		const QJsonObject& scale = properties.find("Scaling")->toObject();
		ui.xS->setValue(scale.find("x")->toDouble());
		ui.yS->setValue(scale.find("y")->toDouble());
		ui.zS->setValue(scale.find("z")->toDouble());

		// Rotation
		const QJsonObject& rot = properties.find("Rotation")->toObject();
		double debug1 = rot.find("ax")->toDouble();
		double debug2 = rot.find("ay")->toDouble();
		double debug3 = rot.find("az")->toDouble();
		double debug4 = rot.find("angle")->toDouble();
		ui.ax->setValue(rot.find("ax")->toDouble());
		ui.ay->setValue(rot.find("ay")->toDouble());
		ui.az->setValue(rot.find("az")->toDouble());
		ui.angle->setValue(rot.find("angle")->toDouble());

		// Shading
		if (properties.find("Shading")->toString() == "Smooth")
			ui.rbSmooth->setChecked(true);
		else // Flat is default
			ui.rbFlat->setChecked(true);

		// Color
		const QJsonObject& col = properties.find("Color")->toObject();
		setColorButton(col.find("r")->toInt(), col.find("g")->toInt(), col.find("b")->toInt());

		m_properties = properties; // Copy properties
	}
	enableSignals();
}

// QJson currently is implemented for read-only. So it is not possible to get a reference to nested Json values
// In order to modify nested values, one has to copy the object, modify it and copy it back.

void MeshProperties::nameChanged(const QString& text)
{
	m_properties["name"] = text;
	emit propertiesChanged(m_properties, Name);
}

void MeshProperties::disabledChanged(int state)
{
	m_properties["disabled"] = state;
	emit propertiesChanged(m_properties, Visibility);
}

void MeshProperties::voxelizeChanged(int state)
{
	m_properties["voxelize"] = state;
	emit propertiesChanged(m_properties, Visibility);
}

void MeshProperties::positionChanged()
{
	QJsonObject pos
	{
		{ "x", ui.xP->value() },
		{ "y", ui.yP->value() },
		{ "z", ui.zP->value() }
	};
	m_properties["Position"] = pos;
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
	m_properties["Scaling"] = scale;
	emit propertiesChanged(m_properties, Scaling);
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

	m_properties["Rotation"] = rot;
	emit propertiesChanged(m_properties, Rotation);
}

void MeshProperties::shadingToggled(bool b)
{
	if (!b) return;

	m_properties["Shading"] = ui.rbSmooth->isChecked() ? "Smooth" : "Flat";
	emit propertiesChanged(m_properties, Shading);
}

void MeshProperties::pickColor()
{
	QJsonObject json = m_properties["Color"].toObject();
	QColor col = QColorDialog::getColor(QColor(json["r"].toInt(), json["g"].toInt(), json["b"].toInt()), this);
	json["r"] = col.red();
	json["g"] = col.green();
	json["b"] = col.blue();

	m_properties["Color"] = json;

	setColorButton(col.red(), col.green(), col.blue());

	emit propertiesChanged(m_properties, Color);
}

void MeshProperties::buttonClicked(QAbstractButton* button)
{
	// Apply or Ok button was clicked
	QDialogButtonBox::StandardButton sb = ui.buttonBox->standardButton(button);
	if (sb == QDialogButtonBox::Apply || sb == QDialogButtonBox::Ok)
	{
		emit propertiesChanged(m_properties, Position|Scaling|Rotation|Visibility|Shading|Name);
	}
}

void MeshProperties::blockSignals()
{
	ui.nameEdit->blockSignals(true);

	// Visibility
	ui.cbDisabled->blockSignals(true);
	ui.cbVoxelize->blockSignals(true);

	// Position
	ui.xP->blockSignals(true);
	ui.yP->blockSignals(true);
	ui.zP->blockSignals(true);

	// Scaling
	ui.xS->blockSignals(true);
	ui.yS->blockSignals(true);
	ui.zS->blockSignals(true);

	// Rotation
	ui.ax->blockSignals(true);
	ui.ay->blockSignals(true);
	ui.az->blockSignals(true);
	ui.angle->blockSignals(true);

	//Shading
	ui.rbSmooth->blockSignals(true);
	ui.rbFlat->blockSignals(true);
}

void MeshProperties::enableSignals()
{
	ui.nameEdit->blockSignals(false);

	// Visibility
	ui.cbDisabled->blockSignals(false);
	ui.cbVoxelize->blockSignals(false);

	// Position
	ui.xP->blockSignals(false);
	ui.yP->blockSignals(false);
	ui.zP->blockSignals(false);

	// Scaling
	ui.xS->blockSignals(false);
	ui.yS->blockSignals(false);
	ui.zS->blockSignals(false);

	// Rotation
	ui.ax->blockSignals(false);
	ui.ay->blockSignals(false);
	ui.az->blockSignals(false);
	ui.angle->blockSignals(false);

	//Shading
	ui.rbSmooth->blockSignals(false);
	ui.rbFlat->blockSignals(false);
}

void MeshProperties::setColorButton(int r, int g, int b)
{
	ui.pbCol->setStyleSheet("QPushButton {background-color: rgb(" + QString::number(r) + "," + QString::number(g) + "," + QString::number(b) + "); border-style: outset}");


}