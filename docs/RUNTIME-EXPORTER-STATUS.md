# Optional KSP runtime exporter status

## Capture shortcut acceptance

- On Linux Mint, `Ctrl+Alt+F8` switches the virtual terminal and prevents the game from receiving the capture command.
- A loaded flight must instead request one snapshot with `Ctrl+Shift+F8`, without requiring Alt.
- Pressing the previous shortcut must not be required for capture. The snapshot path, provenance, and validation rules must remain unchanged.
- The new shortcut still needs a loaded-game check under Proton; a successful compile alone does not prove that KSP receives the key event.

`KspMission.RuntimeExporter.dll` compiles as a .NET Framework 4.7.2 KSP addon against the managed assemblies extracted from the supplied KSP 1.12.5 archive into ignored `.work/reference-assemblies/KSP_x64_Data/`. The compile did not execute archived game or Principia DLLs. The plugin has **not** been loaded into a game and has not produced a real snapshot.

In a disposable KSP copy with the required mods and a loaded flight save, `Ctrl+Shift+F8` requests a snapshot. It refuses if Principia is absent, not running or more than 1 ms out of sync with game UT. It reads Principia's parent-relative celestial states through the installed adapter, sums the hierarchy in the instantaneous right-handed AliceSun basis, subtracts the gravitational-parameter-weighted centre of mass, and writes one UTF-8 JSON file under `PluginData/KspMission/`. It records the game version, save folder, exact capture UT, and SHA-256 hashes for the loaded Principia adapter, JNSQ version file and Reborn configuration. The output is labeled `runtime_observed_uncompared`, and the basis is frozen at the capture instant. This is an initial state for an independent model; it does not reproduce Principia's integration settings or claim its future trajectory.

The separate `KspMission.Snapshot.Tool` checks the output schema and reports the exact-file SHA-256. Its reader tests currently pass on synthetic fixtures. A parsed file still cannot prove that an actual game capture happened; that requires an isolated KSP run with file and version evidence. The installed-version API access currently uses reflection on Principia's internal adapter, so runtime compatibility remains unverified until that run.

Remaining work before using an export for mission predictions: controlled install and launch of all pinned mods in an isolated KSP copy, compare exported states and frame transform against the loaded body table, record resolved ModuleManager conditions and calendar, pass the canonical snapshot to M2, and compare propagated states at several UTs with the installed Principia version. The supplied Principia release may be affected by the [Levy replacement advisory](https://github.com/mockingbirdnest/Principia); the in-game full version string must be checked before calling the runtime state verified.
