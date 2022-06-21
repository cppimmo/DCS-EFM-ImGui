/* =============================================================================
** DCS-EFM-ImGui, file: FmGui.hpp Created: 18-JUN-2022
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
#ifndef __FMGUI_HPP__
#define __FMGUI_HPP__

/* 
 * This a preprocessor flag specific to the author's project. It should be left
 * disabled to build for other projects.
 */
// #define FMGUI_CPPIMMO

#if defined FMGUI_CPPIMMO
#include "Winclude.h"
#else
#include <Windows.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <type_traits>
#include <string>
#if defined FMGUI_CPPIMMO
#include <optional>
#endif

/*
 * ImGui headers not included in this file. The user will need to do this
 * themselves
 */
// #include <imgui/imgui.h>

// Define FMGUI_ENABLE_IO to enable output.
#define FMGUI_ENABLE_IO

#if !defined(FMGUI_FASTCALL)
#define FMGUI_FASTCALL __fastcall
#endif

// Disable ImGui XINPUT.
#define IMGUI_IMPL_WIN32_DISABLE_GAMEPAD

// Forward declare IDXGISwapChain structure.
struct IDXGISwapChain;

#if defined FMGUI_CPPIMMO
namespace Utils
{
#endif

namespace FmGui
{

using IDXGISwapChainPresentPtr =
	std::add_pointer<HRESULT FMGUI_FASTCALL(IDXGISwapChain *pSwapChain,
		UINT syncInterval, UINT flags)>::type;
using ImGuiRoutinePtr = std::add_pointer<void(void)>::type;
using ImGuiInputRoutinePtr = std::add_pointer<void(UINT uMsg, WPARAM wParam,
												   LPARAM lParam)>::type;

/*
 * Set pointer to function that uses the ImGui immediate mode widgets.
 * See ImGuiRoutinePtr for a specification.
 * Example:
 * void FMImGuiRoutine(void)
 * {
 * 	   ImGui::ShowDemoWindow();
 * }
 * Elsewhere perform a call to SetImGuiRoutinePtr(FMImGuiRoutine);
 */
void SetImGuiRoutinePtr(ImGuiRoutinePtr pRoutine);
/*
 * Set pointer to function that handles Win32 WndProc input.
 * See ImGuiInputRoutinePtr for a specification.
 * Example:
 * void FMImGuiInputRoutine(UINT uMsg, WPARAM wParam, LPARAM lParam)
 * {
 * 	   // Toggle widgets on Alt + W keypress.
 * 	   static bool areWidgetsEnabled = true;
 * 	   if (uMsg == WM_KEYDOWN) {
 * 	   	   if (wParam == 'W' && (GetAsyncKeyState(VK_MENU) & 0x8000)) {
 * 	   	   	   areWidgetsEnabled = !areWidgetsEnabled;
 * 	   	   	   FmGui::ChangeWidgetVisibility(areWidgetsEnabled);
 * 	   	   }
 * 	   }
 * }
 * Elsewhere perform a call to SetImGuiInputRoutinePtr(FMImGuiInputRoutine);
 * 
 * Supplementary reading:
 * https://docs.microsoft.com/en-us/windows/win32/inputdev/using-keyboard-input
 * https://docs.microsoft.com/en-us/windows/win32/inputdev/keyboard-input
 * https://docs.microsoft.com/en-us/windows/win32/learnwin32/keyboard-input
 */
void SetImGuiInputRoutinePtr(ImGuiInputRoutinePtr pInputRoutine);
/*
 * Redirect FmGui's _DEBUG stdout output to a different file.
 */
void RedirectStdOut(std::FILE *const pFile);
/*
 * Redirect FmGui's _DEBUG stderr output to a different file.
 */
void RedirectStdErr(std::FILE *const pFile);
/*
 * Set all widget visibility and return previous value.
 */
bool SetWidgetVisibility(bool isEnabled);
/*
 * Start the FmGui and ImGui.
 */
#if defined FMGUI_CPPIMMO
[[nodiscard]] bool StartupHook(void);
#else
bool StartupHook(void);
#endif
/*
 * Return formatted string of the D3D context memory addresses.
 */
std::string AddressDump(void);
/*
 * Return formatted string of the D3D debug layer warning/error
 * messages. Note: DirectX 11 does not have a callback to do this
 * in real time. Only a vaild call if the hook was started successfully.
 */
#if defined FMGUI_CPPIMMO
std::optional<std::string> DebugLayerMessageDump(void);
#else
std::string DebugLayerMessageDump(void);
#endif
/*
 * Shutdown the FmGui and ImGui.
 */
#if defined FMGUI_CPPIMMO
[[nodiscard]] bool ShutdownHook(void);
#else
bool ShutdownHook(void);
#endif

} // namespace FmGui

namespace FmGui
{
/*
 * These functions aren't meant for users.
 */
template<typename Type>
inline void
ReleaseCOM(Type *pInterface)
{
	if (pInterface != nullptr) {
		pInterface->Release();
		pInterface = nullptr;
	}
}

template<typename Type>
inline void
ReleaseCOM(Type **ppInterface)
{
	if (*ppInterface != nullptr) {
		(*ppInterface)->Release();
		(*ppInterface) = nullptr;
	}
}

} // namespace FmGui

#if defined FMGUI_CPPIMMO
} // namespace Utils
#endif

#endif // __FMGUI_HPP__