#include "common.fx"

cbuffer cb
{
	float4x4 g_mWorldViewProj;

}

struct VSIn
{
	float3 pos : POSITION;
	float3 col : COLOR;
};

struct PSIn
{
	float4 pos : SV_Position;
	float3 col : COLOR;
};


PSIn vsProject(VSIn vsIn)
{
	PSIn vsOut;
	vsOut.pos = mul(float4(vsIn.pos, 1.0f), g_mWorldViewProj);
	vsOut.col = vsIn.col;
	return vsOut;
}

float4 psSimple(PSIn psIn) : SV_Target
{
	return float4(psIn.col, 1.0f);
}


technique11 Simple
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, vsProject()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, psSimple()));
		SetRasterizerState(CullNone);
		SetDepthStencilState(DepthDefault, 0);
		SetBlendState(BlendDisable, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
	}
}
