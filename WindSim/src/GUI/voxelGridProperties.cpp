#include "voxelGridProperties.h"

#include <WindTunnelLib/WindTunnel.h>

#include <QFileDialog>
#include <QMessageBox>

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

	// Simulator
	connect(ui.leSim, SIGNAL(textChanged(const QString &)), this, SLOT(simulatorSettingsChanged()));
	connect(ui.pbSim, SIGNAL(clicked()), this, SLOT(chooseSimulatorSettings()));

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

	// Glyph settings
	connect(ui.gbGlyph, SIGNAL(toggled(bool)), this, SLOT(glyphSettingsChanged()));
	connect(ui.rbOriX, SIGNAL(toggled(bool)), this, SLOT(glyphSettingsChanged()));
	connect(ui.rbOriY, SIGNAL(toggled(bool)), this, SLOT(glyphSettingsChanged()));
	connect(ui.rbOriZ, SIGNAL(toggled(bool)), this, SLOT(glyphSettingsChanged()));
	connect(ui.hsPos, SIGNAL(valueChanged(int)), this, SLOT(glyphSettingsChanged()));
	connect(ui.sbOriX, SIGNAL(valueChanged(int)), this, SLOT(glyphSettingsChanged()));
	connect(ui.sbOriY, SIGNAL(valueChanged(int)), this, SLOT(glyphSettingsChanged()));

	// WindTunnel

	// Run Simulation
	connect(ui.cbRunSim, SIGNAL(stateChanged(int)), this, SLOT(runSimulationChanged(int)));

	// Smoke settings
	connect(ui.gbSmoke, SIGNAL(toggled(bool)), this, SLOT(smokeSettingsChanged()));
	connect(ui.hsRadius, SIGNAL(valueChanged(int)), this, SLOT(smokeSettingsChanged()));
	connect(ui.hsSmokePosX, SIGNAL(valueChanged(int)), this, SLOT(smokeSettingsChanged()));
	connect(ui.hsSmokePosY, SIGNAL(valueChanged(int)), this, SLOT(smokeSettingsChanged()));
	connect(ui.hsSmokePosZ, SIGNAL(valueChanged(int)), this, SLOT(smokeSettingsChanged()));

	// Line settings
	connect(ui.gbLines, SIGNAL(toggled(bool)), this, SLOT(lineSettingsChanged()));
	connect(ui.cbLineOri, SIGNAL(currentIndexChanged(int)), this, SLOT(lineSettingsChanged()));
	connect(ui.cbLineType, SIGNAL(currentIndexChanged(int)), this, SLOT(lineSettingsChanged()));
	connect(ui.hsLinePos, SIGNAL(valueChanged(int)), this, SLOT(lineSettingsChanged()));

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

		// Glyph settings
		const QJsonObject& gs = properties.find("glyphs")->toObject();
		ui.gbGlyph->setChecked(gs["enabled"].toBool());
		Orientation o = static_cast<Orientation>(gs["orientation"].toInt());
		if (o == XY_PLANE) ui.rbOriZ->setChecked(true);
		else if (o == XZ_PLANE) ui.rbOriY->setChecked(true);
		else ui.rbOriX->setChecked(true);
		setSliderValue(ui.hsPos, gs["position"].toDouble());
		const QJsonObject& quant = gs["quantity"].toObject();
		ui.sbOriX->setValue(quant["x"].toInt());
		ui.sbOriY->setValue(quant["y"].toInt());

		// WindTunnel Settings

		// Run Simulation
		ui.cbRunSim->setChecked(properties["runSimulation"].toInt() == Qt::Checked);

		// Simulator
		ui.leSim->setText(properties["windTunnelSettings"].toString());

		// Smoke
		const QJsonObject& smoke = properties.find("smoke")->toObject();
		ui.gbSmoke->setChecked(smoke["enabled"].toBool());
		setSliderValue(ui.hsRadius, smoke["seedRadius"].toDouble());
		const QJsonObject& smokePos = smoke["seedPosition"].toObject();
		setSliderValue(ui.hsSmokePosX, smokePos["x"].toDouble());
		setSliderValue(ui.hsSmokePosY, smokePos["y"].toDouble());
		setSliderValue(ui.hsSmokePosZ, smokePos["z"].toDouble());

		// Lines
		const QJsonObject& lines = properties.find("lines")->toObject();
		ui.gbLines->setChecked(lines["enabled"].toBool());
		ui.cbLineOri->setCurrentText(lines["orientation"].toString());
		ui.cbLineType->setCurrentText(lines["type"].toString());
		setSliderValue(ui.hsLinePos, lines["position"].toDouble());

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

void VoxelGridProperties::simulatorSettingsChanged()
{
	m_properties["windTunnelSettings"] = ui.leSim->text();
}

void VoxelGridProperties::chooseSimulatorSettings()
{
	QString settings = QFileDialog::getOpenFileName(this, tr("Choose WindTunnel settings"), QString(), tr("Json-files (*.json *.txt)"));

	if (settings.isEmpty())
		return;

	ui.leSim->setText(settings);

	m_properties["windTunnelSettings"] = settings;
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
	// New resolutions
	int nrx = ui.spResX->value();
	int nry = ui.spResY->value();
	int nrz = ui.spResZ->value();

	// If linked to voxel size: recalculate it
	if (ui.tbLink->isChecked())
	{
		// Old resolutions
		QJsonObject or = m_properties["resolution"].toObject();
		int orx = or["x"].toInt();
		int ory = or["y"].toInt();
		int orz = or["z"].toInt();

		// Calculate new voxel sizes
		double nvsx = ui.dspSizeX->value() * orx / nrx;
		double nvsy = ui.dspSizeY->value() * ory / nry;
		double nvsz = ui.dspSizeZ->value() * orz / nrz;

		// Set new voxel size
		ui.dspSizeX->blockSignals(true);
		ui.dspSizeY->blockSignals(true);
		ui.dspSizeZ->blockSignals(true);

		ui.dspSizeX->setValue(nvsx);
		ui.dspSizeY->setValue(nvsy);
		ui.dspSizeZ->setValue(nvsz);

		ui.dspSizeX->blockSignals(false);
		ui.dspSizeY->blockSignals(false);
		ui.dspSizeZ->blockSignals(false);

		QJsonObject vs
		{
			{ "x", nvsx },
			{ "y", nvsy },
			{ "z", nvsz }
		};
		m_properties["voxelSize"] = vs;
	}

	QJsonObject res
	{
		{ "x", nrx },
		{ "y", nry },
		{ "z", nrz }
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

void VoxelGridProperties::glyphSettingsChanged()
{
	QJsonObject quantity{ { "x", ui.sbOriX->value() }, { "y", ui.sbOriY->value() } };
	QJsonObject gs
	{
		{ "enabled", ui.gbGlyph->isChecked() },
		{ "orientation", ui.rbOriX->isChecked() ? YZ_PLANE : ui.rbOriY->isChecked() ? XZ_PLANE : XY_PLANE },
		{ "position", sliderFloatValue(ui.hsPos) }, // Assume minimum slider value is zero -> transform to [0 - 1]
		{ "quantity", quantity }
	};
	m_properties["glyphs"] = gs;

	emit propertiesChanged( m_properties, GlyphSettings);
}

void VoxelGridProperties::runSimulationChanged(int state)
{
	m_properties["runSimulation"] = state;
	emit propertiesChanged(m_properties, RunSimulation);
}

void VoxelGridProperties::smokeSettingsChanged()
{
	QJsonObject position
	{
		{ "x", sliderFloatValue(ui.hsSmokePosX) },
		{ "y", sliderFloatValue(ui.hsSmokePosY) },
		{ "z", sliderFloatValue(ui.hsSmokePosZ) }
	};
	QJsonObject smoke
	{
		{ "enabled", ui.gbSmoke->isChecked() },
		{ "seedRadius", sliderFloatValue(ui.hsRadius) },
		{ "seedPosition", position }
	};
	m_properties["smoke"] = smoke;

	emit propertiesChanged(m_properties, SmokeSettings);
}

void VoxelGridProperties::lineSettingsChanged()
{
	QJsonObject lines
	{
		{ "enabled", ui.gbLines->isChecked() },
		{ "orientation", ui.cbLineOri->currentText() },
		{ "type", ui.cbLineType->currentText() },
		{ "position", sliderFloatValue(ui.hsLinePos) }
	};
	m_properties["lines"] = lines;

	emit propertiesChanged(m_properties, LineSettings);
}

void VoxelGridProperties::buttonClicked(QAbstractButton* button)
{
	// Apply or Ok button was clicked
	QDialogButtonBox::StandardButton sb = ui.buttonBox->standardButton(button);
	if (sb == QDialogButtonBox::Apply || sb == QDialogButtonBox::Ok)
	{
		emit propertiesChanged(m_properties, Position | Name | Voxelization | Visibility | Resolution | VoxelSize | WindTunnelSettings | GlyphSettings | RunSimulation | SmokeSettings | LineSettings);
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

	// GlyphSettings
	ui.gbGlyph->blockSignals(true);
	ui.rbOriX->blockSignals(true);
	ui.rbOriY->blockSignals(true);
	ui.rbOriZ->blockSignals(true);
	ui.hsPos->blockSignals(true);
	ui.sbOriX->blockSignals(true);
	ui.sbOriY->blockSignals(true);

	// WindTunnel

	// Simulator
	ui.leSim->blockSignals(true);

	// Run Simulation
	ui.cbRunSim->blockSignals(true);

	// Smoke
	ui.gbSmoke->blockSignals(true);
	ui.hsRadius->blockSignals(true);
	ui.hsSmokePosX->blockSignals(true);
	ui.hsSmokePosY->blockSignals(true);
	ui.hsSmokePosZ->blockSignals(true);

	// Lines
	ui.gbLines->blockSignals(true);
	ui.cbLineOri->blockSignals(true);
	ui.cbLineType->blockSignals(true);
	ui.hsLinePos->blockSignals(true);
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

	// GlyphSettings
	ui.gbGlyph->blockSignals(false);
	ui.rbOriX->blockSignals(false);
	ui.rbOriY->blockSignals(false);
	ui.rbOriZ->blockSignals(false);
	ui.hsPos->blockSignals(false);
	ui.sbOriX->blockSignals(false);
	ui.sbOriY->blockSignals(false);

	// WindTunnel

	// Simulator
	ui.leSim->blockSignals(false);

	// Run Simulation
	ui.cbRunSim->blockSignals(false);

	// Smoke
	ui.gbSmoke->blockSignals(false);
	ui.hsRadius->blockSignals(false);
	ui.hsSmokePosX->blockSignals(false);
	ui.hsSmokePosY->blockSignals(false);
	ui.hsSmokePosZ->blockSignals(false);

	// Lines
	ui.gbLines->blockSignals(false);
	ui.cbLineOri->blockSignals(false);
	ui.cbLineType->blockSignals(false);
	ui.hsLinePos->blockSignals(false);
}
