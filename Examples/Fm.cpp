
// Some code above...
// Include imgui.h
#include <imgui.h>

// Include implot.h if it is enabled in FmGui.hpp:
#if defined FMGUI_ENABLE_IMPLOT
#include <implot.h>
#endif
// Include FmGui.hpp
#include "FmGui.hpp"

// Some code...

// The user's ImGuiRoutine function:
static void FmGuiRoutine(void)
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
	/*
	 * Use ImPlot extension if it is enabled.
	 */
#if defined FMGUI_ENABLE_IMPLOT
	ImPlot::ShowDemoWindow();
#endif
}

// The user's ImGuiInputRoutine function (optional):
static void FmGuiInputRoutine(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	/*
	 * Handle input. See the links below for Win32 input handling.
	 * https://docs.microsoft.com/en-us/windows/win32/inputdev/keyboard-input
	 * https://docs.microsoft.com/en-us/windows/win32/learnwin32/keyboard-input
	 * https://docs.microsoft.com/en-us/windows/win32/inputdev/using-keyboard-input
	 */
	if (uMsg == WM_KEYDOWN)
	{
		if (wParam == 'W')
		{
			std::printf("W key pressed!\n");
		}
	}
}

// Some code...
/*
 * NOTE: This function may not be exported from your DLL if you are using the ED
 * EFM API template that is bundled with the game's install. You will need to add
 * an exported declaration to the extern "C" block of the ED_FM_TEMPLATE_API.h header
 * (or your project's equivalent):
 * 
 * // The ED_FM_TEMPLATE_API preproccessor definition may have a different name
 * // It is equivalent to: __declspec(dllexport)
 * ED_FM_TEMPLATE_API void ed_fm_set_plugin_data_install_path(const char *path);
 * // For ed_fm_release as well:
 * ED_FM_TEMPLATE_API void ed_fm_release(void);
 */
void ed_fm_set_plugin_data_install_path(const char *path)
{
	/*
	 * You may wish to use the preprocessor to only enable FmGui when your project
	 * is in DEBUG configuration. This can be done with the #ifdef _DEBUG and
	 * a closing #endif in most projects using MSVC. For a more standard abiding
	 * solution you may use #ifndef NDEBUG and a closing #endif.
	 */
// #ifdef _DEBUG
	/*
	 * Optional configuration. For more information and default values see the
	 * FmGuiConfig struct in FmGui.hpp.
	 */
	FmGui::Config config;
	/*
	 * This following line is known to cause crashes on the second mission's
	 * quit:
	 * fmGuiConfig.imGuiIniFileName = std::string(path) + "/bin/imgui.ini";
	 */
	config.style = FmGui::Style::kCLASSIC; // FmGuiStyle::kDARK

	// Start the FmGui and associated hook.
	// You can ommit the config arugment entirely: FmGui::StartupHook() is valid.
	if (!FmGui::StartupHook(config))
	{
		std::fprintf(stderr, "FmGui::StartupHook failed!\n");
	}
	else
	{
		// Print the addresses of the D3D11 context:
		std::printf("%s\n", FmGui::AddressDump().c_str());
		// Set the pointers to your ImGui and ImGui Input routine functions:
		FmGui::SetRoutinePtr(FmGuiRoutine);
		FmGui::SetInputRoutinePtr(FmGuiInputRoutine);
		// Set the widget visibility to ON:
		FmGui::SetWidgetVisibility(true);
	}
	/*
	 * The closing #endif for use if you want FmGui to only be enabled for DEBUG
	 * builds.
	 */
// #endif
}

// Some code...

void ed_fm_release(void)
{
	// Finally close the FmGui and associated hook.
	if (!FmGui::ShutdownHook())
	{
		std::fprintf(stderr, "FmGui::ShutdownHook failed...\n");
	}
}

// Some code...

/*
 * The following is some additional ideas for using FmGui in your flight model/systems code:
 * - If you use classes to abstract your code you can make an interface class
 *   that your flight model & systems classes can implement as seen below:
 *
 *   class IFmGuiable
 *   {
 *   public:
 *       virtual ~IFmGuiable(void) = default;
 *       virtual constexpr const char *VFmGuiWidgetName(void) = 0;
 *       virtual void VFmGuiRoutine(void) = 0;
 *   };
 *   
 *   // Here is an example systems class that implements this interface.
 *   class FuelSystem : public IFmGuiable
 *   {
 *   public:
 *       FuelSystem(void) = default;
 *
 *       constexpr const char *VFmGuiWidgetName(void) override
 *       {
 *           return "Fuel System";
 *       }
 *
 *       void VFmGuiRoutine(void) override
 *       {
 *           ImGui::Begin("FuelSystem ImGui Window");
 *           ImGui::Text("Total Volume = %.2f", this->totalVolume);
 *           ImGui::Text("Total Capacity = %.2f", this->totalCapacity);
 *           ImGui::End();
 *       }
 *   private:
 *       double totalVolume = 1000.0;
 *       double totalCapacity = 1000.0;
 *   };
 *
 *   // Now in your ImGuiRoutine you can perform a call to this virtual function
 *   // your systems class:
 *
 *   std::unique_ptr<FuelSystem> pFuelSystem = std::make_unique<FuelSystem>();
 *
 *   static void FmGuiRoutine(void)
 *   {
 *       // Call the Gui routine for this particular object.
 *       if (ImGui::CollapsingHeader(pFuelSystem->VFmGuiWidgetName())
 *       {
 *            pFuelSystem->VFmGuiRoutine();
 *       }
 *   }
 */
