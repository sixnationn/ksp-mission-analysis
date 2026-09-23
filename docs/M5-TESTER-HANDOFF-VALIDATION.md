# M5 tester handoff validation

23 September 2026. The repository is public at
[sixnationn/ksp-mission-analysis](https://github.com/sixnationn/ksp-mission-analysis).
The packaging failure cases were specified first in
`M5-TESTER-HANDOFF-CONTRACT.md` at commit `0981059`. This slice adds a
tester guide and stages Ubuntu 24.04 and Windows MSYS2 UCRT64 **development
test bundles** after each desktop job's full build and CTest. Each archive
contains sibling desktop and worker executables, the guide, `source-commit.txt`
and `runtime-dependencies.txt`. It includes no game or mod files.

Local Windows staging produced five nonempty files. Deliberately missing
desktop and guide inputs, and a simulated `libmissing.dll => not found`,
caused the stage command to fail. `bash -n` and `git diff --check` passed. A
downloaded Windows bundle launched in headless validation mode and rejected a
missing snapshot with exit code 1, without opening GTK.

The first [CI run 35843368955](https://github.com/sixnationn/ksp-mission-analysis/actions/runs/35843368955)
passed Ubuntu staging and both desktop 16/16 CTest suites, but Windows
staging failed because the MSYS2 CI shell had no `git` command. Commit
`de9c910` uses the supplied `GITHUB_SHA` in CI and retains Git fallback for
local staging. The follow-up [CI run 35843905191](https://github.com/sixnationn/ksp-mission-analysis/actions/runs/35843905191)
passed all six Ubuntu and Windows jobs. Both desktop jobs ran 16/16 CTest
targets before staging. Downloaded archives each contained the expected five
files and the exact `de9c910bed06ef344b955ba9f92b30240ba2f257` source
commit. Both dependency records had zero unresolved-library lines in their
respective CI environments. The Ubuntu artifact is about 1.01 MB and the
Windows artifact about 1.07 MB as uploaded by GitHub Actions. The guide
instructs Ubuntu testers to restore executable permission after ZIP
extraction if needed.

These checks prove archive contents and CI dependency resolution, not a
normal-size rendered window or successful KSP/Principia flight capture. The
bundles require the documented GTK or MSYS2 runtime packages on the tester's
machine. The synthetic scene is visualization-only; raw JNSQ Reborn Real
configuration remains provisional. A tester with a disposable loaded game
can try the exporter and report runtime failures. First-release numerical
and installed-game acceptance remain open.

Automatic approval review rejected recursive removal of three ignored local
staging directories under `artifacts/m5/` without a more specific reason; the
cleanup did not execute. No existing game installation was changed.
