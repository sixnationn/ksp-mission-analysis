# Test the current desktop build

This is an external KSP mission-analysis app. It is **not** a mod to put in
`GameData`. Ubuntu 24.04 is the primary test target. These are development
test bundles, not standalone installers.

## Ubuntu: open the app

1. On [Actions → Build and check](https://github.com/sixnationn/ksp-mission-analysis/actions/workflows/build.yml), open a successful `main` run and download its **`ksp-desktop-ubuntu-tester`** artifact. Unzip it.
2. Open a terminal **inside the unzipped folder** and run:

   ```bash
   chmod +x ksp_desktop ksp_worker
   ./ksp_desktop
   ```

This opens a clearly labeled **synthetic** 3D scene. Check that bodies and
paths appear and that selection, orbit, pan and zoom work. Mission search
needs a real runtime snapshot. Keep `ksp_desktop` and `ksp_worker` together.
If the app does not start, see [Ubuntu runtime libraries](#ubuntu-runtime-libraries).

### Get a newer tester build

The window shows the commit from its bundled `source-commit.txt`. Press
**Check updates**. If this is the latest successful `main` build, the window
says so. Otherwise it opens that build's GitHub Actions page. Download its
`ksp-desktop-ubuntu-tester` artifact, close the old app, and unzip the new
bundle into a new folder. Keep the new `ksp_desktop` and `ksp_worker` together.
Windows testers choose `ksp-desktop-windows-tester` on the same page.

An already downloaded ZIP or folder never changes when `main` changes. This
button checks and opens a download page; it does not replace running files or
touch KSP. If the check cannot reach GitHub, it opens the
[successful main builds](https://github.com/sixnationn/ksp-mission-analysis/actions/workflows/build.yml?query=branch%3Amain+is%3Asuccess)
page instead. An older bundle without the button needs a fresh download once.

## Ubuntu: import a Principia flight

The desktop imports a **snapshot JSON file made during a loaded KSP flight**.
It cannot import a KSP save, a `GameData` folder or the provisional JNSQ
config catalog directly.
If you already have a `snapshot-...json` from this exporter, skip to step 3.

1. Use a **disposable copy** of your modded KSP 1.12.5 installation. The
   desktop ZIP does **not** contain the in-game exporter. Put a compiled
   `KspMission.RuntimeExporter.dll` in that copy at
   `GameData/KspMission/Plugins/`. If you do not have the DLL, see
   [building the exporter](#build-the-in-game-exporter-if-needed).
2. Start a flight with Principia loaded and press **Ctrl+Alt+F8**. If capture
   succeeds, KSP writes `snapshot-...json` in
   `<your KSP folder>/PluginData/KspMission/`. This is the file to import.
3. Close the synthetic desktop window. Back in the terminal **inside the
   unzipped desktop folder**, run the following. Change only the first line
   to the full path of your actual JSON file:

   ```bash
   snapshot="/absolute/path/to/KSP/PluginData/KspMission/snapshot-...json"
   hash="$(sha256sum "$snapshot" | cut -d ' ' -f1)"
   ./ksp_desktop --snapshot "$snapshot" --sha256 "$hash"
   ```

### If that KSP instance runs through Steam Proton

Find the **game folder Steam actually launches**. For the Steam installation,
use KSP's **Manage → Browse local files** command; for another Proton shortcut,
check that shortcut's target. You want the folder containing `KSP_x64.exe`,
`GameData/` and `KSP_x64_Data/`. If you have several KSP copies, use a
disposable copy launched by the same shortcut for this test.

Put the exporter DLL in **that copy's** `GameData/KspMission/Plugins/`, launch
it through your usual Proton shortcut and press the hotkey in a loaded flight.
The JSON appears in **that copy's** `PluginData/KspMission/`. Open the JSON
with the native Linux `ksp_desktop` using its normal Linux path in step 3.
Do not look for the exporter or JSON inside Proton's `compatdata/.../pfx`:
that is the Wine prefix, while this exporter writes under KSP's game folder.
[Valve's Proton FAQ](https://github.com/ValveSoftware/Proton/wiki/Proton-FAQ)
explains the separate prefix location.

This exporter currently requires the loaded JNSQ and JNSQ Reborn files at
`GameData/JNSQ/Version/JNSQ.version` and
`GameData/JNSQ-Reborn/JNSQReborn-Configuration.cfg`. A different modded
Principia system cannot yet use this runtime capture path; the synthetic
viewer still works without KSP. If capture fails, keep that instance's
`KSP.log` and the on-screen error.

The window should say **“Runtime JSON accepted”** and show the imported
source and bodies. The hash is calculated from the exact file, so do not edit
the JSON between the second and third commands. If the app says **“Import
rejected”**, run this in the same terminal to see the reason without opening
a window:

```bash
./ksp_desktop --validate-snapshot --snapshot "$snapshot" --sha256 "$hash"
```

If no JSON appears after the hotkey, keep the KSP log and mod version list.
The exporter has compiled but has **not yet been proven in a loaded game**;
a capture failure is useful test evidence. An accepted JSON is still labeled
`runtime_observed_uncompared`: the independent Newtonian preview has not
been checked against the installed Principia trajectory.

## Build the in-game exporter if needed

The exporter is a separate DLL; it is not in the desktop ZIP. You need the
approved **.NET 10 SDK** and the managed DLLs in your **disposable KSP 1.12.5
copy**. The output is a `net472` DLL loaded by KSP through Proton, not a Linux
program to launch with `dotnet`. If `dotnet --version` does not report 10.x,
follow [Microsoft's Ubuntu SDK instructions](https://learn.microsoft.com/en-us/dotnet/core/install/linux-ubuntu-install)
for your Mint/Ubuntu base before continuing.

On **Linux Mint or Ubuntu**, first get the source if you only have the desktop
ZIP:

```bash
git clone https://github.com/sixnationn/ksp-mission-analysis.git
cd ksp-mission-analysis
```

From the repository root, run the next block. Change only its first line to
the full path of the disposable KSP copy that
your Proton shortcut launches. It must contain `KSP_x64.exe` and
`KSP_x64_Data/Managed/Assembly-CSharp.dll`:

```bash
ksp="/absolute/path/to/your/disposable/KSP"
dotnet build src/KspMission.RuntimeExporter/KspMission.RuntimeExporter.csproj \
  -c Release "-p:KspManagedDir=$ksp/KSP_x64_Data/Managed"
mkdir -p "$ksp/GameData/KspMission/Plugins"
cp src/KspMission.RuntimeExporter/bin/Release/net472/KspMission.RuntimeExporter.dll \
  "$ksp/GameData/KspMission/Plugins/"
```

The first build may restore Microsoft's
[.NET Framework reference assemblies](https://learn.microsoft.com/dotnet/framework/migration-guide/reference-assemblies)
through the normal SDK restore. The DLLs under `KSP_x64_Data/Managed` are read
from your own KSP copy and are not added to this repository.

Start **that same copy** through Proton, load a flight and press Ctrl+Alt+F8.
Look for `snapshot-...json` under its `PluginData/KspMission/`, then use the
[import command above](#ubuntu-import-a-principia-flight). A successful
`dotnet build` only proves compilation; a loaded-game export and Principia
comparison still need tester evidence. The Linux exporter command has not
yet been run on this Windows development host. If it fails, send the full
terminal error and your `dotnet --version` output.

On **Windows**, use the same repository source and managed DLLs, with a Windows
path:

```powershell
dotnet build src/KspMission.RuntimeExporter/KspMission.RuntimeExporter.csproj -c Release "-p:KspManagedDir=C:\path\to\disposable\KSP\KSP_x64_Data\Managed"
```

Copy `src/KspMission.RuntimeExporter/bin/Release/net472/KspMission.RuntimeExporter.dll`
to that copy's `GameData/KspMission/Plugins/`. The [game runtime dependencies](https://github.com/sixnationn/ksp-mission-analysis/blob/main/docs/GAME-RUNTIME-DEPENDENCIES.md)
list the required JNSQ, Reborn Real, Principia and support-mod versions.

## Ubuntu runtime libraries

The Ubuntu desktop bundle needs GTK 4/gtkmm, libepoxy, OpenGL and the C++
runtime on the test computer. The approved Ubuntu package list is in
[dependencies](https://github.com/sixnationn/ksp-mission-analysis/blob/main/docs/DEPENDENCIES.md).
The artifact's `runtime-dependencies.txt` records libraries on the CI runner;
it does not install them on your computer. If launch fails, send the terminal
error and that file.

## Windows and source checks

For Windows, download **`ksp-desktop-windows-tester`** from the same successful
run. Use an MSYS2 **UCRT64** shell with the packages listed in the
[CI workflow](https://github.com/sixnationn/ksp-mission-analysis/blob/main/.github/workflows/build.yml).
Keep `ksp_desktop.exe` and `ksp_worker.exe` together; run
`./ksp_desktop.exe`. For a captured JSON, use the same `--snapshot` and
`--sha256` options with `ksp_desktop.exe`. Both artifacts are retained for
seven days. `source-commit.txt` names the source revision in each bundle.

To run the source checks, clone that revision. On Ubuntu install the documented
build packages; on Windows use the MSYS2 UCRT64 shell and workflow packages:

```text
cmake -S . -B build/tester -DCMAKE_BUILD_TYPE=Release
cmake --build build/tester --config Release
ctest --test-dir build/tester -C Release --output-on-failure
```

The .NET importer checks use the separate .NET 10 SDK. Source tests use
synthetic or runtime-shaped data; they do not validate a loaded KSP game.

## What to send back

Include your OS, display resolution/scaling, `source-commit.txt`, launch
command, and whether you used the synthetic scene or a captured JSON. If
capture or import fails, send the exact error, KSP and Principia versions,
mod list and relevant game log. For a visual issue, send one full-window
screenshot. For an imported source, report whether search, cancel, report
save and read-only reopen worked. Do not send a KSP save or game archive
unless you choose to share it.
