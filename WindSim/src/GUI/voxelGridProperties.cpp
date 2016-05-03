#include "voxelGridProperties.h"

#include <QFileDialog>
#include <QMessageBox>

VoxelGridProperties::VoxelGridProperties(QJsonObject properties, QWidget* parent)
	: QDialog(parent, Qt::WindowTitleHint | Qt::WindowCloseButtonHint),
	m_properties(properties)
{
	ui.setupUi(this);
	ui.gradient->setColorSpinBoxes(ui.spR, ui.spG, ui.spB, ui.spA);
	ui.gradient->setRangeSpinBoxes(ui.dspRangeMin, ui.dspRangeMax);

	ui.buttonBox->button(QDialogButtonBox::Apply)->setDefault(true);

	updateProperties(m_properties);

	// Name
	connect(ui.nameEdit, SIGNAL(textChanged(const QString &)), this, SLOT(nameChanged(const QString &)));

	// Enabled
	connect(ui.cbEnabled, SIGNAL(stateChanged(int)), this, SLOT(enabledChanged(int)));

	// Simulator
	connect(ui.leSim, SIGNAL(textChanged(const QString &)), this, SLOT(simulatorSettingsChanged()));
	connect(ui.pbSim, SIGNAL(clicked()), this, SLOT(chooseSimulatorSettings()));
	connect(ui.pbReinit, SIGNAL(clicked()), this, SLOT(restartSimulation()));

	// Voxel settings
	connect(ui.gbVoxel, SIGNAL(toggled(bool)), this, SLOT(voxelSettingsChanged()));
	connect(ui.rbSolid, SIGNAL(toggled(bool)), this, SLOT(voxelSettingsChanged()));
	connect(ui.rbWireframe, SIGNAL(toggled(bool)), this, SLOT(voxelSettingsChanged()));

	// Position
	connect(ui.xP, SIGNAL(valueChanged(double)), this, SLOT(positionChanged()));
	connect(ui.yP, SIGNAL(valueChanged(double)), this, SLOT(positionChanged()));
	connect(ui.zP, SIGNAL(valueChanged(double)), this, SLOT(positionChanged()));

	// Resolution
	connect(ui.spResX, SIGNAL(valueChanged(int)), this, SLOT(resolutionChanged()));
	connect(ui.spResY, SIGNAL(valueChanged(int)), this, SLOT(resolutionChanged()));
	connect(ui.spResZ, SIGNAL(valueChanged(int)), this, SLOT(resolutionChanged()));

	// Grid Size
	connect(ui.dspSizeX, SIGNAL(valueChanged(double)), this, SLOT(gridSizeChanged()));
	connect(ui.dspSizeY, SIGNAL(valueChanged(double)), this, SLOT(gridSizeChanged()));
	connect(ui.dspSizeZ, SIGNAL(valueChanged(double)), this, SLOT(gridSizeChanged()));

	// Glyph settings
	connect(ui.gbGlyph, SIGNAL(toggled(bool)), this, SLOT(glyphSettingsChanged()));
	connect(ui.rbOriX, SIGNAL(toggled(bool)), this, SLOT(glyphSettingsChanged()));
	connect(ui.rbOriY, SIGNAL(toggled(bool)), this, SLOT(glyphSettingsChanged()));
	connect(ui.rbOriZ, SIGNAL(toggled(bool)), this, SLOT(glyphSettingsChanged()));
	connect(ui.hsPos, SIGNAL(valueChanged(int)), this, SLOT(glyphSettingsChanged()));
	connect(ui.sbOriX, SIGNAL(valueChanged(int)), this, SLOT(glyphSettingsChanged()));
	connect(ui.sbOriY, SIGNAL(valueChanged(int)), this, SLOT(glyphSettingsChanged()));

	// Volume settings
	connect(ui.cmbMetric, SIGNAL(currentTextChanged(const QString&)), this, SLOT(switchVolumeMetric(const QString&)));
	connect(ui.gbVolume, SIGNAL(toggled(bool)), this, SLOT(volumeSettingsChanged()));
	connect(ui.hsStepSize, SIGNAL(valueChanged(int)), this, SLOT(volumeSettingsChanged()));
	connect(ui.gradient, SIGNAL(transferFunctionChanged()), this, SLOT(volumeSettingsChanged()));

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
	// Trigger function of voxel grid if necessary
	connect(this, SIGNAL(triggerFunction(const QJsonObject&)), obj, SLOT(onTriggerFunction(const QJsonObject&)));
}

void VoxelGridProperties::updateProperties(const QJsonObject& properties)
{
	ObjectType type = stringToObjectType(properties["type"].toString().toStdString());
	if (type != ObjectType::VoxelGrid)
		return;

	// We just update the dialog with the changes, made outside of it. So we do not want to create any signals because of these changes
	// Doing so, would create annother invalid modify command
	blockSignals(true);
	{
		// Name
		ui.nameEdit->setText(properties["name"].toString());
		setWindowTitle("Voxel Grid Properties: " + ui.nameEdit->text());

		// Visibility
		ui.cbEnabled->setChecked(properties["enabled"].toBool());

		// Voxel settings
		const QJsonObject& voxel = properties.find("voxel")->toObject();
		ui.gbVoxel->setChecked(voxel["enabled"].toBool());
		VoxelType t = VoxelType(voxel["type"].toInt());
		if (t == Solid) ui.rbSolid->setChecked(true);
		else ui.rbWireframe->setChecked(true);

		// Position
		const QJsonObject& pos = properties.find("position")->toObject();
		ui.xP->setValue(pos.find("x")->toDouble()); // right: x in DX1
		ui.yP->setValue(pos.find("y")->toDouble()); // up: y in DX11
		ui.zP->setValue(pos.find("z")->toDouble()); // forward: z in DX11

		// Resolution
		const QJsonObject& res = properties.find("resolution")->toObject();
		ui.spResX->setValue(res.find("x")->toDouble());
		ui.spResY->setValue(res.find("y")->toDouble());
		ui.spResZ->setValue(res.find("z")->toDouble());

		// GridSize
		const QJsonObject& s = properties.find("gridSize")->toObject();
		ui.dspSizeX->setValue(s.find("x")->toDouble());
		ui.dspSizeY->setValue(s.find("y")->toDouble());
		ui.dspSizeZ->setValue(s.find("z")->toDouble());

		// Glyph settings
		const QJsonObject& gs = properties.find("glyphs")->toObject();
		ui.gbGlyph->setChecked(gs["enabled"].toBool());
		Orientation o = static_cast<Orientation>(gs["orientation"].toInt());
		QString key = "";
		if (o == XY_PLANE)
		{
			ui.rbOriZ->setChecked(true);
			key = "XY";
		}
		else if (o == XZ_PLANE)
		{
			ui.rbOriY->setChecked(true);
			key = "XZ";
		}
		else
		{
			ui.rbOriX->setChecked(true);
			key = "YZ";
		}
		setSliderValue(ui.hsPos, gs["position"].toObject()[key].toDouble());
		const QJsonObject& quant = gs["quantity"].toObject();
		ui.sbOriX->setValue(quant["x"].toInt());
		ui.sbOriY->setValue(quant["y"].toInt());

		// Volume settings
		const QJsonObject& volume = properties.find("volume")->toObject();
		ui.gbVolume->setChecked(volume["enabled"].toBool());
		setSliderValue(ui.hsStepSize, volume["stepSize"].toDouble());
		const QString& metric = volume["metric"].toString();
		ui.cmbMetric->setCurrentText(Metric::toGUI(metric));
		const QJsonObject& fcntns = volume["transferFunctions"].toObject();
		for (auto jit = fcntns.begin(); jit != fcntns.end(); ++jit)
		{
			TransferFunction txfn = TransferFunction::fromJson(jit->toObject());
			ui.gradient->setTransferFunction(jit.key(), txfn);
		}
		switchVolumeMetric(ui.cmbMetric->currentText());

		// WindTunnel Settings

		// Run Simulation
		ui.cbRunSim->setChecked(properties["runSimulation"].toBool());

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
		setSliderValue(ui.hsLinePos, lines["position"].toObject()[ui.cbLineOri->currentText()].toDouble());

		m_properties = properties; // Copy properties
	}
	blockSignals(false);
}

void VoxelGridProperties::nameChanged(const QString& text)
{
	m_properties["name"] = text;
	emit propertiesChanged(m_properties, Name);
}

void VoxelGridProperties::enabledChanged(int state)
{
	m_properties["enabled"] = state == Qt::Checked ? true : false;
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

void VoxelGridProperties::voxelSettingsChanged()
{
	QJsonObject vs
	{
		{ "enabled", ui.gbVoxel->isChecked() },
		{ "type", ui.rbSolid->isChecked() ? Solid : Wireframe }
	};
	m_properties["voxel"] = vs;

	emit propertiesChanged(m_properties, VoxelSettings);
}

void VoxelGridProperties::positionChanged()
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

void VoxelGridProperties::gridSizeChanged()
{
	QJsonObject vs
	{
		{ "x", ui.dspSizeX->value() },
		{ "y", ui.dspSizeY->value() },
		{ "z", ui.dspSizeZ->value() }
	};

	m_properties["gridSize"] = vs;
}

void VoxelGridProperties::glyphSettingsChanged()
{
	QJsonObject old = m_properties["glyphs"].toObject();
	QJsonObject position = old["position"].toObject();
	Orientation oldOri = Orientation(old["orientation"].toInt());
	Orientation newOri = ui.rbOriX->isChecked() ? YZ_PLANE : ui.rbOriY->isChecked() ? XZ_PLANE : XY_PLANE;
	QString oriKey = ui.rbOriX->isChecked() ? "YZ" : ui.rbOriY->isChecked() ? "XZ" : "XY";
	if (oldOri != newOri) // Change position to one of new orientation
	{
		ui.hsPos->blockSignals(true);
		setSliderValue(ui.hsPos, position[oriKey].toDouble());
		ui.hsPos->blockSignals(false);
	}
	else // Update position for current orientation
	{
		position[oriKey] = sliderFloatValue(ui.hsPos);
	}

	QJsonObject quantity{ { "x", ui.sbOriX->value() }, { "y", ui.sbOriY->value() } };
	QJsonObject gs
	{
		{ "enabled", ui.gbGlyph->isChecked() },
		{ "orientation", ui.rbOriX->isChecked() ? YZ_PLANE : ui.rbOriY->isChecked() ? XZ_PLANE : XY_PLANE },
		{ "position", position },
		{ "quantity", quantity }
	};
	m_properties["glyphs"] = gs;

	emit propertiesChanged( m_properties, GlyphSettings);
}

void VoxelGridProperties::volumeSettingsChanged()
{
	QString currentMetric = Metric::fromGUI(ui.cmbMetric->currentText());

	// Only update modified transfer function
	QJsonObject functions = m_properties["volume"].toObject()["transferFunctions"].toObject();
	functions[currentMetric] = ui.gradient->getTransferFunction(currentMetric).toJson();

	QJsonObject volume
	{
		{ "enabled", ui.gbVolume->isChecked() },
		{ "stepSize", sliderFloatValue(ui.hsStepSize) },
		{ "metric", currentMetric },
		{ "transferFunctions", functions }
	};

	m_properties["volume"] = volume;

	emit propertiesChanged(m_properties, VolumeSettings);
}

void VoxelGridProperties::switchVolumeMetric(const QString& metric)
{
	ui.gradient->switchToMetric(Metric::fromGUI(metric));
}

void VoxelGridProperties::runSimulationChanged(int state)
{
	m_properties["runSimulation"] = state == Qt::Checked ? true : false;
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
	QJsonObject old = m_properties["lines"].toObject();
	QJsonObject position = old["position"].toObject();
	QString ori = old["orientation"].toString();
	if (ui.cbLineOri->currentText() != ori)
	{
		ui.hsLinePos->blockSignals(true);
		setSliderValue(ui.hsLinePos, position[ui.cbLineOri->currentText()].toDouble());
		ui.hsLinePos->blockSignals(false);
	}
	else
	{
		position[ui.cbLineOri->currentText()] = sliderFloatValue(ui.hsLinePos);
	}

	QJsonObject lines
	{
		{ "enabled", ui.gbLines->isChecked() },
		{ "orientation", ui.cbLineOri->currentText() },
		{ "type", ui.cbLineType->currentText() },
		{ "position", position }
	};
	m_properties["lines"] = lines;

	emit propertiesChanged(m_properties, LineSettings);
}

void VoxelGridProperties::restartSimulation()
{
	QJsonObject data{ { "id", m_properties["id"].toInt() }, { "function", "restartSimulation" } };
	emit triggerFunction(data);
}

void VoxelGridProperties::buttonClicked(QAbstractButton* button)
{
	// Apply or Ok button was clicked
	QDialogButtonBox::StandardButton sb = ui.buttonBox->standardButton(button);
	if (sb == QDialogButtonBox::Apply || sb == QDialogButtonBox::Ok)
	{
		emit propertiesChanged(m_properties, All);
	}
}

void VoxelGridProperties::blockSignals(bool b)
{
	ui.nameEdit->blockSignals(b);

	// Visibility
	ui.cbEnabled->blockSignals(b);

	// Voxel settings
	ui.gbVoxel->blockSignals(b);
	ui.rbSolid->blockSignals(b);
	ui.rbWireframe->blockSignals(b);

	// Position
	ui.xP->blockSignals(b);
	ui.yP->blockSignals(b);
	ui.zP->blockSignals(b);

	// Resolution
	ui.spResX->blockSignals(b);
	ui.spResY->blockSignals(b);
	ui.spResZ->blockSignals(b);

	// GridSize
	ui.dspSizeX->blockSignals(b);
	ui.dspSizeY->blockSignals(b);
	ui.dspSizeZ->blockSignals(b);

	// GlyphSettings
	ui.gbGlyph->blockSignals(b);
	ui.rbOriX->blockSignals(b);
	ui.rbOriY->blockSignals(b);
	ui.rbOriZ->blockSignals(b);
	ui.hsPos->blockSignals(b);
	ui.sbOriX->blockSignals(b);
	ui.sbOriY->blockSignals(b);

	// VolumeSettings
	ui.gbVolume->blockSignals(b);
	ui.cmbMetric->blockSignals(b);
	ui.hsStepSize->blockSignals(b);
	ui.gradient->blockSignals(b);

	// WindTunnel

	// Simulator
	ui.leSim->blockSignals(b);

	// Run Simulation
	ui.cbRunSim->blockSignals(b);

	// Smoke
	ui.gbSmoke->blockSignals(b);
	ui.hsRadius->blockSignals(b);
	ui.hsSmokePosX->blockSignals(b);
	ui.hsSmokePosY->blockSignals(b);
	ui.hsSmokePosZ->blockSignals(b);

	// Lines
	ui.gbLines->blockSignals(b);
	ui.cbLineOri->blockSignals(b);
	ui.cbLineType->blockSignals(b);
	ui.hsLinePos->blockSignals(b);
}
