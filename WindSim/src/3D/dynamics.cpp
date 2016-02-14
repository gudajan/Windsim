#include "dynamics.h"
#include "mesh3D.h"
#include "common.h"


#include <d3d11.h>
#include <d3dcompiler.h>
#include "d3dx11effect.h"

using namespace DirectX;

HRESULT Dynamics::createShaderFromFile(const std::wstring& shaderPath, ID3D11Device* device, const bool reload)
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

	s_shaderVariables.texToProj = s_effect->GetVariableByName("g_mTexToProj")->AsMatrix();
	s_shaderVariables.objectToWorld = s_effect->GetVariableByName("g_mObjectToWorld")->AsMatrix();
	s_shaderVariables.worldToVoxelTex = s_effect->GetVariableByName("g_mWorldToVoxelTex")->AsMatrix();
	s_shaderVariables.position = s_effect->GetVariableByName("g_vPosition")->AsVector();

	s_shaderVariables.torqueUAV = s_effect->GetVariableByName("g_torqueUAV")->AsUnorderedAccessView();
	s_shaderVariables.velocitySRV = s_effect->GetVariableByName("g_velocitySRV")->AsShaderResource();

	return S_OK;
}

void Dynamics::releaseShader()
{
	SAFE_RELEASE(s_effect);
}

HRESULT Dynamics::create(ID3D11Device* device)
{
	HRESULT hr;

	D3D11_BUFFER_DESC bd;
	bd.ByteWidth = 4 * sizeof(int32_t); // 4 Ints
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	bd.CPUAccessFlags = 0;
	bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	bd.StructureByteStride = sizeof(int32_t); // Integers

	int initData[] = { 0, 0, 0, 0};
	D3D11_SUBRESOURCE_DATA srd;
	srd.pSysMem = initData;

	V_RETURN(device->CreateBuffer(&bd, &srd, &m_torqueTex));

	bd.ByteWidth = 4 * sizeof(int32_t); // 4 Ints
	bd.Usage = D3D11_USAGE_STAGING;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	bd.MiscFlags = 0;
	bd.BindFlags = 0;
	bd.StructureByteStride = 0;
	V_RETURN(device->CreateBuffer(&bd, &srd, &m_torqueTexStaging));

	//bd.ByteWidth = sizeof(int32_t);
	//V_RETURN(device->CreateBuffer(&bd, nullptr, &m_torqueCounterStaging));

	D3D11_UNORDERED_ACCESS_VIEW_DESC uavd;
	uavd.Format = DXGI_FORMAT_UNKNOWN;
	uavd.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uavd.Buffer.FirstElement = 0;
	uavd.Buffer.NumElements = 4; // 4 Ints
	uavd.Buffer.Flags = 0;
	V_RETURN(device->CreateUnorderedAccessView(m_torqueTex, &uavd, &m_torqueUAV));

	return S_OK;
}

void Dynamics::release()
{
	SAFE_RELEASE(m_torqueTex);
	SAFE_RELEASE(m_torqueTexStaging);
	SAFE_RELEASE(m_torqueCounterStaging);
	SAFE_RELEASE(m_torqueUAV);
}
Dynamics::Dynamics(Mesh3D& mesh)
	: m_torqueTex(nullptr)
	, m_torqueTexStaging(nullptr)
	, m_torqueCounterStaging(nullptr)
	, m_torqueUAV(nullptr)
	, m_mesh(mesh)
	, m_inertiaTensor()
	, m_centerOfMass(0.0f, 0.0f, 0.0f)
	, m_rotationAxis(0.0f, 0.0f, 0.0f)
	, m_angVel({ 0.0f, 0.0f, 0.0f })
	, m_rot()
{
	XMStoreFloat4(&m_rot, XMQuaternionIdentity());
}

void Dynamics::calculate(ID3D11Device* device, ID3D11DeviceContext* context, const XMFLOAT4X4& objectToWorld, const XMFLOAT4X4& worldToVoxelTex, const XMUINT3& texResolution, ID3D11ShaderResourceView* velocityField, double elapsedTime)
{

	// #############################
	// Copy calculated torque from last frame to CPU
	// #############################
	int count = 0;
	std::vector<float> torque(3, 0.0f);

	D3D11_MAPPED_SUBRESOURCE msr;
	//context->Map(m_torqueCounterStaging, 0, D3D11_MAP_READ, 0, &msr);
	//if (msr.pData)
	//	count = static_cast<uint32_t*>(msr.pData)[0];
	//context->Unmap(m_torqueCounterStaging, 0);

	context->Map(m_torqueTexStaging, 0, D3D11_MAP_READ, 0, &msr);
	if (msr.pData)
	{
		int32_t* data = static_cast<int32_t*>(msr.pData);
		count = data[3];
		for (int i = 0; i < 3; ++i)
		{
			torque[i] = data[i] * 0.000001f / count; // Convert int to float with floating point precision of 6 decimals
		}

	}
	context->Unmap(m_torqueTexStaging, 0);

	if (std::isnan(torque[0])) // if velocity not yet initialized
	{
		torque = { 0.0f, 0.0f, 0.0f };
	}

	// Negative acceleration because of friction
	// Fr = cr * Fn; In our case: Fn = m * g; Fv = m * a; Fv = Fr; -> cr * FN = m * a -> cr * m * g = m * a -> a = cr * g;
	//float g = 9.81; // gravity
	//float cr = 0.0225; // Rolling resistance cooefficient for rolling bearing
	//float acc = cr * g;

	float frictionFactor = 0.95; // after one second, x% of the original velocity remains if acceleration is zero

	// Transform torque to Body Inertial frame
	XMVECTOR trq = XMVectorSet(torque[0], torque[1], torque[2], 0);
	XMVECTOR rot;
	XMVECTOR scale;
	XMVECTOR trans;
	XMMatrixDecompose(&scale, &rot, &trans, XMLoadFloat4x4(&objectToWorld));
	trq = XMVector3Rotate(trq, XMQuaternionNormalize(rot));

	// Calculate torque arround local rotation axis
	XMVECTOR localRotationAxis = XMLoadFloat3(&m_rotationAxis);
	if (!XMVector3Equal(localRotationAxis, XMVectorZero()))
		trq = XMVector3Dot(trq, localRotationAxis) * localRotationAxis;

	// Calculate new dynamic rotation from angular velocity, rotation in previous frame and elapsed time
	XMVECTOR oldAngVel = XMLoadFloat3(&m_angVel);
	XMVECTOR dynRot = XMLoadFloat4(&m_rot);
	XMVECTOR angMotion = XMVectorSet(m_angVel.x, m_angVel.y, m_angVel.z, 0) * elapsedTime;
	if (!XMVector3Equal(angMotion, XMVectorZero()))
	{
		XMVECTOR newRot = XMQuaternionRotationAxis(angMotion, XMVectorGetX(XMVector3Length(angMotion))); // Angular motion describes axis and its magnitude the angle
		XMStoreFloat4(&m_rot, XMQuaternionNormalize(XMQuaternionMultiply(newRot, dynRot))); // Perform rotation and store; Order is important ("first" rotation on the right, "second" on the left)
	}

	// Calculate new angular velocity from angular velocity in previous frame and current angular acceleration
	XMVECTOR angAcc = XMVector3Transform(trq, XMMatrixInverse(nullptr, XMLoadFloat3x3(&m_inertiaTensor)));
	XMVECTOR newAngVel = oldAngVel * std::pow(frictionFactor, elapsedTime) + angAcc * elapsedTime;
	//XMVECTOR newAngVel = oldAngVel + (angAcc - (acc * XMVector3Normalize(oldAngVel))) * elapsedTime;
	XMStoreFloat3(&m_angVel, newAngVel);

	// #############################
	// Calculate new torque on GPU
	// #############################
	//// Save old renderTarget
	//ID3D11RenderTargetView* tempRTV = nullptr;
	//ID3D11DepthStencilView* tempDSV = nullptr;
	//context->OMGetRenderTargets(1, &tempRTV, &tempDSV);
	//context->OMSetRenderTargets(0, nullptr, nullptr);

	// Save old viewport
	D3D11_VIEWPORT tempVP[1];
	UINT vpCount = 1;
	context->RSGetViewports(&vpCount, tempVP);

	// Set Layout and bind vertex and index buffer
	context->IASetInputLayout(m_mesh.getInputLayout());
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	const unsigned int strides[] = { sizeof(float) * 6 }; // 3 floats postion, 3 floats normal for mesh layout
	const unsigned int offsets[] = { 0 };
	ID3D11Buffer* vb = m_mesh.getVertexBuffer();
	ID3D11Buffer* ib = m_mesh.getIndexBuffer();
	context->IASetVertexBuffers(0, 1, &vb, strides, offsets);
	context->IASetIndexBuffer(ib, DXGI_FORMAT_R32_UINT, 0);

	// Clear torque from last frame
	const UINT initVals[] = { 0, 0, 0, 0 };
	context->ClearUnorderedAccessViewUint(m_torqueUAV, initVals);
	// Initialize counter of UAV with zero and bind UAV
	// Effects framework hides this function
	//UINT counts[] = { 0 };
	//context->OMSetRenderTargetsAndUnorderedAccessViews(D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL, nullptr, nullptr, 1, 1, &m_torqueUAV, counts);
	s_shaderVariables.torqueUAV->SetUnorderedAccessView(m_torqueUAV);


	// Prepare binding of the rest of fixed shader variables via effects framework
	s_shaderVariables.objectToWorld->SetMatrix(reinterpret_cast<const float*>(objectToWorld.m));
	s_shaderVariables.worldToVoxelTex->SetMatrix(reinterpret_cast<const float*>(worldToVoxelTex.m));
	XMFLOAT3 geometricCenter;
	m_mesh.getBoundingBox(geometricCenter, XMFLOAT3());
	s_shaderVariables.position->SetFloatVector(XMVector3Transform(XMLoadFloat3(&geometricCenter), XMLoadFloat4x4(&objectToWorld)).m128_f32);
	s_shaderVariables.velocitySRV->SetResource(velocityField);
	// Create orthogonal projection aligned with voxel grid texture space ([0,1]^3)
	XMMATRIX proj = XMMatrixOrthographicOffCenterLH(0, 1, 0, 1, 0, 1);

	// The mesh is rendered from all three main directions: for X,Y,Z direction: 1. Set Viewport 2. Create and bind ViewProjection matrix 3. Render

	D3D11_VIEWPORT vp;
	vp.TopLeftX = 10.0f;
	vp.TopLeftY = 10.0f;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;

	// Along X
	vp.Width = static_cast<float>(texResolution.z);
	vp.Height = static_cast<float>(texResolution.y);
	context->RSSetViewports(1, &vp);
	// Perform rotation and mirroring to enforce orthographic rendering along x-instead of z-axis
	// Effectively switches x and z axis
	XMMATRIX view = XMMatrixRotationNormal(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), degToRad(90)) * XMMatrixScaling(1.0f, 1.0f, -1.0f);
	s_shaderVariables.texToProj->SetMatrix(reinterpret_cast<float*>((view * proj).r));
	s_effect->GetTechniqueByName("Torque")->GetPassByName("Accumulate")->Apply(0, context);
	context->DrawIndexed(m_mesh.getNumIndices(), 0, 0);

	// Along Y
	vp.TopLeftX = 10.0f + 10.0f + static_cast<float>(texResolution.z);
	vp.Width = static_cast<float>(texResolution.x);
	vp.Height = static_cast<float>(texResolution.z);
	context->RSSetViewports(1, &vp);
	// Perform rotation and mirroring to enforce orthographic rendering along y-instead of z-axis
	// Effectively switches y and z axis
	view = XMMatrixRotationNormal(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), degToRad(-90)) * XMMatrixScaling(1.0f, 1.0f, -1.0f);
	s_shaderVariables.texToProj->SetMatrix(reinterpret_cast<const float*>((view * proj).r));
	s_effect->GetTechniqueByIndex(0)->GetPassByIndex(0)->Apply(0, context);
	context->DrawIndexed(m_mesh.getNumIndices(), 0, 0);

	// Along Z
	vp.TopLeftX = 10.0f + 10.0f + static_cast<float>(texResolution.z) + 10.0f + static_cast<float>(texResolution.x);
	vp.Width = static_cast<float>(texResolution.x);
	vp.Height = static_cast<float>(texResolution.y);
	context->RSSetViewports(1, &vp);
	// No view transformation necessary
	s_shaderVariables.texToProj->SetMatrix(reinterpret_cast<const float*>(proj.r));
	s_effect->GetTechniqueByName("Torque")->GetPassByName("Accumulate")->Apply(0, context);
	context->DrawIndexed(m_mesh.getNumIndices(), 0, 0);

	// Unbind resources
	s_shaderVariables.torqueUAV->SetUnorderedAccessView(nullptr);
	s_shaderVariables.velocitySRV->SetResource(nullptr);
	s_effect->GetTechniqueByName("Torque")->GetPassByName("Accumulate")->Apply(0, context);

	// Restore old render targets and viewport
	//context->OMSetRenderTargets(1, &tempRTV, tempDSV);
	context->RSSetViewports(1, tempVP);

	//SAFE_RELEASE(tempRTV);
	//SAFE_RELEASE(tempDSV);


	// #############################
	// Copy torque to CPU accessable memory (available in next frame)
	// #############################
	context->CopyResource(m_torqueTexStaging, m_torqueTex);
	//context->CopyStructureCount(m_torqueCounterStaging, 0, m_torqueUAV);
}

void Dynamics::reset()
{
	XMStoreFloat4(&m_rot, XMQuaternionIdentity());
	XMStoreFloat3(&m_angVel, XMVectorZero());
}

Dynamics::ShaderVariables::ShaderVariables()
	: texToProj(nullptr),
	objectToWorld(nullptr),
	worldToVoxelTex(nullptr),
	position(nullptr),
	velocitySRV(nullptr),
	torqueUAV(nullptr)
{
}

ID3DX11Effect* Dynamics::s_effect = nullptr;
Dynamics::ShaderVariables Dynamics::s_shaderVariables;