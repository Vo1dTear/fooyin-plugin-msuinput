# Fooyin MSU-1 Plugin

**Fooyin MSU-1 Plugin** is a CMake project that automatically downloads **Fooyin** and integrates the `msuinput` plugin into the Fooyin source tree.

## Requirements

* CMake ≥ 3.16
* Git
* A C++ compiler (tested with GCC 15.2.1)

## Dependencies

Before building this project, make sure you have all dependencies required by Fooyin.
See the [Fooyin build instructions](https://github.com/fooyin/fooyin/blob/master/BUILD.md) for detailed steps on installing libraries and setting up your build environment.



## Build Instructions


### 1. Clone this repository

```bash
git clone -b full https://github.com/Vo1dTear/fooyin-plugin-msuinput.git
cd fooyin-plugin-msuinput
```


### 2. Preparation

Prepare the project (download Fooyin and inject msuinput into the source tree)

```bash
cmake -S . -B build -G Ninja
cmake --build build --target prepare_msu
```

#### _OPTIONAL_

Update Fooyin to the latest version and inject msuinput into the new source tree:

```bash
cmake --build build --target fooyin_update
cmake --build build --target prepare_msu
```


### 3. Build Fooyin normally

```bash
mkdir -p build/fooyin-build
cd build/fooyin-build
cmake -G Ninja ../fooyin-src -DCMAKE_BUILD_TYPE=Release
cmake --build . -- -j$(nproc)
```


### 4. Copy the plugin to the project root (manual)

```bash
cd ../..
cmake --build build --target copy_msu_lib
```

After this step, the compiled plugin `fyplugin_msuinput.so` will be available in the root of this repository.


## Installing the Plugin

There are two ways to install the MSU-1 plugin:

1. **Manual installation:**
   Copy the plugin to your local Fooyin plugins directory:

```bash
cp fyplugin_msuinput.so ~/.local/lib/fooyin/plugins/
```

2. **Install from Fooyin GUI:**
   Open Fooyin, then go to **Settings → Plugins → Install...**, and select `fyplugin_msuinput.so` from your project folder.
