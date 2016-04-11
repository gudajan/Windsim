#include "axes.h"
#include "common.h"
#include "settings.h"
#include "dx11renderer.h"

#include "d3dx11effect.h"
#include <d3dcompiler.h>
#include <d3d11.h>

using namespace DirectX;

Axes::Axes(DX11Renderer* renderer)
	: Object3D(renderer)
{
	createAxesData();
	m_numIndices = m_indexData.size();
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

	s_shaderVariables.view = s_effect->GetVariableByName("g_mView")->AsMatrix();
	s_shaderVariables.proj = s_effect->GetVariableByName("g_mProj")->AsMatrix();
	s_shaderVariables.viewProj = s_effect->GetVariableByName("g_mViewProj")->AsMatrix();
	s_shaderVariables.viewProjInv = s_effect->GetVariableByName("g_mViewProjInv")->AsMatrix();
	s_shaderVariables.worldViewProj = s_effect->GetVariableByName("g_mWorldViewProj")->AsMatrix();

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

void Axes::render(ID3D11Device* device, ID3D11DeviceContext* context, const XMFLOAT4X4& world, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection, double elapsedTime)
{
	if (s_effect == nullptr)
	{
		return;
	}

	XMMATRIX w = XMLoadFloat4x4(&world);
	XMMATRIX v = XMLoadFloat4x4(&view);
	XMMATRIX p = XMLoadFloat4x4(&projection);

	XMMATRIX viewProj = v * p;
	XMMATRIX worldViewProj = w * viewProj;

	s_shaderVariables.worldViewProj->SetMatrix(reinterpret_cast<const float*>(worldViewProj.r));
	s_effect->GetTechniqueByIndex(0)->GetPassByIndex(0)->Apply(0, context);

	const unsigned int strides[] = { sizeof(float) * 6 }; // 3 floats postion + 3 floats color
	const unsigned int offsets[] = { 0 };
	context->IASetVertexBuffers(0, 1, &m_vertexBuffer, strides, offsets);
	context->IASetIndexBuffer(m_indexBuffer, DXGI_FORMAT_R32_UINT, 0);
	context->IASetInputLayout(s_inputLayout);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	context->DrawIndexed(m_numIndices, 0, 0);

	D3D11_VIEWPORT tempVP[1];
	UINT vpCount = 1;
	context->RSGetViewports(&vpCount, tempVP);

	int width = 120;
	int height = 120;
	D3D11_VIEWPORT vp;
	vp.TopLeftX = 1.0f;
	vp.TopLeftY = m_renderer->getHeight() - 1.0f - height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.Width = width;
	vp.Height = height;
	context->RSSetViewports(1, &vp);

	XMMATRIX ortho = XMMatrixOrthographicOffCenterLH(0, width, 0, height, 0, 1000);
	XMMATRIX orthoViewProj = v * ortho;

	s_shaderVariables.view->SetMatrix(reinterpret_cast<const float*>(v.r));
	s_shaderVariables.proj->SetMatrix(reinterpret_cast<const float*>(ortho.r));
	s_shaderVariables.viewProj->SetMatrix(reinterpret_cast<const float*>(orthoViewProj.r));
	s_shaderVariables.viewProjInv->SetMatrix(reinterpret_cast<const float*>(XMMatrixInverse(nullptr, orthoViewProj).r));
	s_effect->GetTechniqueByIndex(0)->GetPassByIndex(1)->Apply(0, context);

	UINT s = 0;
	UINT o = 0;
	context->IASetVertexBuffers(0, 0, nullptr, &s, &o);
	context->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, 0);
	context->IASetInputLayout(nullptr);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
	context->Draw(1, 0);

	context->RSSetViewports(1, tempVP);
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


Axes::ShaderVariables Axes::s_shaderVariables;
ID3DX11Effect* Axes::s_effect = nullptr;
ID3D11InputLayout* Axes::s_inputLayout = nullptr;