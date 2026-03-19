# Third-Party Notices

This project contains code that is derived from the upstream `openmultiplayer/launcher` project:

- Upstream project: `openmultiplayer/launcher`
- Upstream repository: <https://github.com/openmultiplayer/launcher>
- Upstream license for the covered source files: Mozilla Public License 2.0
- Official MPL-2.0 text: <https://mozilla.org/MPL/2.0/>

## MPL-2.0-Covered Files In This Repository

The following files in this repository contain code that is derived from, or directly informed by, the upstream project and are treated as MPL-2.0-covered source files:

- `inject_helper/src/main.rs`
  - Derived from the upstream Windows launch/injection path.
  - Main upstream references:
    - `src-tauri/src/injector.rs`
    - `src-tauri/src/main.rs`
    - `src-tauri/src/constants.rs`

- `omp-launcher-classic/main.cpp`
  - Contains derivative integration logic for the open.mp server list source, partner-tab behavior, client DLL lookup, and the Windows launcher/helper invocation flow.
  - Main upstream references:
    - `src/utils/game.ts`
    - `src-tauri/src/main.rs`
    - `src-tauri/src/constants.rs`

## Project-Specific Files

Files not listed above are project-specific unless they contain their own notice saying otherwise.

## Distribution Note

If you distribute executable builds of this project, you should also make the corresponding source form of the MPL-covered files above available under MPL-2.0 and keep the notices in those files intact.

This notice is intended to make the covered files and their upstream origin explicit in the repository itself.
