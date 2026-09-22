# M0 input resolution

Inspected on 22 September 2026. Archives were downloaded to the ignored `.work/inputs/` directory and read as ZIPs. No DLL from an archive was executed and no existing KSP installation was changed.

| Input | Exact archive size | SHA-256 | Finding |
|---|---:|---|---|
| Supplied `KSP.zip` | 2,779,239,024 bytes | `AAE62E3A0D3D621A8213888CCFC8801B11CAAF36F27EAF3C0E5C85450EF2686D` | `ksp/readme.txt` says KSP 1.12.5; `buildID64.txt` says build 03190, Steam. Only Squad and SquadExpansion are present under GameData. The archive has a Windows executable and no ModuleManager cache or save. |
| Supplied `principia lévy for 1.12.5.zip` | 184,863,361 bytes | `F703BF1AD2DBCB22371551571BA0EB61A092FEFAF97BC0D707063273D49F1DBD` | 48 entries, including Windows, Linux and macOS binaries. Adapter DLL file version is `2026.09.11.256`. Its bundled gravity and initial-state configs are for Real Solar System, not JNSQ. |
| JNSQ 0.10.2 release ZIP | 2,046,110,130 bytes | `7384D08FAB73F4952E37085090039340A31E3481BBB9EFC5E0AE5C737B5E5BE2` | 32 body configs and an optional, separate 10X rescale. `JNSQ.version` permits KSP 1.12.0 through 1.12.99. README requires Kopernicus and ModuleManager 4+. |
| JNSQ Reborn v1.0.1 release ZIP | 408,934,154 bytes | `B5FABD5114CCEE74874E08CF8A86D257FA4484E6791A0F325546730A743C972C` | 32 replacement body configs and 32 Real rescale body patches. The source config defaults to `SystemScale=Standard`; Real is the explicit selected setting for this project. |

The exact JNSQ and Reborn release body/config files were extracted into the ignored `.work/release-configs/` directory for repeatable local comparison. File-level hashes belong in importer output. This is release content, not a resolved in-game config cache.

## Selected system interpretation

Reborn Real applies its own per-body patches to Reborn's standard bodies. Kerbin's source radius of 1,600,000 m becomes 6,400,000 m; its 43,200 s rotation period becomes 86,400 s. Its source semi-major axis is also multiplied by four. JNSQ optional `Rescale_10X` is a different package and must not be stacked with Reborn Real. `geeASL` is present in source configs; a runtime gravitational parameter is not established by these ZIPs, so the provisional catalog must keep the distinction.

Conditional source values matter. JNSQ Minmus uses 146,970,000 m without Principia and 58,550,000 m with it before optional scale. Reborn Mun uses 90,960,000 m without Principia and 93,840,000 m with it before Real scaling. These are source branches, not verified loaded orbit states. Principia's fallback orbital elements can be Jacobi coordinates, and the supplied archive contains no JNSQ save state. Do not convert those elements as parent-relative Cartesian states.

The requested tool display uses a 365-day, no-leap year and explicit `Y0,D0` origin. Reborn's `RealTime=True` source patch declares January through December with February at 28 days and a 2001/1 calendar offset. Its Real rescale contains an offset-time patch. Neither the KSP UTC origin nor a loaded Kronometer value is established by raw files alone; keep the tool display origin, game UT zero and state epoch separate.

## Build targets and dependencies

Ubuntu/Linux is the primary product target and Windows is also required. The current host has .NET SDK 9.0.310 and 10.0.203. The M1 adapter uses the installed SDK and standard library, so **no new development dependency has been installed or is needed for M1**. WSL is not installed here, and the supplied KSP archive contains a Windows game executable; native Ubuntu execution remains a separate validation step.

Consolidated future development dependency candidates, all deferred beyond M1:

| Candidate | Reason before a future install |
|---|---|
| .NET SDK/runtime on Ubuntu | Build and run the cross-platform import/export utilities natively. |
| C++20 compiler, CMake and Ninja on Ubuntu and Windows | Compile and test the later numerical core and worker on both targets. |
| GTK 4/gtkmm 4 and OpenGL development packages on both targets | Build and inspect the later desktop UI and 3D view. |
| A numerical integration library, candidate to be selected after a license and packaging spike | Avoid an unvalidated custom propagation implementation in later milestones. |

For an isolated KSP runtime comparison, JNSQ's required Kopernicus and ModuleManager 4+ plus Reborn's listed dependencies must be audited separately as game mods. Reborn's README lists additional visual and terrain packages, including EVE Volumetrics V5. The external importer does not need visual assets to read source configs. Missing runtime packages, a resolved ModuleManager cache and a Principia save prevent a `runtime_verified` claim today.

## M0 result

The exact KSP version and release input hashes are known. The scale, force, frame and time boundaries are defined in `SNAPSHOT-CONTRACT.md`. The GTK/numerical-library spike is planned for Ubuntu first and Windows second; no such library was installed in M0. M1 can proceed with a provisional release-config catalog, while runtime equivalence remains a separate evidence gate.
