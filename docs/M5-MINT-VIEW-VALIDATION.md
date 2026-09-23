# Linux Mint ultrawide tester checkpoint

The tester ran commit `2d38b6d` on Linux Mint 22.3 x86_64 at 3440×1440,
launched `./ksp_desktop`, and captured the synthetic scene. They reported
orbit shapes changing with window aspect ratio, difficulty dragging to an
overhead view, planar synthetic orbits, and uncertainty about whether bodies
outside the Mars and Venus mission roles participate in the model. The supplied
screenshot is review input, not a capture of the corrected build.

Commit `62cc408` applies aspect correction to the shared draw and marker-pick
projection, stops upward drag at the orbital normal, and adds a Top view
control. The synthetic fixture now has five bodies, including a non-target
world, with orbital inclinations of 0°, 9°, 18° and 27°. The mission panel
states the source body count and that route roles choose stops only. The
planetary integrator and spacecraft force calculation already iterate over
all bodies in the imported snapshot; the current requested route still has
one stay target and one flyby target.

Commit `5341c2a` makes the overhead drag clamp reversible: dragging down after
reaching the top view immediately tilts the camera back toward the orbital
plane. [CI run 35853537274](https://github.com/sixnationn/ksp-mission-analysis/actions/runs/35853537274)
passed all six Ubuntu and Windows jobs and uploaded the
`ksp-desktop-ubuntu-tester` artifact from that commit.

The new headless geometry check covers equal screen-pixel radii at ultrawide,
square and portrait viewports, drag bounds and an inclined tangential orbit.
The MSYS2 UCRT64 desktop built and the full local CTest suite passed 17/17;
the focused view test also passed after the drag correction.
This is source and mathematical evidence. The corrected GTK scene still needs
rendered inspection on the tester's Linux Mint display, including resize,
drag, Top view, marker selection and the extra inclined orbit.

The [tester guide](TESTING.md) now locates the actual Steam Proton KSP game
folder, keeps the exporter and JSON out of the Wine prefix, and uses the
captured JSON with the native Linux desktop. The exporter still requires the
specific JNSQ and JNSQ Reborn files and has not produced a validated loaded
Principia capture.

## Zoom follow-up from build 4872e45

The tester's newer Linux Mint screenshot shows outer orbit arcs ending within
the scene after zooming in. Source inspection found the depth coordinate was
divided by the same zoom factor as screen X and Y. At the closest allowed zoom,
an otherwise visible path sample could exceed OpenGL's [-1, 1] depth interval
and disappear. The focused geometry test reproduced this before the change.
Depth now uses the scene scale without zoom while screen X and Y still zoom;
the focused test and desktop build pass locally. Paths beyond the scene edges
remain normally cropped. The fixed GTK scene still needs a fresh Mint screenshot
and hands-on zoom check.

The same tester supplied feedback that the Ubuntu guide lacked exporter build
steps. The guide now includes repository clone, .NET SDK check, Linux build and
copy commands using the disposable Proton KSP copy's Managed directory. This
Linux command follows the project build contract and Microsoft's SDK
documentation; it has not yet been run on a Linux KSP installation.
