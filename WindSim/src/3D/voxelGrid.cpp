#include "voxelGrid.h"
#include "objectManager.h"
#include "meshActor.h"
#include "mesh3D.h"

#include <d3d11.h>
#include <d3dcompiler.h>
#include "d3dx11effect.h"

using namespace DirectX;

VoxelGrid::VoxelGrid(ObjectManager* manager, XMUINT3 resolution, XMFLOAT3 voxelSize)
	: Object3D(),
	m_manager(manager),
	m_resolution(resolution),
	m_voxelSize(voxelSize),
	m_resize(false),
	m_cubeIndices(0),
	m_grid(resolution.x * resolution.y * resolution.z, 0),
	m_gridTextureGPU(nullptr),
	m_gridTextureStaging(nullptr),
	m_gridUAV(nullptr),
	m_gridSRV(nullptr)
{
	createGridData();
}

HRESULT VoxelGrid::createShaderFromFile(const std::wstring& shaderPath, ID3D11Device* device, const bool reload)
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

	s_shaderVariables.worldViewProj = s_effect->GetVariableByName("g_mWorldViewProj")->AsMatrix();
	s_shaderVariables.objWorld = s_effect->GetVariableByName("g_mObjWorld")->AsMatrix();
	s_shaderVariables.voxelWorldInv = s_effect->GetVariableByName("g_mVoxelWorldInv")->AsMatrix();
	s_shaderVariables.voxelProj = s_effect->GetVariableByName("g_mVoxelProj")->AsMatrix();
	s_shaderVariables.camPosVS = s_effect->GetVariableByName("g_vCamPosVS")->AsVector();
	s_shaderVariables.resolution = s_effect->GetVariableByName("g_vResolution")->AsVector();
	s_shaderVariables.gridUAV = s_effect->GetVariableByName("g_uavGrid")->AsUnorderedAccessView();
	s_shaderVariables.gridSRV = s_effect->GetVariableByName("g_srvGrid")->AsShaderResource();

	// The same layout as meshes, as only they are voxelized
	D3D11_INPUT_ELEMENT_DESC meshLayout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA }
	};

	D3DX11_PASS_DESC pd;
	s_effect->GetTechniqueByIndex(0)->GetPassByName("Voxelize")->GetDesc(&pd);

	V_RETURN(device->CreateInputLayout(meshLayout, sizeof(meshLayout) / sizeof(D3D11_INPUT_ELEMENT_DESC), pd.pIAInputSignature, pd.IAInputSignatureSize, &s_meshInputLayout));

	D3D11_INPUT_ELEMENT_DESC gridLayout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA },
	};

	s_effect->GetTechniqueByIndex(0)->GetPassByName("RenderGridBox")->GetDesc(&pd);

	V_RETURN(device->CreateInputLayout(gridLayout, sizeof(gridLayout) / sizeof(D3D11_INPUT_ELEMENT_DESC), pd.pIAInputSignature, pd.IAInputSignatureSize, &s_gridInputLayout));

	return S_OK;
}

void VoxelGrid::releaseShader()
{
	SAFE_RELEASE(s_meshInputLayout);
	SAFE_RELEASE(s_gridInputLayout);
	SAFE_RELEASE(s_effect);
}

HRESULT VoxelGrid::create(ID3D11Device* device, bool clearClientBuffers)
{
	HRESULT hr;

	release();

	Object3D::create(device, false); // Create vertex and index buffer for grid rendering

	// Create Texture3D for grid, filled in the pixel shader
	D3D11_TEXTURE3D_DESC td = {};
	td.Width = m_resolution.x;
	td.Height = m_resolution.y;
	td.Depth = m_resolution.z;
	td.MipLevels = 1;
	td.Format = DXGI_FORMAT_R8_UINT; // The smallest possible bytes per element are 8: 0 -> false else -> true
	td.Usage = D3D11_USAGE_DEFAULT;
	td.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	td.CPUAccessFlags = 0; // No CPU Access for this texture
	td.MiscFlags = 0;

	V_RETURN(device->CreateTexture3D(&td, nullptr, &m_gridTextureGPU));

	// Create Unordered Access View for grid
	V_RETURN(device->CreateUnorderedAccessView(m_gridTextureGPU, nullptr, &m_gridUAV));

	// Create Shader Resource View for grid
	V_RETURN(device->CreateShaderResourceView(m_gridTextureGPU, nullptr, &m_gridSRV));

	// Now create a texture in system memory, which is used by the GPU to copy the texture from the GPU memory to system memory
	td.Usage = D3D11_USAGE_STAGING; // GPU Read/Write and GPU to CPU copiing -> texture in system memory
	td.BindFlags = 0; // No binding to graphics pipeline allowed (and not needed)
	td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	V_RETURN(device->CreateTexture3D(&td, nullptr, &m_gridTextureStaging));

	return S_OK;
}

void VoxelGrid::release()
{
	Object3D::release();
	SAFE_RELEASE(m_gridTextureGPU);
	SAFE_RELEASE(m_gridTextureStaging);
	SAFE_RELEASE(m_gridUAV);
	SAFE_RELEASE(m_gridSRV);
}

void VoxelGrid::render(ID3D11Device* device, ID3D11DeviceContext* context, const XMFLOAT4X4& world, const XMFLOAT4X4& view, const XMFLOAT4X4& projection)
{
	if (m_resize)
	{
		createGridData();
		create(device, false);
		m_resize = false;
	}
	renderGridBox(device, context, world, view, projection);
	//voxelize(device, context, world);
	//renderVoxel(device, context, world, view, projection);
}

bool VoxelGrid::resize(XMUINT3 resolution, XMFLOAT3 voxelSize)
{
	XMVECTOR currentRes = XMLoadUInt3(&m_resolution);
	XMVECTOR newRes = XMLoadUInt3(&resolution);
	XMVECTOR currentVS = XMLoadFloat3(&m_voxelSize);
	XMVECTOR newVS = XMLoadFloat3(&voxelSize);

	bool resEqual = XMVector3EqualInt(currentRes, newRes);
	bool vsEqual = XMVector3Equal(currentVS, newVS);

	if (resEqual && vsEqual)
		return false;

	m_resolution = resolution;
	m_voxelSize = voxelSize;
	m_resize = true;
	return true;
}

void VoxelGrid::createGridData()
{
	float x = m_resolution.x * m_voxelSize.x;
	float y = m_resolution.y * m_voxelSize.y;
	float z = m_resolution.z * m_voxelSize.z;

	m_vertexData =
	{
		0.0f, 0.0f, 0.0f, // left, lower, front
		0.0f, 0.0f, z, // left, lower, back
		0.0f, y, 0.0f, // left, upper, front
		0.0f, y, z, // left, upper, back
		x, 0.0f, 0.0f, // right, lower, front
		x, 0.0f, z, // right, lower, back
		x, y, 0.0f, // right, upper, front
		x, y, z // right, upper, back
	};

	m_indexData =
	{
		// Box lines
		0, 1,
		0, 2,
		0, 4,
		1, 3,
		1, 5,
		2, 3,
		2, 6,
		3, 7,
		4, 5,
		4, 6,
		5, 7,
		6, 7,
		// Box triangles
		0, 1, 2,
		0, 2, 4,
		0, 5, 1,
		0, 4, 5,
		1, 2, 3,
		1, 5, 3,
		2, 6, 4,
		2, 3, 6,
		3, 7, 6,
		3, 5, 6,
		4, 6, 5,
		5, 6 ,7
	};
	m_numIndices = 12 * 2; // 12 lines with 2 vertices
	m_cubeIndices = 12 * 3; // 12 triangles with 3 vertices

}

void VoxelGrid::renderGridBox(ID3D11Device* device, ID3D11DeviceContext* context, const XMFLOAT4X4& world, const XMFLOAT4X4& view, const XMFLOAT4X4& projection)
{
	XMMATRIX worldViewProj = XMLoadFloat4x4(&world) * XMLoadFloat4x4(&view) * XMLoadFloat4x4(&projection);

	s_shaderVariables.worldViewProj->SetMatrix(reinterpret_cast<float*>((worldViewProj).r));

	s_effect->GetTechniqueByIndex(0)->GetPassByName("RenderGridBox")->Apply(0, context);

	const unsigned int strides[] = { sizeof(float) * 3 }; // 3 floats postion
	const unsigned int offsets[] = { 0 };
	context->IASetVertexBuffers(0, 1, &m_vertexBuffer, strides, offsets);
	context->IASetIndexBuffer(m_indexBuffer, DXGI_FORMAT_R32_UINT, 0);
	context->IASetInputLayout(s_gridInputLayout);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	context->DrawIndexed(m_numIndices, 0, 0);
}

void VoxelGrid::voxelize(ID3D11Device* device, ID3D11DeviceContext* context, const XMFLOAT4X4& world)
{

	// Save old renderTarget
	ID3D11RenderTargetView* tempRTV = nullptr;
	ID3D11DepthStencilView* tempDSV = nullptr;
	context->OMGetRenderTargets(1, &tempRTV, &tempDSV);
	// Save old viewport
	D3D11_VIEWPORT tempVP[1];
	UINT vpCount = 1;
	context->RSGetViewports(&vpCount, tempVP);

	// Remove Render Target and set Unordered Access View
	context->OMSetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr, 0, 1, &m_gridUAV, nullptr); // Last argument ignored

	// Set viewport, so projection plane resolution matches grid resolution
	D3D11_VIEWPORT vp;
	vp.TopLeftX = 0.0f;
	vp.TopLeftY = 0.0f;
	vp.Width = static_cast<float>(m_resolution.x);
	vp.Height = static_cast<float>(m_resolution.y);
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	context->RSSetViewports(1, &vp);

	// Passed projection and view transformations of current camera are ignored
	// Transform mesh into "Voxel Space" (coordinates should be in range [0, resolution] so we can use floor(position) in pixel shader for accessing the grid)
	// World to Grid Object Space:
	XMMATRIX worldToGridSpace = XMMatrixInverse(nullptr, XMLoadFloat4x4(&world));
	// Grid Object Space to Voxel Space
	XMMATRIX gridToVoxel = XMMatrixScalingFromVector(XMVectorReciprocal(XMLoadFloat3(&m_voxelSize))); // Scale with 1 / voxelSize so position is index into grid

	// Create orthogonal projection aligned with voxel grid looking along z-axis
	XMMATRIX proj = XMMatrixOrthographicLH(m_resolution.x, m_resolution.y, 0, m_resolution.z);


	s_shaderVariables.voxelWorldInv->SetMatrix(reinterpret_cast<float*>((worldToGridSpace * gridToVoxel).r));
	s_shaderVariables.voxelProj->SetMatrix(reinterpret_cast<float*>(proj.r));
	s_shaderVariables.gridUAV->SetUnorderedAccessView(m_gridUAV);

	s_effect->GetTechniqueByIndex(0)->GetPassByName("Voxelize")->Apply(0, context);

	const unsigned int strides[] = { sizeof(float) * 6 }; // 3 floats postion, 3 floats normal
	const unsigned int offsets[] = { 0 };

	context->IASetInputLayout(s_meshInputLayout);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Reset voxel grid
	std::fill(m_grid.begin(), m_grid.end(), 0);

	for (const auto& act : m_manager->getActors())
	{
		if (act.second->getType() == ObjectType::Mesh)
		{
			std::shared_ptr<MeshActor> ma = std::dynamic_pointer_cast<MeshActor>(act.second);

			// Clear UAV for each mesh
			const UINT iniVals[] = { 0, 0, 0, 0 };
			context->ClearUnorderedAccessViewUint(m_gridUAV, iniVals);


			// Mesh Object Space -> World Space:
			XMMATRIX objToWorld = XMLoadFloat4x4(&ma->getWorld());

			s_shaderVariables.objWorld->SetMatrix(reinterpret_cast<float*>(objToWorld.r));


			ID3D11Buffer* vb = ma->getMesh().getVertexBuffer();
			ID3D11Buffer* ib = ma->getMesh().getIndexBuffer();
			context->IASetVertexBuffers(0, 1, &vb, strides, offsets);
			context->IASetIndexBuffer(ib, DXGI_FORMAT_R32_UINT, 0);
			context->DrawIndexed(ma->getMesh().getNumIndices(), 0, 0);


			// Copy texture from GPU memory to system memory where it is accessable by the cpu
			context->CopyResource(m_gridTextureStaging, m_gridTextureGPU);

			// Get CPU Access
			unsigned char* data = nullptr;
			D3D11_MAPPED_SUBRESOURCE msr = {};
			msr.pData = data;
			msr.RowPitch = m_resolution.x;
			msr.DepthPitch = m_resolution.x * m_resolution.y;
			context->Map(m_gridTextureStaging, 0, D3D11_MAP_READ, 0, &msr); // data contains pointer to texture data

			// OR data with voxel grid
			std::transform(m_grid.begin(), m_grid.end(), data, m_grid.begin(), [](const unsigned char& v1, const unsigned char& v2) { return v1 + v2; });

			// Remove CPU Access
			context->Unmap(m_gridTextureStaging, 0);
		}
	}
	// Remove Unordered Access View
	context->OMSetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr, 0, 0, nullptr, nullptr);

	// Restore old render targets and viewport
	context->OMSetRenderTargets(1, &tempRTV, tempDSV);
	context->RSSetViewports(1, tempVP);
}

void VoxelGrid::renderVoxel(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& world, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection)
{
	XMMATRIX w = XMLoadFloat4x4(&world);
	XMMATRIX v = XMLoadFloat4x4(&view);
	XMMATRIX worldViewProj = w * v * XMLoadFloat4x4(&projection);
	XMMATRIX gridToVoxel = XMMatrixScalingFromVector(XMVectorReciprocal(XMLoadFloat3(&m_voxelSize)));

	s_shaderVariables.worldViewProj->SetMatrix(reinterpret_cast<float*>((worldViewProj).r));
	s_shaderVariables.voxelWorldInv->SetMatrix(reinterpret_cast<float*>((gridToVoxel).r));
	s_shaderVariables.resolution->SetIntVector(reinterpret_cast<int*>(&m_resolution));

	//Calculate camera position in Voxel Space (Texture space of grid texture -> [0, resolution])
	XMVECTOR camPos = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f); // In camera space
	camPos = XMVector3Transform(camPos, XMMatrixInverse(nullptr, v)); // World Space
	camPos = XMVector3Transform(camPos, XMMatrixInverse(nullptr, w)); // Grid Object Space
	camPos = XMVector3Transform(camPos, gridToVoxel); // Voxel Space
	s_shaderVariables.camPosVS->SetFloatVector(camPos.m128_f32);

	s_shaderVariables.gridSRV->SetResource(m_gridSRV);

	s_effect->GetTechniqueByIndex(0)->GetPassByName("RenderVoxel")->Apply(0, context);



	const unsigned int strides[] = { sizeof(float) * 3 }; // 3 floats postion
	const unsigned int offsets[] = { 0 };
	context->IASetVertexBuffers(0, 1, &m_vertexBuffer, strides, offsets);
	context->IASetIndexBuffer(m_indexBuffer, DXGI_FORMAT_R32_UINT, 0);
	context->IASetInputLayout(s_gridInputLayout);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	context->DrawIndexed(m_cubeIndices, m_numIndices, 0);

}

VoxelGrid::ShaderVariables::ShaderVariables()
	: worldViewProj(nullptr),
	objWorld(nullptr),
	voxelWorldInv(nullptr),
	voxelProj(nullptr),
	camPosVS(nullptr),
	resolution(nullptr),
	gridUAV(nullptr),
	gridSRV(nullptr)
{
}

ID3DX11Effect* VoxelGrid::s_effect = nullptr;
VoxelGrid::ShaderVariables VoxelGrid::s_shaderVariables;
ID3D11InputLayout* VoxelGrid::s_meshInputLayout = nullptr;
ID3D11InputLayout* VoxelGrid::s_gridInputLayout = nullptr;