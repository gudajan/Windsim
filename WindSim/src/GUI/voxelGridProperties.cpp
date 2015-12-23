#include "voxelGridProperties.h"

VoxelGridProperties::VoxelGridProperties(QJsonObject properties, QWidget* parent)
	: QDialog(parent),
	m_properties(properties)
{
	ui.setupUi(this);

	updateProperties(m_properties);

	// Name
	connect(ui.nameEdit, SIGNAL(textChanged(const QString &)), this, SLOT(nameChanged(const QString &)));

	// Disabled
	connect(ui.cbDisabled, SIGNAL(stateChanged(int)), this, SLOT(disabledChanged(int)));

	// Show voxel
	connect(ui.cbShowVoxel, SIGNAL(stateChanged(int)), this, SLOT(showVoxelChanged(int)));

	// Position
	connect(ui.xP, SIGNAL(valueChanged(double)), this, SLOT(positionChanged()));
	connect(ui.yP, SIGNAL(valueChanged(double)), this, SLOT(positionChanged()));
	connect(ui.zP, SIGNAL(valueChanged(double)), this, SLOT(positionChanged()));

	// Resolution
	connect(ui.spResX, SIGNAL(valueChanged(int)), this, SLOT(resolutionChanged()));
	connect(ui.spResY, SIGNAL(valueChanged(int)), this, SLOT(resolutionChanged()));
	connect(ui.spResZ, SIGNAL(valueChanged(int)), this, SLOT(resolutionChanged()));

	// Voxel Size
	connect(ui.dspSizeX, SIGNAL(valueChanged(double)), this, SLOT(voxelSizeChanged()));
	connect(ui.dspSizeY, SIGNAL(valueChanged(double)), this, SLOT(voxelSizeChanged()));
	connect(ui.dspSizeZ, SIGNAL(valueChanged(double)), this, SLOT(voxelSizeChanged()));

	// Button
	connect(ui.buttonBox, SIGNAL(clicked(QAbstractButton*)), this, SLOT(buttonClicked(QAbstractButton*)));

	setModal(false);
}

void VoxelGridProperties::setup(QObject* obj)
{
	// Create modifyCmd if properties changed in dialog
	connect(this, SIGNAL(propertiesChanged(const QJsonObject&, Modifications)), obj, SLOT(modifyCmd(const QJsonObject&, Modifications)));
}

void VoxelGridProperties::updateProperties(const QJsonObject& properties)
{
	ObjectType type = stringToObjectType(properties["type"].toString().toStdString());
	if (type != ObjectType::VoxelGrid)
		return;

	// We just update the dialog with the changes, made outside of it. So we do not want to create any signals because of these changes
	// Doing so, would create annother invalid modify command
	blockSignals();
	{
		// Name
		ui.nameEdit->setText(properties["name"].toString());
		setWindowTitle("Voxel Grid Properties: " + ui.nameEdit->text());

		// Visibility
		ui.cbDisabled->setChecked(properties["disabled"].toInt() != Qt::Unchecked);
		ui.cbShowVoxel->setChecked(properties["renderVoxel"].toInt() == Qt::Checked);

		// Position
		const QJsonObject& pos = properties.find("Position")->toObject();
		ui.xP->setValue(pos.find("x")->toDouble()); // right: x in DX1
		ui.yP->setValue(pos.find("y")->toDouble()); // up: y in DX11
		ui.zP->setValue(pos.find("z")->toDouble()); // forward: z in DX11

		// Resolution
		const QJsonObject& res = properties.find("resolution")->toObject();
		ui.spResX->setValue(res.find("x")->toDouble());
		ui.spResY->setValue(res.find("y")->toDouble());
		ui.spResZ->setValue(res.find("z")->toDouble());

		// VoxelSize
		const QJsonObject& vs = properties.find("voxelSize")->toObject();
		ui.dspSizeX->setValue(vs.find("x")->toDouble());
		ui.dspSizeY->setValue(vs.find("y")->toDouble());
		ui.dspSizeZ->setValue(vs.find("z")->toDouble());

		m_properties = properties; // Copy properties
	}
	enableSignals();
}

void VoxelGridProperties::nameChanged(const QString& text)
{
	m_properties["name"] = text;
	emit propertiesChanged(m_properties, Name);
}

void VoxelGridProperties::disabledChanged(int state)
{
	m_properties["disabled"] = state;
	emit propertiesChanged(m_properties, Visibility);
}

void VoxelGridProperties::showVoxelChanged(int state)
{
	m_properties["renderVoxel"] = state;
	emit propertiesChanged(m_properties, Visibility);
}

void VoxelGridProperties::positionChanged()
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

void VoxelGridProperties::resolutionChanged()
{
	QJsonObject res
	{
		{ "x", ui.spResX->value() },
		{ "y", ui.spResY->value() },
		{ "z", ui.spResZ->value() }
	};
	m_properties["resolution"] = res;
}

void VoxelGridProperties::voxelSizeChanged()
{
	QJsonObject vs
	{
		{ "x", ui.dspSizeX->value() },
		{ "y", ui.dspSizeY->value() },
		{ "z", ui.dspSizeZ->value() }
	};

	m_properties["voxelSize"] = vs;
}

void VoxelGridProperties::buttonClicked(QAbstractButton* button)
{
	// Apply or Ok button was clicked
	QDialogButtonBox::StandardButton sb = ui.buttonBox->standardButton(button);
	if (sb == QDialogButtonBox::Apply || sb == QDialogButtonBox::Ok)
	{
		emit propertiesChanged(m_properties, Position | Name | Visibility | Resolution | VoxelSize );
	}
}

void VoxelGridProperties::blockSignals()
{
	ui.nameEdit->blockSignals(true);

	// Visibility
	ui.cbDisabled->blockSignals(true);
	ui.cbShowVoxel->blockSignals(true);

	// Position
	ui.xP->blockSignals(true);
	ui.yP->blockSignals(true);
	ui.zP->blockSignals(true);

	// Resolution
	ui.spResX->blockSignals(true);
	ui.spResY->blockSignals(true);
	ui.spResZ->blockSignals(true);

	// VoxelSize
	ui.dspSizeX->blockSignals(true);
	ui.dspSizeY->blockSignals(true);
	ui.dspSizeZ->blockSignals(true);
}

void VoxelGridProperties::enableSignals()
{
	ui.nameEdit->blockSignals(false);

	// Visibility
	ui.cbDisabled->blockSignals(false);
	ui.cbShowVoxel->blockSignals(false);

	// Position
	ui.xP->blockSignals(false);
	ui.yP->blockSignals(false);
	ui.zP->blockSignals(false);

	// Resolution
	ui.spResX->blockSignals(false);
	ui.spResY->blockSignals(false);
	ui.spResZ->blockSignals(false);

	// VoxelSize
	ui.dspSizeX->blockSignals(false);
	ui.dspSizeY->blockSignals(false);
	ui.dspSizeZ->blockSignals(false);
}
