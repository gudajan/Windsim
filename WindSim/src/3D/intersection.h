#ifndef INTERSECTION_H
#define INTERSECTION_H

#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <vector>
#include <limits>
#include <future>
#include <algorithm>

namespace Geometry
{

	// Assuming a vertex in the vertexBuffer has the size vertexSize in bytes and the position consists of 3 floats at the beginning of each vertex
	// TODO perhabs solve with lambda for getting vertex position to allow arbitrary layouts
	static bool intersect(const DirectX::XMFLOAT3&  origin, const DirectX::XMFLOAT3& direction, const std::vector<float>& vb, const std::vector<uint32_t>& ib, int vertexSize, float& distance)
	{
		using namespace DirectX;

		const XMVECTOR dir = XMLoadFloat3(&direction);
		const XMVECTOR ori = XMLoadFloat3(&origin);

		auto function = [dir, ori, vb, ib, vertexSize](int start, int end)
		{
			float dist = std::numeric_limits<float>::infinity();
			// Iterate each triangle
			for (int i = start; i < end; i += 3)
			{
				int i0 = ib[i] * vertexSize; // Real index into vertexbuffer
				int i1 = ib[i + 1] * vertexSize;
				int i2 = ib[i + 2] * vertexSize;

				const XMVECTOR p0 = XMVectorSet(vb[i0], vb[i0 + 1], vb[i0 + 2], 1.0f);
				const XMVECTOR p1 = XMVectorSet(vb[i1], vb[i1 + 1], vb[i1 + 2], 1.0f);
				const XMVECTOR p2 = XMVectorSet(vb[i2], vb[i2 + 1], vb[i2 + 2], 1.0f);

				// Intersect triangle
				float curDist = std::numeric_limits<float>::infinity();
				bool intersects = TriangleTests::Intersects(ori, dir, p0, p1, p2, curDist);
				if (intersects && curDist < dist)
					dist = curDist;
			}
			return dist;
		};

		int partSize = ib.size() / 8;
		std::vector<std::future<float>> handles;
		handles.reserve(8);
		for (int i = 0; i < 8; ++i)
		{
			handles.push_back(std::async(std::launch::async, function, i * partSize, (i + 1) * partSize));
		}

		distance = std::numeric_limits<float>::infinity();

		for (int i = 0; i < 8; ++i)
		{
			float dist = handles[i].get();
			if(dist < distance)
				distance = dist;
		}

		// No intersection occured
		if (std::isinf<float>(distance))
			return false;

		return true;
	}
}

#endif