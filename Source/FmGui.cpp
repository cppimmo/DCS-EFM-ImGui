/* =============================================================================
** DCS-EFM-ImGui, file: FmGui.cpp Created: 18-JUN-2022
**
** Copyright 2022 Brian Hoffpauir, USA
** All rights reserved.
**
** Redistribution and use of this source file, with or without modification, is
** permitted provided that the following conditions are met:
**
** 1. Redistributions of this source file must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
**
** 2. Redistributions in binary form must reproduce the above copyright notice,
**    this list of conditions and the following disclaimer in the documentation
**    and/or other materials provided with the distribution.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR IMPLIED
** WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
** MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO
** EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
** PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
** OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
** WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
** OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
** ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
** =============================================================================
**/
#include "FmGui.hpp"
// #include "cppimmo/FmGui.hpp"
#if defined FMGUI_CPPIMMO
#include "cppimmo/Logger.hpp"
#endif
#include <MinHook.h>

#include <sstream>

/* DirectX headers here: */
#include <d3d11.h>

/* ImGui Implementation Headers here: */
#include <imgui/imgui.h>
#include <imgui/imgui_impl_dx11.h>
#include <imgui/imgui_impl_win32.h>

/* Simple linking solution. */
#pragma comment(lib, "d3d11.lib")

/* Function from imgui_impl_win32; defined elsewhere */
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#if defined FMGUI_CPPIMMO
namespace Utils
{
#endif

namespace FmGui
{
// Functions
static LPVOID LookupSwapChainVTable(void);
static HRESULT FMGUI_FASTCALL SwapChainPresentImpl(
	IDXGISwapChain *pSwapChain,
	UINT syncInterval,
	UINT flags
);
static HRESULT GetDevice(
	IDXGISwapChain *const pSwapChain,
	ID3D11Device **ppDevice
);
static HRESULT GetDeviceContext(
	IDXGISwapChain *const pSwapChain,
	ID3D11Device **ppDevice,
	ID3D11DeviceContext **ppDeviceContext
);
static void OnResize(IDXGISwapChain *pSwapChain, UINT newWidth, UINT newHeight);
static LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
// Variables
static ID3D11Device *pDevice = nullptr;
static ID3D11DeviceContext *pDeviceContext = nullptr;
static ID3D11RenderTargetView *pRenderTargetView = nullptr;
static constexpr LPCSTR dxgiModuleName = "dxgi.dll";
static std::FILE *pFileStdout = stdout;
static std::FILE *pFileStderr = stderr;
// Function pointer type
static IDXGISwapChainPresentPtr pSwapChainPresentTrampoline = nullptr;
static HWND hWnd = nullptr;
// WndProc used by application, in this case DCS: World
static WNDPROC pWndProcApp = nullptr;
static bool isInitialized = false, areWidgetsEnabled = false;
static ImGuiRoutinePtr pWidgetRoutine = nullptr;
static ImGuiInputRoutinePtr pInputRoutine = nullptr;
// ImGui Configuration
// static constexpr ImGuiConfigFlags imGuiConfigFlags =
// 	ImGuiConfigFlags_NavNoCaptureKeyboard;
// static constexpr const char *imGuiConfigFileName = "imgui.ini";
static ImGuiContext *pImGuiContext = nullptr;
static bool isImGuiImplWin32Initialized = false;
static bool isImGuiImplDX11Initialized = false;
static FmGuiConfig fmGuiConfig;
} // namespace FmGui

void
FmGui::SetImGuiRoutinePtr(ImGuiRoutinePtr pRoutine)
{
	pWidgetRoutine = pRoutine;
}

void
FmGui::SetImGuiInputRoutinePtr(ImGuiInputRoutinePtr pInputRoutine)
{
	FmGui::pInputRoutine = pInputRoutine;
}

void
FmGui::RedirectStdOut(std::FILE *const pFile)
{
	pFileStdout = pFile;
}

void
FmGui::RedirectStdErr(std::FILE *const pFile)
{
	pFileStderr = pFile;
}

std::string
FmGui::AddressDump(void)
{
	std::ostringstream oss; oss.precision(2);
	oss << std::hex
		<< "ID3D11Device Pointer Location: "
		<< reinterpret_cast<void *>(&pDevice) << '\n'
		<< "ID3D11DeviceContext Pointer Location: "
		<< reinterpret_cast<void *>(&pDeviceContext) << '\n'
		<< "ID3D11RenderTargetView Pointer Location: "
		<< reinterpret_cast<void *>(&pRenderTargetView) << '\n';
	// << "IDXGISwapChain Pointer Location: "
	// << reinterpret_cast<void *>(&pSwapChain) << '\n';
	return oss.str();
}

#if defined FMGUI_CPPIMMO
std::optional<std::string>
#else
std::string
#endif
FmGui::DebugLayerMessageDump(void)
{
	ID3D11InfoQueue *pInfoQueue = nullptr;
	if (!pDevice || FAILED(pDevice->QueryInterface(__uuidof(ID3D11InfoQueue),
		reinterpret_cast<void **>(&pInfoQueue)))) {
#if defined FMGUI_CPPIMMO
		LOG_WRITELN_T("QueryInterface failed!", Utils::Logger::Type::ERROR);
#elif !defined FMGUI_CPPIMMO && defined FMGUI_ENABLE_IO
		std::fprintf(pFileStderr, "QueryInterface failed!\n");
#endif
#if defined FMGUI_CPPIMMO
		return std::nullopt;
#else
		return std::string();
#endif
	}

	if (FAILED(pInfoQueue->PushEmptyStorageFilter())) {
#if defined FMGUI_CPPIMMO
		LOG_WRITELN_T("PushEmptyStorageFilter failed!", Utils::Logger::Type::ERROR);
#elif !defined FMGUI_CPPIMMO && defined FMGUI_ENABLE_IO
		std::fprintf(pFileStderr, "PushEmptyStorageFilter failed!\n");
#endif
#if defined FMGUI_CPPIMMO
		return std::nullopt;
#else
		return std::string();
#endif
	}

	std::ostringstream oss;
	const UINT64 messageCount = pInfoQueue->GetNumStoredMessages();

	for (UINT64 index = 0; index < messageCount; ++index) {
		SIZE_T messageSize = 0;
		// Get the size of the message.
		if (FAILED(pInfoQueue->GetMessageW(index, nullptr, &messageSize))) {
#if defined FMGUI_CPPIMMO
			LOG_WRITELN_T("GetMessageW failed!", Utils::Logger::Type::ERROR);
#elif !defined FMGUI_CPPIMMO && defined FMGUI_ENABLE_IO
			std::fprintf(pFileStderr, "GetMessageW failed!\n");
#endif
#if defined FMGUI_CPPIMMO
			return std::nullopt;
#else
			return std::string();
#endif
		}
		// Allocate memory.
		D3D11_MESSAGE *message = (D3D11_MESSAGE *)std::malloc(messageSize);
		if (!message) {
#if defined FMGUI_CPPIMMO
			LOG_WRITELN_T("message memory alloc failed!", Utils::Logger::Type::ERROR);
#elif !defined FMGUI_CPPIMMO && defined FMGUI_ENABLE_IO
			std::fprintf(pFileStderr, "message memory alloc failed!\n");
#endif
#if defined FMGUI_CPPIMMO
			return std::nullopt;
#else
			return std::string();
#endif
		}
		// Get the message itself.
		if (FAILED(pInfoQueue->GetMessageW(index, message, &messageSize))) {
#if defined FMGUI_CPPIMMO
			LOG_WRITELN_T("GetMessageW failed!", Utils::Logger::Type::ERROR);
#elif !defined FMGUI_CPPIMMO && defined FMGUI_ENABLE_IO
			std::fprintf(pFileStderr, "GetMessageW failed!\n");
#endif
#if defined FMGUI_CPPIMMO
			return std::nullopt;
#else
			return std::string();
#endif
		}
		
		oss << "D3D11 MESSAGE|ID:" << static_cast<int>(message->ID)
			<< "|CATEGORY:" << static_cast<int>(message->Category)
			<< "|SEVERITY:" << static_cast<int>(message->Severity)
			<< "|DESC_LEN:" << message->DescriptionByteLength
			<< "|DESC:" << message->pDescription;
		std::free(message);
	}
	pInfoQueue->ClearStoredMessages();

	ReleaseCOM(pInfoQueue);
#if defined FMGUI_CPPIMMO
	return std::optional<std::string>{ oss.str() };
#else
	return std::string();
#endif
}

static LPVOID
FmGui::LookupSwapChainVTable(void)
{
	/*
	 * The following code has been disabled for now.It may be further explored
	 * in the future.
	 */
	/* WNDCLASSEXA wndClassEx;
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

	if (RegisterClassExA(&wndClassEx) == 0) {
#if defined FMGUI_CPPIMMO
		LOG_WRITELN_T("RegisterClassEx failed!", Utils::Logger::Type::ERROR);
#elif !defined FMGUI_CPPIMMO && defined FMGUI_ENABLE_IO
		std::fprintf(pFileStderr, "RegisterClassEx failed!\n");
#endif
	}

	constexpr int fakeWndWidth = 100, fakeWndHeight = 100;
	HWND hLocalWnd = CreateWindowExA(
		NULL,
		wndClassEx.lpszClassName,
		"Fake Window",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		fakeWndWidth,
		fakeWndHeight,
		nullptr,
		nullptr,
		wndClassEx.hInstance,
		nullptr
	);
	if (!hLocalWnd) {
#if defined FMGUI_CPPIMMO
		LOG_WRITELN_T("CreateWindowEx failed!", Utils::Logger::Type::ERROR);
#elif !defined FMGUI_CPPIMMO && defined FMGUI_ENABLE_IO
		std::fprintf(pFileStderr, "CreateWindowEx failed!\n");
#endif
		UnregisterClassA(wndClassEx.lpszClassName, wndClassEx.hInstance);
		return nullptr;
	}
	*/

	D3D_FEATURE_LEVEL featureLevel;
	const D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_11_0
	};
	const UINT numFeatureLevels = std::size(featureLevels);

	UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined FMGUI_CPPIMMO && IS_DEBUG
	creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#elif defined _DEBUG
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
	if (!hWndFg) {
#if defined FMGUI_CPPIMMO && IS_DEBUG
		LOG_WRITELN_T("GetForegroundWindow failed!", Utils::Logger::Type::ERROR);
#elif defined FMGUI_ENABLE_IO
		std::fprintf(pFileStderr, "GetForegroundWindow failed!\n");
#endif
		return nullptr;
	}
	else {
		constexpr int titleBufferSize = 256;
		CHAR title[titleBufferSize];
		GetWindowTextA(hWndFg, title, titleBufferSize);
#if defined FMGUI_CPPIMMO && IS_DEBUG
		LOG_WRITELN_T("FMGUI has window: \"" + std::string(title) + "\".\n",
					  Utils::Logger::Type::MESSAGE);
#elif defined FMGUI_ENABLE_IO
		std::fprintf(pFileStderr, "FMGUI has window: \"%s\".\n", title);
#endif
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
		featureLevels,
		numFeatureLevels,
		D3D11_SDK_VERSION,
		&swapChainDesc,
		&pLocalSwapChain,
		&pLocalDevice,
		&featureLevel,
		&pLocalDeviceContext
	))) {
#if defined FMGUI_CPPIMMO
		LOG_WRITELN_T("D3D11CreateDeviceAndSwapChain failed!",
					  Utils::Logger::Type::ERROR);
#elif !defined FMGUI_CPPIMMO && defined FMGUI_ENABLE_IO
		std::fprintf(pFileStderr, "D3D11CreateDeviceAndSwapChain failed!\n");
#endif
		/* DestroyWindow(hLocalWnd);
		UnregisterClassA(wndClassEx.lpszClassName, wndClassEx.hInstance); */
		return nullptr;
	}

	DWORD_PTR *pLocalSwapChainVTable = nullptr;
	pLocalSwapChainVTable = reinterpret_cast<DWORD_PTR *>(pLocalSwapChain);
	pLocalSwapChainVTable = reinterpret_cast<DWORD_PTR *>(
		pLocalSwapChainVTable[0]);
	LPVOID resultant = reinterpret_cast<LPVOID>(pLocalSwapChainVTable[8]);

	ReleaseCOM(pLocalDevice);
	ReleaseCOM(pLocalDeviceContext);
	ReleaseCOM(pLocalSwapChain);

	/*DestroyWindow(hLocalWnd);
	UnregisterClassA(wndClassEx.lpszClassName, wndClassEx.hInstance); */
	return resultant;
}

bool
FmGui::SetWidgetVisibility(bool isEnabled)
{
	const bool previousValue = areWidgetsEnabled;
	areWidgetsEnabled = isEnabled;
	return previousValue;
}

bool
FmGui::StartupHook(const FmGuiConfig &config)
{
	fmGuiConfig = config;
#if defined FMGUI_CPPIMMO
	LOG_WRITELN_T("Redirecting Direct3D routines...",
				  Utils::Logger::Type::MESSAGE);
#elif !defined FMGUI_CPPIMMO && defined FMGUI_ENABLE_IO
	std::fprintf(pFileStdout, "Redirecting Direct3D routines...\n");
#endif

	// HMODULE hDxgi = GetModuleHandleA(dxgiModuleName);
	// DWORD_PTR dwpDxgi = reinterpret_cast<DWORD_PTR>(hDxgi);
	LPVOID pSwapChainPresentOriginal = LookupSwapChainVTable();
	if (!pSwapChainPresentOriginal) {
#if defined FMGUI_CPPIMMO
		LOG_WRITELN_T("LookupSwapChainVTable failed!",
					  Utils::Logger::Type::ERROR);
#elif !defined FMGUI_CPPIMMO && defined FMGUI_ENABLE_IO
		std::fprintf(pFileStderr, "LookupSwapChainVTable failed!\n");
#endif
	}
	// DWORD_PTR hDxgi = (DWORD_PTR)GetModuleHandle(L"dxgi.dll");
	// 
	// LPVOID pSwapChainPresentOriginal = reinterpret_cast<LPVOID>(
	// 	(IDXGISwapChainPresentPtr)((DWORD_PTR)hDxgi + 0x5070));

	auto status = MH_Initialize();
	if (status != MH_OK) {
#if defined FMGUI_CPPIMMO
		LOG_WRITELN_T("MH_Initialize failed: "
					  + std::string(MH_StatusToString(status)),
					  Utils::Logger::Type::ERROR);
#elif !defined FMGUI_CPPIMMO && defined FMGUI_ENABLE_IO
		std::fprintf(pFileStderr, "MH_Initialize failed: %s!\n",
					 MH_StatusToString(status));
#endif
		return false;
	}

	status = MH_CreateHook(pSwapChainPresentOriginal, &SwapChainPresentImpl,
		reinterpret_cast<LPVOID *>(&pSwapChainPresentTrampoline));
	if (status != MH_OK) {
#if defined FMGUI_CPPIMMO
		LOG_WRITELN_T("MH_CreateHook failed: "
					  + std::string(MH_StatusToString(status)),
					  Utils::Logger::Type::ERROR);
#elif !defined FMGUI_CPPIMMO && defined FMGUI_ENABLE_IO
		std::fprintf(pFileStderr, "MH_CreateHook failed: %s!\n",
					 MH_StatusToString(status));
#endif
		return false;
	}
	status = MH_EnableHook(pSwapChainPresentOriginal);
	if (status != MH_OK) {
#if defined FMGUI_CPPIMMO
		LOG_WRITELN_T("MH_EnableHook failed: "
					  + std::string(MH_StatusToString(status)),
					  Utils::Logger::Type::ERROR);
#elif !defined FMGUI_CPPIMMO && defined FMGUI_ENABLE_IO
		std::fprintf(stderr, "MH_EnableHook failed: %s!\n",
					 MH_StatusToString(status));
#endif
		return false;
	}
#if defined FMGUI_CPPIMMO
	LOG_WRITELN("Direct3D Redirection complete.");
#elif !defined FMGUI_CPPIMMO && defined FMGUI_ENABLE_IO
	std::fprintf(pFileStdout, "Direct3D Redirection complete.\n");
#endif
	return true;
}

static HRESULT FMGUI_FASTCALL
FmGui::SwapChainPresentImpl(IDXGISwapChain *pSwapChain, UINT syncInterval,
					 UINT flags)
{
	if (!isInitialized) {
		bool boolResult;
		HRESULT hResult;

#if defined FMGUI_CPPIMMO
		LOG_WRITELN("Setting up present hook...");
#elif !defined FMGUI_CPPIMMO && defined FMGUI_ENABLE_IO
		std::fprintf(pFileStdout, "Setting up present hook...\n");
#endif
		hResult = GetDevice(pSwapChain, &pDevice);
		if (FAILED(hResult)) {
#if defined FMGUI_CPPIMMO
			LOG_WRITELN_T("FmGui::GetDevice failed!",
						  Utils::Logger::Type::ERROR);
#elif !defined FMGUI_CPPIMMO && defined FMGUI_ENABLE_IO
			std::fprintf(pFileStderr, "FmGui::GetDevice failed!\n");
#endif
			return pSwapChainPresentTrampoline(pSwapChain, syncInterval, flags);
		}
		hResult = GetDeviceContext(pSwapChain, &pDevice,
								   &pDeviceContext);
		if (FAILED(hResult)) {
#if defined FMGUI_CPPIMMO
			LOG_WRITELN_T("FmGui::GetDeviceContext failed!",
						  Utils::Logger::Type::ERROR);
#elif !defined FMGUI_CPPIMMO && defined FMGUI_ENABLE_IO
			std::fprintf(stderr, "FmGui::GetDeviceContext failed!\n");
#endif
			return pSwapChainPresentTrampoline(pSwapChain, syncInterval, flags);
		}
		pImGuiContext = ImGui::CreateContext();
		if (!pImGuiContext) {
#if defined FMGUI_CPPIMMO
			LOG_WRITELN_T("ImGui::CreateContext failed!",
						  Utils::Logger::Type::ERROR);
#elif !defined FMGUI_CPPIMMO && defined FMGUI_ENABLE_IO
			std::fprintf(stderr, "ImGui::CreateContext failed!\n");
#endif
			return pSwapChainPresentTrampoline(pSwapChain, syncInterval, flags);
		}
		ImGui::SetCurrentContext(pImGuiContext);
		ImGuiIO &imGuiIO = ImGui::GetIO();
		// Configuration of the current ImGui context.
		imGuiIO.ConfigFlags |= fmGuiConfig.imGuiConfigFlags;
		if (fmGuiConfig.imGuiIniFileName.empty())
			imGuiIO.IniFilename = nullptr;
		else
			imGuiIO.IniFilename = fmGuiConfig.imGuiIniFileName.c_str();
		imGuiIO.IniSavingRate = fmGuiConfig.imGuiIniSavingRate;
		switch (fmGuiConfig.imGuiStyle) {
		case FmGuiStyle::CLASSIC:
			ImGui::StyleColorsClassic();
			break;
		case FmGuiStyle::DARK:
			ImGui::StyleColorsDark();
			break;
		case FmGuiStyle::LIGHT:
			ImGui::StyleColorsLight();
			break;
		}

		// Get the IDXGISwapChain's description.
		DXGI_SWAP_CHAIN_DESC swapChainDesc;
		ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
		if (FAILED(pSwapChain->GetDesc(&swapChainDesc))) {
#if defined FMGUI_CPPIMMO
			LOG_WRITELN_T("IDXGISwapChain::GetDesc failed!",
						  Utils::Logger::Type::ERROR);
#elif !defined FMGUI_CPPIMMO && defined FMGUI_ENABLE_IO
			std::fprintf(stderr, "IDXGISwapChain::GetDesc failed!\n");
#endif
			return pSwapChainPresentTrampoline(pSwapChain, syncInterval, flags);
		}
		// Set global window handle to the OutputWindow of the IDXGISwapChain.
		hWnd = swapChainDesc.OutputWindow;
		pWndProcApp = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(hWnd,
			GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc)));
		if (pWndProcApp == NULL) {
#if defined FMGUI_CPPIMMO
			LOG_WRITELN_T("SetWindowLongPtrW failed!",
						  Utils::Logger::Type::ERROR);
#elif !defined FMGUI_CPPIMMO && defined FMGUI_ENABLE_IO
			std::fprintf(pFileStderr, "SetWindowLongPtrW failed!\n");
#endif
			return S_FALSE;
		}

		// ImGui Win32 and DX11 implementation initialization.
		bool result = ImGui_ImplWin32_Init(hWnd);
		isImGuiImplWin32Initialized = result;
		if (!result) {
#if defined FMGUI_CPPIMMO
			LOG_WRITELN_T("ImGui_ImplWin32_Init failed!",
						  Utils::Logger::Type::ERROR);
#elif !defined FMGUI_CPPIMMO && defined FMGUI_ENABLE_IO
			std::fprintf(pFileStderr, "ImGui_ImplWin32_Init failed!\n");
#endif
			return S_FALSE;
		}

		result = ImGui_ImplDX11_Init(pDevice, pDeviceContext);
		isImGuiImplDX11Initialized = result;
		if (!result) {
#if defined FMGUI_CPPIMMO
			LOG_WRITELN_T("ImGui_ImplDX11_Init failed!",
						  Utils::Logger::Type::ERROR);
#elif !defined FMGUI_CPPIMMO && defined FMGUI_ENABLE_IO
			std::fprintf(pFileStderr, "ImGui_ImplDX11_Init failed!\n");
#endif
			return S_FALSE;
		}
		imGuiIO.ImeWindowHandle = hWnd;

		// Retrieve the back buffer from the IDXGISwapChain.
		ID3D11Texture2D *pSwapChainBackBuffer = nullptr;
		hResult = pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
			reinterpret_cast<LPVOID *>(&pSwapChainBackBuffer));
		if (FAILED(hResult)) {
#if defined FMGUI_CPPIMMO
			LOG_WRITELN_T("IDXGISwapChain::GetBuffer failed!",
						  Utils::Logger::Type::ERROR);
#elif !defined FMGUI_CPPIMMO && defined FMGUI_ENABLE_IO
			std::fprintf(pFileStderr, "IDXGISwapChain::GetBuffer failed!\n");
#endif
			return S_FALSE;
		}
		hResult = pDevice->CreateRenderTargetView(pSwapChainBackBuffer,
												  nullptr, &pRenderTargetView);
		if (FAILED(hResult)) {
#if defined FMGUI_CPPIMMO
			LOG_WRITELN_T("ID3D11Device::CreateRenderTargetView failed!",
						  Utils::Logger::Type::ERROR);
#elif !defined FMGUI_CPPIMMO && defined FMGUI_ENABLE_IO
			std::fprintf(pFileStderr,
						 "ID3D11Device::CreateRenderTargetView failed!\n");
#endif
			return S_FALSE;
		}
		ReleaseCOM(pSwapChainBackBuffer);
		isInitialized = true;
	}
	else {
		ImGui::SetCurrentContext(pImGuiContext);
		// Check for NULL context.
		if (!ImGui::GetCurrentContext())
			return pSwapChainPresentTrampoline(pSwapChain, syncInterval, flags);
		ImGui_ImplWin32_NewFrame();
		ImGui_ImplDX11_NewFrame();

		ImGui::NewFrame();
		if (areWidgetsEnabled) {
			if (pWidgetRoutine != nullptr)
				pWidgetRoutine();
		}
		ImGui::EndFrame();
		ImGui::Render();

		pDeviceContext->OMSetRenderTargets(1, &pRenderTargetView, nullptr);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	}
	return pSwapChainPresentTrampoline(pSwapChain, syncInterval, flags);
}

static HRESULT
FmGui::GetDevice(IDXGISwapChain *const pSwapChain, ID3D11Device **ppDevice)
{
	HRESULT retVal = pSwapChain->GetDevice(__uuidof(ID3D11Device),
		reinterpret_cast<PVOID *>(ppDevice));
	return retVal;
}

static HRESULT
FmGui::GetDeviceContext(IDXGISwapChain *const pSwapChain,
	ID3D11Device **ppDevice, ID3D11DeviceContext **ppDeviceContext)
{
	(*ppDevice)->GetImmediateContext(ppDeviceContext);
	return (ppDeviceContext != nullptr) ? S_OK : E_FAIL;
}

bool
FmGui::ShutdownHook(void)
{
	// Reverse order of initialization.
	auto mhStatus = MH_DisableHook(MH_ALL_HOOKS);
	if (mhStatus != MH_OK) {
#if defined FMGUI_CPPIMMO
		LOG_WRITELN_T("MH_DisableHook failed!", Utils::Logger::Type::ERROR);
#elif !defined FMGUI_CPPIMMO && defined FMGUI_ENABLE_IO
		std::fprintf(pFileStderr, "MH_DisableHook failed!\n");
#endif
		return false;
	}
	mhStatus = MH_Uninitialize();
	if (mhStatus != MH_OK) {
#if defined FMGUI_CPPIMMO
		LOG_WRITELN_T("MH_Uninitialize failed!", Utils::Logger::Type::ERROR);
#elif !defined FMGUI_CPPIMMO && defined FMGUI_ENABLE_IO
		std::fprintf(pFileStderr, "MH_Uninitialize failed!\n");
#endif
		return false;
	}

	if (isImGuiImplDX11Initialized) {
		ImGui_ImplDX11_Shutdown();
		isImGuiImplDX11Initialized = false;
	}
	
	if (isImGuiImplWin32Initialized) {
		ImGui_ImplWin32_Shutdown();
		isImGuiImplWin32Initialized = false;
	}
	
	if (pImGuiContext != nullptr) {
		ImGui::DestroyContext(pImGuiContext);
		pImGuiContext = nullptr;
	}

	ReleaseCOM(pDevice);
	ReleaseCOM(pDeviceContext);
	ReleaseCOM(pRenderTargetView);
	if (hWnd) {
		// Set hWnd's WndProc back to it's original proc.
		if (SetWindowLongPtrW(hWnd, GWLP_WNDPROC,
			reinterpret_cast<LONG_PTR>(pWndProcApp)) == 0) {
#if defined FMGUI_CPPIMMO
			LOG_WRITELN_T("SetWindowLongPtrW failed!",
						  Utils::Logger::Type::ERROR);
#elif !defined FMGUI_CPPIMMO && defined FMGUI_ENABLE_IO
			std::fprintf(pFileStderr, "SetWindowLongPtrW failed!\n");
#endif
			return false;
		}
	}
	// Set the Present initialization check to false.
	isInitialized = false;
	return true;
}

static void
FmGui::OnResize(IDXGISwapChain *pSwapChain, UINT newWidth, UINT newHeight)
{
	/*
	 * This function is not fully implemented yet.
	 *
	 * First release the RenderTargetView.
	 * Then Resize the SwapChain buffers.
	 * Get the BackBuffer, Recreate the RenderTargetView,
	 * and finally release the BackBuffer pointer.
	 * Also set the render targets of the DeviceContext as well as the
	 * rasterizer viewport.
	 */
	ReleaseCOM(pRenderTargetView);

	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
	if (FAILED(pSwapChain->GetDesc(&swapChainDesc))) {
#if defined FMGUI_CPPIMMO
		LOG_WRITELN_T("IDXGISwapChain::GetDesc failed!",
					  Utils::Logger::Type::ERROR);
#elif !defined FMGUI_CPPIMMO && defined FMGUI_ENABLE_IO
		std::fprintf(pFileStderr, "IDXGISwapChain::GetDesc failed!\n");
#endif
		return;
	}
	
	if (FAILED(pSwapChain->ResizeBuffers(
		swapChainDesc.BufferCount,
		newWidth, newHeight,
		swapChainDesc.BufferDesc.Format, 0u))) {
#if defined FMGUI_CPPIMMO
		LOG_WRITELN_T("IDXGISwapChain::ResizeBuffers failed!",
					  Utils::Logger::Type::ERROR);
#elif !defined FMGUI_CPPIMMO && defined FMGUI_ENABLE_IO
		std::fprintf(pFileStderr, "IDXGISwapChain::ResizeBuffers failed!\n");
#endif
		return;
	}

	ID3D11Texture2D *pSwapChainBackBuffer = nullptr;
	if (FAILED(pSwapChain->GetBuffer(0u, __uuidof(ID3D11Texture2D),
		reinterpret_cast<void **>(&pSwapChainBackBuffer)))) {
#if defined FMGUI_CPPIMMO
		LOG_WRITELN_T("IDXGISwapChain::GetBuffer failed!",
					  Utils::Logger::Type::ERROR);
#elif !defined FMGUI_CPPIMMO && defined FMGUI_ENABLE_IO
		std::fprintf(pFileStderr, "IDXGISwapChain::GetBuffer failed!\n");
#endif
		ReleaseCOM(pSwapChainBackBuffer);
		return;
	}

	if (FAILED(pDevice->CreateRenderTargetView(pSwapChainBackBuffer, nullptr,
											   &pRenderTargetView))) {
#if defined FMGUI_CPPIMMO
		LOG_WRITELN_T("ID3D11Device::CreateRenderTargetView failed!",
					  Utils::Logger::Type::ERROR);
#elif !defined FMGUI_CPPIMMO && defined FMGUI_ENABLE_IO
		std::fprintf(pFileStderr,
					 "ID3D11Device::CreateRenderTargetView failed!\n");
#endif
		ReleaseCOM(pSwapChainBackBuffer);
		return;
	}
	ReleaseCOM(pSwapChainBackBuffer);
	pDeviceContext->OMSetRenderTargets(1, &pRenderTargetView, nullptr);
	CD3D11_VIEWPORT viewport(0.0f, 0.0f, static_cast<float>(newWidth),
							 static_cast<float>(newHeight), 0.0f, 1.0f);
	pDeviceContext->RSSetViewports(1, &viewport);
}

static LRESULT
FmGui::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	// Check if there is a valid context.
	if (ImGui::GetCurrentContext()) {
		ImGuiIO &imGuiIO = ImGui::GetIO();
		POINT cursorPos; GetCursorPos(&cursorPos);
		if (FmGui::hWnd)
			ScreenToClient(FmGui::hWnd, &cursorPos);
		imGuiIO.MousePos.x = cursorPos.x;
		imGuiIO.MousePos.y = cursorPos.y;
	}
	// Only handle if widgets are enabled.
	if (areWidgetsEnabled) {
		// Check for a non-NULL context and handle ImGui events.
		if (ImGui::GetCurrentContext()
			&& ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam)) {
			return TRUE;
		}
		// Handle user's non-NULL input routine.
		if (pInputRoutine != nullptr)
			pInputRoutine(uMsg, wParam, lParam);
	}
	// Other events.
	switch (uMsg) {
	case WM_SIZE: {
		const UINT newWidth = static_cast<UINT>(LOWORD(lParam));
		const UINT newHeight = static_cast<UINT>(HIWORD(lParam));
		// TODO: Resize IDXGISwapChain
		// OnResize(newWidth, newHeight);
		break;
	}
	}
	return CallWindowProcW(pWndProcApp, hWnd, uMsg, wParam, lParam);
}

#if defined FMGUI_CPPIMMO
} // namespace Utils
#endif

/*
 * FmGuiConfig's members need be defined where imgui/imgui.h is included. 
 */
FmGuiConfig::FmGuiConfig(void)
	: imGuiStyle(FmGuiStyle::DARK),
	  imGuiConfigFlags(static_cast<ImGuiConfigFlags>(ImGuiConfigFlags_NavNoCaptureKeyboard)),
	  imGuiIniFileName(),
	  imGuiIniSavingRate(5.0f)
{
}
