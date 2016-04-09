#include "common.fx"

cbuffer cb
{
	float4x4 g_mViewProj;
	float4x4 g_mTexToProj;
	float4x4 g_mObjectToWorld;
	float4x4 g_mWorldToVoxelTex;

	float3 g_vPosition; // Rotation center in World Space (center of mass)
	float3 g_vAngVel; // Global angular velocity
	float3 g_vVoxelSize;

	int g_renderDirection;
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

struct PSOut
{
	float4 col : SV_Target;
	float depth : SV_Depth;
};

RWStructuredBuffer<int> g_torqueUAV;
Texture3D<float> g_pressureSRV;
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

	float scale = 1.0f;
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

float4 psPressureTorque(PSTexIn psIn, bool isFrontFace : SV_IsFrontFace) : SV_Target
{
	// Steps to calculate torque:
	// - Sample pressure from pressure field
	// - Calc force on current fragment; p = F/a => F = p * a
	//      As each fragment is rendered once for each main coordinate axis; use weights for each pass.
	//      If we accumulate the calculated areas of each pass, the overall objects surface area sums up correctly
	//      Additionally, if the fragment has a "better"(smaller wrt its normal) angle to the camera , the calculated force has implicitly more influence to
	//      the total force, which is good because it is more accurate in this case (the position of the fragment is more precise has it does not span a huge area
	//        -> the sampling of the pressure is more precise)
	//      The fragment normal is calculated via screenspace derivates of the fragments world space coordinates and than transformed as necessary
	//      The weight is then given by the dot product of the view direction ((0,0,1) in projection space) and the normal
	// - Calc torque from force and rotation lever

	// Calc fragement normal
	float3 normalWS = normalize(cross(ddx(psIn.posWS), ddy(psIn.posWS)));
	normalWS = isFrontFace ? normalWS : -normalWS; // Flip normal if back face
	float3 normalTS = mul(float4(normalWS, 0.0f), g_mWorldToVoxelTex).xyz;

	// Sample pressure just above the surface
	float p = g_pressureSRV.SampleLevel(SamLinear, psIn.posTS + 0.5 * normalTS, 0.0);

	// Perpendicular fragment surface area, dependent on the voxel sizes
	float a[3] = { g_vVoxelSize.z * g_vVoxelSize.y, g_vVoxelSize.x * g_vVoxelSize.z, g_vVoxelSize.x * g_vVoxelSize.y };

	float w = abs(dot(float3(0.0f, 0.0f, 1.0f), normalize(mul(float4(normalTS, 0.0f), g_mTexToProj).xyz))); // Weight for this fragment (equivalent to the angle between normal and view ray)

	float3 F = - p * normalWS * a[g_renderDirection] * w;

	// Calc torque
	// Rotation arround point (3DOF):
	float3 r = psIn.posWS - g_vPosition;

	float3 torque = cross(r, F);

	// Convert to int
	int3 intTorque = torque * 100000;

	InterlockedAdd(g_torqueUAV[0], intTorque.x);
	InterlockedAdd(g_torqueUAV[1], intTorque.y);
	InterlockedAdd(g_torqueUAV[2], intTorque.z);

	InterlockedAdd(g_torqueUAV[3], 1);

	float f = p / 50;
	return float4(F * 50, 1);
}

// Equivalent to pressure with different pressure calculation
float4 psVelocityTorque(PSTexIn psIn, bool isFrontFace : SV_IsFrontFace) : SV_Target
{
	// Calc fragement normal
	float3 normalWS = normalize(cross(ddx(psIn.posWS), ddy(psIn.posWS)));
	normalWS = isFrontFace ? normalWS : -normalWS; // Flip normal if back face
	float3 normalTS = mul(float4(normalWS, 0.0f), g_mWorldToVoxelTex).xyz;

	// Sample flow velocity just above the surface
	float3 velocity = g_velocitySRV.SampleLevel(SamLinear, psIn.posTS + 0.75 * normalTS, 0.0).xyz;

	// Calc stagnation pressure from flow velocity
	float airDensity = 1.2256; // kg/m^3
	float g = 9.81; // Gravity

	float pNorm = -dot(velocity, normalWS) * 0.17; // Normal pressure to surface through velocity (This is just an approximation); use magic number to adjust to pressure method
	float p = 0.5 * airDensity * pNorm; // Dynamic pressure, p may be negative

	//p += airDensity * g * psIn.posWS.y; // Fluid pressure

	// Perpendicular fragment surface area, dependent on the voxel sizes
	float a[3] = { g_vVoxelSize.z * g_vVoxelSize.y, g_vVoxelSize.x * g_vVoxelSize.z, g_vVoxelSize.x * g_vVoxelSize.y };

	float w = abs(dot(float3(0.0f, 0.0f, 1.0f), normalize(mul(float4(normalTS, 0.0f), g_mTexToProj).xyz)));

	float3 F = - p * normalWS * (a[g_renderDirection] * w);

	// Calc torque
	// Rotation arround point (3DOF):
	float3 r = psIn.posWS - g_vPosition;

	float3 torque = cross(r, F);

	// Convert to int
	int3 intTorque = torque * 100000;

	InterlockedAdd(g_torqueUAV[0], intTorque.x);
	InterlockedAdd(g_torqueUAV[1], intTorque.y);
	InterlockedAdd(g_torqueUAV[2], intTorque.z);

	InterlockedAdd(g_torqueUAV[3], 1);

	float f = p / 500;
	return float4(velocity / 700, 1);
}

PSOut psLine(PSLineIn frag)
{
	PSOut psOut;
	psOut.col = float4(0.7f, 0.2f, 0.2f, 1.0f);
	psOut.depth = 0.0f;
	return psOut;
}

technique11 Torque
{
	pass PressureCalc
	{
		SetVertexShader(CompileShader(vs_5_0, vsVoxelTex()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, psPressureTorque()));
		SetRasterizerState(CullNone);
		SetDepthStencilState(DepthDisable, 0);
		SetBlendState(BlendDisable, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
	}
	pass VelocityCalc
	{
		SetVertexShader(CompileShader(vs_5_0, vsVoxelTex()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, psVelocityTorque()));
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
		SetDepthStencilState(DepthDefault, 0);
		SetBlendState(BlendDisable, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
	}
}