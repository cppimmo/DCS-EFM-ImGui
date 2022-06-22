
// Some code above...
// Include FmGui.hpp
#include "FmGui.hpp"
// Include imgui.h
#include <imgui/imgui.h>

// Some code...

// The user's ImGuiRoutine function:
void
FmGuiRoutine(void)
{
	/*
	 * Set up your ImGui widgets here. Refer to the ImGui documentation and
	 * examples for creating widgets.
	 */
	ImGui::ShowDemoWindow();
	// ImGui::ShowAboutWindow();
	// ImGui::ShowUserGuide();
	ImGui::Begin("Hello, world!");
	ImGui::Text("Here, have some text.");
	ImGui::End();
}

// The user's ImGuiInputRoutine function:
void
FmGuiInputRoutine(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	/*
	 * Handle input. See the links below for Win32 input handling.
	 * https://docs.microsoft.com/en-us/windows/win32/inputdev/keyboard-input
	 * https://docs.microsoft.com/en-us/windows/win32/learnwin32/keyboard-input
	 * https://docs.microsoft.com/en-us/windows/win32/inputdev/using-keyboard-input
	 */
	if (uMsg == WM_KEYDOWN) {
		if (wParam == 'W') {
			std::printf("W key pressed!\n");
		}
	}
}


// Some code...

void
ed_fm_set_plugin_data_install_path(const char *path)
{
	/*
	 * Optional configuration. For more information and default values see the
	 * FmGuiConfig struct in FmGui.hpp.
	 */
	FmGuiConfig fmGuiConfig;
	/*
	 * This following line is known to cause crashes on the second mission's
	 * quit:
	 * fmGuiConfig.imGuiIniFileName = std::string(path) + "/bin/imgui.ini";
	 */
	fmGuiConfig.imGuiStyle = FmGuiStyle::CLASSIC; // FmGuiStyle::DARK

	// Start the FmGui and associated hook.
	// You can ommit the fmGuiConfig arugment entirely: FmGui::StartupHook() is valid.
	if (!FmGui::StartupHook(fmGuiConfig)) {
		std::fprintf(stderr, "FmGui::StartupHook failed!\n");
	}
	else {
		// Print the addresses of the D3D11 context.
		std::printf("%s\n", FmGui::AddressDump().c_str());
		// Set the pointers to your ImGui and ImGui Input routines.
		FmGui::SetImGuiRoutinePtr(FmGuiRoutine);
		FmGui::SetImGuiInputRoutinePtr(FmGuiInputRoutine);
		// Set the widget visibility to ON.
		FmGui::SetWidgetVisibility(true);
	}
}

// Some code...

void
ed_fm_release(void)
{
	// Finally close the FmGui and associated hook.
	if (!FmGui::ShutdownHook()) {
		std::fprintf(stderr, "FmGui::ShutdownHook failed...\n");
	}
}

// Some code...
