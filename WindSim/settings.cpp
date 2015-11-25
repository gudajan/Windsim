#include "settings.h"
#include "common.h"

// Default values:
Settings conf = {
	// Camera
	{
		{ 0.1f, 2.0f }, // FirstPerson
		{ 0.3f, 0.0025f, 0.1f }, // ModelView
		10.0f,
		FirstPerson
	},
	// Mesh
	{
		{ 204, 204, 204 }, // DefaultColor rgb
		{230, 230, 230} // Hover Color rgb
	},
	// Grid
	{ 16.0f, 1.0f, 0.5f}, // Scale, Stepsize, grey color
	//General
	{
		{190, 150, 150} // Selection Color
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

	conf.cam.defaultDist = std::stof(getIniVal(iniMap, "Camera", "defaultDist", std::to_string(conf.cam.defaultDist)));
	std::string type = conf.cam.type == FirstPerson ? "FirstPerson" : "ModelView";
	type = getIniVal(iniMap, "Camera", "Type", type);
	conf.cam.type = type == "ModelView" ? ModelView : FirstPerson;

	conf.mesh.dc.r = std::stoi(getIniVal(iniMap, "Mesh", "DefaultColor.red", std::to_string(conf.mesh.dc.r)));
	conf.mesh.dc.g = std::stoi(getIniVal(iniMap, "Mesh", "DefaultColor.green", std::to_string(conf.mesh.dc.g)));
	conf.mesh.dc.b = std::stoi(getIniVal(iniMap, "Mesh", "DefaultColor.blue", std::to_string(conf.mesh.dc.b)));

	conf.mesh.hc.r = std::stoi(getIniVal(iniMap, "Mesh", "HoverColor.red", std::to_string(conf.mesh.hc.r)));
	conf.mesh.hc.g = std::stoi(getIniVal(iniMap, "Mesh", "HoverColor.green", std::to_string(conf.mesh.hc.g)));
	conf.mesh.hc.b = std::stoi(getIniVal(iniMap, "Mesh", "HoverColor.blue", std::to_string(conf.mesh.hc.b)));

	conf.grid.scale = std::stoi(getIniVal(iniMap, "Grid", "scale", std::to_string(conf.grid.scale)));
	conf.grid.step = std::stoi(getIniVal(iniMap, "Grid", "stepsize", std::to_string(conf.grid.step)));
	conf.grid.col = std::stoi(getIniVal(iniMap, "Grid", "color", std::to_string(conf.grid.col)));

	conf.gen.sc.r = std::stoi(getIniVal(iniMap, "General", "SelectionColor.red", std::to_string(conf.gen.sc.r)));
	conf.gen.sc.g = std::stoi(getIniVal(iniMap, "General", "SelectionColor.green", std::to_string(conf.gen.sc.g)));
	conf.gen.sc.b = std::stoi(getIniVal(iniMap, "General", "SelectionColor.blue", std::to_string(conf.gen.sc.b)));
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