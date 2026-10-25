<h1 align="center">TuxTimings</h1>

![TuxTimings](screenshot.png "TuxTimings")

### Supported CPUs

Zen 5 Granite Ridge (Ryzen 9000-series AM5) is the primary tested target for PM-table based telemetry. Other AMD Zen families (Vermeer, Cezanne, Matisse, Renoir, Raven Ridge) have PM table mappings and may work with varying completeness.

If your CPU family is not listed or shows incomplete readings, you can help by running the **debug dump** from the app (PM table + AOD dump) and opening a GitHub issue with the dump attached so I can add proper PM table support.

### Documentation


### Prerequisites

- **[ryzen_smu](https://github.com/amkillam/ryzen_smu/)** kernel module — required for all readings. Install it separately. TuxTimings can request loading it when starting; it leaves shared modules loaded on exit.
- **GTK4 runtime** — needed to run the application  
  - Arch: `gtk4`  
  - Debian/Ubuntu: `libgtk-4-1`

> If building manually: also install `gcc`, `make`, `pkg-config`, GTK4 development headers, and GMP headers (Pi benchmark).  
> - Arch: `gtk4` + `gmp`  
> - Debian/Ubuntu: `libgtk-4-dev` + `libgmp-dev`

> **Module lifecycle:** The application can request `ryzen_smu`, `aod_voltages`, sensor drivers and optional `tuxbench` at startup. Shared SMU/AOD/JC42/SPD5118 drivers remain loaded on exit. Only a `tuxbench` module loaded by this application is eligible for removal. Installation scripts manage DKMS separately; compiling or staging sources does not load a module.

### Installing

#### Arch Linux / CachyOS (recommended)

Install `ryzen_smu` first, then build and install TuxTimings:

```bash
yay -S ryzen_smu-dkms-git
makepkg -si
```

This uses the included PKGBUILD to build and install via pacman. The `aod-voltages` kernel module (for memory voltage readings) is bundled and built automatically via DKMS. Uninstall with `sudo pacman -R tuxtimings`.

#### Ubuntu / Debian

```bash
sudo apt update

# Runtime deps (needed to run/install the .deb)
sudo apt install -y libgtk-4-1 libgmp10 policykit-1 dmidecode kmod

# Build deps (only needed if building from source / creating the .deb)
sudo apt install -y build-essential pkg-config libgtk-4-dev libgmp-dev

./install.sh --deb
sudo apt install ./tuxtimings_*.deb
```

#### Any Linux (direct install)

```bash
./install.sh
```

Builds from source and installs to `/opt/TuxTimings/` with a launcher at `/usr/bin/tuxtimings`, polkit policy, desktop file, and icon. Root privileges are requested automatically via sudo.

#### Uninstall (direct install)

```bash
./install.sh --uninstall
```

### Updating

#### From a git clone (recommended)

Fast-forward the current branch from its configured upstream, then rebuild/reinstall:

```bash
./update.sh
./install.sh          # or: ./install.sh --deb
```

Notes:
- `./update.sh` keeps the current branch and only accepts a fast-forward from its configured upstream.
- Tracked edits, staged edits, untracked files (including ignored build outputs), divergence, detached HEAD and a missing upstream stop the update. Save your work explicitly before retrying.
- No reset, branch switch, stash or cleanup is performed. `--keep-untracked` remains accepted for compatibility; preservation is always enabled.
- Fetching may update remote-tracking references before a divergent history is refused. Existing worktree files and local commits are preserved.

#### Stable builds (tags / releases)

For a reproducible/stable build, download the source files from the latest stable release.

### Building without installing

```bash
make -C Linux
./Linux/tuxtimings --version
./Linux/tuxtimings
```

The installer builds as the invoking user even when started through `sudo` (using `runuser` with separate arguments); root without a real invoking user must build unprivileged first. Generated build headers and manifests are readable for packaging.

The local executable elevates itself using its own resolved path. Running a local build never silently substitutes `/opt/TuxTimings/bin/tuxtimings`. The installed launcher delegates to that installed binary; `--version` requires neither elevation nor hardware access. Arguments and session environment values are forwarded as individual arguments, including spaces.

`VERSION` supplies the release base. Builds embed the Git revision, clean/dirty state and compilation profile; an archive without `.git` is explicitly marked `archive` with an unknown revision. The same identity appears in the UI, `--version`, diagnostic reports and packaged `usr/share/doc/tuxtimings/build-info.txt`. Debian package versions derive from this build metadata; Arch retains its release package version and carries the exact build identity in the manifest. A dirty marker is not a cryptographic identifier of every local edit.

The default `PROFILE=portable` compiles for baseline x86-64 with generic tuning. CLFLUSHOPT is confined to a runtime-selected function with a CLFLUSH fallback, and AVX-512 benchmark kernels retain runtime detection. An explicit `make -C Linux PROFILE=native` enables host-specific optimization for local use. Build-profile changes trigger recompilation. Software compilation and dispatch fixtures do not establish benchmark performance or hardware support across every AMD system; the optional tuxbench kernel module has a separate build and instruction profile.

To inspect DKMS source contents without installing or loading anything:

```bash
./install.sh --stage-dkms /tmp/tuxtimings-dkms-sources
python3 Linux/tests/test_delivery.py
```

The staging recipe is shared by the direct installer and `PKGBUILD`, and includes local module headers such as `aod_aml.h`. Delivery tests use temporary trees, a local Git remote and fake `pkexec`; the Debian fixture builds and extracts a package without installing it. Packaging refuses to overwrite an existing package of the same name.

### Software validation

`make -B -C Linux/tests check` runs the synthetic suites, including delivery
fixtures in temporary directories. `make -C Linux/tests check-ui` additionally
requires a working GTK display; it substitutes the backend and benchmarks.
Set `UI_CAPTURE_DIR=/chosen/path` to retain PNGs and a build/scenario manifest
in a unique subdirectory; temporary captures are removed by default.
These tests do not certify
hardware readings or authorize module loading and firmware calls.

### License

This project is licensed under the **GNU General Public License v3.0**. See [LICENSE](LICENSE) for the full text.

### References and projects used

- **[ryzen_smu](https://github.com/amkillam/ryzen_smu/)** — Kernel module for reading AMD SMN and PM table; build and load separately at runtime.
- **[ZenStates-Core](https://github.com/irusanov/ZenStates-Core)** — PM table offsets and timing formulas (reimplemented in our Linux backend; used with permission).
- **[ZenTimings](https://github.com/nickspacewalker/ZenTimings)** — Windows version; TuxTimings is based on this concept.
- **[GTK4](https://gtk.org/)** — Native Linux UI toolkit.
- **[Linux kernel](https://github.com/torvalds/linux)** — SMN/sysfs interface and platform support.
- **[AMD's public documentation](https://www.amd.com/en/support/tech-docs)** — SMN/PM table and DRAM timing references.
- **Tux icon** — Tux the penguin originally by Larry Ewing, created with GIMP (`lewing@isc.tamu.edu`), used and/or modified under the terms of the original image permission.

Huge credit goes to ZenTimings and irusanov without his project this wouldn't be possible.

### Disclaimer

This project's code was made with the assistance of [Claude](https://claude.ai/) (Anthropic). While it has been reviewed and tested, use it at your own risk.
