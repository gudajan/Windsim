#ifndef COMMON_H
#define COMMON_H

#include <DirectXMath.h>

#include <cstdint>
#include <cmath>

#ifndef V_RETURN
#define V_RETURN(x)    { hr = (x); if( FAILED(hr) ) { return hr; } }
#endif

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p) = nullptr; } }
#endif


struct Vertex
{
	DirectX::XMFLOAT3 p;
	DirectX::XMFLOAT3 n;
};

struct Triangle
{
	uint32_t a;
	uint32_t b;
	uint32_t c;
};

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

#endif