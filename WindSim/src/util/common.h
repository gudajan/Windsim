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

enum class ObjectType {Invalid, Mesh, Sky, Axes, Marker, VoxelGrid };

Q_DECLARE_METATYPE(ObjectType) // Necessary for qRegisterMetaType() and to pass ObjectType via signals/slots

static inline std::string objectTypeToString(ObjectType type)
{
	switch (type)
	{
	case(ObjectType::Mesh) : return "Mesh";
	case(ObjectType::Sky) : return "Sky";
	case(ObjectType::Axes) : return "Axes";
	case(ObjectType::Marker) : return "Marker";
	case(ObjectType::VoxelGrid) : return "VoxelGrid";
	}
	return "Invalid";
}

static inline ObjectType stringToObjectType(const std::string& str)
{
	if(str == "Mesh") return ObjectType::Mesh;
	else if (str == "Sky") return ObjectType::Sky;
	else if (str == "Axes") return ObjectType::Axes;
	else if (str == "Marker") return ObjectType::Marker;
	else if (str == "VoxelGrid") return ObjectType::VoxelGrid;
	return ObjectType::Invalid;
}

enum CameraType { FirstPerson, ModelView };

enum class Shading{ Smooth, Flat }; // Mesh Shading type

enum Orientation {XY_PLANE, XZ_PLANE, YZ_PLANE};

enum Modification
{
	Position = 0x1,
	Scaling = 0x2,
	Rotation = 0x4,
	Visibility = 0x8, // disabled/renderVoxel
	Voxelization = 0x10, // voxelize
	Shading = 0x20,
	Name = 0x40,
	Color = 0x80,
	Resolution = 0x100,
	VoxelSize = 0x200,
	SimulatorExe = 0x400,
	GlyphSettings = 0x800,

	All = UINT_MAX
};

Q_DECLARE_FLAGS(Modifications, Modification);
Q_DECLARE_OPERATORS_FOR_FLAGS(Modifications);

static inline float degToRad(float degrees)
{
	return degrees * DirectX::XM_PI / 180.0f;
}
static inline float radToDeg(float radians)
{
	return radians * 180.0f / DirectX::XM_PI;
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