RasterizerState CullNone
{
	CullMode = None;
};

DepthStencilState DepthDefault
{
};

BlendState BlendDisable
{
};

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

	// this will all get inlined anyway, might as well make it pretty
	const float3 v = -normalize(inFragment.posView);
	const float3 l = v; // special case: headlight
	const float3 h = l; // still headlight

	return g_vColor * (ka + kd * saturate(dot(n, l))) + ks * pow(saturate(dot(n, h)), s);
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