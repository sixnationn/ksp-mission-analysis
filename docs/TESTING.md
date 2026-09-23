# Test the current desktop build

This is an external KSP mission-analysis application. It is not a mod you
install into `GameData`. Ubuntu 24.04 is the primary test target; Windows
2022 or a comparable Windows system can use the MSYS2 UCRT64 build. You can
inspect the synthetic 3D scene without owning KSP. The supplied CI archives
are dynamic **development test bundles**, not standalone installers.

## Get a matching build

On the public repository's [Actions → Build and check](https://github.com/sixnationn/ksp-mission-analysis/actions/workflows/build.yml) page, choose a
successful run for `main` and download either `ksp-desktop-ubuntu-tester` or
`ksp-desktop-windows-tester` from its artifacts. Extract the ZIP. Keep
`ksp_desktop` and `ksp_worker` (or both `.exe` files) in the same directory;
the desktop looks for its worker there by default. `source-commit.txt` names
the exact source revision, and `runtime-dependencies.txt` records the CI
machine's dynamic library scan. Artifacts are retained for seven days.

The Ubuntu bundle needs GTK 4/gtkmm, libepoxy, OpenGL and the C++ runtime on
the test machine. The approved Ubuntu package list and build commands are in
[dependencies](https://github.com/sixnationn/ksp-mission-analysis/blob/main/docs/DEPENDENCIES.md) and the CI workflow. The Windows bundle must
run with the MSYS2 **UCRT64** runtime packages listed in that workflow; open
the UCRT64 shell, install those listed packages if absent, and navigate to the
extracted directory. The dependency record is evidence from the build runner,
not proof that another computer already has every library. If a binary fails
to launch, include its error and the dependency record in the report.

## First visual test, no game required

From the extracted directory, run the platform's executable in a terminal:

```text
./ksp_desktop
./ksp_desktop.exe   # Windows UCRT64 shell
```

This opens a **synthetic fixture** for checking the 3D scene and controls. At
normal display size, check that the source is labeled synthetic, orbit paths
and bodies are visible, selecting a body works, and orbit/pan/zoom responds.
The synthetic scene cannot establish a real JNSQ transfer, and mission search
should remain unavailable until an acceptable runtime source is imported.
If you see a blank or clipped window, capture a full-resolution screenshot
with the display size and scaling setting.

## Run the source checks

To run the full source checks, clone the repository at the commit in the
bundle. On Ubuntu, install the already documented build packages; on Windows,
run these commands in an MSYS2 UCRT64 shell with the workflow's packages:

```text
cmake -S . -B build/tester -DCMAKE_BUILD_TYPE=Release
cmake --build build/tester --config Release
ctest --test-dir build/tester -C Release --output-on-failure
```

The .NET importer checks use the separate .NET 10 SDK. All source tests use
synthetic or runtime-shaped data and do not validate a loaded KSP installation.

## Optional exact runtime snapshot

If you own KSP 1.12.5 and want to test the real import path, use a separate
disposable copy of the game. The required JNSQ, Reborn Real, Principia and
support-mod versions, plus known load gaps, are recorded in
[game runtime dependencies](https://github.com/sixnationn/ksp-mission-analysis/blob/main/docs/GAME-RUNTIME-DEPENDENCIES.md). The KSP exporter
has compiled but **has not yet been proven in a loaded game**; a failed
capture is useful test evidence. The exporter build has been checked on
Windows only. Build it there with .NET 10, the .NET Framework reference
assemblies, and a directory of assemblies from your own game copy containing
`Assembly-CSharp.dll`:

```text
dotnet build src/KspMission.RuntimeExporter/KspMission.RuntimeExporter.csproj -c Release -p:KspManagedDir=/path/to/KSP/Managed
```

Put the resulting `KspMission.RuntimeExporter.dll` under
`GameData/KspMission/Plugins/` in that disposable copy. In a loaded flight,
`Ctrl+Alt+F8` requests a JSON capture under the game's
`PluginData/KspMission/` directory. Keep the game log and mod version list
if it refuses or crashes. Do not place the exporter into your existing KSP
installation for this test.

For an exporter-created snapshot, calculate its SHA-256 and validate the
exact bytes before opening them. In an Ubuntu or MSYS2 UCRT64 shell:

```text
sha256sum /path/to/snapshot.json
./ksp_desktop --validate-snapshot --snapshot /path/to/snapshot.json --sha256 HEX_FROM_ABOVE
./ksp_desktop --snapshot /path/to/snapshot.json --sha256 HEX_FROM_ABOVE
```

Use `ksp_desktop.exe` for the Windows commands. A failed validation should
print `REJECTED` and exit nonzero; a successful validation prints `ACCEPTED`
with the source hash, confidence, body count, coverage and no-leap display
date. Use the hash of the exact file you pass, without editing it afterward.
An accepted JSON schema and an independent Newtonian preview still do not
prove Principia agreement. The raw JNSQ Reborn Real config importer only
produces a provisional catalog and cannot replace a loaded flight snapshot.

## What to send back

Include the operating system, display resolution/scaling, source commit,
launch command, whether the test was synthetic or used an actual runtime
snapshot, and any terminal error. For a visual issue, send one full-resolution
screenshot of the whole window. For an imported source, report whether search,
cancel, report save and read-only reopen behaved as expected. Do not send a
KSP save or proprietary game archive unless you explicitly choose to share it.
