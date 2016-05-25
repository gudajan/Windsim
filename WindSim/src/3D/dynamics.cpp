#include "dynamics.h"
#include "mesh3D.h"
#include "common.h"
#include "settings.h"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
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

	s_shaderVariables.viewProj        = s_effect->GetVariableByName("g_mViewProj")->AsMatrix();
	s_shaderVariables.texToProj       = s_effect->GetVariableByName("g_mTexToProj")->AsMatrix();
	s_shaderVariables.objectToWorld   = s_effect->GetVariableByName("g_mObjectToWorld")->AsMatrix();
	s_shaderVariables.worldToVoxelTex = s_effect->GetVariableByName("g_mWorldToVoxelTex")->AsMatrix();
	s_shaderVariables.position        = s_effect->GetVariableByName("g_vPosition")->AsVector();
	s_shaderVariables.angVel          = s_effect->GetVariableByName("g_vAngVel")->AsVector();
	s_shaderVariables.voxelSize       = s_effect->GetVariableByName("g_vVoxelSize")->AsVector();
	s_shaderVariables.renderDirection = s_effect->GetVariableByName("g_renderDirection")->AsScalar();

	s_shaderVariables.torqueUAV       = s_effect->GetVariableByName("g_torqueUAV")->AsUnorderedAccessView();
	s_shaderVariables.pressureSRV     = s_effect->GetVariableByName("g_pressureSRV")->AsShaderResource();
	s_shaderVariables.velocitySRV     = s_effect->GetVariableByName("g_velocitySRV")->AsShaderResource();

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
	SAFE_RELEASE(m_torqueUAV);
}
Dynamics::Dynamics(Mesh3D& mesh)
	: m_torqueTex(nullptr)
	, m_torqueTexStaging(nullptr)
	, m_torqueUAV(nullptr)
	, m_mesh(mesh)
	, m_mass(0.0)
	, m_inertiaTensor()
	, m_centerOfMass(0.0f, 0.0f, 0.0f)
	, m_rotationAxis(0.0f, 0.0f, 0.0f)
	, m_angVel({ 0.0f, 0.0f, 0.0f })
	, m_angAcc({0.0f, 0.0f, 0.0f})
	, m_renderRot()
	, m_calcRot()
{
	reset();
}

void Dynamics::calculate(ID3D11Device* device, ID3D11DeviceContext* context, const XMFLOAT4X4& objectToWorld, const XMFLOAT4& objRot, const XMFLOAT4X4& worldToVoxelTex, const XMUINT3& texResolution, const XMFLOAT3& voxelSize, ID3D11ShaderResourceView* field, double elapsedTime)
{
	// Calculate new dynamic rotation from current angular velocity and rotation in previous frame and elapsed time
	XMVECTOR dynRot = XMLoadFloat4(&m_renderRot);
	XMVECTOR angMotion = XMVectorSet(m_angVel.x, m_angVel.y, m_angVel.z, 0) * elapsedTime;
	if (!XMVector3Equal(angMotion, XMVectorZero()))
	{
		// Note: that angular motion already given in left-handed system
		XMVECTOR newRot = XMQuaternionRotationAxis(angMotion, XMVectorGetX(XMVector3Length(angMotion))); // Angular motion vector describes axis and its magnitude the angle
		XMStoreFloat4(&m_renderRot, XMQuaternionNormalize(XMQuaternionMultiply(newRot, dynRot))); // Perform rotation and store; Order is important ("first" rotation on the right, "second" on the left)
	}

	// Update current dynamic acceleration by applying current angular acceleration to the current angular velocity
	float frictionFactor = conf.dyn.frictionCoefficient; // after one second, x% of the original velocity remains if acceleration would be zero

	XMVECTOR oldAngVel = XMLoadFloat3(&m_angVel);
	XMVECTOR angAcc = XMLoadFloat3(&m_angAcc);
	//XMVECTOR newAngVel = oldAngVel * std::pow(frictionFactor, elapsedTime) + angAcc * elapsedTime;
	XMVECTOR newAngVel = oldAngVel + angAcc * elapsedTime;
	XMStoreFloat3(&m_angVel, newAngVel);

	// #############################
	// Copy calculated torque from last frame to CPU
	// #############################
	int count = 0;
	std::vector<float> torque(3, 0.0f);

	D3D11_MAPPED_SUBRESOURCE msr;
	context->Map(m_torqueTexStaging, 0, D3D11_MAP_READ, 0, &msr);
	if (msr.pData)
	{
		int32_t* data = static_cast<int32_t*>(msr.pData);
		count = data[3];
		for (int i = 0; i < 3; ++i)
		{
			torque[i] = data[i] * 0.000001f; // Convert int to float with floating point precision of 6 decimals
		}
	}
	context->Unmap(m_torqueTexStaging, 0);

	if (std::isnan(torque[0])) // if velocity not yet initialized
	{
		torque = { 0.0f, 0.0f, 0.0f };
	}

	if (torque[0] <= 0.00001)
	{
		int x = 0;
	}

	// Transform torque to Body Inertial frame
	XMVECTOR trq = XMVectorSet(torque[0], torque[1], torque[2], 0);
	XMVECTOR oRot = XMLoadFloat4(&objRot);
	XMVECTOR axis;
	float angle;
	XMQuaternionToAxisAngle(&axis, &angle, oRot);
	XMStoreFloat3(&m_debugTrq, trq);
	trq = XMVector3Rotate(trq, XMQuaternionInverse(oRot));

	//// Torque because of friction
	//// Friction torque = Fn * f * d/2; see http://www.roymech.co.uk/Useful_Tables/Tribology/Bearing%20Friction.html
	float f = 0.0015; // single roll ball bearing friction coefficient
	float d = 0.5; // Diameter of the bone of the bearing / diameter of shaft
	float Fn = m_mass * 9.81; // Normal force on earth

	trq += Fn * f * 0.5 * d * XMVectorNegate(XMVector3Normalize(newAngVel)); // Friction torque in opposite direction of current velocity

	// Calculate torque arround local rotation axis
	XMVECTOR localRotationAxis = XMLoadFloat3(&m_rotationAxis);
	if (!XMVector3Equal(localRotationAxis, XMVectorZero()))
		trq = XMVector3Dot(trq, localRotationAxis) * localRotationAxis;

	// Calculate and store new angular acceleration
	XMStoreFloat3(&m_angAcc, XMVector3Transform(trq, XMMatrixInverse(nullptr, XMLoadFloat3x3(&m_inertiaTensor)))); // t = I * a -> a =  I^-1 * t


	// #############################
	// Calculate new torque on GPU
	// #############################

	// Save old renderTarget
	ID3D11RenderTargetView* tempRTV = nullptr;
	ID3D11DepthStencilView* tempDSV = nullptr;
	context->OMGetRenderTargets(1, &tempRTV, &tempDSV);
	context->OMSetRenderTargets(0, nullptr, nullptr);

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
	s_shaderVariables.torqueUAV->SetUnorderedAccessView(m_torqueUAV);

	// Prepare binding of the rest of fixed shader variables via effects framework
	XMVECTOR trans;
	XMVECTOR scale;
	XMVECTOR rot;
	XMMatrixDecompose(&scale, &rot, &trans, XMLoadFloat4x4(&objectToWorld));
	s_shaderVariables.objectToWorld->SetMatrix(reinterpret_cast<const float*>(objectToWorld.m));
	s_shaderVariables.worldToVoxelTex->SetMatrix(reinterpret_cast<const float*>(worldToVoxelTex.m));
	s_shaderVariables.position->SetFloatVector((XMVector3Rotate(XMLoadFloat3(&m_centerOfMass), rot) + trans).m128_f32); // Center of mass already scaled
	s_shaderVariables.voxelSize->SetFloatVector(reinterpret_cast<const float*>(&voxelSize));
	std::string pass;
	if (conf.dyn.method == Pressure)
	{
		s_shaderVariables.pressureSRV->SetResource(field);
		pass = "PressureCalc";
	}
	else
	{
		s_shaderVariables.velocitySRV->SetResource(field);
		pass = "VelocityCalc";
	}

	// Create orthogonal projection aligned with voxel grid texture space ([0,1]^3)
	XMMATRIX proj = XMMatrixOrthographicOffCenterLH(0, 1, 0, 1, 0, 1);

	// The mesh is rendered from all three main directions: for X,Y,Z direction: 1. Set Viewport 2. Create and bind ViewProjection matrix 3. Render

	float factor = 8.0f; // The higher, the more accurate is the force calculation
	D3D11_VIEWPORT vp;
	vp.TopLeftX = 10.0f;
	vp.TopLeftY = 10.0f;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;

	// Along X
	vp.Width = static_cast<float>(texResolution.z * factor);
	vp.Height = static_cast<float>(texResolution.y * factor);
	context->RSSetViewports(1, &vp);
	// Perform rotation and translation to enforce orthographic rendering along x-instead of z-axis
	XMMATRIX view = XMMatrixRotationNormal(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), degToRad(-90)) * XMMatrixTranslation(1.0f, 0.0f, 0.0f);
	s_shaderVariables.texToProj->SetMatrix(reinterpret_cast<float*>((view * proj).r));
	s_shaderVariables.renderDirection->SetInt(0); // 0 -> X direction
	s_effect->GetTechniqueByName("Torque")->GetPassByName(pass.c_str())->Apply(0, context);
	context->DrawIndexed(m_mesh.getNumIndices(), 0, 0);

	// Along Y
	vp.TopLeftX = 10.0f + 10.0f + static_cast<float>(texResolution.z);
	vp.Width = static_cast<float>(texResolution.x * factor);
	vp.Height = static_cast<float>(texResolution.z * factor);
	context->RSSetViewports(1, &vp);
	// Perform rotation and translation to enforce orthographic rendering along y-instead of z-axis
	view = XMMatrixRotationNormal(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), degToRad(90)) * XMMatrixTranslation(0.0f, 1.0f, 0.0f);
	s_shaderVariables.texToProj->SetMatrix(reinterpret_cast<const float*>((view * proj).r));
	s_shaderVariables.renderDirection->SetInt(1); // 1 -> Y direction
	s_effect->GetTechniqueByName("Torque")->GetPassByName(pass.c_str())->Apply(0, context);
	context->DrawIndexed(m_mesh.getNumIndices(), 0, 0);

	// Along Z
	vp.TopLeftX = 10.0f + 10.0f + static_cast<float>(texResolution.z) + 10.0f + static_cast<float>(texResolution.x);
	vp.Width = static_cast<float>(texResolution.x * factor);
	vp.Height = static_cast<float>(texResolution.y * factor);
	context->RSSetViewports(1, &vp);
	// No view transformation necessary
	s_shaderVariables.texToProj->SetMatrix(reinterpret_cast<const float*>((proj).r));
	s_shaderVariables.renderDirection->SetInt(2); // 2 -> Z direction
	s_effect->GetTechniqueByName("Torque")->GetPassByName(pass.c_str())->Apply(0, context);
	context->DrawIndexed(m_mesh.getNumIndices(), 0, 0);

	// Unbind resources
	s_shaderVariables.torqueUAV->SetUnorderedAccessView(nullptr);
	s_shaderVariables.pressureSRV->SetResource(nullptr);
	s_shaderVariables.velocitySRV->SetResource(nullptr);
	s_effect->GetTechniqueByName("Torque")->GetPassByName(pass.c_str())->Apply(0, context);

	// Restore old render targets and viewport
	context->OMSetRenderTargets(1, &tempRTV, tempDSV);
	context->RSSetViewports(1, tempVP);

	SAFE_RELEASE(tempRTV);
	SAFE_RELEASE(tempDSV);


	// #############################
	// Copy torque to CPU accessable memory (available in next frame)
	// #############################
	context->CopyResource(m_torqueTexStaging, m_torqueTex);
}

void Dynamics::render(ID3D11Device* device, ID3D11DeviceContext* context, const XMFLOAT4& objRot, const XMFLOAT3& objTrans, const XMFLOAT4X4& view, const XMFLOAT4X4& projection, float elapsedTime, bool showAccelArrow)
{
	if (showAccelArrow)
	{
		XMVECTOR rot = XMLoadFloat4(&objRot);
		XMVECTOR trans = XMLoadFloat3(&objTrans);
		// Transform angular acceleration to world space
		// Display the angular acceleration like in a right handed coordinate system -> flip direction
		s_shaderVariables.angVel->SetFloatVector(reinterpret_cast<float*>(XMVector3Rotate(XMVectorNegate(XMLoadFloat3(&m_angAcc)), rot).m128_f32));
		//s_shaderVariables.angVel->SetFloatVector(reinterpret_cast<float*>(&m_debugTrq));
		s_shaderVariables.viewProj->SetMatrix(reinterpret_cast<float*>((XMLoadFloat4x4(&view) * XMLoadFloat4x4(&projection)).r));
		// Transform center of mass to world space
		s_shaderVariables.position->SetFloatVector((XMVector3Rotate(XMLoadFloat3(&m_centerOfMass), rot) + trans).m128_f32);

		s_effect->GetTechniqueByName("Torque")->GetPassByName("AngularVelocity")->Apply(0, context);

		context->IASetInputLayout(nullptr);
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

		UINT stride = 0;
		UINT offset = 0;
		context->IASetVertexBuffers(0, 0, nullptr, &stride, &offset);
		context->Draw(1, 0);
	}
}


void Dynamics::reset()
{
	XMStoreFloat4(&m_renderRot, XMQuaternionIdentity());
	XMStoreFloat4(&m_calcRot, XMQuaternionIdentity());
	XMStoreFloat3(&m_angVel, XMVectorZero());
	XMStoreFloat3(&m_angAcc, XMVectorZero());
}

Dynamics::ShaderVariables::ShaderVariables()
	: viewProj(nullptr),
	texToProj(nullptr),
	objectToWorld(nullptr),
	worldToVoxelTex(nullptr),
	position(nullptr),
	angVel(nullptr),
	voxelSize(nullptr),
	renderDirection(nullptr),
	pressureSRV(nullptr),
	torqueUAV(nullptr)
{
}

ID3DX11Effect* Dynamics::s_effect = nullptr;
Dynamics::ShaderVariables Dynamics::s_shaderVariables;