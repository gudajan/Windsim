#include "common.fx"

cbuffer cb
{
	float4x4 g_mTexToProj;
	float4x4 g_mObjectToWorld;
	float4x4 g_mWorldToVoxelTex;

	float3 g_vPosition; // Rotation center in World Space
}

RWStructuredBuffer<int> g_torqueUAV;
Texture3D<float4> g_velocitySRV;

// =============================================================================
// STRUCTURES
// =============================================================================
struct VSMeshIn
{
	float3 pos : POSITION;
	float3 normal : NORMAL;
};

struct PSIn
{
	float4 pos : SV_Position;
	float3 posTS : POSITION_TEX;
	float3 posWS : POSITION_WORLD;
};

// =============================================================================
// VERTEX SHADER
// =============================================================================

PSIn vsVoxelTex(VSMeshIn vsIn)
{
	PSIn vsOut;

	float4 pos = mul(float4(vsIn.pos, 1.0f), g_mObjectToWorld);
	vsOut.posWS = pos.xyz;
	float4 posTS = mul(pos, g_mWorldToVoxelTex);
	vsOut.posTS = posTS.xyz; // Transform to voxel grid texture coordinates
	vsOut.pos = mul(posTS, g_mTexToProj);


	return vsOut;
}

// =============================================================================
// PIXEL SHADER
// =============================================================================

float4 psTorque(PSIn psIn) : SV_Target
{
	float3 velocity = g_velocitySRV.SampleLevel(SamLinear, psIn.posTS, 0).xyz;

	// calc torque
	float3 F = velocity;
	float3 r = psIn.posWS - g_vPosition;

	float3 torque = cross(r, F);

	// Convert to int
	int3 intTorque = torque * 100000;

	InterlockedAdd(g_torqueUAV[0], intTorque.x);
	InterlockedAdd(g_torqueUAV[1], intTorque.y);
	InterlockedAdd(g_torqueUAV[2], intTorque.z);

	InterlockedAdd(g_torqueUAV[3], 1);

	return float4(normalize(torque.xyz), 1);
}

technique11 Torque
{
	pass Accumulate
	{
		SetVertexShader(CompileShader(vs_5_0, vsVoxelTex()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, psTorque()));
		SetRasterizerState(CullNone);
		SetDepthStencilState(DepthDisable, 0);
		SetBlendState(BlendDisable, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
	}
}