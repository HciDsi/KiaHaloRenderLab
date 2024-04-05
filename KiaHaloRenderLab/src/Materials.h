#include "Dx12Render/MathHelper.h"
#include "Dx12Render/D3DApp.h"

struct MaterialConstants
{
	DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
	float Roughness = 0.25f;
	float Matellic = 0.25f;

	// Used in texture mapping.
	DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();
};

//++++++++++++++++++++++++++Material++++++++++++++++++++++++++
class Material
{
public:
	Material();
	~Material();

	void SetDiffuseAlbedo(float r, float g, float b, float a);
	void SetFresnelR0(float r[]);
	void SetRoughness(float r);
	void SetMatellic(float m);

	std::string Name;

	int MatCBIndex = -1;

	int DiffuseSrvHeapIndex = -1;

	int NormalSrvHeapIndex = -1;

	int NumFramesDirty = 0;

	DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
	float Matellic = .25f;
	float Roughness = .25f;
	DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();
};