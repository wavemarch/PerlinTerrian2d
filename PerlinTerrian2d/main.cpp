#include <Windows.h>

#include "d3dApp.h"
#include "d3dUtil.h"
#include "d3dx11effect.h"

struct Vertex {
	XMFLOAT3 pos;
	XMFLOAT3 normal;
};

inline float tx(float t) {
	return 6 * t*t*t*t*t - 15 * t*t*t*t + 10 * t*t*t;
}

inline float tt(float t) {
	return 3 * t*t - 2 * t*t*t;
}

inline float Lerp(float a, float b, float t) {
//	return a * (1.0f - t) + b * t;
	float pt = tt(t);
	return a * (1.0f - pt) + b * pt;
}

inline float Access(float *h, int i, int j, int width) {
	return h[i*width + j];
}

struct MyMaterial {
	XMFLOAT4 ambient;
	XMFLOAT4 diffuse;
	XMFLOAT4 specular;
};

struct MyDirectionalLight {
	XMFLOAT4 ambient;
	XMFLOAT4 diffuse;
	XMFLOAT4 specular;
	XMFLOAT3 direction;
	float pad;
};

class PerlinTerrian2d : public D3DApp {
public:
	PerlinTerrian2d(HINSTANCE hInstance);
	~PerlinTerrian2d();

	bool Init();
	void OnResize();
	void UpdateScene(float dt);
	void DrawScene();

	void OnMouseDown(WPARAM btnState, int x, int y);
	void OnMouseUp(WPARAM btnState, int x, int y);
	void OnMouseMove(WPARAM btnState, int x, int y);

private:
	int mapHeight, mapWidth;
	float *height;

	int gHeight, gWidth;
	XMFLOAT2 *gradients;

	const int factor = 50;

	float DotGradient(int iG, int jG, int iH, int jH);
	void GenerateGradients();
	void GenerateHeightMap();

	void BuildGeometry();
	void BuildEffect();
	void InputAssamble();

	ID3D11RasterizerState *mSolidState;
	ID3D11RasterizerState *mWireFrameState;

private:
	ID3D11Buffer *mVertexBuffer;
	ID3D11Buffer *mIndexBuffer;
	int M;

	ID3DX11Effect *mEffect;
	ID3DX11EffectTechnique *mTech;
	ID3DX11EffectMatrixVariable *mWorldViewProj;
	ID3DX11EffectMatrixVariable *mWorldTransForm; //newly added

	ID3DX11EffectVectorVariable *mEyePos; //newly added
	ID3DX11EffectVariable *mMaterial; //newly added
	ID3DX11EffectVariable *mDirecLight; //newly added

	ID3D11InputLayout *mInputLayout;

	XMFLOAT4X4 mWorld;
	XMFLOAT4X4 mView;
	XMFLOAT4 selfEyePos; //newly added
	XMFLOAT4X4 mProj;

	float yawAngleToOzzie;
	float pitchAngleToOzzie;
	float radiusFromOzzie;

	POINT mLastMousePos;
};

PerlinTerrian2d::PerlinTerrian2d(HINSTANCE hInstance) 
:D3DApp(hInstance), mVertexBuffer(NULL), mIndexBuffer(NULL), mEffect(NULL), mWorldViewProj(NULL), mEyePos(NULL), mWorldTransForm(NULL), mMaterial(NULL), mDirecLight(NULL), mInputLayout(NULL) {
	auto identity = XMMatrixIdentity();

	XMStoreFloat4x4(&mWorld, identity);
	XMStoreFloat4x4(&mView, identity);
	XMStoreFloat4x4(&mProj, identity);

	yawAngleToOzzie = 1.0f * MathHelper::Pi;
	pitchAngleToOzzie = 0.0f * MathHelper::Pi;

	radiusFromOzzie = 100.f;

	mLastMousePos.x = 0;
	mLastMousePos.y = 0;
}

PerlinTerrian2d::~PerlinTerrian2d() {
	ReleaseCOM(mVertexBuffer);
	ReleaseCOM(mIndexBuffer);
	ReleaseCOM(mEffect);
	ReleaseCOM(mInputLayout);
	ReleaseCOM(mSolidState);
	ReleaseCOM(mWireFrameState);

	if (gradients != NULL)
		delete[]gradients;

	if (height != NULL)
		delete[]height;
}

bool PerlinTerrian2d::Init() {
	if (!D3DApp::Init())
		return false;

	BuildGeometry();
	BuildEffect();
	InputAssamble();

	D3D11_RASTERIZER_DESC mSolidDecs;
	D3D11_RASTERIZER_DESC mWireFrameDecs;

	ZeroMemory(&mSolidDecs, sizeof(D3D11_RASTERIZER_DESC));
	mSolidDecs.FillMode = D3D11_FILL_SOLID;
	md3dDevice->CreateRasterizerState(&mSolidDecs, &mSolidState);

	ZeroMemory(&mWireFrameDecs, sizeof(D3D11_RASTERIZER_DESC));
	mWireFrameDecs.FillMode = D3D11_FILL_WIREFRAME;
	mWireFrameDecs.CullMode = D3D11_CULL_BACK;
	mWireFrameDecs.FrontCounterClockwise = false;
	mWireFrameDecs.DepthClipEnable = true;
	md3dDevice->CreateRasterizerState(&mWireFrameDecs, &mWireFrameState);

	return true;
}

void PerlinTerrian2d::OnResize() {
	D3DApp::OnResize();

	auto proj = XMMatrixPerspectiveFovLH(0.25f * XM_PI, AspectRatio(), 0.1f, 2000.0f);
	XMStoreFloat4x4(&mProj, proj);
}

void PerlinTerrian2d::UpdateScene(float dt) {
	float y = sinf(pitchAngleToOzzie) * radiusFromOzzie;
	float xzLength = sqrtf(radiusFromOzzie*radiusFromOzzie - y*y);
	float x = sinf(yawAngleToOzzie) * xzLength;
	float z = cosf(yawAngleToOzzie) * xzLength;

	XMVECTOR eyePos = XMVectorSet(x, y, z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f);

	auto view = XMMatrixLookAtLH(eyePos, target, up);
	XMStoreFloat4x4(&mView, view);

	XMStoreFloat4(&selfEyePos, eyePos);
}

void PerlinTerrian2d::DrawScene() {
	md3dImmediateContext->ClearRenderTargetView(mRenderTargetView, (const float*)&Colors::LightSteelBlue);
	md3dImmediateContext->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	md3dImmediateContext->IASetInputLayout(mInputLayout);
	md3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	md3dImmediateContext->RSSetState(mSolidState);

	UINT vStride = sizeof(Vertex);
	UINT vOffset = 0;
	md3dImmediateContext->IASetVertexBuffers(0, 1, &mVertexBuffer, &vStride, &vOffset);
	md3dImmediateContext->IASetIndexBuffer(mIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

	XMMATRIX world = XMLoadFloat4x4(&mWorld);
	XMMATRIX view = XMLoadFloat4x4(&mView);

	XMMATRIX proj = XMLoadFloat4x4(&mProj);

	XMMATRIX worldViewProj = world * view * proj;
	mWorldViewProj->SetMatrix(reinterpret_cast<float*>(&worldViewProj));
	mWorldTransForm->SetMatrix(reinterpret_cast<float*>(&world));
	mEyePos->SetFloatVector(reinterpret_cast<float*>(&selfEyePos));

	MyMaterial mt;
	mt.ambient = XMFLOAT4(0.48f, 0.77f, 0.46f, 1.0f);
	mt.diffuse = XMFLOAT4(0.48f, 0.77f, 0.46, 1.0f);
	mt.specular = XMFLOAT4(1.0f, 1.0f, 1.0f, 4.0f);
	mMaterial->SetRawValue((void*)&mt, 0, sizeof(mt));

	MyDirectionalLight lit;
	lit.ambient = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	lit.diffuse = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	lit.specular = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	lit.direction = XMFLOAT3(0.0f, -1.0f, 0.0f);
	lit.pad = 1.0f;
	mDirecLight->SetRawValue((void*)&lit, 0, sizeof(lit));

	D3DX11_TECHNIQUE_DESC tDecs;
	mTech->GetDesc(&tDecs);

	for (UINT i = 0; i < tDecs.Passes; ++i) {
		ID3DX11EffectPass *pPass = mTech->GetPassByIndex(i);
		pPass->Apply(0, md3dImmediateContext);
		md3dImmediateContext->DrawIndexed(M, 0, 0);
	}
	HR(mSwapChain->Present(0, 0));
}

void PerlinTerrian2d::OnMouseDown(WPARAM mButton, int x, int y) {
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);
}

void PerlinTerrian2d::OnMouseMove(WPARAM mButton, int x, int y) {
	if (mButton & MK_LBUTTON) {
		float dx = XMConvertToRadians(0.25f * (x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * (y - mLastMousePos.y));

		yawAngleToOzzie += dx;
		pitchAngleToOzzie += dy;

		pitchAngleToOzzie = MathHelper::Clamp(pitchAngleToOzzie, 0.1f, XM_PI - 0.1f);

	}else if (mButton & MK_RBUTTON) {
		float dx = 0.005f * XMConvertToRadians(x - mLastMousePos.x);
		float dy = 0.005f * XMConvertToRadians(y - mLastMousePos.y);

		radiusFromOzzie += (dx - dy);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void PerlinTerrian2d::OnMouseUp(WPARAM mButton, int x, int y) {
	ReleaseCapture();
}

float PerlinTerrian2d::DotGradient(int iG, int jG, int iH, int jH) {
	float gx = gradients[iG * gWidth + jG].x;
	float gy = gradients[iG * gWidth + jG].y;

	float dx = iH - iG * factor;
	float dy = jH - jG * factor;

	return gx * dx + gy * dy;
}

void PerlinTerrian2d::GenerateGradients() {
	gHeight = 20;
	gWidth = 20;
	gradients = new XMFLOAT2[gHeight*gWidth];

	srand(time(NULL));

	int index = 0;
	XMFLOAT2 *p = gradients;
	for (int i = 0; i < gHeight; ++i){
		for (int j = 0; j < gWidth; ++j){
			p->x = (rand() % 100) / 100.0f;
			p->y = sqrtf(1.0f - p->x*p->x);
			++p;
		}
	}
	p = NULL;
}

void PerlinTerrian2d::GenerateHeightMap() {
	GenerateGradients();

	mapHeight = (gHeight -1) * factor;
	mapWidth = (gWidth -1) * factor;

	height = new float[mapHeight * mapWidth];

	float *p = height;
	for (int i = 0; i < mapHeight; ++i) {
		for (int j = 0; j < mapWidth; ++j) {
			int iG = i / factor;
			int jG = j / factor;
			
			int IG = iG + 1;
			int JG = jG + 1;

			int leftBound = jG * factor;
			int rightBound = JG * factor;

			int upBound = iG * factor;
			int lowerBound = IG*factor;

			float leftUp = DotGradient(iG, jG, i, j);
			float rightUp = DotGradient(iG, JG, i, j);
			float tL = ((float)j - jG * factor) / factor;
			float Up = Lerp(leftUp, rightUp, tL);

			float leftLow = DotGradient(IG, jG, i, j);
			float rightLow = DotGradient(IG, JG, i, j);
			float Low = Lerp(leftLow, rightLow, tL);

			float tUp = ((float)(i - iG*factor)) / factor;
			
			*p = Lerp(Up, Low, tUp);

			++p;
		}
	}
	p = NULL;
}

void PerlinTerrian2d::BuildGeometry() {

	GenerateHeightMap();

	int N = mapHeight * mapWidth;

	Vertex *vertices = new Vertex[N];
	ZeroMemory(vertices, sizeof(Vertex)* N);

	Vertex *pV = vertices;
	float *pH = height;

	float xfactor = 2.0f;
	float yfactor = 2.0f;
	for (int i = 0; i < mapHeight; ++i) {
		for (int j = 0; j < mapWidth; ++j) {
			pV->pos.x = j * xfactor;
			pV->pos.z = i * yfactor;
			pV->pos.y = *pH * 5-200;

			++pV;
			++pH;
		}
	}

	pV = vertices;
	for (int i = 0; i < mapHeight; ++i) {
		for (int j = 0; j < mapWidth; ++j){
			float h = Access(height, i, j, mapWidth);

			float leftH = j>0 ? Access(height, i, j - 1, mapWidth) : 0;
			float rightH = j + 1 < mapWidth ? Access(height, i, j + 1, mapWidth) : 0;
			
			float upH = i > 0 ? Access(height, i - 1, j, mapWidth) : 0;
			float downH = i + 1 < mapHeight ? Access(height, i + 1, j, mapWidth) : 0;

			float gx = (rightH - leftH) / (2.0f * xfactor);
			float gz = (downH - upH) / (2.0f * yfactor);  

			XMFLOAT3 norm(-gx, 1.0f, -gz);
			XMVECTOR xnorm = XMLoadFloat3(&norm);
			xnorm = XMVector3Normalize(xnorm);
			XMStoreFloat3(&norm, xnorm); // need to output a log to see it

			pV->normal.x = norm.x;
			pV->normal.y = norm.y;
			pV->normal.z = norm.z;

			++pV;
		}
	}
	
	pV = NULL;
	pH = NULL;

	D3D11_BUFFER_DESC vbd;
	ZeroMemory(&vbd, sizeof(D3D11_BUFFER_DESC));
	vbd.ByteWidth = sizeof(Vertex)* N;
	vbd.Usage = D3D11_USAGE_IMMUTABLE;
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA sdata;
	ZeroMemory(&sdata, sizeof(D3D11_SUBRESOURCE_DATA));
	sdata.pSysMem = (void*)vertices;

	md3dDevice->CreateBuffer(&vbd, &sdata, &mVertexBuffer);

	M = (mapHeight - 1) * (mapWidth - 1) * 2 * 3;
	UINT *indices = new UINT[M];
	UINT *pI = indices;

	for (int i = 0; i + 1 < mapHeight; ++i) {
		for (int j = 0; j + 1 < mapWidth; ++j) {
			int I = i + 1;
			int J = j + 1;

			*(pI++) = i * mapWidth + j;
			*(pI++) = I * mapWidth + J;
			*(pI++) = i * mapWidth + J;
		}
	}
	
	for (int i = 1; i < mapHeight; ++i){
		for (int j = 1; j < mapWidth; ++j){
			int _i = i - 1;
			int _j = j - 1;

			*(pI++) = i * mapWidth + j;
			*(pI++) = _i * mapWidth + _j;
			*(pI++) = i * mapWidth + _j;
		}
	}
	int tt = pI - indices;
	assert(M == tt);
	pI = NULL;

	D3D11_BUFFER_DESC ibd;
	ZeroMemory(&ibd, sizeof(D3D11_BUFFER_DESC));
	ibd.ByteWidth = sizeof(UINT)* M;
	ibd.Usage = D3D11_USAGE_IMMUTABLE;
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA idata;
	ZeroMemory(&idata, sizeof(D3D11_SUBRESOURCE_DATA));
	idata.pSysMem = indices;

	md3dDevice->CreateBuffer(&ibd, &idata, &mIndexBuffer);

	delete []vertices;
	delete []indices;
}

void PerlinTerrian2d::BuildEffect() {
	DWORD shaderFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
	shaderFlags |= D3D10_SHADER_DEBUG;
	shaderFlags |= D3D10_SHADER_SKIP_OPTIMIZATION;
#endif

	ID3D10Blob *compiledShader = NULL;
	ID3D10Blob *compilationMsgs = 0;
	HRESULT hr = D3DX11CompileFromFile(L"color.fx", NULL, NULL, NULL, "fx_5_0", shaderFlags, NULL, NULL, &compiledShader, &compilationMsgs, NULL);

	if (compilationMsgs != NULL) {
		MessageBoxA(NULL, (char*)compilationMsgs->GetBufferPointer(), NULL, NULL);
		ReleaseCOM(compilationMsgs);
	}

	if (FAILED(hr)) {
		DXTrace(__FILE__, (DWORD)__LINE__, hr, L"D3DX11CompileFromFile", true);
	}

	HR(D3DX11CreateEffectFromMemory(compiledShader->GetBufferPointer(), compiledShader->GetBufferSize(), NULL, md3dDevice, &mEffect));

	mTech = mEffect->GetTechniqueByName("ColorTech");
	mWorldViewProj = mEffect->GetVariableByName("gWorldViewProj")->AsMatrix();
	mWorldTransForm = mEffect->GetVariableByName("gWorld")->AsMatrix();
	mEyePos = mEffect->GetVariableByName("eyePos")->AsVector();
	mMaterial = mEffect->GetVariableByName("gMaterial");
	mDirecLight = mEffect->GetVariableByName("dLight");

	ReleaseCOM(compiledShader);
}

void PerlinTerrian2d::InputAssamble() {
	D3D11_INPUT_ELEMENT_DESC vertexDecs[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};

	D3DX11_PASS_DESC pdecs;
	mTech->GetPassByName("P0")->GetDesc(&pdecs);

	HR(md3dDevice->CreateInputLayout(vertexDecs, 2, pdecs.pIAInputSignature, pdecs.IAInputSignatureSize, &mInputLayout));
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd)
{
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	PerlinTerrian2d app(hInstance);

	if (!app.Init())
		return 0;

	return app.Run();
}