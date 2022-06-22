# Changelog

<!-- https://keepachangelog.com -->

## Version 1.0.0 - 21 Mar 2022

### Added
- Initial release.

### Fixed

### Changed

### Known Issues

## Version 1.0.1 - 22 Mar 2022

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

## Version 1.0.2 - 

### Added

### Fixed

### Changed
- Update instructions in *README.md*.

### Known Issues
