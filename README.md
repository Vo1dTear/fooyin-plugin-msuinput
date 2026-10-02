# Fooyin MSU-1 Plugin

**Fooyin MSU-1 Plugin** is a CMake project that builds the `msuinput` plugin for Fooyin.

## Requirements

* CMake 3.16 or newer
* Git
* A C++ compiler (tested with GCC 15.2.1)
* Qt and the fooyin development files, including `FooyinConfig.cmake`
* Ninja or Make

## Dependencies

The plugin depends on fooyin's Core and Gui libraries and their development dependencies.

## Build

Clone the repository and configure a release build:

```sh
git clone https://github.com/Vo1dTear/fooyin-plugin-msuinput.git
cd fooyin-plugin-msuinput
mkdir -p build
cd build
cmake -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    ..
cmake --build .
```

If Ninja is not installed, omit `-G Ninja` and use the default CMake generator.

The build generates the following file, relative to the repository root:

```text
build/msuinput/fyplugin_msuinput.so
```

## Installation

### Arch Linux (AUR)

Install [`fooyin-plugin-msuinput-git`](https://aur.archlinux.org/packages/fooyin-plugin-msuinput-git) using an AUR helper:

```sh
yay -S fooyin-plugin-msuinput-git
```

Alternatively, with `paru`:

```sh
paru -S fooyin-plugin-msuinput-git
```

### From source

Run the following commands from the `build` directory used above. Choose either a system-wide or a local user installation.

#### System-wide installation

Install the plugin using CMake:

```sh
sudo cmake --install .
```

For a fooyin installation using `/usr/lib/fooyin/plugins`, the installed file is:

```text
/usr/lib/fooyin/plugins/fyplugin_msuinput.so
```

The exact plugin directory can vary by fooyin installation. CMake uses the plugin installation path provided by fooyin.

#### Local user installation

Copy the generated plugin to fooyin's user plugin directory:

```sh
mkdir -p ~/.local/lib/fooyin/plugins
cp msuinput/fyplugin_msuinput.so ~/.local/lib/fooyin/plugins/
```

Restart fooyin after installing or updating the plugin.

## Tests

From the repository root, enable and run the decoder regression tests:

```sh
cmake -S . -B build -DMSU_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

The tests use generated audio data and simulated I/O failures; no music files are required.

## Loop count

* `0`: repeat indefinitely.
* `1`: play once.
* `2`–`16`: play the selected number of times. For example, `2` plays once and repeats once.

Repeats start at the file's loop point, skipping the introduction.
With **Enable Loop** disabled, the track plays once.
