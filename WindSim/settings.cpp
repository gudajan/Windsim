#include "settings.h"

// Default values:
Settings conf = {
	// Camera
	{
		{ 0.1f, 2.0f }, // FirstPerson
		{ 0.3f, 0.01f, 10.0f, 0.1f } // ModelView
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
}

std::string loadFile(const std::string& path)
{
	std::ifstream in(path, std::ios::in | std::ios::binary);
	if (in)
	{
		std::string str;
		in.seekg(0, std::ios::end);
		str.resize(in.tellg());
		in.seekg(0, std::ios::beg);
		in.read(&str[0], str.size());
		in.close();
		return str;
	}
	throw std::runtime_error("Failed to open the file '" + path + "'");
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