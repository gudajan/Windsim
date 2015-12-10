#include "common.fx"

cbuffer cb
{
	float4x4 g_mWorldView;
	float4x4 g_mWorldViewIT;
	float4x4 g_mWorldViewProj;
	bool g_bEnableFlatShading;
	float4 g_vColor;
}

struct VSIn
{
	float3 pos : POSITION;
	float3 normal : NORMAL;
};

struct PSIn
{
	float4 pos : SV_Position;
	float3 posView : POSITION;
	float3 normalView : NORMAL;
};


PSIn vsProject(VSIn inVertex)
{
	PSIn outVertex;
	outVertex.pos = mul(float4(inVertex.pos, 1.0f), g_mWorldViewProj);
	outVertex.posView = mul(float4(inVertex.pos, 1.0f), g_mWorldView).xyz;
	outVertex.normalView = mul(float4(inVertex.normal, 0.0f), g_mWorldViewIT).xyz;
	return outVertex;
}

float4 psBlinnPhong(PSIn inFragment) : SV_Target
{
	const float ka = 0.2f; // ambient
	const float kd = 0.6f; // diffuse
	const float ks = 0.2f; // specular
	const float s = 30.0f; // shininess (alpha)

	float3 n;
	if (g_bEnableFlatShading)
	{
		// Calculate the normal from screenspace derivatives for flat shading
		n = normalize(cross(ddx(inFragment.posView), ddy(inFragment.posView)));
	}
	else
	{
		n = -normalize(inFragment.normalView);
	}

	return BlinnPhongIllumination(n, -normalize(inFragment.posView), g_vColor.xyz, ka, kd, ks, s);
}


technique11 Simple
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, vsProject()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, psBlinnPhong()));
		SetRasterizerState(CullNone);
		SetDepthStencilState(DepthDefault, 0);
		SetBlendState(BlendDisable, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
	}
}