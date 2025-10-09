# Fooyin MSU-1 Plugin

**Fooyin MSU-1 Plugin** is a CMake project that builds the `msuinput` plugin for Fooyin.

---

## Requirements

* CMake ≥ 3.16
* Git
* Ninja
* A C++ compiler (tested with GCC 15.2.1)

---

## Dependencies

Ensure all Fooyin dependencies are installed.
See the [Fooyin build instructions](https://github.com/fooyin/fooyin/blob/master/BUILD.md) for details.

---

## Build Instructions

### 1. Clone the repository

```bash
git clone https://github.com/Vo1dTear/fooyin-msuinput.git
cd fooyin-msuinput
```

### 2. Build the plugin

```bash
mkdir -p build
cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

* This generates the shared library:

```
build/msuinput/fyplugin_msuinput.so
```

---

## Installing the Plugin (Optional)

You can install the plugin either system-wide or locally:

### a) System-wide installation

```bash
sudo cmake --install .
```

* Installs to:

```
/usr/lib/fooyin/plugins/fyplugin_msuinput.so
```

### b) Local user installation

```bash
mkdir -p ~/.local/lib/fooyin/plugins
cp build/msuinput/fyplugin_msuinput.so ~/.local/lib/fooyin/plugins/
```
