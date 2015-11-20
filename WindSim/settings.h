#ifndef SETTINGS_H
#define SETTINGS_H

#include <string>

#include "libini.hpp"
#include "camera.h"

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
			float zoomSpeed; // The speed, the zoom changes with mouse wheel spin
		} mv;

		float defaultDist; // Default distance of the camera to the object
		CameraType type;

	} cam;
};



extern Settings conf;
extern void loadIni(const std::string& path);
extern const std::string& getIniVal(libini::ini_model& map, const std::string& category, const std::string& value, const std::string& default);

#endif