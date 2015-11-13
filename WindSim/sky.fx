RasterizerState CullNone
{
	CullMode = None;
};

DepthStencilState EnableDepth
{
	DepthFunc = LESS_EQUAL; // Important for including positions, which are located ON the farplane (like the skybox)
};

BlendState BlendDisable
{
};

cbuffer cb
{
	float4x4 g_mWorldViewProj;
}

//Skybox vertex shader input
struct VSSkyIn
{
	float3 Coord : POSITION;
};
//Skybox pixel shader input
struct PSSkyIn
{
	float4 Pos : SV_POSITION;
	float3 Tex : TEXCOORD1;
};

PSSkyIn VSSky(VSSkyIn inVert){
	PSSkyIn outPix;
	// Set z to w, so after the projection (with homogenous division), the positon is located on the Z-far plane
	outPix.Pos = mul(float4(inVert.Coord, 0), g_mWorldViewProj).xyww;
	outPix.Tex = inVert.Coord;
	return outPix;
}

float4 PSGradient(PSSkyIn inPix) : SV_Target
{
	// Map cube position onto sphere
	float3 sp = normalize(inPix.Tex);

	// Draw a color gradient from horizon to top and bottom
	// inPix.Tex has values [-1, 1]
	float3 horizonColor = float3(1.0f, 1.0f, 1.0f); // color at the horizon (0)
	float3 topColor = float3(0.3f, 0.4f, 0.5f); // color at top (1.0)
	float3 bottomColor = float3(0.2f, 0.25f, 0.3f); // color at bottom (-1.0)

	// Calculate interpolation cooeficient for gradient depending on the y-value of the position
	float intensity = 1.0f / (abs(sp.y) + 1.0f);
	float3 color = 0;
	if (sp.y >= 0)
	{
		color = lerp(topColor, horizonColor, intensity * float3(1.0f, 1.0f, 1.0f));
	}
	else
	{
		// Use pow, to increase sharpness for interpolation
		color = lerp(bottomColor, horizonColor, pow(intensity,2) * float3(1.0f, 1.0f, 1.0f));
	}
	return float4(color, 1.0f);
}

technique11 Sky
{
	pass Gradient
	{
		SetVertexShader(CompileShader(vs_5_0, VSSky()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, PSGradient()));
		SetRasterizerState(CullNone);
		SetDepthStencilState(EnableDepth, 0);
		SetBlendState(BlendDisable, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
	}
}