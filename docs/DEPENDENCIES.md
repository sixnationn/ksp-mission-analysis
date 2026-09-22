# Dependencies proposed for M2-M6

Status: the user approved this consolidated list and CI on 22 September 2026. MSYS2 UCRT64 and the listed Windows packages are installed on the development host. The current Windows host also has .NET 10 and MSYS2 CMake/Ninja. The workflow now installs the approved GTK and OpenGL packages for native Ubuntu and Windows GTK build checks. CI results for those new jobs remain pending until the next push.

The full product retains the planned C++ numerical worker, GTK 4/gtkmm desktop interface, and the existing .NET importer. Ubuntu is the primary build target, with a Windows build as well. Package versions can differ between distributions; the code must be tested on both.

| Where | Proposed additions | Reason |
|---|---|---|
| Ubuntu 24.04 | `build-essential`, `cmake`, `ninja-build`, `pkg-config` | Build and run the C++ numerical worker and tests natively. |
| Ubuntu 24.04 | `libboost-dev`, `nlohmann-json3-dev`, `libnlopt-cxx-dev` | Adaptive ODE stepping, strict snapshot/report JSON, and constrained local optimization. These are implementation candidates; numerical acceptance tests still determine whether they are adequate. |
| Ubuntu 24.04 | `libgtkmm-4.0-dev`, `libepoxy-dev`, `libgl1-mesa-dev` | GTK desktop interface and its OpenGL trajectory view. |
| Ubuntu 24.04 | .NET SDK 10 from Microsoft's supported package source | Build and run the existing importer and a KSP exporter build utility. |
| Windows | MSYS2 UCRT64 with `mingw-w64-ucrt-x86_64-gcc`, `mingw-w64-ucrt-x86_64-cmake`, `mingw-w64-ucrt-x86_64-ninja`, `mingw-w64-ucrt-x86_64-pkgconf` | Build the same C++ worker and GTK application against a consistent Windows toolchain. Existing MSVC remains available for the dependency-free M2 core. |
| Windows | MSYS2 UCRT64 `mingw-w64-ucrt-x86_64-boost`, `mingw-w64-ucrt-x86_64-nlohmann-json`, `mingw-w64-ucrt-x86_64-nlopt`, `mingw-w64-ucrt-x86_64-gtkmm-4.0`, `mingw-w64-ucrt-x86_64-libepoxy` | Match the numerical, data and UI libraries on Windows. |
| GitHub | An Ubuntu and Windows GitHub Actions build/check workflow | Native Linux and Windows verification on each push, because this Windows host has no WSL installation. This adds project CI infrastructure, not a local dependency. |

The KSP/Principia runtime comparison will use a **separate disposable KSP copy**. Kopernicus, ModuleManager, JNSQ, Reborn, Principia and their required game-mod dependencies will be pinned in a separate game-mod manifest after auditing the supplied versions. No existing KSP installation will be changed.

Sources: [Ubuntu gtkmm package](https://packages.ubuntu.com/noble/libgtkmm-4.0-dev), [Ubuntu NLopt C++ package](https://packages.ubuntu.com/noble/libnlopt-cxx-dev), [MSYS2 gtkmm](https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-gtkmm-4.0), [MSYS2 Boost](https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-boost), [MSYS2 NLopt](https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-nlopt), [MSYS2 JSON](https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-nlohmann-json), [NLopt algorithm documentation](https://nlopt.readthedocs.io/en/latest/NLopt_Algorithms/).
