#ifndef COMMON_H
#define COMMON_H

#include <DirectXMath.h>

#include <cstdint>
#include <string>
#include <fstream>
#include <cmath>

#include <QMetaType>

#ifndef V_RETURN
#define V_RETURN(x)    { hr = (x); if( FAILED(hr) ) { return hr; } }
#endif

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p) = nullptr; } }
#endif

enum class ObjectType {Invalid, Mesh, Sky, Axes };

Q_DECLARE_METATYPE(ObjectType) // Necessary for qRegisterMetaType() and to pass ObjectType via signals/slots

static inline std::string objectTypeToString(ObjectType type)
{
	switch (type)
	{
	case(ObjectType::Mesh) : return "Mesh";
	case(ObjectType::Sky) : return "Sky";
	case(ObjectType::Axes) : return "Axes";
	}
	return "Invalid";
}

static inline ObjectType stringToObjectType(const std::string& str)
{
	if(str == "Mesh") return ObjectType::Mesh;
	else if (str == "Sky") return ObjectType::Sky;
	else if (str == "Axes") return ObjectType::Axes;
	return ObjectType::Invalid;
}

enum class Shading{ Smooth, Flat }; // Mesh Shading type

static inline float degToRad(float degree)
{
	return degree / 180.0f * DirectX::XM_PI;
}
static inline float radToDeg(float radians)
{
	return radians / DirectX::XM_PI * 180.0f;
}

// Normalize the given angle in radians to [0°, 360°], respectively returning a value between [0 - 2*PI]
static inline float normalizeRad(float rad)
{
	return rad - (std::floor(rad / (2 * DirectX::XM_PI)) * 2 * DirectX::XM_PI);
}

static std::string loadFile(const std::string& path)
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
	throw std::runtime_error("Failed to open the file '" + path + "' for reading");
}

static void storeFile(const std::string& path, const std::string& data)
{
	std::ofstream out(path, std::ios::out | std::ios::binary);
	if (out)
	{
		out.write(data.data(), data.size());
		out.close();
	}
	throw std::runtime_error("Failed to open the file '" + path + "' for writing");
}

#endif