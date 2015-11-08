#ifndef SETTINGS_H
#define SETTINGS_H

#include <string>
#include <fstream>

#include "libini.hpp"

struct Settings
{
	struct Camera
	{
		struct FirstPerson
		{
			float rotationSpeed;
			float translationSpeed;
		} fp;

		struct ModelView
		{
			float rotationSpeed;
			float translationSpeed;
			float defaultDist; // Default distance of the camera to the object, corresponding to zoom factor 1.0
			float zoomSpeed;
		} mv;
	} cam;
};



extern Settings conf;
extern void loadIni(const std::string& path);

extern std::string loadFile(const std::string& path);
extern const std::string& getIniVal(libini::ini_model& map, const std::string& category, const std::string& value, const std::string& default);

#endif