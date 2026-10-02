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

Run the following commands from the `build` directory used above. Choose either a system-wide or a local user installation.

### System-wide installation

Install the plugin using CMake:

```sh
sudo cmake --install .
```

For a fooyin installation using `/usr/lib/fooyin/plugins`, the installed file is:

```text
/usr/lib/fooyin/plugins/fyplugin_msuinput.so
```

The exact plugin directory can vary by fooyin installation. CMake uses the plugin installation path provided by fooyin.

### Local user installation

Copy the generated plugin to fooyin's user plugin directory:

```sh
mkdir -p ~/.local/lib/fooyin/plugins
cp msuinput/fyplugin_msuinput.so ~/.local/lib/fooyin/plugins/
```

Restart fooyin after installing or updating the plugin.
