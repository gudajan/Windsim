#ifndef DYNAMICS_H
#define DYNAMICS_H

#include <DirectXMath.h>
#include <Windows.h>

#include <string>

struct ID3D11Buffer;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;

struct ID3DX11EffectMatrixVariable;
struct ID3DX11EffectScalarVariable;
struct ID3DX11EffectVectorVariable;
struct ID3DX11EffectUnorderedAccessViewVariable;
struct ID3DX11EffectShaderResourceVariable;
struct ID3DX11Effect;
struct ID3D11InputLayout;

class Mesh3D;

class Dynamics
{
public:
	static HRESULT createShaderFromFile(const std::wstring& path, ID3D11Device* device, const bool reload = false);
	static void releaseShader();

	Dynamics(Mesh3D& mesh);

	void calculate(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& objectToWorld, const DirectX::XMFLOAT4X4& worldToVoxelTex, const DirectX::XMUINT3& texResolution, const DirectX::XMFLOAT3& voxelSize, ID3D11ShaderResourceView* velocityField, double elapsedTime);
	void render(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4& objRot, const DirectX::XMFLOAT3& objTrans, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection, float elapsedTime);

	HRESULT create(ID3D11Device* device);
	void release();

	void setInertia(const DirectX::XMFLOAT3X3& inertia) { m_inertiaTensor = inertia; };
	void setCenterOfMass(const DirectX::XMFLOAT3& centerOfMass) { m_centerOfMass = centerOfMass; };
	void setRotationAxis(const DirectX::XMFLOAT3& axis) { DirectX::XMStoreFloat3(&m_rotationAxis, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&axis))); };
	const DirectX::XMFLOAT4& getRotation() const { return m_rot; };
	const DirectX::XMFLOAT3& getCenterOfMass() const { return m_centerOfMass; };

	void reset();

private:
	struct ShaderVariables
	{
		ShaderVariables();

		ID3DX11EffectMatrixVariable* viewProj;
		ID3DX11EffectMatrixVariable* texToProj;
		ID3DX11EffectMatrixVariable* objectToWorld;
		ID3DX11EffectMatrixVariable* worldToVoxelTex;
		ID3DX11EffectVectorVariable* position;
		ID3DX11EffectVectorVariable* angVel;
		ID3DX11EffectVectorVariable* voxelSize;
		ID3DX11EffectScalarVariable* renderDirection;

		ID3DX11EffectUnorderedAccessViewVariable* torqueUAV;
		ID3DX11EffectShaderResourceVariable* velocitySRV;
	};
	static ShaderVariables s_shaderVariables;
	static ID3DX11Effect* s_effect;

	ID3D11Buffer* m_torqueTex; // Saves the overall torque
	ID3D11Buffer* m_torqueTexStaging;
	ID3D11UnorderedAccessView* m_torqueUAV;


	Mesh3D& m_mesh;

	DirectX::XMFLOAT3X3 m_inertiaTensor;
	DirectX::XMFLOAT3 m_centerOfMass;
	DirectX::XMFLOAT3 m_rotationAxis;

	DirectX::XMFLOAT3 m_angVel;
	DirectX::XMFLOAT3 m_debugTrq;
	DirectX::XMFLOAT3 m_angAcc;
	DirectX::XMFLOAT4 m_rot; // Additional rotation arround the center of mass through Dynamics simulation

};

#endif