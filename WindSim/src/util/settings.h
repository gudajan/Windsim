#ifndef SETTINGS_H
#define SETTINGS_H

#include <string>

#include "libini.hpp"
#include "common.h"

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

	struct Mesh
	{
		struct DefaultColor
		{
			int r;
			int g;
			int b;
		} dc;
		struct HoverColor
		{
			int r;
			int g;
			int b;
		} hc;
	} mesh;

	struct Grid
	{
		float scale; // The overall scale of the grid
		float step; // The size of one grid square => number of steps = scale / step
		float col; // grey color [0.0, 1.0]
	} grid;

	struct General
	{
		struct SelectionColor
		{
			int r;
			int g;
			int b;
		} sc;
		// Step size when transforming in the 3D view with steps
		float translationStep;
		float scalingStep;
		float rotationStep;
	} gen;

	struct Dynamics
	{
		bool useDynWorldForCalc; // Use dynamic world matrix for torque calculation
		bool showDynDuringMod; // Show dynamic transformation during object modification (e.g. translation)
	} dyn;
};



extern Settings conf;
extern void loadIni(const std::string& path);
extern const std::string& getIniVal(libini::ini_model& map, const std::string& category, const std::string& value, const std::string& default);

#endif