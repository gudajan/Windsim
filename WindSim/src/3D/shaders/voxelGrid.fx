#include "common.fx"

RWTexture3D<uint> g_uavGrid;
Texture3D<uint> g_srvGrid;

cbuffer cb
{
	float4x4 g_mWorldViewProj; // Object space -> world space -> Camera/View space -> projection space
	float4x4 g_mObjToWorld; // Mesh object space -> world space
	float4x4 g_mGridToVoxel; // Grid Object Space -> Voxel Space (use position as 3D index into voxel grid texture)
	float4x4 g_mWorldToVoxel; // Combined matrix: world space -> grid object space -> voxel space
	float4x4 g_mVoxelProj; // Projection matrix for voxelization (orthogonal, aligned with the voxelgrid)
	float4x4 g_mVoxelWorldViewProj; // Voxel space -> grid object space -> world space -> camera/view space -> projection space

	float4 g_vCamPosVS; // Camera position in Voxel space
	int3 g_vResolution; // Resolution of voxel grid
	float3 g_vVoxelSize; // VoxelSize in grid object space
}

// =============================================================================
// STRUCTURES
// =============================================================================
struct VSGridIn
{
	float3 pos : POSITION;
};

struct VSMeshIn
{
	float3 pos : POSITION;
	float3 normal : NORMAL;
};

struct PSGridBoxIn
{
	float4 pos : SV_Position;
};

struct PSVoxelIn
{
	float4 pos : SV_Position;
	float3 voxelPos : INDEX;
};

struct PSOut
{
	float4 col : SV_Target;
	float Depth : SV_Depth;
};

// =============================================================================
// FUNCTIONS
// =============================================================================

bool gridIndex(in float3 pos, out int3 index)
{
	if (any(pos < float3(0.0f, 0.0f, 0.0f)) || any(pos > float3(g_vResolution))) // Index out of bounds
		return false;
	index = int3(pos);
	return true;
}

bool getValue(in float3 posVS, out uint voxel)
{
	int3 index;
	if (!gridIndex(posVS, index))
	{
		voxel = 0;
		return false;
	}
	voxel = g_srvGrid[index];
	return true;
}

bool intersectBox(in float3 origin, in float3 dir, in float3 boxMin, in float3 boxMax, out float t0, out float t1)
{
	float3 invR = 1.0 / dir;
	float3 tbot = invR * (boxMin - origin);
	float3 ttop = invR * (boxMax - origin);
	float3 tmin = min(ttop, tbot);
	float3 tmax = max(ttop, tbot);
	float2 t = max(tmin.xx, tmin.yz);
	t0 = max(t.x, t.y);
	t = min(tmax.xx, tmax.yz);
	t1 = min(t.x, t.y);
	return t0 <= t1;
}


// =============================================================================
// VERTEX SHADER
// =============================================================================
PSGridBoxIn vsGridBox(VSGridIn vsIn)
{
	PSGridBoxIn vsOut;
	vsOut.pos = mul(float4(vsIn.pos, 1.0f), g_mWorldViewProj);
	return vsOut;
}

PSVoxelIn vsVoxelize(VSMeshIn vsIn)
{
	PSVoxelIn vsOut;
	vsOut.voxelPos = mul(mul(float4(vsIn.pos, 1.0f), g_mObjToWorld), g_mWorldToVoxel).xyz; // Transform from mesh object space -> World Space -> grid object space -> grid voxel space
	vsOut.pos = mul(mul(mul(float4(vsIn.pos, 1.0f), g_mObjToWorld), g_mWorldToVoxel), g_mVoxelProj);

	// Shift z coordinate about half a voxel so the voxel position appears to be in its center instead of the lower, left, front corner
	vsOut.pos.z += 2.0f / g_vVoxelSize.z; // size of voxel in projection space
	vsOut.voxelPos.z += 0.5;
	// Clamp z coordinate to grid
	// This way, a fragment is generated although it lies outside the grid
	// As the voxelization algorithm depends on the odd number of fragments (entering and leaving the object) it is important to also generate the leaving fragment if it lies outside the grid
	vsOut.pos.z = clamp(vsOut.pos.z, - 1.0f, 1.0f);
	return vsOut;
}

PSVoxelIn vsVolume(VSGridIn vsIn)
{
	PSVoxelIn vsOut;
	vsOut.voxelPos = mul(float4(vsIn.pos, 1.0f), g_mGridToVoxel).xyz; // Transfrom from grid object space -> grid voxel space (aka texture space)
	vsOut.pos = mul(float4(vsIn.pos, 1.0f), g_mWorldViewProj);
	return vsOut;
}

// =============================================================================
// PIXEL SHADER
// =============================================================================
float4 psGridBox(PSGridBoxIn psIn) : SV_Target
{
	return float4(0.0f, 0.0f, 0.0f, 1.0f);
}

//float4 psVoxelize(PSVoxelIn psIn) : SV_Target
void psVoxelize(PSVoxelIn psIn)
{
	int3 index;
	int zRun = g_vResolution.z;
	if (psIn.voxelPos.z < 0)
		zRun = 0;
	if (gridIndex(psIn.voxelPos, index))
		zRun = index.z;

	for (int z = 0; z < zRun; ++z)
	{
		InterlockedXor(g_uavGrid[uint3(index.xy, z)], 1);
	}

	//return float4(abs(psIn.voxelPos / g_vResolution), 1.0f);
}

// Ray casting into voxel grid -> render first voxel
PSOut psVolume(PSVoxelIn psIn)
{
	PSOut psOut = (PSOut)0;
	psOut.Depth = 0.0f;

	float3 rayDir = normalize(psIn.voxelPos - g_vCamPosVS.xyz);
	float stepSize = 0.01; // In Voxel space, all sides of a voxel are of length 1
	float3 currentPos = psIn.voxelPos; // Start raycasting at the incoming fragment, which corresponds to the border of the voxel grid
	int3 index;
	if (!gridIndex(currentPos + rayDir, index)) // If fragment is backface: start ray casting from view plane position instead of the fragment
		currentPos = g_vCamPosVS.xyz + rayDir; // We would need the intersection of the view plane and the ray, casted from the camera; just fake it by using the RayDir
	bool stepping = true;
	while (stepping)
	{
		uint voxel = 0;
		stepping = getValue(currentPos, voxel);
		if (!stepping)
		{
			discard;
		}
		if (voxel > 0) // Found a voxel
		{
			stepping = false;
			break;
		}
		currentPos += rayDir * stepSize; // Step about one voxel
	}

	// TODO: calculate normal and perform lighting

	// Calculate output
	float tmin = 0;
	float tmax = 0;
	if (intersectBox(g_vCamPosVS.xyz, rayDir, floor(currentPos), ceil(currentPos), tmin, tmax)) // Get intersection point of voxel and ray
	{
		float4 pos = mul(float4(g_vCamPosVS.xyz + tmin * rayDir, 1.0f), g_mVoxelWorldViewProj).xyzw;
		psOut.Depth = pos.z / pos.w;
		psOut.col = float4(0.7, 0.2, 0.2, 1.0f);
	}
	return psOut;
}


technique11 Voxel
{
	pass Voxelize
	{
		SetVertexShader(CompileShader(vs_5_0, vsVoxelize()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, psVoxelize()));
		SetRasterizerState(CullNone);
		SetDepthStencilState(DepthDisable, 0);
		SetBlendState(BlendDisable, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
	}
	pass RenderGridBox
	{
		SetVertexShader(CompileShader(vs_5_0, vsGridBox()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, psGridBox()));
		SetRasterizerState(CullNone);
		SetDepthStencilState(DepthDefault, 0);
		SetBlendState(BlendDisable, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
	}
	pass RenderVoxel
	{
		SetVertexShader(CompileShader(vs_5_0, vsVolume()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, psVolume()));
		SetRasterizerState(CullNone);
		SetDepthStencilState(DepthDefault, 0);
		SetBlendState(BlendDisable, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
	}
}
