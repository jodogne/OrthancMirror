This folder shows how it is possible to link against the "Core" and
"OrthancCppClient" libraries from the Orthanc distribution. It is
shown how a sample REST API can be created.

This is the same sample than in folder "../RestApi", but with a
different build script and the use of C++ lambda functions.

The build script of this folder does not rely on the default CMake
script from Orthanc. It dynamically links against the standard system
Linux libraries. This results in a simpler, standalone build
script. However, it will only work on Linux-based systems.
