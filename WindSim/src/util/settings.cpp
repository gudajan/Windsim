#include "settings.h"

// Default values:
Settings conf = {
	// Camera
	{
		{ 0.1f, 2.0f }, // FirstPerson
		{ 0.3f, 0.0018f, 0.1f }, // ModelView
		10.0f,
		FirstPerson
	},
	// OpenCL
	{
		-1, // device
		-1, // platform
		false, // showInfo
		false // printInfo
	},
	// Mesh
	{
		{ 204, 204, 204 }, // DefaultColor rgb
		{ 170, 255, 255 }, // Hover Color rgb
		{ 255, 170, 50 } // Selection Color
	},
	// Grid
	{ 16.0f, 1.0f, 0.5f}, // Scale, Stepsize, grey color
	//Transformation
	{
		0.5, // translationStep
		0.5, // scalingStep
		10 // RotationStep
	},

	// Dynamics
	{
		false,
		Pressure,
		0.85
	}
};


void loadIni(const std::string& path)
{
	libini::ini_model iniMap;

	iniMap = libini::parse(loadFile(path));

	conf.cam.fp.rotationSpeed = std::stof(getIniVal(iniMap, "Camera", "FirstPerson.rotationSpeed", std::to_string(conf.cam.fp.rotationSpeed)));
	conf.cam.fp.translationSpeed = std::stof(getIniVal(iniMap, "Camera", "FirstPerson.translationSpeed", std::to_string(conf.cam.fp.translationSpeed)));

	conf.cam.mv.rotationSpeed = std::stof(getIniVal(iniMap, "Camera", "ModelView.rotationSpeed", std::to_string(conf.cam.mv.rotationSpeed)));
	conf.cam.mv.translationSpeed = std::stof(getIniVal(iniMap, "Camera", "ModelView.translationSpeed", std::to_string(conf.cam.mv.translationSpeed)));
	conf.cam.mv.zoomSpeed = std::stof(getIniVal(iniMap, "Camera", "ModelView.zoomSpeed", std::to_string(conf.cam.mv.zoomSpeed)));

	conf.cam.defaultDist = std::stof(getIniVal(iniMap, "Camera", "DefaultDist", std::to_string(conf.cam.defaultDist)));
	std::string type = conf.cam.type == FirstPerson ? "FirstPerson" : "ModelView";
	type = getIniVal(iniMap, "Camera", "Type", type);
	conf.cam.type = type == "ModelView" ? ModelView : FirstPerson;

	conf.opencl.device = std::stoi(getIniVal(iniMap, "OpenCL", "Device", std::to_string(conf.opencl.device)));
	conf.opencl.platform = std::stoi(getIniVal(iniMap, "OpenCL", "Platform", std::to_string(conf.opencl.platform)));
	conf.opencl.showInfo = std::stoi(getIniVal(iniMap, "OpenCL", "ShowInfo", std::to_string(conf.opencl.showInfo)));
	conf.opencl.printInfo = std::stoi(getIniVal(iniMap, "OpenCL", "PrintInfo", std::to_string(conf.opencl.printInfo)));

	conf.mesh.dc.r = std::stoi(getIniVal(iniMap, "Mesh", "DefaultColor.red", std::to_string(conf.mesh.dc.r)));
	conf.mesh.dc.g = std::stoi(getIniVal(iniMap, "Mesh", "DefaultColor.green", std::to_string(conf.mesh.dc.g)));
	conf.mesh.dc.b = std::stoi(getIniVal(iniMap, "Mesh", "DefaultColor.blue", std::to_string(conf.mesh.dc.b)));

	conf.mesh.hc.r = std::stoi(getIniVal(iniMap, "Mesh", "HoverColor.red", std::to_string(conf.mesh.hc.r)));
	conf.mesh.hc.g = std::stoi(getIniVal(iniMap, "Mesh", "HoverColor.green", std::to_string(conf.mesh.hc.g)));
	conf.mesh.hc.b = std::stoi(getIniVal(iniMap, "Mesh", "HoverColor.blue", std::to_string(conf.mesh.hc.b)));

	conf.mesh.sc.r = std::stoi(getIniVal(iniMap, "Mesh", "SelectionColor.red", std::to_string(conf.mesh.sc.r)));
	conf.mesh.sc.g = std::stoi(getIniVal(iniMap, "Mesh", "SelectionColor.green", std::to_string(conf.mesh.sc.g)));
	conf.mesh.sc.b = std::stoi(getIniVal(iniMap, "Mesh", "SelectionColor.blue", std::to_string(conf.mesh.sc.b)));

	conf.grid.scale = std::stof(getIniVal(iniMap, "CoordinateGrid", "Scale", std::to_string(conf.grid.scale)));
	conf.grid.step = std::stof(getIniVal(iniMap, "CoordinateGrid", "StepDist", std::to_string(conf.grid.step)));
	conf.grid.col = std::stof(getIniVal(iniMap, "CoordinateGrid", "Brightness", std::to_string(conf.grid.col)));

	conf.trans.translationStep = std::stof(getIniVal(iniMap, "Transformation", "TranslationStep", std::to_string(conf.trans.translationStep)));
	conf.trans.scalingStep = std::stof(getIniVal(iniMap, "Transformation", "ScalingStep", std::to_string(conf.trans.scalingStep)));
	conf.trans.rotationStep = std::stof(getIniVal(iniMap, "Transformation", "RotationStep", std::to_string(conf.trans.rotationStep)));

	conf.dyn.showDynDuringMod = std::stoi(getIniVal(iniMap, "Dynamics", "ShowDynDuringMod", std::to_string(conf.dyn.showDynDuringMod)));
	conf.dyn.frictionCoefficient = std::stof(getIniVal(iniMap, "Dynamics", "FrictionCoefficient", std::to_string(conf.dyn.frictionCoefficient)));
	std::string method = conf.dyn.method == Pressure ? "Pressure" : "Velocity";
	method = getIniVal(iniMap, "Dynamics", "Method", method);
	conf.dyn.method= method == "Pressure" ? Pressure : Velocity;
}

void storeIni(const std::string& path)
{
	std::stringstream out;

	out << "[OpenCL]\n";
	out << "Platform=" << conf.opencl.platform << std::endl;
	out << "Device=" << conf.opencl.device << std::endl;
	out << "ShowInfo=" << conf.opencl.showInfo << std::endl;
	out << "PrintInfo=" << conf.opencl.printInfo << std::endl;
	out << std::endl;
	out << "[Dynamics]\n";
	out << "ShowDynDuringMod=" << conf.dyn.showDynDuringMod << std::endl;
	out << "FrictionCoefficient=" << conf.dyn.frictionCoefficient << std::endl;
	out << "Method=" << (conf.dyn.method == Pressure ? "Pressure" : "Velocity") << std::endl;
	out << std::endl;
	out << "[Camera]\n";
	out << "FirstPerson.rotationSpeed=" << conf.cam.fp.rotationSpeed << std::endl;
	out << "FirstPerson.translationSpeed=" << conf.cam.fp.translationSpeed << std::endl;
	out << "ModelView.rotationSpeed=" << conf.cam.mv.rotationSpeed << std::endl;
	out << "ModelView.translationSpeed=" << conf.cam.mv.translationSpeed << std::endl;
	out << "ModelView.zoomSpeed=" << conf.cam.mv.zoomSpeed << std::endl;
	out << "DefaultDist=" << conf.cam.defaultDist << std::endl;
	out << "Type=" << (conf.cam.type == ModelView ? "ModelView" : "FirstPerson") << std::endl;
	out << std::endl;
	out << "[Transformation]\n";
	out << "TranslationStep=" << conf.trans.translationStep << std::endl;
	out << "ScalingStep=" << conf.trans.scalingStep << std::endl;
	out << "RotationStep=" << conf.trans.rotationStep << std::endl;
	out << std::endl;
	out << "[Mesh]\n";
	out << "DefaultColor.red=" << conf.mesh.dc.r << std::endl;
	out << "DefaultColor.green=" << conf.mesh.dc.g << std::endl;
	out << "DefaultColor.blue=" << conf.mesh.dc.b << std::endl;
	out << "HoverColor.red=" << conf.mesh.hc.r << std::endl;
	out << "HoverColor.green=" << conf.mesh.hc.g << std::endl;
	out << "HoverColor.blue=" << conf.mesh.hc.b << std::endl;
	out << "SelectionColor.red=" << conf.mesh.sc.r << std::endl;
	out << "SelectionColor.green=" << conf.mesh.sc.g << std::endl;
	out << "SelectionColor.blue=" << conf.mesh.sc.b << std::endl;
	out << std::endl;
	out << "[CoordinateGrid]\n";
	out << "Scale=" << conf.grid.scale << std::endl;
	out << "StepDist=" << conf.grid.step << std::endl;
	out << "Brightness=" << conf.grid.col << std::endl;
	out << std::endl;

	storeFile(path, out.str());
}

const std::string& getIniVal(libini::ini_model& map, const std::string& category, const std::string& value, const std::string& default)
{
	auto catIt = map.find(category);
	if (catIt != map.end())
	{
		auto it = catIt->second.find(value);
		if (it != catIt->second.end())
		{
			return map[category][value];
		}
	}
	return default;
}