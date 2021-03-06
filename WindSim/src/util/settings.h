#ifndef SETTINGS_H
#define SETTINGS_H

#include <string>

#include "common.h"

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
			float zoomSpeed; // The speed, the zoom changes with mouse wheel spin
		} mv;

		float defaultDist; // Default distance of the camera to the object
		CameraType type;

	} cam;

	struct OpenCL
	{
		int device;
		int platform;
		bool showInfo;
		bool printInfo;
	} opencl;

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
		struct SelectionColor
		{
			int r;
			int g;
			int b;
		} sc;
	} mesh;

	struct Grid
	{
		float scale; // The overall scale of the grid
		float step; // The size of one grid square => number of steps = scale / step
		float col; // grey color [0.0, 1.0]
	} grid;

	struct Transformation
	{
		// Step size when transforming in the 3D view with steps
		float translationStep;
		float scalingStep;
		float rotationStep;
	} trans;

	struct Dynamics
	{
		bool showDynDuringMod; // Show dynamic transformation during object modification (e.g. translation)
		DynamicsMethod method; // The method, used for calculating dynamics
		float frictionCoefficient; // The amount of velocity, which remains after one second without further force effect
	} dyn;
};



extern Settings conf;
extern void loadIni(const std::string& path);
extern void storeIni(const std::string& path);
extern const std::string& getIniVal(libini::ini_model& map, const std::string& category, const std::string& value, const std::string& default);

#endif