RasterizerState CullNone
{
	CullMode = None;
};

DepthStencilState DepthDefault
{
};

DepthStencilState DepthDisable
{
	DEPTHENABLE = FALSE;
};

DepthStencilState DepthEqual
{
	DepthFunc = LESS_EQUAL;
};


BlendState BlendDisable
{
};

BlendState BlendBackToFront
{
	BlendEnable[0] = TRUE;
	SrcBlend = ONE;
	DestBlend = INV_SRC_ALPHA;
	BlendOp = ADD;

	SrcBlendAlpha = ONE;
	DestBlendAlpha = INV_SRC_ALPHA;
	BlendOpAlpha = ADD;
};


// normal dir, view dir, diffuse color, ambient constant, diffuse constant, specular constant, shininess constant
float4 BlinnPhongIllumination(in float3 n, in float3 v, in float3 col, in float ca, in float cd, in float cs, in int css)
{
	const float3 ambientDiffuseColor = col;
	const float4 lightColor = float4(1.0f, 1.0f, 1.0f, 1.0f);

	n = normalize(n);
	v = normalize(v);
	const float3 l = v; // Special case: Headlight
	const float3 h = l; // still headlight

	float4 ambient = ca * float4(ambientDiffuseColor, 1.0f);
	float4 diffuse = cd * float4(ambientDiffuseColor, 1.0f) * saturate(dot(n, l));
	float4 specular = cs * pow(saturate(dot(n, h)), css);

	float4 color = ambient + (diffuse + specular) * lightColor;
	color.a = 1.0f;

	return color;
}