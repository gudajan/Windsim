#include "settings.h"
#include "common.h"

// Default values:
Settings conf = {
	// Camera
	{
		{ 0.1f, 2.0f }, // FirstPerson
		{ 0.3f, 0.01f, 10.0f, 0.1f }, // ModelView
		FirstPerson
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
	conf.cam.mv.defaultDist = std::stof(getIniVal(iniMap, "Camera", "ModelView.defaultDist", std::to_string(conf.cam.mv.defaultDist)));
	conf.cam.mv.zoomSpeed = std::stof(getIniVal(iniMap, "Camera", "ModelView.zoomSpeed", std::to_string(conf.cam.mv.zoomSpeed)));

	std::string type = conf.cam.type == FirstPerson ? "FirstPerson" : "ModelView";
	type = getIniVal(iniMap, "Camera", "Type", type);
	conf.cam.type = type == "ModelView" ? ModelView : FirstPerson;
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