#include "common.fx"

RWTexture3D<uint> g_uavGrid;
Texture3D<uint> g_srvGrid;

cbuffer cb
{
	float4x4 g_mWorldViewProj;
	float4x4 g_mObjWorld;
	float4x4 g_mVoxelWorldInv;
	float4x4 g_mVoxelProj;
	float4 g_vCamPosVS;
	int3 g_vResolution;
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

// =============================================================================
// FUNCTIONS
// =============================================================================
bool getValue(in float3 posVS, out uint voxel)
{
	int3 index = uint3(floor(posVS));
	if (any(index < int3(0, 0, 0)) || any(index > int3(g_vResolution))) // Index out of bounds
		return false;

	voxel = g_srvGrid[index];
	return true;
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
	vsOut.voxelPos = mul(float4(vsIn.pos, 1.0f), g_mObjWorld * g_mVoxelWorldInv).xyz; // Transform from mesh object space -> grid object space -> grid voxel space
	vsOut.pos = mul(float4(vsOut.voxelPos, 1.0f), g_mVoxelProj);
	return vsOut;
}

PSVoxelIn vsVolume(VSGridIn vsIn)
{
	PSVoxelIn vsOut;
	vsOut.voxelPos = mul(float4(vsIn.pos, 1.0f), g_mVoxelWorldInv).xyz; // Transfrom from grid object space -> grid voxel space (aka texture space)
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

void psVoxelize(PSVoxelIn psIn)
{
	InterlockedXor(g_uavGrid[uint3(psIn.voxelPos)], 1); // Write 1 to voxel, which is set
}

// Ray casting into voxel grid -> render first voxel
[earlydepthstencil]
float4 psVolume(PSVoxelIn psIn) : SV_Target
{
	float3 rayDir = normalize(psIn.voxelPos - g_vCamPosVS.xyz);
	float stepSize = 0.5; // In Voxel space, all sides of a voxel are of length 1
	float3 currentPos = psIn.voxelPos; // Start raycasting at the incoming fragment, which corresponds to the border of the voxel grid
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

	return float4(0.5, 0.5, 0.5, 1.0f);
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
		SetVertexShader(CompileShader(vs_5_0, vsGridBox()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, psGridBox()));
		SetRasterizerState(CullNone);
		SetDepthStencilState(DepthDefault, 0);
		SetBlendState(BlendDisable, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
	}
}
