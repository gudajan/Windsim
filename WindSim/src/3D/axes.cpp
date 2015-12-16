#include "axes.h"
#include "common.h"
#include "settings.h"

#include "d3dx11effect.h"
#include <d3dcompiler.h>
#include <d3d11.h>

using namespace DirectX;

Axes::Axes()
	: Object3D()
{
	createAxesData();
	m_numIndices = m_indexData.size();
}

Axes::Axes(Axes&& other)
{
}

Axes::~Axes()
{
}

HRESULT Axes::createShaderFromFile(const std::wstring& shaderPath, ID3D11Device* device, const bool reload)
{
	HRESULT hr;

	releaseShader();

	if (reload)
	{
		ID3DBlob* blob;
		if (D3DX11CompileEffectFromFile(shaderPath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, D3DCOMPILE_DEBUG, 0, device, &s_effect, &blob) != S_OK)
		{
			char* buffer = reinterpret_cast<char*>(blob->GetBufferPointer());
			OutputDebugStringA(buffer);
			return E_FAIL;
		}
	}
	else
	{
		V_RETURN(D3DX11CreateEffectFromFile(shaderPath.c_str(), 0, device, &s_effect));
	}

	s_worldViewProj = s_effect->GetVariableByName("g_mWorldViewProj")->AsMatrix();

	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA }
	};

	D3DX11_PASS_DESC pd;
	s_effect->GetTechniqueByIndex(0)->GetPassByIndex(0)->GetDesc(&pd);

	V_RETURN(device->CreateInputLayout(layout, sizeof(layout) / sizeof(D3D11_INPUT_ELEMENT_DESC), pd.pIAInputSignature, pd.IAInputSignatureSize, &s_inputLayout));

	return S_OK;
}

void Axes::releaseShader()
{
	SAFE_RELEASE(s_inputLayout);
	SAFE_RELEASE(s_effect);
}

void Axes::render(ID3D11Device* device, ID3D11DeviceContext* context, const XMFLOAT4X4& world, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection)
{
	if (s_effect == nullptr)
	{
		return;
	}

	XMMATRIX worldViewProj = XMLoadFloat4x4(&world) * XMLoadFloat4x4(&view) * XMLoadFloat4x4(&projection);

	s_worldViewProj->SetMatrix(reinterpret_cast<float*>(worldViewProj.r));

	s_effect->GetTechniqueByIndex(0)->GetPassByIndex(0)->Apply(0, context);

	const unsigned int strides[] = { sizeof(float) * 6 }; // 3 floats postion + 3 floats color
	const unsigned int offsets[] = { 0 };
	context->IASetVertexBuffers(0, 1, &m_vertexBuffer, strides, offsets);
	context->IASetIndexBuffer(m_indexBuffer, DXGI_FORMAT_R32_UINT, 0);
	context->IASetInputLayout(s_inputLayout);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	context->DrawIndexed(m_numIndices, 0, 0);
}

void Axes::createAxesData()
{
	float scale = conf.grid.scale;
	float step = conf.grid.step;
	float col = conf.grid.col;

	// Axes
	m_vertexData =
	{
		// Positon           | Color
		-scale,  0.0f,  0.0f, 1.0f, 0.0f, 0.0f, // left x; red
		 scale,  0.0f,  0.0f, 1.0f, 0.0f, 0.0f, // right x
		 0.0f, -scale,  0.0f, 0.0f, 1.0f, 0.0f, // down y; green
		 0.0f,  scale,  0.0f, 0.0f, 1.0f, 0.0f, // up y
		 0.0f,  0.0f, -scale, 0.0f, 0.0f, 1.0f, // front z; blue
		 0.0f,  0.0f,  scale, 0.0f, 0.0f, 1.0f  // back z
	};

	m_indexData =
	{
		0, 1, // x axis
		2, 3, // y axis
		4, 5  // z axis
	};

	// Grid
	// 2 * scale x 2 * scale in xz-plane
	// lines in z-direction
	for (int x = -scale; x <= scale; x += step)
	{
		// Push two vertices
		// Front, Back
		for (int z : {-scale, scale})
		{
			//Position
			m_vertexData.push_back(x); // x
			m_vertexData.push_back(0.0f); // y
			m_vertexData.push_back(z); // z

			//Color (grey)
			m_vertexData.push_back(col); // r
			m_vertexData.push_back(col); // g
			m_vertexData.push_back(col); // b

			m_indexData.push_back(m_indexData.size()); // Push index for pushed back vertex (in this case indexBuffer is just incrementing ints)
		}
	}

	// lines in z-direction
	for (int z = -scale; z <= scale; z += step)
	{
		// Push two vertices
		// Front, Back
		for (int x : {-scale, scale})
		{
			//Position
			m_vertexData.push_back(x); // x
			m_vertexData.push_back(0.0f); // y
			m_vertexData.push_back(z); // z

			//Color (grey)
			m_vertexData.push_back(col); // r
			m_vertexData.push_back(col); // g
			m_vertexData.push_back(col); // b

			m_indexData.push_back(m_indexData.size()); // Push index for pushed back vertex
		}
	}
}


ID3DX11EffectMatrixVariable* Axes::s_worldViewProj = nullptr;
ID3DX11Effect* Axes::s_effect = nullptr;
ID3D11InputLayout* Axes::s_inputLayout = nullptr;