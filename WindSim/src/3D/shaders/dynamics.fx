#include "common.fx"

cbuffer cb
{
	float4x4 g_mViewProj;
	float4x4 g_mTexToProj;
	float4x4 g_mObjectToWorld;
	float4x4 g_mWorldToVoxelTex;

	float3 g_vPosition; // Rotation center in World Space (center of mass)
	float3 g_vAngVel; // Global angular velocity

}


// =============================================================================
// STRUCTURES
// =============================================================================
struct VSMeshIn
{
	float3 pos : POSITION;
	float3 normal : NORMAL;
};

struct PSTexIn
{
	float4 pos : SV_Position;
	float3 posTS : POSITION_TEX;
	float3 posWS : POSITION_WORLD;
};

struct PSLineIn
{
	float4 pos : SV_Position;
};

RWStructuredBuffer<int> g_torqueUAV;
Texture3D<float4> g_velocitySRV;

// =============================================================================
// VERTEX SHADER
// =============================================================================

PSTexIn vsVoxelTex(VSMeshIn vsIn)
{
	PSTexIn vsOut;

	float4 pos = mul(float4(vsIn.pos, 1.0f), g_mObjectToWorld);
	vsOut.posWS = pos.xyz;
	float4 posTS = mul(pos, g_mWorldToVoxelTex);
	vsOut.posTS = posTS.xyz; // Transform to voxel grid texture coordinates
	vsOut.pos = mul(posTS, g_mTexToProj);


	return vsOut;
}

uint vsAngVel(uint vid : SV_VertexID) : VertexID
{
	return vid;
}

// =============================================================================
// GEOMETRY SHADER
// =============================================================================

[maxvertexcount(7)]
void gsArrow(point uint input[1] : VertexID, inout TriangleStream<PSLineIn> stream)
{
	PSLineIn psIn;

	float scale = 10.0f;
	float arrowHeadSize = 0.3f;
	float width = 0.1f;

	// The orientation of the glyph
	float3 x = g_vAngVel * scale;
	float3 y = normalize(cross(x, float3(0.0f, 0.0f, 1.0f))) * scale * length(g_vAngVel); // In voxel grid object space

	//Arrow end
	psIn.pos = mul(float4(g_vPosition + y * width, 1.0f), g_mViewProj);
	stream.Append(psIn);

	//Arrow head
	psIn.pos = mul(float4(g_vPosition + x + y * width, 1.0f), g_mViewProj);
	stream.Append(psIn);

	psIn.pos = mul(float4(g_vPosition - y * width, 1.0f), g_mViewProj);
	stream.Append(psIn);

	psIn.pos = mul(float4(g_vPosition + x - y * width, 1.0f), g_mViewProj);
	stream.Append(psIn);

	stream.RestartStrip();

	//Arrow head UP
	psIn.pos = mul(float4(g_vPosition + y * arrowHeadSize + x , 1.0f), g_mViewProj);
	stream.Append(psIn);

	psIn.pos = mul(float4(g_vPosition + x * (1 + arrowHeadSize), 1.0f), g_mViewProj);
	stream.Append(psIn);

	//Arrow head DOWN
	psIn.pos = mul(float4(g_vPosition - y  * arrowHeadSize + x , 1.0f), g_mViewProj);
	stream.Append(psIn);
}

// =============================================================================
// PIXEL SHADER
// =============================================================================

float4 psTorque(PSTexIn psIn) : SV_Target
{
	float3 velocity = g_velocitySRV.SampleLevel(SamLinear, psIn.posTS, 0).xyz;

	// calc torque
	float3 F = velocity;

	// Rotation arround point (3DOF):
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

float4 psLine(PSLineIn frag) : SV_Target
{
	return float4(0.7f, 0.2f, 0.2f, 1.0f);
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
	pass AngularVelocity
	{
		SetVertexShader(CompileShader(vs_5_0, vsAngVel()));
		SetGeometryShader(CompileShader(gs_5_0, gsArrow()));
		SetPixelShader(CompileShader(ps_5_0, psLine()));
		SetRasterizerState(CullNone);
		SetDepthStencilState(DepthDisable, 0);
		SetBlendState(BlendDisable, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
	}
}