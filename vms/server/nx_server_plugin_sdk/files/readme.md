# Nx Server Plugin SDK

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

---------------------------------------------------------------------------------------------------
## License

The whole contents of this package, including all C/C++ source code, is licensed as Open Source
under the terms of Mozilla Public License 2.0: www.mozilla.org/MPL/2.0/, with the possible
exception of certain files which may be licensed under the terms of other open-source licenses
explicitly referenced in those files.

See the license texts in `licenses/` directory located in the root directory of this package.

---------------------------------------------------------------------------------------------------

## Introduction

This package provides a Server SDK for creating Plugins for the Server. A _Plugin_ is a dynamic
library or directory containing this library and other files needed for its operation, implementing
one or more _Integrations_, which are logical entities extending the Server functionality in some
way. Although these terms ("Plugin" and "Integration") can be used interchangeably in most cases,
their specific meanings matter in certain contexts.

The SDK supports four types of Integrations:
- [Analytics Integrations](#analytics-integrations)
- [Camera Integrations](#camera-integrations)
- [Storage Integrations](#storage-integrations)
- [Cloud Storage Integrations](#cloud-storage-integrations)

Plugins can contain Integrations of different types. Sample implementations are available for each
Integration type; detailed information can be found in the corresponding sections of this document.

## Common Technical Details

### General Information

From the developers' point of view, any Plugin created using this SDK is a dynamic library (`.dll`
on Windows, `.so` on Linux) which exports a single `extern "C"` entry point function. Such
function is a factory for objects inherited from a dedicated SDK abstract class (in other words,
implementing a dedicated interface). The base interface is different for different
Integration types and defines the name and the prototype of the entry point function.

ATTENTION: If you consider linking your Plugin to any dynamic libraries, including the ones from
the OS, and the C++ standard library, consult
[src/nx/sdk/dynamic_libraries.md](@ref md_src_nx_sdk_dynamic_libraries) to avoid potential crashes.

To make it possible to develop Plugins using a different C++ compiler (e.g. with an incompatible
ABI) rather than the one used to compile the Server itself, or potentially in languages other than
C++, a COM-like approach is offered: all objects created in a Plugin or passed to a Plugin inherit
abstract classes declared in header files of this SDK, which have only pure virtual functions with
C-style arguments (not using C++ standard library classes). To manage lifetime of such objects,
they incorporate a reference counter.

### Helper tools

To simplify implementation of a Plugin, a number of helper classes are provided with this SDK that
implement the SDK interfaces and handle such complexities as reference counting and interface
requesting. Such classes are located in folders named `helpers`. If such a helper class does
not address all the requirements of the Plugin author, it may be subclassed or not used at all -
the Plugin can always implement everything from scratch using just interfaces from the header
files.

Also, some tools are provided with this SDK, which are recommended though not required to be used
by a Plugin:

- `src/nx/sdk/helpers/` - utilities and typical/base classes implementing interfaces, including:
    - `ref_countable.h` - `class nx::sdk::RefCountable`: base class for objects implementing
        `IRefCountable` interface.
    - `ptr.h` - `class nx::sdk::Ptr`: Smart Pointer to reference-countable objects.
    - `to_string.h` and `uuid_helper.h`: Conversion functions.

- `src/nx/sdk/analytics/helpers/` - Analytics-related utilities and typical/base classes
    implementing interfaces, including:
    - `integration.h` - `class nx::sdk::analytics::Integration`: Base class for a typical
        implementation of the `nx::sdk::analytics::IIntegration` interface; use it to start
        developing an Analytics Integration
        (see [Analytics Integrations](#analytics-integrations)).
    - `pixel_format.h`: Utilities for `enum IUncompressedFrame::PixelFormat`.

- Pure C++ library `nx_kit` (which itself is an independent project with its own
    [documentation](nx_kit/readme.md)) that offers convenient debug output (logging) and support
    for .ini files with options for experimenting (e.g. switching alternative implementations, or
    parameterizing the code), and a rudimentary framework for simple unit tests (used by the SDK
    itself, and Plugin samples).

### Documentation

The SDK C++ files have extensive documentation comments in Doxygen format, from which HTML files
are generated and included into the SDK: see `docs/html/index.html`.

### Samples: building and installing

For every type of Integration there are some samples. They can be compiled and linked using CMake.

Prerequisites:
```
- CMake >= 3.14
- Windows (7, 10 or 11): Microsoft Visual Studio >= 2019
    - ATTENTION: When linking to the C++ runtime dynamically (Visual Studio does it by default),
        avoid using the latest build tools (toolset) to avoid crashes - see the details in
        [src/nx/sdk/dynamic_libraries.md](@ref md_src_nx_sdk_dynamic_libraries).
- Linux (Ubuntu 20.04, 22.04, 24.04) including ARM (e.g. Raspberry Pi or Nvidia Tegra) native or
    cross-compiling:
    - g++ >= 9.5
    - make or Ninja
- Qt6 (required by certain samples only)
- Raspberry Pi sysroot (required by certain samples only)
```

To compile the samples, and (if not cross-compiling) run unit tests, execute the commands collected
into the provided scripts (use their source code as a reference; run with `-h` or `/?` to see
the possible options):
```
# Windows, x64:
build.bat

# Linux or Windows with Cygwin, x64:
build.sh

# Linux, 64-bit ARM cross-compiling (e.g. Nvidia Tegra):
# NOTE: The provided file toolchain_arm64.cmake defines which cross-compiler will be used.
build_arm64.sh

# Linux, 32-bit ARM cross-compiling, without Raspberry Pi specific samples:
# NOTE: The provided file toolchain_arm32.cmake defines which cross-compiler will be used.
build_arm32.sh

# Linux, Raspberry Pi (32-bit ARM) cross-compiling, with samples specific to Raspberry Pi:
# NOTE: The provided file toolchain_arm32.cmake defines which cross-compiler will be used.
build_arm32.sh -DrpiFirmware=<path_to_rpi_sysroot>
```

ATTENTION: Some of the samples require the Qt library. The Qt version should be the same as the one
that comes with the Server (see the installed Server files), otherwise an unpredictable behavior
may take place. To build these samples, specify the paths to `qt` And `qt-host` Conan packages
distributed by Nx using CMake parameters `QT_DIR` and `QT_HOST_PATH`:
```
# Windows:
build.bat

# Linux:
build.sh QT_DIR=<absolute-path-to-Qt6-dir> QT_HOST_PATH=<absolute-path-to-qt6-host-dir>.
```

`absolute-path-to-qt-dir` and `absolute-path-to-qt-host-dir` are paths to the Conan packages that
can be installed from [open-source Nx artifactory](http://artifactory.nxvms.dev/).
The Conan remote URL for this artifactory is
`https://artifactory.nxvms.dev/artifactory/api/conan/conan`.

Also, if you do not want to run unit tests, it can be achieved by setting the
`NX_SDK_NO_TESTS` environment variable to `1` (the tests will be built anyway).

You can also build the samples using any IDE that supports CMake projects. The necessary steps
depend on the IDE; for example, on Windows, you can use the Visual Studio GUI to compile a sample
by opening `..\server_plugin_sdk-build\<plugin_type>\<sample_name>` using the option "Open a local
folder" from the start windows or `Open` -> `Folder` of the main menu. `plugin_type` here is
`analytics` for `Analytics Integrations`, `device` for `Camera Integrations`, `storage` for
`Storage Integrations` and `cloud_storage` for `Cloud Storage Integrations`.

After the successful build, locate the built artifacts:
```
# Windows:
..\server_plugin_sdk-build\samples\<plugin_type>\<sample_name>\<sample_name>.dll

# Linux:
../server_plugin_sdk-build/samples/<plugin_type>/<sample_name>/lib<sample_name>.so
```

To install a Plugin, just copy its library file (or directory, if this Plugin consists of more than
one file) to the dedicated folder in the Server installation directory:
```
# Windows:
C:\Program Files\<vms-installation-dir>\MediaServer\plugins\

# Linux:
/opt/<vms-installation-dir>/mediaserver/bin/plugins/
```

Some of the Plugins (e.g. `stub_analytics_plugin`) require more than one file to work. In this
case, the library file with the other necessary files and directories must be copied to the
separate subdirectory under the Server Plugin directory, e.g:
```
# Windows:
C:\Program Files\<vms-installation-dir>\MediaServer\plugins\
- stub_analytics_plugin\
  - object_streamer\
  - stub_analytics_plugin.dll

# Linux:
/opt/<vms-installation-dir>/mediaserver/bin/plugins/
- stub_analytics_plugin/
  - object_streamer/
  - libstub_analytics_plugin.so
```

ATTENTION: After copying a Plugin library file/directory, the Server has to be restarted.

## Specific Integration Types

### Analytics Integrations

#### General Information

Analytics Integrations are Server Integrations for the Video Management System (VMS) that can
analyze video streams and produce metadata.

From the business logic point of view, a typical Analytics Integration is connected to a video
camera, may receive video frames from the camera or interact with the camera using a certain
proprietary way, and may generate metadata - events and objects (rectangles on a frame) - which is
sent to the Server to be stored in its database and visualized in the Client.

Analytics Integrations implement the nx::sdk::analytics::IIntegration interface
(src/nx/sdk/analytics/i_integration.h), which is derived from
nx::sdk::IIntegration (src/nx/sdk/i_integration.h).

#### Samples

This package includes two samples of Plugins, implementing Analytics Integrations, written in C++,
located at `samples/analytics`.

The first one is called `Sample Analytics Plugin`, located at
`samples/analytics/sample_analytics_plugin/`. It receives video frames from a camera, ignores them,
and generates stub metadata Objects looking as a rectangle moving diagonally across the frame. Also
an Event is generated when the trajectory finishes a loop. This Plugin has many comments in the
source code, which help to better understand how Analytics Integrations should be written.

The second sample is called `Stub Analytics Plugin`, located at
`samples/analytics/stub_analytics_plugin/`. Its dynamic library declares a number of relatively
small Integrations. They ignore all the received video frames from a camera, but demonstrate an
extensive feature set - can generate various metadata Objects and Events, and tend to use every
feature available in the SDK. These Plugins have certain settings which allow to test various SDK
features. Some settings are declared in the manifests and thus can be edited in the GUI, while some
other settings which influence the certain Integration in general, including how to generate its
manifest, are located in the .ini files (see `stub_analytics_plugin_*_ini.h`), backed by the .ini
file mechanism `nx/kit/ini_config.h`.

### Camera Integrations

#### General Information

From the business logic point of view, a Camera Integration creates its own type of a Device (such
as a Camera or an I/O Module), capable of e.g. supplying video frames to the Server.

Such an Integration can, for example, be one of the following:

- Driver for the particular IP camera model, like the included sample `axis_camera_plugin`.
- Driver for the particular camera type, connecting to a camera in its own proprietary way, like
    the included sample `rpi_camera_plugin`.
- Driver for a virtual camera, like the included sample `image_library_plugin`.

Base class for Camera Integrations is `class nxpl::PluginInterface` (`src/plugins/plugin_api.h`).

#### Samples

This package includes a number of sample Plugins, implementing Camera Integrations, written in C++,
located at `samples/device`.

`Axis Camera Plugin` is an example of the IP camera driver. It is located at
`samples/device/axis_camera_plugin`. This Plugin needs Qt6 to be built.

`RPi Camera Plugin` is an example of the driver for camera with the proprietary control interface.
This Plugin needs Raspberry Pi sysroot to be built.

`Image Library Plugin` is an example of the virtual camera.

### Storage Integrations

#### General Information

From the business logic point of view, a Storage Integration creates its own type of a Storage
which can be used by the Server for storing video archive and analytics metadata.

Base class for Storage Integrations is `class nxpl::PluginInterface` (`src/plugins/plugin_api.h`).

#### Samples

This package includes a number of sample Plugins, implementing Storage Integrations written in C++,
located at `samples/storage`.

### Cloud Storage Integrations

#### General Information

From the business logic point of view, an Cloud Storage Integration creates its own type of a
Storage capable of receiving the video stream from the Server to be recorded into some storage in
order to be retrieved afterwards.

This type of Plugins is conceived as an alternative to the long-existing Storage Integrations for
those cases when the underlying implementation is stream-based rather than file-based. Integrations
created with this SDK are used seamlessly by the Video Management System (VMS) along with Storage
Integrations - from the VMS user point of view, both types of Integrations create the video
recording and playback looks the same whether the conventional storages or stream based are used.

Here are the main ideas behind this type of Integrations:
* A Cloud Storage Integrations able to write and read chunks of media data corresponding to the
given device and stream index to and from the backend. Chunk is essentially a part of continuos
media data from device. Server will cut this incoming stream into parts which in turn will be fed
and afterwards read from the Integration on the per packet basis.
* Media stream consists of media data packets. Each packet has some typed attributes such as
timestamp and duration and opaque array of binary data. Integration does not need to know the
structure of the provided binary packet data. It should store it as is and return back when
requested.
* Apart from handling continuos media data SDK provides interface for storing and requesting
various types of metadata such as object detection data, motion and bookmarks. This data is
provided in the form of JSON text objects. The SDK contains helper C++ data structs which
correspond to the each of the JSONs provided. The JSON text representation of those objects is
chosen from the performance perspective to eliminate the need for excess serialization and
deserialization of data since Integration itself should generally be only a thin layer between
Server and backend which should only transmit the data and not perform any computations itself.

ATTENTION: This part of SDK is currently considered a work-in progress. Integrations written with
it are not recommended for production systems. In future versions of Server, there may be breaking
changes affecting Cloud Storage Integrations. As a result, Plugins developed now might not be
compatible with these upcoming versions. As part of this work-in-progress, Cloud Storage
Integrations use several header files also used for Camera Integrations -
`camera/camera_plugin.h`, `camera/camera_plugin_types.h` and `plugins/plugin_api.h`; the entities
in these files are named using outdated conventions and may lack proper documentation.

Cloud Storage Integrations implement the nx::sdk::cloud_storage::IIntegration interface
(src/nx/sdk/cloud_storage/i_integration.h), which is derived from `nx::sdk::IIntegration`
(src/nx/sdk/i_integration.h).

#### Samples

This package includes two samples of Plugins, implementing Cloud Storage Integrations written in
C++, located at `samples/cloud_storage`.

The first one is called `Sample Cloud Storage Plugin`, located at
`samples/cloud_storage/sample_cloud_storage_plugin/`. It receives the video stream from the
Server, ignores it, and returns nothing when the Server tries to retrieve the video stream from
the Storage. This Plugin has many comments in the source code, which help to better understand how
Cloud Storage Integrations should be written.

The second sample is called `Stub Cloud Storage Plugin`, located at
`samples/cloud_storage/stub_cloud_storage_plugin/`. It works using the local file system as the
underlying storage, and tends to use every feature available in the SDK.

## Compatibility and Breaking Changes

Plugin libraries compiled using nx_sdk 1.7.1 (which has been included with VMS 3.2) are compatible
with VMS 4.0+, because the binary interface (ABI) did not change. The same way, Plugins compiled
with this newer SDK should work with VMS 3.2 as well.

The current SDK does not offer any new features or API extensions for Camera Integrations and
Storage Integrations starting from nx_sdk 1.7.1, which comes with VMS 3.2.

Re-compiling the source code of Plugins written with nx_sdk 1.7.1 using this newer SDK requires
some simple adjustments due the following breaking changes in SDK source code:

- Changed prototypes of the base interface `class PluginInterface` methods (does not affect binary
    compatibility) `addRef()` and `releaseRef()`:
    - Now they return `int` instead of `unsigned int`.
    - Now they are `const`.
- Removed unused interface `nxpl::Plugin3` which only added `setLocale()` to `nxpl::Plugin2`.
- SDK headers moved:
    - `include/plugins/camera_*.h` -> `src/camera/`.
    - `include/plugins/plugin_*.h` -> `src/plugins/`.
- Helper utilities, formerly in `plugin_tools.h`:
    - `ScopedRef<>` replaced with `Ptr<>` (`src/nx/sdk/ptr.h`).
    - `alignUp()`, `mallocAligned()`, `freeAligned()` moved to `nx/kit/utils.h`.
