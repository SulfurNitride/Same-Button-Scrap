set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_BUILD_TYPE release)

# Keep the current pkg-config supplied by CI visible while vcpkg builds ports.
set(VCPKG_ENV_PASSTHROUGH_UNTRACKED
    PKG_CONFIG
)
