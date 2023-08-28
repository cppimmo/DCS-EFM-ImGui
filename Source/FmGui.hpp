/* =============================================================================
 * DCS-EFM-ImGui, file: FmGui.hpp Created: 18-JUN-2022
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
#ifndef FMGUI_FMGUI_HPP
#define FMGUI_FMGUI_HPP

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <cstdio>
#include <cstdlib>
#include <type_traits>
#include <string>
#include <vector>

/**
 * Adjust ImGui & ImPlot header names as needed.
 */
#include <imgui.h>
/**
 * Note: The build process must define FMGUI_ENABLE_IMPLOT to enable the ImPlot extension.
 */
#if defined FMGUI_ENABLE_IMPLOT
#include <implot.h>
#endif

#if !defined(FMGUI_FASTCALL)
#define FMGUI_FASTCALL __fastcall
#endif

// Disable ImGui XINPUT
#define IMGUI_IMPL_WIN32_DISABLE_GAMEPAD

// Forward declare IDXGISwapChain structure.
struct IDXGISwapChain;

// Forward declare typedef for ImGuiConfigFlags.
using ImGuiConfigFlags = int;

//! FmGui types & functions.
namespace FmGui
{
	//! ImGui display style.
	enum struct Style
	{
		kCLASSIC,
		kDARK,
		kLIGHT
	};
	//! FmGui configuration data.
	struct Config
	{
		/**
		 * Enumeration that can be set to the three default styles provided by ImGui
		 * in the form Style::kCLASSIC, Style::kDARK, & Style::kLIGHT.
		 * Default value: Style::kDARK
		 */
		Style style = Style::kDARK;
		/**
		 * The configuration flags passed to the ImGui context. See ImGui
		 * documentation for ImGuiConfigFlags.
		 * Default value: ImGuiConfigFlags_NavNoCaptureKeyboard
		 */
		ImGuiConfigFlags configFlags = ImGuiConfigFlags_NavNoCaptureKeyboard;
		/**
		 * Full path and filename of the auto generated ImGui .ini configuration file.
		 * This can be a full or relative path. See Examples/Fm.cpp for more info.
		 * Setting this string to empty results in no configuration file.
		 * Default value: "imgui.ini"
		 */
		std::string iniFileName = "imgui.ini";
		/*
		 * The rate in seconds at which the .ini configuration file is updated.
		 * Only applicable when the ImGui .ini is enabled.
		 * Default value: 5.0f
		 */
		float iniSavingRate = 5.0f;
	};
	//! FmGui message severity levels.
	enum struct MessageSeverity
	{
		kNOTIFICATION,
		kLOW,
		kMEDIUM,
		kHIGH
	};
	//! FmGui message data.
	struct Message
	{
		MessageSeverity severity;
		std::string content;
		std::string file;
		std::string function;
		std::size_t line;
	};

	using MessageVector = std::vector<Message>;
	//! Alias for message callback.
	using MessageCallback = std::add_pointer<void(const Message &message)>::type;
	//! Alias for FmGui routine function.
	using RoutinePtr = std::add_pointer<void(void)>::type;
	//! Alias for FmGui input routine function.
	using InputRoutinePtr = std::add_pointer<void(UINT uMsg, WPARAM wParam, LPARAM lParam)>::type;
	/**
	 * Set pointer to function that uses the ImGui immediate mode widgets.
	 * See RoutinePtr for a specification.
	 * Example:
	 * void FmGuiRoutine(void)
	 * {
	 * 	   ImGui::ShowDemoWindow();
	 * }
	 * Elsewhere perform a call to SetRoutinePtr(FmGuiRoutine);
	 */
	void SetRoutinePtr(RoutinePtr pRoutine);
	/**
	 * Set pointer to optional function that handles Win32 WndProc input.
	 * See InputRoutinePtr for a specification.
	 * Example:
	 * void FmGuiInputRoutine(UINT uMsg, WPARAM wParam, LPARAM lParam)
	 * {
	 * 	   // Toggle widgets on Alt + W keypress.
	 * 	   static bool bWidgetsEnabled = true;
	 * 	   if (uMsg == WM_KEYDOWN) {
	 * 	   	   if (wParam == 'W' && (GetAsyncKeyState(VK_MENU) & 0x8000)) {
	 * 	   	   	   bWidgetsEnabled = !bWidgetsEnabled;
	 * 	   	   	   FmGui::ChangeWidgetVisibility(bWidgetsEnabled);
	 * 	   	   }
	 * 	   }
	 * }
	 * Elsewhere perform a call to SetInputRoutinePtr(FmGuiInputRoutine);
	 * 
	 * Supplementary reading:
	 * https://docs.microsoft.com/en-us/windows/win32/inputdev/using-keyboard-input
	 * https://docs.microsoft.com/en-us/windows/win32/inputdev/keyboard-input
	 * https://docs.microsoft.com/en-us/windows/win32/learnwin32/keyboard-input
	 */
	void SetInputRoutinePtr(InputRoutinePtr pInputRoutine);
	/**
	 * Set all widget visibility and return previous value.
	 * 
	 * @param  bEnabled - .
	 * @retval - .
	 */
	bool SetWidgetVisibility(bool bEnabled);
	/**
     * Start FmGui and ImGui.
	 * 
	 * You can supply an optional configuration using an Config object.
	 * Example:
	 * Config config;
	 * config.style = Style::kDARK;
	 * if (!FmGui::StartupHook(config)) {
	 *     // FAILED!
	 * }
	 * 
     * @param  config - The FmGui::Config structure; default config if argument is not passed.
     * @retval        - True on success; false on failure.
	 */
	bool StartupHook(const Config &config = Config());
	/**
	 * Retrieve D3D context memory addresses. 
	 * @retval - Formatted string of the D3D context memory addresses.
	 */
	std::string AddressDump(void);
	/**
	 * Retrieve message of the last error generated by FmGui.
	 * @retval - The last message.
	 */
	const Message &GetLastError(void);
	/**
	 * Return a vector of error messages in order of first to last occurence.
	 */
	std::vector<Message> GetEveryMessage(void);
	/**
	 * Sets the MessageCallback to be used by FmGui.
	 * 
	 * @param pMessageCallback - The pointer to the message callback function.
	 */
	void SetMessageCallback(MessageCallback pMessageCallback);
	/**
	 * Return formatted string of the D3D debug layer warning/error messages.
	 *
	 * Note: DirectX 11 does not have a callback to do this in real time. Only a vaild call
	 * if the hook was started successfully.
	 * 
	 * @retval - An empty string to indicate and error.
	 */
	std::string DebugLayerMessageDump(void);
	/**
	 * Shutdown the FmGui and ImGui.
	 * @retval - True on success; false on failure.
	 */
	bool ShutdownHook(void);
} // End namespace (FmGui)

#endif /* !FMGUI_FMGUI_HPP_ */
