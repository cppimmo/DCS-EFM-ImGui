/* =============================================================================
 * DCS-EFM-ImGui, file: FmGui.cpp Created: 18-JUN-2022
 *
 * Copyright 2022-2023 Brian Hoffpauir, USA
 * All rights reserved.
 *
 * Redistribution and use of this source file, with or without modification, is
 * permitted provided that the following conditions are met:
 *
 * 1. Redistributions of this source file must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * =============================================================================
 */
#include "FmGui.hpp"

#include <MinHook.h>

#include <sstream>
#include <stack>
#include <algorithm>

// DirectX headers here:
#include <d3d11.h>

// ImGui Implementation Headers here:
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

// Simple linking solution for DirectX 11:
#pragma comment(lib, "d3d11.lib")

// Function from imgui_impl_win32; defined elsewhere:
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND s_hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

using IDXGISwapChainPresentPtr = std::add_pointer<HRESULT FMGUI_FASTCALL(IDXGISwapChain *pSwapChain, UINT syncInterval, UINT flags)>::type;

namespace FmGui
{
	// Functions
	static LPVOID LookupSwapChainVTable(void);
	static std::string MinHookStatusToStdString(MH_STATUS mhStatus);
	static HRESULT FMGUI_FASTCALL SwapChainPresentImpl(IDXGISwapChain *pSwapChain, UINT syncInterval, UINT flags);
	static HRESULT GetDevice(IDXGISwapChain *const pSwapChain, ID3D11Device **ppDevice);
	static HRESULT GetDeviceContext(IDXGISwapChain *const pSwapChain, ID3D11Device **ppDevice, ID3D11DeviceContext **ppDeviceContext);
	static void OnResize(IDXGISwapChain *pSwapChain, UINT newWidth, UINT newHeight);
	static LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	
	template <typename Type>
	inline void ReleaseCOM(Type *pInterface)
	{
		if (pInterface != nullptr)
		{
			// Call release member function of COM object:
			pInterface->Release();
			pInterface = nullptr;
		}
	}

	template <typename Type>
	inline void ReleaseCOM(Type **ppInterface)
	{
		if (*ppInterface != nullptr)
		{
			(*ppInterface)->Release();
			(*ppInterface) = nullptr;
		}
	}
	// Variables
	static ID3D11Device *s_pDevice = nullptr;
	static ID3D11DeviceContext *s_pDeviceContext = nullptr;
	static ID3D11RenderTargetView *s_pRenderTargetView = nullptr;
	static constexpr LPCSTR kDXGI_MODULE_NAME = "dxgi.dll";
	static std::FILE *s_pFileStdout = stdout;
	static std::FILE *s_pFileStderr = stderr;
	// Function pointer type
	static IDXGISwapChainPresentPtr s_pSwapChainPresentTrampoline = nullptr;
	static HWND s_hWnd = nullptr;
	// WndProc used by application, in this case DCS: World
	static WNDPROC s_pWndProcApp = nullptr;
	static bool s_bInitialized = false, s_bWidgetsEnabled = false;
	static RoutinePtr s_pWidgetRoutine = nullptr;
	static InputRoutinePtr s_pInputRoutine = nullptr;
	static ImGuiContext *s_pImGuiContext = nullptr;
#if defined FMGUI_ENABLE_IMPLOT
	static ImPlotContext *s_pImPlotContext = nullptr;
#endif
	static bool s_bImplWin32Initialized = false;
	static bool s_bImplDX11Initialized = false;
	static Config s_config;
	static MessageCallback s_pMessageCallback = nullptr;
	static std::stack<Message> s_msgStack;
	static constexpr std::stack<Message>::size_type kMSG_STACK_MAX_SIZE = 24;
} // End namespace (FmGui)

// Todo turn this logic into a inline static function.
#define PUSH_MSG(SEVERITY, CONTENT) ;
	//\
	//if (s_msgStack.size() > kMSG_STACK_MAX_SIZE) \
	//{ \
	//	s_msgStack.pop(); \
	//} \
	//else \
	//{ \
	//	if (s_msgStack.top().content != CONTENT) \
	//	{ \
	//		s_msgStack.emplace(SEVERITY, CONTENT, __FILE__, __func__, __LINE__); \
	//		if (s_pMessageCallback != nullptr) \
	//			s_pMessageCallback(s_msgStack.top()); \
	//	} \
	//} \

void FmGui::SetRoutinePtr(RoutinePtr pRoutine)
{
	s_pWidgetRoutine = pRoutine;
}

void FmGui::SetInputRoutinePtr(InputRoutinePtr pInputRoutine)
{
	s_pInputRoutine = pInputRoutine;
}

void FmGui::SetMessageCallback(MessageCallback pMessageCallback)
{
	s_pMessageCallback = pMessageCallback;
}

std::string FmGui::AddressDump(void)
{
	std::ostringstream oss;
	oss.precision(2);
	oss << std::hex
		<< "ID3D11Device Pointer Location: "
		<< reinterpret_cast<void *>(&s_pDevice) << '\n'
		<< "ID3D11DeviceContext Pointer Location: "
		<< reinterpret_cast<void *>(&s_pDeviceContext) << '\n'
		<< "ID3D11RenderTargetView Pointer Location: "
		<< reinterpret_cast<void *>(&s_pRenderTargetView) << '\n';
	// << "IDXGISwapChain Pointer Location: "
	// << reinterpret_cast<void *>(&pSwapChain) << '\n';
	return oss.str();
}

std::string FmGui::DebugLayerMessageDump(void)
{
	ID3D11InfoQueue *pInfoQueue = nullptr;
	if (!s_pDevice || FAILED(s_pDevice->QueryInterface(__uuidof(ID3D11InfoQueue),
													   reinterpret_cast<void **>(&pInfoQueue))))
	{
		PUSH_MSG(MessageSeverity::kHIGH, "QueryInterface failed!");
		return std::string();
	}

	if (FAILED(pInfoQueue->PushEmptyStorageFilter()))
	{
		PUSH_MSG(MessageSeverity::kHIGH, "ID3D11InfoQueue::PushEmptyStorageFilter failed!");
		return std::string();
	}

	std::ostringstream oss;
	const UINT64 kMsgCount = pInfoQueue->GetNumStoredMessages();

	for (UINT64 index = 0; index < kMsgCount; ++index)
	{
		SIZE_T msgSize = 0;
		// Get the size of the message.
		if (FAILED(pInfoQueue->GetMessage(index, nullptr, &msgSize)))
		{
			PUSH_MSG(MessageSeverity::kHIGH, "GetMessage failed!");
			return std::string();
		}
		// Allocate memory.
		D3D11_MESSAGE *pMessage = (D3D11_MESSAGE *)std::malloc(msgSize);
		if (!pMessage)
		{
			PUSH_MSG(MessageSeverity::kHIGH, "std::malloc failed!");
			return std::string();
		}
		// Get the message itself.
		if (FAILED(pInfoQueue->GetMessage(index, pMessage, &msgSize)))
		{
			PUSH_MSG(MessageSeverity::kHIGH, "ID3D11InfoQueue::GetMessaged failed!");
			std::free(pMessage);
			return std::string();
		}
		
		oss << "D3D11 MESSAGE|ID:" << static_cast<int>(pMessage->ID)
			<< "|CATEGORY:"        << static_cast<int>(pMessage->Category)
			<< "|SEVERITY:"        << static_cast<int>(pMessage->Severity)
			<< "|DESC_LEN:"        << pMessage->DescriptionByteLength
			<< "|DESC:"            << pMessage->pDescription;
		std::free(pMessage);
	}
	pInfoQueue->ClearStoredMessages();
	ReleaseCOM(pInfoQueue);
	return std::string();
}

LPVOID FmGui::LookupSwapChainVTable(void)
{
#if 0
	/*
	 * The following code has been disabled for now.It may be further explored
	 * in the future.
	 */
	WNDCLASSEXA wndClassEx;
	ZeroMemory(&wndClassEx, sizeof(wndClassEx));
	wndClassEx.cbSize = sizeof(WNDCLASSEXA);
	wndClassEx.style = CS_HREDRAW | CS_VREDRAW;
	wndClassEx.lpfnWndProc = nullptr;
	wndClassEx.cbClsExtra = 0;
	wndClassEx.cbWndExtra = 0;
	wndClassEx.hInstance = GetModuleHandle(nullptr);
	wndClassEx.hIcon = nullptr;
	wndClassEx.hCursor = nullptr;
	wndClassEx.hbrBackground = nullptr;
	wndClassEx.lpszMenuName = nullptr;
	wndClassEx.lpszClassName = "FmGuiWndClassName";
	wndClassEx.hIconSm = nullptr;

	if (RegisterClassExA(&wndClassEx) == 0)
	{
		PUSH_MSG(MessageSeverity::kHIGH, "RegisterClassEx failed!");
		return nullptr;
	}

	constexpr int kFAKE_WND_WIDTH = 100, kFAKE_WND_HEIGHT = 100;
	HWND hLocalWnd = CreateWindowExA(
		NULL,
		wndClassEx.lpszClassName,
		"Fake Window",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		kFAKE_WND_WIDTH, kFAKE_WND_HEIGHT,
		nullptr,
		nullptr,
		wndClassEx.hInstance,
		nullptr
	);

	if (!hLocalWnd)
	{
		PUSH_MSG(MessageSeverity::kHIGH, "CreateWindowEx failed!");
		UnregisterClassA(wndClassEx.lpszClassName, wndClassEx.hInstance);
		return nullptr;
	}
#endif
	/**
	 * Setup temporary D3D11 device and swap chain in order to retrieve the address
	 * of the Present virtual member function in the VTable.
	 */
	D3D_FEATURE_LEVEL featureLevel;
	const D3D_FEATURE_LEVEL kFeatureLevels[] = {
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_11_0
	};
	constexpr UINT kNUM_FEAT_LEVELS = std::size(kFeatureLevels);

	UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined _DEBUG
	creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
	ID3D11Device *pLocalDevice = nullptr;
	ID3D11DeviceContext *pLocalDeviceContext = nullptr;
	IDXGISwapChain *pLocalSwapChain = nullptr;

	DXGI_RATIONAL refreshRateRational;
	ZeroMemory(&refreshRateRational, sizeof(refreshRateRational));
	refreshRateRational.Numerator = 60;
	refreshRateRational.Denominator = 1;

	DXGI_MODE_DESC bufferModeDesc;
	ZeroMemory(&bufferModeDesc, sizeof(bufferModeDesc));
	bufferModeDesc.Width = 100;
	bufferModeDesc.Height = 100;
	bufferModeDesc.RefreshRate = refreshRateRational;
	bufferModeDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	bufferModeDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;

	DXGI_SAMPLE_DESC sampleDesc;
	ZeroMemory(&sampleDesc, sizeof(sampleDesc));
	sampleDesc.Count = 1;
	sampleDesc.Quality = 0;

	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
	swapChainDesc.BufferDesc = bufferModeDesc;
	swapChainDesc.SampleDesc = sampleDesc;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 1;
	
	HWND hWndFg = GetForegroundWindow(); // GetActiveWindow()
	if (!hWndFg)
	{
		PUSH_MSG(MessageSeverity::kHIGH, "GetForegroundWindow failed!");
		return nullptr;
	}
	else
	{
		constexpr std::size_t kTITLE_BUFF_SIZE = 256;
		CHAR title[kTITLE_BUFF_SIZE];
		GetWindowTextA(hWndFg, title, kTITLE_BUFF_SIZE);

		char buffer[kTITLE_BUFF_SIZE];
		std::snprintf(buffer, kTITLE_BUFF_SIZE, "FmGui has window \"%s\".", title);
		PUSH_MSG(MessageSeverity::kNOTIFICATION, std::string(buffer, kTITLE_BUFF_SIZE));
	}

	swapChainDesc.OutputWindow = hWndFg;
	swapChainDesc.Windowed = TRUE;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	if (FAILED(D3D11CreateDeviceAndSwapChain(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		creationFlags,
		kFeatureLevels,
		kNUM_FEAT_LEVELS,
		D3D11_SDK_VERSION,
		&swapChainDesc,
		&pLocalSwapChain,
		&pLocalDevice,
		&featureLevel,
		&pLocalDeviceContext)))
	{
		PUSH_MSG(MessageSeverity::kHIGH, "D3D11CreateDeviceAndSwapChain failed!");
		// DestroyWindow(hLocalWnd);
		// UnregisterClassA(wndClassEx.lpszClassName, wndClassEx.hInstance);
		return nullptr;
	}

	DWORD_PTR *pLocalSwapChainVTable = nullptr;
	pLocalSwapChainVTable = reinterpret_cast<DWORD_PTR *>(pLocalSwapChain);
	pLocalSwapChainVTable = reinterpret_cast<DWORD_PTR *>(pLocalSwapChainVTable[0]);
	LPVOID result = reinterpret_cast<LPVOID>(pLocalSwapChainVTable[8]);

	ReleaseCOM(pLocalDevice);
	ReleaseCOM(pLocalDeviceContext);
	ReleaseCOM(pLocalSwapChain);
	// DestroyWindow(hLocalWnd);
	// UnregisterClassA(wndClassEx.lpszClassName, wndClassEx.hInstance);
	return result;
}

bool FmGui::SetWidgetVisibility(bool bEnabled)
{
	bool bPrevValue = s_bWidgetsEnabled;
	s_bWidgetsEnabled = bEnabled;
	return bPrevValue;
}

std::string FmGui::MinHookStatusToStdString(MH_STATUS mhStatus)
{
	constexpr std::size_t kBUFF_SIZE = 124;
	char buffer[kBUFF_SIZE];
	
	const int kWritten = std::snprintf(buffer, kBUFF_SIZE, "%s", MH_StatusToString(mhStatus));
	return (kWritten > 0) ? std::string(buffer, kWritten) : "";
}

bool FmGui::StartupHook(const Config &config)
{
	// Copy the given config:
	s_config = config;

	PUSH_MSG(MessageSeverity::kNOTIFICATION, "Redirecting Direct3D routines...");
	// HMODULE hDxgi = GetModuleHandleA(kDXGI_MODULE_NAME);
	// DWORD_PTR dwpDxgi = reinterpret_cast<DWORD_PTR>(hDxgi);
	LPVOID pSwapChainPresentOriginal = LookupSwapChainVTable();
	if (!pSwapChainPresentOriginal)
	{
		PUSH_MSG(MessageSeverity::kHIGH, "FmGui::LookupSwapChainVTable!");
	}
	// DWORD_PTR hDxgi = (DWORD_PTR)GetModuleHandle(L"dxgi.dll");
	// 
	// LPVOID pSwapChainPresentOriginal = reinterpret_cast<LPVOID>(
	// 	(IDXGISwapChainPresentPtr)((DWORD_PTR)hDxgi + 0x5070));
	
	// Initialize MinHook:
	MH_STATUS mhStatus = MH_Initialize();
	if (mhStatus != MH_OK)
	{
		PUSH_MSG(MessageSeverity::kHIGH, "MH_Initialize failed: " + MinHookStatusToStdString(mhStatus) + '!');
		return false;
	}

	// Create swap chain present trampoline using MinHook:
	mhStatus = MH_CreateHook(pSwapChainPresentOriginal, &SwapChainPresentImpl,
							 reinterpret_cast<LPVOID *>(&s_pSwapChainPresentTrampoline));
	if (mhStatus != MH_OK)
	{
		PUSH_MSG(MessageSeverity::kHIGH, "MH_CreateHook failed: " + MinHookStatusToStdString(mhStatus) + '!');
		return false;
	}
	
	// Enable the created hook:
	mhStatus = MH_EnableHook(pSwapChainPresentOriginal);
	if (mhStatus != MH_OK)
	{
		PUSH_MSG(MessageSeverity::kHIGH, "MH_EnableHook failed: " + MinHookStatusToStdString(mhStatus) + '!');
		return false;
	}

	PUSH_MSG(MessageSeverity::kNOTIFICATION, "Direct3D Redirection complete.");
	return true;
}

HRESULT FMGUI_FASTCALL FmGui::SwapChainPresentImpl(IDXGISwapChain *pSwapChain, UINT syncInterval, UINT flags)
{
	if (!s_bInitialized)
	{
		// Setup ImGui & ImPlot contexts:
		bool bResult;
		HRESULT hResult;

		PUSH_MSG(MessageSeverity::kNOTIFICATION, "Setting up present hook...");

		hResult = GetDevice(pSwapChain, &s_pDevice);
		if (FAILED(hResult))
		{
			PUSH_MSG(MessageSeverity::kHIGH, "FmGui::GetDevice failed!");
			return s_pSwapChainPresentTrampoline(pSwapChain, syncInterval, flags);
		}

		hResult = GetDeviceContext(pSwapChain, &s_pDevice, &s_pDeviceContext);
		if (FAILED(hResult))
		{
			PUSH_MSG(MessageSeverity::kHIGH, "FmGui::GetDeviceContext failed!");
			return s_pSwapChainPresentTrampoline(pSwapChain, syncInterval, flags);
		}

		s_pImGuiContext = ImGui::CreateContext();
		if (!s_pImGuiContext)
		{
			PUSH_MSG(MessageSeverity::kHIGH, "ImGui::CreateContext failed!");
			return s_pSwapChainPresentTrampoline(pSwapChain, syncInterval, flags);
		}
#ifdef FMGUI_ENABLE_IMPLOT
		s_pImPlotContext = ImPlot::CreateContext();
		if (!s_pImPlotContext)
		{
			PUSH_MSG(MessageSeverity::kHIGH, "ImPlot::CreateContext failed!");
			return s_pSwapChainPresentTrampoline(pSwapChain, syncInterval, flags);
		}
#endif
		ImGui::SetCurrentContext(s_pImGuiContext);
#ifdef FMGUI_ENABLE_IMPLOT
		ImPlot::SetCurrentContext(s_pImPlotContext);
#endif
		ImGuiIO &imGuiIO = ImGui::GetIO();
		// Configure the current ImGui content:
		imGuiIO.ConfigFlags |= s_config.configFlags;
#ifdef FMGUI_ENABLE_IMPLOT
		/**
		 * For ImPlot enable meshes with over 64,000 vertices while using the
		 * default backend 16 bit value for indexed drawing.
		 * https://github.com/ocornut/imgui/issues/2591
		 * (Extremely Important Note) Option 2:
		 * https://github.com/epezent/implot/blob/master/README.md
		 */
		imGuiIO.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
#endif
		if (s_config.iniFileName.empty())
			imGuiIO.IniFilename = nullptr;
		else
			imGuiIO.IniFilename = s_config.iniFileName.c_str();
		
		imGuiIO.IniSavingRate = s_config.iniSavingRate;
		switch (s_config.style)
		{
		case Style::kCLASSIC:
			ImGui::StyleColorsClassic();
			break;
		case Style::kDARK:
			ImGui::StyleColorsDark();
			break;
		case Style::kLIGHT:
			ImGui::StyleColorsLight();
			break;
		}

		// Get the IDXGISwapChain's description.
		DXGI_SWAP_CHAIN_DESC swapChainDesc;
		ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
		if (FAILED(pSwapChain->GetDesc(&swapChainDesc)))
		{
			PUSH_MSG(MessageSeverity::kHIGH, "IDXGISwapChain::GetDesc failed!");
			return s_pSwapChainPresentTrampoline(pSwapChain, syncInterval, flags);
		}
		// Set global window handle to the OutputWindow of the IDXGISwapChain.
		s_hWnd = swapChainDesc.OutputWindow;
		s_pWndProcApp = reinterpret_cast<WNDPROC>(SetWindowLongPtr(s_hWnd, GWLP_WNDPROC,
																 reinterpret_cast<LONG_PTR>(WndProc)));
		if (s_pWndProcApp == NULL)
		{
			PUSH_MSG(MessageSeverity::kHIGH, "SetWindowLongPtr failed!");
			return S_FALSE;
		}

		// ImGui Win32 and DX11 implementation initialization.
		bResult = ImGui_ImplWin32_Init(s_hWnd);
		s_bImplWin32Initialized = bResult;
		if (!bResult)
		{
			PUSH_MSG(MessageSeverity::kHIGH, "ImGui_ImplWin32_Init failed!");
			return S_FALSE;
		}

		bResult = ImGui_ImplDX11_Init(s_pDevice, s_pDeviceContext);
		s_bImplDX11Initialized = bResult;
		if (!bResult)
		{
			PUSH_MSG(MessageSeverity::kHIGH, "ImGui_ImplDX11_Init failed!");
			return S_FALSE;
		}
		imGuiIO.ImeWindowHandle = s_hWnd;		
		//imGuiIO.SetPlatformImeDataFn(ImGui::GetMainViewport(), );
		//ImGui::GetMainViewport()->PlatformHandleRaw = s_hWnd;

		// Retrieve the back buffer from the IDXGISwapChain.
		ID3D11Texture2D *pSwapChainBackBuffer = nullptr;
		hResult = pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<LPVOID *>(&pSwapChainBackBuffer));
		if (FAILED(hResult))
		{
			PUSH_MSG(MessageSeverity::kHIGH, "IDXGISwapChain::GetBuffer failed!");
			return S_FALSE;
		}

		hResult = s_pDevice->CreateRenderTargetView(pSwapChainBackBuffer, nullptr, &s_pRenderTargetView);
		if (FAILED(hResult))
		{
			PUSH_MSG(MessageSeverity::kHIGH, "ID3D11Device::CreateRenderTargetView failed!");
			return S_FALSE;
		}
		ReleaseCOM(pSwapChainBackBuffer);

		s_bInitialized = true; // Set to initialized
	}
	else
	{
		// Prepare next frame for ImGui: 
		ImGui::SetCurrentContext(s_pImGuiContext);
#ifdef FMGUI_ENABLE_IMPLOT
		ImPlot::SetCurrentContext(s_pImPlotContext);
#endif
		// Check for NULL context.
		if (!ImGui::GetCurrentContext())
			return s_pSwapChainPresentTrampoline(pSwapChain, syncInterval, flags);

		ImGui_ImplWin32_NewFrame();
		ImGui_ImplDX11_NewFrame();

		ImGui::NewFrame();
		if (s_bWidgetsEnabled)
		{
			if (s_pWidgetRoutine != nullptr)
				s_pWidgetRoutine();
		}
		ImGui::EndFrame();
		ImGui::Render();

		s_pDeviceContext->OMSetRenderTargets(1, &s_pRenderTargetView, nullptr);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	}
	return s_pSwapChainPresentTrampoline(pSwapChain, syncInterval, flags);
}

HRESULT FmGui::GetDevice(IDXGISwapChain *const pSwapChain, ID3D11Device **ppDevice)
{
	HRESULT retVal = pSwapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<PVOID *>(ppDevice));
	return retVal;
}

HRESULT FmGui::GetDeviceContext(IDXGISwapChain *const pSwapChain, ID3D11Device **ppDevice, ID3D11DeviceContext **ppDeviceContext)
{
	(*ppDevice)->GetImmediateContext(ppDeviceContext);
	return (ppDeviceContext != nullptr) ? S_OK : E_FAIL;
}

bool FmGui::ShutdownHook(void)
{
	// Reverse order of initialization.
	MH_STATUS mhStatus = MH_DisableHook(MH_ALL_HOOKS);
	if (mhStatus != MH_OK)
	{
		PUSH_MSG(MessageSeverity::kHIGH, "MH_DisableHook failed: " + MinHookStatusToStdString(mhStatus) + '!');
		return false;
	}

	mhStatus = MH_Uninitialize();
	if (mhStatus != MH_OK)
	{
		PUSH_MSG(MessageSeverity::kHIGH, "MH_Uninitialize failed: " + MinHookStatusToStdString(mhStatus) + '!');
		return false;
	}

	if (s_bImplDX11Initialized)
	{
		ImGui_ImplDX11_Shutdown();
		s_bImplDX11Initialized = false;
	}
	
	if (s_bImplWin32Initialized)
	{
		ImGui_ImplWin32_Shutdown();
		s_bImplWin32Initialized = false;
	}
#if defined FMGUI_ENABLE_IMPLOT
	if (s_pImPlotContext != nullptr)
	{
		ImPlot::DestroyContext(s_pImPlotContext);
	}
#endif
	
	if (s_pImGuiContext != nullptr)
	{
		ImGui::DestroyContext(s_pImGuiContext);
		s_pImGuiContext = nullptr;
	}

	ReleaseCOM(s_pDevice);
	ReleaseCOM(s_pDeviceContext);
	ReleaseCOM(s_pRenderTargetView);
	if (s_hWnd)
	{
		// Set hWnd's WndProc back to it's original proc.
		if (SetWindowLongPtr(s_hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(s_pWndProcApp)) == 0)
		{
			PUSH_MSG(MessageSeverity::kHIGH, "SetWindowLongPtr failed!");
			return false;
		}
	}
	// Set the Present initialization check to false.
	s_bInitialized = false;
	return true;
}

const FmGui::Message &FmGui::GetLastError(void)
{
	return s_msgStack.top();
}

FmGui::MessageVector FmGui::GetEveryMessage(void)
{
	MessageVector errorMsgs;
	while (!s_msgStack.empty())
	{
		errorMsgs.push_back(s_msgStack.top());
		s_msgStack.pop();
	}
	std::reverse(std::begin(errorMsgs), std::end(errorMsgs));
	return errorMsgs;
}

void FmGui::OnResize(IDXGISwapChain *pSwapChain, UINT newWidth, UINT newHeight)
{
	/**
	 * This function is not fully implemented yet.
	 *
	 * First release the RenderTargetView.
	 * Then Resize the SwapChain buffers.
	 * Get the BackBuffer, Recreate the RenderTargetView,
	 * and finally release the BackBuffer pointer.
	 * Also set the render targets of the DeviceContext as well as the
	 * rasterizer viewport.
	 */
	ReleaseCOM(s_pRenderTargetView);

	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
	if (FAILED(pSwapChain->GetDesc(&swapChainDesc)))
	{
		PUSH_MSG(MessageSeverity::kHIGH, "IDXGISwapChain::GetDesc failed!");
		return;
	}
	
	if (FAILED(pSwapChain->ResizeBuffers(swapChainDesc.BufferCount,
										 newWidth, newHeight,
										 swapChainDesc.BufferDesc.Format, 0u)))
	{
		PUSH_MSG(MessageSeverity::kHIGH, "IDXGISwapChain::ResizeBuffers failed!");
		return;
	}

	ID3D11Texture2D *pSwapChainBackBuffer = nullptr;
	if (FAILED(pSwapChain->GetBuffer(0u, __uuidof(ID3D11Texture2D),
									 reinterpret_cast<void **>(&pSwapChainBackBuffer))))
	{
		PUSH_MSG(MessageSeverity::kHIGH, "IDXGISwapChain::GetBuffer failed!");
		ReleaseCOM(pSwapChainBackBuffer);
		return;
	}

	if (FAILED(s_pDevice->CreateRenderTargetView(pSwapChainBackBuffer, nullptr, &s_pRenderTargetView)))
	{
		PUSH_MSG(MessageSeverity::kHIGH, "ID3D11Device::CreateRenderTargetView failed!");
		ReleaseCOM(pSwapChainBackBuffer);
		return;
	}
	ReleaseCOM(pSwapChainBackBuffer);

	s_pDeviceContext->OMSetRenderTargets(1, &s_pRenderTargetView, nullptr);
	CD3D11_VIEWPORT viewport(0.0f, 0.0f, static_cast<float>(newWidth), static_cast<float>(newHeight), 0.0f, 1.0f);
	s_pDeviceContext->RSSetViewports(1, &viewport);
}

LRESULT FmGui::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	// Check if there is a valid context:
	if (ImGui::GetCurrentContext())
	{
		ImGuiIO &imGuiIO = ImGui::GetIO();
		POINT cursorPos; GetCursorPos(&cursorPos);
		
		if (s_hWnd)
			ScreenToClient(s_hWnd, &cursorPos);
	
		imGuiIO.MousePos.x = cursorPos.x;
		imGuiIO.MousePos.y = cursorPos.y;
	}
	// Only handle if widgets are enabled:
	if (s_bWidgetsEnabled)
	{
		// Check for a non-NULL context and handle ImGui events
		if (ImGui::GetCurrentContext() && ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
		{
			return TRUE;
		}
		// Handle user's non-NULL input routine
		if (s_pInputRoutine != nullptr)
			s_pInputRoutine(uMsg, wParam, lParam);
	}
	// Other events:
	switch (uMsg)
	{
	case WM_SIZE:
	{
		const UINT newWidth = static_cast<UINT>(LOWORD(lParam));
		const UINT newHeight = static_cast<UINT>(HIWORD(lParam));
		// TODO: Resize IDXGISwapChain
		// OnResize(newWidth, newHeight);
		break;
	}
	default:
		break;
	}
	return CallWindowProc(s_pWndProcApp, hWnd, uMsg, wParam, lParam);
}
