# Changelog

<!-- https://keepachangelog.com -->

## Version 1.0.0 - 21 Jun 2022

### Added
- Initial release.

### Fixed

### Changed

### Known Issues

## Version 1.0.1 - 22 Jun 2022

### Added
- Add more error handling.
- Add basic configuration structure with defaults.
- Implement configuration structure as argument to `FmGui::StartupHook().`
- Add comments.
- Add implementation for OnResize. Not used yet.

### Fixed
- Fixed crashes due to derefrencing ImGuiContext on mission restart.

### Changed
- Updated instructions in *README.md*.
  - Inform users that static linking is prefered.
- Updated *Examples/Fm.cpp*.

### Known Issues
- Crashes on second mission quit when ImGui IniFileName is not null pointer.

## Version 1.0.2 - 23 Jun 2022

### Added

- Add to instructions in *README.md*.
  - Add setup subheadings to table of contents.
  - Add ImPlot extension instructions section.
  - Add ImPlot example code to *Examples/Fm.cpp*.
  - Create CMakeLists.txt (not in use).
- Add dependencies ImGui, ImPlot, & MinHook as Git submodules.

### Fixed

### Changed

- Update instructions in *README.md*.
  - Change the recommended include path for ImGui.
  - Move *Include/FmGui.hpp* to *Source/FmGui.hpp*.
  - Add image showing preprocessor config for ImPlot usage.
  - Change image format to JPEG.
- *FmGui.cpp*
  - Remove functions ** and **.
- Update *Examples/Fm.cpp* to reflect changes to library interface.
- Removed CMake project file and *Source/DllMain.cpp*
  - Will possibly add *.cmake* file for use with the `include` CMake command that declares FmGui as an interface library type.

### Known Issues

- Crashes on second mission quit when ImGui IniFileName is not null pointer.
- Message fetching facilities of FmGui not implemented.

### Additional Notes

