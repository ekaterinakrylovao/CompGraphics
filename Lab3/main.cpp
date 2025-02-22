#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <string>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "user32.lib")

// Глобальные переменные
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
D3D_DRIVER_TYPE g_driverType = D3D_DRIVER_TYPE_NULL;
D3D_FEATURE_LEVEL g_featureLevel = D3D_FEATURE_LEVEL_11_0;

ID3D11VertexShader* g_pVertexShader = nullptr;
ID3D11PixelShader* g_pPixelShader = nullptr;
ID3D11InputLayout* g_pVertexLayout = nullptr;
ID3D11Buffer* g_pVertexBuffer = nullptr;
ID3D11Buffer* g_pIndexBuffer = nullptr;
ID3D11Buffer* g_pConstantBufferWorld = nullptr;
ID3D11Buffer* g_pConstantBufferViewProjection = nullptr;

float g_CameraPitch = 0.0f;
float g_CameraYaw = 0.0f;

HWND g_hWnd = nullptr;

bool g_CameraUpdated = false; // Флаг для обновления камеры

// Встроенные шейдеры
const char* vertexShaderCode = R"(
cbuffer ConstantBufferWorld : register(b0)
{
    matrix mWorld;
};

cbuffer ConstantBufferViewProjection : register(b1)
{
    matrix mView;
    matrix mProjection;
};

struct VS_INPUT
{
    float4 Pos : POSITION;
    float4 Color : COLOR;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float4 Color : COLOR;
};

PS_INPUT main(VS_INPUT input)
{
    PS_INPUT output;
    output.Pos = mul(input.Pos, mWorld);
    output.Pos = mul(output.Pos, mView);
    output.Pos = mul(output.Pos, mProjection);
    output.Color = input.Color;
    return output;
}
)";

const char* pixelShaderCode = R"(
struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float4 Color : COLOR;
};

float4 main(PS_INPUT input) : SV_Target
{
    return input.Color;
}
)";

// Определение структуры вершины
struct SimpleVertex
{
    DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT4 Color;
};

// Структуры для константных буферов
struct ConstantBufferWorld
{
    DirectX::XMMATRIX mWorld;
};

struct ConstantBufferViewProjection
{
    DirectX::XMMATRIX mView;
    DirectX::XMMATRIX mProjection;
};

HRESULT InitDevice(HWND hWnd);
void CleanupDevice();
void Render();

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

int WINAPI wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow
)
{
    WNDCLASSEX wcex = { sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, WndProc, 0, 0, hInstance, nullptr, nullptr, nullptr, nullptr, L"DirectXApp", nullptr };
    RegisterClassEx(&wcex);

    g_hWnd = CreateWindow(L"DirectXApp", L"DirectX App", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1000, 600, nullptr, nullptr, hInstance, nullptr);
    if (!g_hWnd) return -1;

    if (FAILED(InitDevice(g_hWnd)))
    {
        CleanupDevice();
        return -1;
    }

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    MSG msg = { 0 };
    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            Render();
        }
    }

    CleanupDevice();
    return (int)msg.wParam;
}

HRESULT InitDevice(HWND hWnd)
{
    HRESULT hr = S_OK;

    RECT rc;
    GetClientRect(hWnd, &rc);
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_DRIVER_TYPE driverTypes[] =
    {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
    };
    UINT numDriverTypes = ARRAYSIZE(driverTypes);

    D3D_FEATURE_LEVEL featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    UINT numFeatureLevels = ARRAYSIZE(featureLevels);

    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = width;
    sd.BufferDesc.Height = height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.Flags = 0;

    for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++)
    {
        g_driverType = driverTypes[driverTypeIndex];
        hr = D3D11CreateDeviceAndSwapChain(nullptr, g_driverType, nullptr, createDeviceFlags, featureLevels, numFeatureLevels,
            D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext);
        if (SUCCEEDED(hr))
            break;
    }
    if (FAILED(hr))
        return hr;

    ID3D11Texture2D* pBackBuffer = nullptr;
    hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    if (FAILED(hr))
    {
        return hr;
    }

    hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
    pBackBuffer->Release();
    if (FAILED(hr))
    {
        return hr;
    }

    g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);

    D3D11_VIEWPORT vp;
    vp.Width = (FLOAT)width;
    vp.Height = (FLOAT)height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    g_pImmediateContext->RSSetViewports(1, &vp);

    // Компиляция вершинного шейдера
    ID3DBlob* pVSBlob = nullptr;
    hr = D3DCompile(vertexShaderCode, strlen(vertexShaderCode), "vertexShader", nullptr, nullptr, "main", "vs_5_0", 0, 0, &pVSBlob, nullptr);
    if (FAILED(hr))
    {
        MessageBox(hWnd, L"Ошибка компиляции вершинного шейдера", L"Ошибка", MB_OK);
        return hr;
    }

    hr = g_pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &g_pVertexShader);
    if (FAILED(hr))
    {
        pVSBlob->Release();
        return hr;
    }

    // Создание входного лейаута
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    UINT numElements = ARRAYSIZE(layout);

    hr = g_pd3dDevice->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &g_pVertexLayout);
    pVSBlob->Release();
    if (FAILED(hr))
    {
        return hr;
    }

    g_pImmediateContext->IASetInputLayout(g_pVertexLayout);

    // Создание вершинного буфера
    SimpleVertex vertices[] =
    {
        { DirectX::XMFLOAT3(-1.0f, 1.0f, -1.0f), DirectX::XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },
        { DirectX::XMFLOAT3(1.0f, 1.0f, -1.0f), DirectX::XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
        { DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f), DirectX::XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
        { DirectX::XMFLOAT3(-1.0f, 1.0f, 1.0f), DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
        { DirectX::XMFLOAT3(-1.0f, -1.0f, -1.0f), DirectX::XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f) },
        { DirectX::XMFLOAT3(1.0f, -1.0f, -1.0f), DirectX::XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
        { DirectX::XMFLOAT3(1.0f, -1.0f, 1.0f), DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) },
        { DirectX::XMFLOAT3(-1.0f, -1.0f, 1.0f), DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f) },
    };

    D3D11_BUFFER_DESC bd;
    ZeroMemory(&bd, sizeof(bd));
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(SimpleVertex) * 8;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA InitData;
    ZeroMemory(&InitData, sizeof(InitData));
    InitData.pSysMem = vertices;

    hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pVertexBuffer);
    if (FAILED(hr))
    {
        return hr;
    }

    // Создание индексного буфера
    WORD indices[] =
    {
        3,1,0,
        2,1,3,

        0,5,4,
        1,5,0,

        3,4,7,
        0,4,3,

        1,6,5,
        2,6,1,

        2,7,6,
        3,7,2,

        6,4,5,
        7,4,6,
    };

    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(WORD) * 36;
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = 0;
    InitData.pSysMem = indices;

    hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pIndexBuffer);
    if (FAILED(hr))
    {
        return hr;
    }

    // Создание константных буферов
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = sizeof(ConstantBufferWorld);
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = g_pd3dDevice->CreateBuffer(&bd, nullptr, &g_pConstantBufferWorld);
    if (FAILED(hr))
    {
        return hr;
    }

    bd.ByteWidth = sizeof(ConstantBufferViewProjection);
    hr = g_pd3dDevice->CreateBuffer(&bd, nullptr, &g_pConstantBufferViewProjection);
    if (FAILED(hr))
    {
        return hr;
    }

    // Компиляция пиксельного шейдера
    ID3DBlob* pPSBlob = nullptr;
    hr = D3DCompile(pixelShaderCode, strlen(pixelShaderCode), "pixelShader", nullptr, nullptr, "main", "ps_5_0", 0, 0, &pPSBlob, nullptr);
    if (FAILED(hr))
    {
        MessageBox(hWnd, L"Ошибка компиляции пиксельного шейдера", L"Ошибка", MB_OK);
        return hr;
    }

    hr = g_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &g_pPixelShader);
    if (FAILED(hr))
    {
        pPSBlob->Release();
        return hr;
    }

    return S_OK;
}

void CleanupDevice()
{
    if (g_pImmediateContext) g_pImmediateContext->ClearState();

    if (g_pConstantBufferWorld) g_pConstantBufferWorld->Release();
    if (g_pConstantBufferViewProjection) g_pConstantBufferViewProjection->Release();
    if (g_pVertexBuffer) g_pVertexBuffer->Release();
    if (g_pIndexBuffer) g_pIndexBuffer->Release();
    if (g_pVertexLayout) g_pVertexLayout->Release();
    if (g_pVertexShader) g_pVertexShader->Release();
    if (g_pPixelShader) g_pPixelShader->Release();

    if (g_pRenderTargetView)
    {
        g_pRenderTargetView->Release();
        g_pRenderTargetView = nullptr;
    }

    if (g_pSwapChain)
    {
        g_pSwapChain->Release();
        g_pSwapChain = nullptr;
    }

    if (g_pImmediateContext)
    {
        g_pImmediateContext->Release();
        g_pImmediateContext = nullptr;
    }

    if (g_pd3dDevice)
    {
        g_pd3dDevice->Release();
        g_pd3dDevice = nullptr;
    }
}

void Render()
{
    // Привязываем Render Target View к контексту устройства
    g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);

    // Управляем временем для плавного вращения
    static ULONGLONG timeStart = 0;
    ULONGLONG timeCur = GetTickCount64();
    if (timeStart == 0)
        timeStart = timeCur;
    float t = (timeCur - timeStart) / 1000.0f; // Время в секундах

    // Обновление мировой матрицы
    DirectX::XMMATRIX world = DirectX::XMMatrixRotationY(t);

    // Позиция камеры
    static DirectX::XMVECTOR eye = DirectX::XMVectorSet(0.0f, 1.0f, -5.0f, 0.0f);
    static DirectX::XMVECTOR at = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    static DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    // Обновляем матрицу вида, если камера была изменена
    if (g_CameraUpdated)
    {
        // Создаем матрицу вращения камеры
        DirectX::XMMATRIX rotationMatrix = DirectX::XMMatrixRotationRollPitchYaw(g_CameraPitch, g_CameraYaw, 0.0f);

        // Применяем вращение к позиции камеры и направлению взгляда
        eye = DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(0.0f, 1.0f, -5.0f, 0.0f), rotationMatrix);
        at = DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), rotationMatrix);
        up = DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), rotationMatrix);

        g_CameraUpdated = false; // Сбрасываем флаг
    }

    // Создаем видовую матрицу
    DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(eye, at, up);

    // Обновление проекционной матрицы
    RECT rc;
    GetClientRect(g_hWnd, &rc);
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;
    DirectX::XMMATRIX projection = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV2, width / (FLOAT)height, 0.01f, 100.0f);

    // Обновление константных буферов
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    ConstantBufferWorld cbWorld;
    cbWorld.mWorld = DirectX::XMMatrixTranspose(world);
    g_pImmediateContext->Map(g_pConstantBufferWorld, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    memcpy(mappedResource.pData, &cbWorld, sizeof(ConstantBufferWorld));
    g_pImmediateContext->Unmap(g_pConstantBufferWorld, 0);

    ConstantBufferViewProjection cbViewProjection;
    cbViewProjection.mView = DirectX::XMMatrixTranspose(view);
    cbViewProjection.mProjection = DirectX::XMMatrixTranspose(projection);
    g_pImmediateContext->Map(g_pConstantBufferViewProjection, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    memcpy(mappedResource.pData, &cbViewProjection, sizeof(ConstantBufferViewProjection));
    g_pImmediateContext->Unmap(g_pConstantBufferViewProjection, 0);

    // Очистка экрана
    float clearColor[4] = { 0.0f, 0.2f, 0.4f, 1.0f };
    g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);

    // Установка шейдеров и константных буферов
    g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
    g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBufferWorld);
    g_pImmediateContext->VSSetConstantBuffers(1, 1, &g_pConstantBufferViewProjection);
    g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);

    // Установка вершинного буфера и индексов
    UINT stride = sizeof(SimpleVertex);
    UINT offset = 0;
    g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
    g_pImmediateContext->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Отрисовка кубика
    g_pImmediateContext->DrawIndexed(36, 0, 0);

    // Презентация кадра
    g_pSwapChain->Present(0, 0);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_SIZE:
        if (g_pSwapChain && wParam != SIZE_MINIMIZED)
        {
            // Освобождаем существующий Render Target View
            if (g_pRenderTargetView)
            {
                g_pRenderTargetView->Release();
                g_pRenderTargetView = nullptr;
            }

            // Изменяем размер буферов SwapChain
            HRESULT hr = g_pSwapChain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            if (FAILED(hr))
            {
                return DefWindowProc(hWnd, message, wParam, lParam);
            }

            // Получаем новый back buffer и создаем Render Target View
            ID3D11Texture2D* pBackBuffer = nullptr;
            hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
            if (SUCCEEDED(hr))
            {
                hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
                pBackBuffer->Release();
            }

            if (FAILED(hr))
            {
                return DefWindowProc(hWnd, message, wParam, lParam);
            }

            // Привязываем Render Target View к контексту устройства
            g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);

            // Обновляем Viewport
            D3D11_VIEWPORT vp;
            vp.Width = (FLOAT)LOWORD(lParam);
            vp.Height = (FLOAT)HIWORD(lParam);
            vp.MinDepth = 0.0f;
            vp.MaxDepth = 1.0f;
            vp.TopLeftX = 0;
            vp.TopLeftY = 0;
            g_pImmediateContext->RSSetViewports(1, &vp);
        }
        break;

    case WM_KEYDOWN:
        switch (wParam)
        {
        case VK_UP:
            g_CameraPitch += 0.01f;
            g_CameraUpdated = true; // Камера обновлена
            break;
        case VK_DOWN:
            g_CameraPitch -= 0.01f;
            g_CameraUpdated = true; // Камера обновлена
            break;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}