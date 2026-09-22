# Initial GTK window checks (written before implementation)

1. Build with the approved MSYS2 UCRT64 gtkmm4 and libepoxy packages, without adding packages. The desktop target must remain optional when those packages are absent so the numerical CMake build still configures.
2. Launch a real window at normal desktop size. The left pane shows the synthetic source and four named bodies with SI radius and gravitational parameter; the header shows snapshot identity, confidence, right-handed inertial frame, Newtonian force model, and UT coverage. No label may imply installed KSP or Principia verification.
3. The center uses GtkGLArea with OpenGL to draw body markers and orbit path samples queried from a synthetic M2 ephemeris. A failed GL context or ephemeris integration must show an explicit error state.
4. Drag rotates, secondary drag pans, wheel zooms, and clicking a body marker or body-list item updates the selected-body readout. These camera and selection actions do not mutate the snapshot or ephemeris.
5. The right pane shows home, Mars-role, Venus-role, central body, launch window, flight time, fixed parking stay of 5,184,000 SI seconds, parking/capture altitude, flyby clearance, and return condition. The bottom pane says there are no candidates and no worker progress. Search and export controls remain insensitive.
6. Record the exact build and launch result. A successful build alone does not establish rendered acceptance; if the Windows GL environment cannot present the window, report the observed error.

## Runtime import slice: failure cases before implementation

7. A runtime-shaped schema 1 fixture with the exact-byte SHA-256 and Principia capture provenance replaces the synthetic body list and marker scene. The source panel shows the imported hash, `runtime_observed_uncompared`, exporter/game/save identity, frame, capture UT, state epoch and SI units. Search/export remain disabled.
8. A path with malformed JSON, a wrong supplied SHA-256, provisional or fabricated confidence, missing Principia provenance, or an unsupported frame produces a visible import error. The current scene stays intact and is explicitly labeled as the retained previous source; the rejected file never becomes analysis-ready.
9. An unreadable path or a valid snapshot whose ephemeris cannot be prepared likewise leaves the previous scene intact and shows the reason. An import is committed only after exact-byte hash check, schema/provenance validation and scene preparation all succeed.
10. Launching with `--snapshot <path> --sha256 <64-hex>` attempts the same import as the in-window path and SHA-256 fields. A source path without its expected digest is rejected visibly. The file is read in binary mode so line endings and whitespace affect its hash.
11. Runtime body markers are drawn from the imported states at capture UT. Any orbit samples are an independent Newtonian preview over an explicitly displayed bounded interval, never an installed-KSP or Principia prediction. Failed rendering shows a visible error.
12. `--validate-snapshot --snapshot <path> --sha256 <digest>` runs the exact import and preview preparation without opening GTK. It exits zero with the source identity on success and nonzero with `REJECTED` for malformed JSON, bad hash, invalid provenance or an unusable ephemeris.
