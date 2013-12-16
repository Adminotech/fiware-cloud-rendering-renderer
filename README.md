FIWARE Cloud Rendering
======================

# Dependency build instructions

You need to build the WebRTC native library manually before you can compile this plugin.

## Windows

Instructions can be found from http://www.webrtc.org/reference/getting-started but some parts are a bit obscure so here is the rundown.

**Note:** These instructions are for Visual Studio, not for cygwin.

### Prerequisites 

* Visual Studio 2008 or newer, with all service packs installed. If you have an Express edition see http://www.webrtc.org/reference/getting-started/prerequisite-sw for more instructions.
* Latest Microsoft Windows SDK
* Latest Microsoft DirectX SDK
* Python 2.7.x and add it to your PATH

### Google depot tools

Depot tools is used in various Google projects, so if you already have it installed skip this step.

1. Download Google depot tools https://src.chromium.org/svn/trunk/tools/depot_tools.zip
2. Extract to `<depot_path>` (freely choose your own location)
3. Add `<depot_path>` as the **LAST** item in your `PATH`
4. Open Windows cmd prompt and `cd <depot_path>`
5. Run command `gclient`. If this does not go back to 3.

### Cloning WebRTC

1. Open Windows cmd prompt and create a working directory `<webrtc_path>` (freely choose your own location)
2. Run command `cd <webrtc_path>`
3. Run command `gclient config http://webrtc.googlecode.com/svn/trunk`
4. Add new environment variable `GYP_MSVS_VERSION` and set the value as your Visual Studio version, eg. `2008`. Close all cmd prompts before you set it.
5. Add net environment variable `WEBRTC_ROOT` and set te value to `<webrtc_path>`. This is used by CloudRenderingPlugin CMake run to locate headers and libraries.
6. Repeat step 2. and run command `gclient sync --force`

You should now have `<webrtc_path>\trunk\all.sln` Visual Studio solution file. It is however misconfigured for Tundra usage, we need to use the DLL version of the runtime. Here is how to configure it. This step needs to be done any time you want to regenerate the project files.

1. Open Windows cmd prompt and `cd <webrtc_path>\trunk`
2. Run command `python build/gyp_chromium -D"component=shared_library" --depth=. all.gyp`

Now you will have `<webrtc_path>\trunk\all.sln` that uses the DLL runtime. You can proceed to build WebRTC.

### Building WebRTC

Open `trunk\all.sln` in Visual Studio and build. You will likely get build errors in trunk especially on Windows. It's usually relatively simple to fix everything quickly. Exclude unit test projects, they have lots of build errors and are not needed.

Build results can be found from `trunk\build\Release|Debug`. Build the needed modes what you need to build in your development.

# Building CloudRenderingPlugin

1. Add `add_subdirectory(src/cloud-rendering-plugins)` at the end of `<tundra_repo>\CMakeBuildConfig.txt`. You may have cloned this repo to another location, make add_subdirectory point to it.
2. Run Tundra CMake.
3. Open `tundra.sln` and build.

# Running CloudRenderingPlugin

```
TundraConsole.exe --config tundra-client.json --plugin CloudRenderingPlugin --cloudRenderer <host:port_of_your_cloud_rendering_service>
```
