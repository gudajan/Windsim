#include "meshProperties.h"

MeshProperties::MeshProperties(QJsonObject properties, QWidget* parent)
	: QDialog(parent),
	m_properties(properties)
{
	ui.setupUi(this);

	// Update here, where signal/slots are not yet connected to avoid unnecessary slot invocation
	updateProperties(m_properties);

	// Name
	connect(ui.nameEdit, SIGNAL(textChanged(const QString &)), this, SLOT(nameChanged(const QString &)));

	// Disabled
	connect(ui.cbDisabled, SIGNAL(stateChanged(int)), this, SLOT(disabledChanged(int)));

	// Position
	connect(ui.xP, SIGNAL(valueChanged(double)), this, SLOT(positionChanged()));
	connect(ui.yP, SIGNAL(valueChanged(double)), this, SLOT(positionChanged()));
	connect(ui.zP, SIGNAL(valueChanged(double)), this, SLOT(positionChanged()));

	// Scaling
	connect(ui.xS, SIGNAL(valueChanged(double)), this, SLOT(scalingChanged()));
	connect(ui.yS, SIGNAL(valueChanged(double)), this, SLOT(scalingChanged()));
	connect(ui.zS, SIGNAL(valueChanged(double)), this, SLOT(scalingChanged()));

	// Rotation
	connect(ui.al, SIGNAL(valueChanged(double)), this, SLOT(rotationChanged()));
	connect(ui.be, SIGNAL(valueChanged(double)), this, SLOT(rotationChanged()));
	connect(ui.ga, SIGNAL(valueChanged(double)), this, SLOT(rotationChanged()));

	// Shading
	connect(ui.rbFlat, SIGNAL(toggled(bool)), this, SLOT(shadingToggled(bool)));
	connect(ui.rbSmooth, SIGNAL(toggled(bool)), this, SLOT(shadingToggled(bool)));

	// Apply changes
	connect(ui.buttonBox, SIGNAL(clicked(QAbstractButton*)), this, SLOT(buttonClicked(QAbstractButton*)));

	setModal(false);
}

MeshProperties::~MeshProperties()
{
}

void MeshProperties::setup(QObject* obj)
{
	connect(obj, SIGNAL(updatePropertiesDialog(const QJsonObject&)), this, SLOT(updateProperties(const QJsonObject&)));
	// Create modifyCmd if properties changed in dialog
	connect(this, SIGNAL(propertiesChanged(const QJsonObject&, Modifications)), obj, SLOT(modifyCmd(const QJsonObject&, Modifications)));
}

void MeshProperties::updateProperties(const QJsonObject& properties)
{
	// Name
	ui.nameEdit->setText(properties["name"].toString());

	// Visibility
	ui.cbDisabled->setChecked(properties["disabled"].toInt() != Qt::Unchecked);

	// Position
	const QJsonObject& pos = properties.find("Position")->toObject();
	ui.xP->setValue(pos.find("x")->toDouble()); // forward: z in DX11
	ui.yP->setValue(pos.find("y")->toDouble());  // right: x in DX1
	ui.zP->setValue(pos.find("z")->toDouble()); // up: y in DX11


	// Scaling
	const QJsonObject& scale = properties.find("Scaling")->toObject();
	ui.xS->setValue(scale.find("x")->toDouble());
	ui.yS->setValue(scale.find("y")->toDouble());
	ui.zS->setValue(scale.find("z")->toDouble());

	// Rotation
	const QJsonObject& rot = properties.find("Rotation")->toObject();
	ui.al->setValue(rot.find("al")->toDouble()); // Roll -> arround z in DX11
	ui.be->setValue(rot.find("be")->toDouble()); // Pitch -> arround x in DX11
	ui.ga->setValue(rot.find("ga")->toDouble()); // Yaw -> arround y in DX11

	//Shading
	if (properties.find("Shading")->toString() == "Smooth")
		ui.rbSmooth->setChecked(true);
	else // Flat is default
		ui.rbFlat->setChecked(true);

	m_properties = properties; // Copy properties
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
		{ "al", ui.al->value() },
		{ "be", ui.be->value() },
		{ "ga", ui.ga->value() }
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

void MeshProperties::buttonClicked(QAbstractButton* button)
{
	// Apply or Ok button was clicked
	QDialogButtonBox::StandardButton sb = ui.buttonBox->standardButton(button);
	if (sb == QDialogButtonBox::Apply || sb == QDialogButtonBox::Ok)
	{
		emit propertiesChanged(m_properties, Position|Scaling|Rotation|Visibility|Shading|Name);
	}
}