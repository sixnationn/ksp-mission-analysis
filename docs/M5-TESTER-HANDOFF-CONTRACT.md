# M5 tester handoff contract

This bounded slice makes the already built desktop and worker available to an
outside tester on Ubuntu 24.04 and Windows 2022/MSYS2 UCRT64. It is a
**development test bundle**, with declared system runtime dependencies, not a
standalone installer or an accepted first release. The GitHub repository is
public, but game archives and installed mod files remain outside it.

Each CI bundle must contain `ksp_desktop` and its sibling `ksp_worker` (with
`.exe` on Windows), a short tester guide, a source commit identifier, and a
runtime dependency record. The desktop's default worker lookup must resolve
within the bundle. The guide must give a fresh tester a path to run the bundled
synthetic scene and the existing headless checks on that operating system.
It must say that the synthetic scene is only a visualization fixture, that
raw JNSQ Reborn Real config is provisional, and that a real mission requires
an exact loaded runtime snapshot and an installed KSP/Principia comparison.
No game files, local runtime captures, user directories or secrets go into a
bundle. Ubuntu is the primary tester target; Windows uses the approved MSYS2
UCRT64 runtime packages.

Observable failure cases before packaging work:

- CI fails if either executable or the guide/source identifier is missing.
- CI fails if the dynamic dependency scan reports an unresolved library in
  its build environment; the produced record makes required runtime libraries
  visible to a tester. A passing scan does not prove another machine has them.
- The guide must not call the synthetic scene or raw config a verified loaded
  JNSQ/Principia system, or call the dynamic artifacts standalone installers.
- The sample launch must preserve sibling worker lookup, and the guide must
  show how to supply both a runtime JSON file and exact SHA-256 when testing
  an imported source.
- CI must still run the full existing tests on Ubuntu and Windows before it
  uploads these bundles. No GTK or KSP window is launched for this slice.

Acceptance is CI artifact inspection plus both platform jobs passing. A
tester must still perform normal-size rendered and interactive review on
their own desktop. Packaging does not establish UI responsiveness, numerical
agreement with Principia, or a complete mission solution.
