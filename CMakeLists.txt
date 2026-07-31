cmake_minimum_required(VERSION 3.24)

project(
    ScrapWithSameButton
    VERSION 0.1.0
    LANGUAGES CXX RC
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND MSVC)
    include_directories(SYSTEM
        "/usr/share/msvc/sdk/include/um"
        "/usr/share/msvc/sdk/include/shared"
        "/usr/share/msvc/sdk/include/winrt"
    )
endif()

find_package(minhook CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

set(COMMONLIBF4_ROOT
    "${CMAKE_SOURCE_DIR}/extern/CommonLibF4"
    CACHE PATH
    "Path to a CommonLibF4 checkout with commonlib-shared initialized"
)

if(NOT EXISTS "${COMMONLIBF4_ROOT}/include/F4SE/F4SE.h")
    message(FATAL_ERROR
        "CommonLibF4 was not found at ${COMMONLIBF4_ROOT}. "
        "Clone it with submodules into extern/CommonLibF4."
    )
endif()

file(GLOB_RECURSE COMMONLIB_SHARED_SOURCES CONFIGURE_DEPENDS
    "${COMMONLIBF4_ROOT}/lib/commonlib-shared/src/*.cpp"
)
file(GLOB_RECURSE COMMONLIBF4_SOURCES CONFIGURE_DEPENDS
    "${COMMONLIBF4_ROOT}/src/*.cpp"
)

add_library(commonlib_shared STATIC ${COMMONLIB_SHARED_SOURCES})
target_compile_features(commonlib_shared PUBLIC cxx_std_23)
target_include_directories(commonlib_shared
    PUBLIC "${COMMONLIBF4_ROOT}/lib/commonlib-shared/include"
)
target_precompile_headers(commonlib_shared
    PRIVATE "${COMMONLIBF4_ROOT}/lib/commonlib-shared/src/REX/PCH.h"
)
target_link_libraries(commonlib_shared
    PUBLIC spdlog::spdlog
    PRIVATE
        advapi32
        bcrypt
        d3d11
        d3dcompiler
        dbghelp
        dxgi
        ole32
        shell32
        user32
        version
        ws2_32
)

add_library(commonlibf4 STATIC ${COMMONLIBF4_SOURCES})
target_compile_features(commonlibf4 PUBLIC cxx_std_23)
target_include_directories(commonlibf4
    PUBLIC
        "${COMMONLIBF4_ROOT}/include"
        "${COMMONLIBF4_ROOT}/lib/commonlib-shared/include"
)
target_precompile_headers(commonlibf4
    PRIVATE "${COMMONLIBF4_ROOT}/include/F4SE/Impl/PCH.h"
)
target_link_libraries(commonlibf4 PUBLIC commonlib_shared)

foreach(target commonlib_shared commonlibf4)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /bigobj
            /EHsc
            /permissive-
            /utf-8
            /wd4200
            /wd4201
            /wd4324
        )
        if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            target_compile_options(${target} PRIVATE
                -fms-compatibility
                -fms-extensions
                -Wno-delete-non-abstract-non-virtual-dtor
                -Wno-deprecated-volatile
                -Wno-ignored-qualifiers
                -Wno-inconsistent-missing-override
                -Wno-invalid-offsetof
                -Wno-microsoft-include
                -Wno-overloaded-virtual
                -Wno-pragma-system-header-outside-header
                -Wno-reinterpret-base-class
                -Wno-switch
                -Wno-unused-local-typedef
                -Wno-unused-private-field
            )
        endif()
    endif()
endforeach()

add_library(scrap_with_same_button_core STATIC
    src/Hooks.cpp
    src/Hooks.h
)
target_compile_features(scrap_with_same_button_core PUBLIC cxx_std_23)
target_include_directories(scrap_with_same_button_core
    PUBLIC "${CMAKE_SOURCE_DIR}/src"
)
target_precompile_headers(scrap_with_same_button_core PRIVATE src/PCH.h)
target_link_libraries(scrap_with_same_button_core
    PUBLIC
        commonlibf4
        minhook::minhook
)

function(add_swsb_variant target runtime_define output_suffix)
    add_library(${target} SHARED
        src/Plugin.cpp
        cmake/version.rc
    )
    target_compile_features(${target} PRIVATE cxx_std_23)
    target_precompile_headers(${target} PRIVATE src/PCH.h)
    target_compile_definitions(${target} PRIVATE
        ${runtime_define}=1
        SWSB_VERSION="${PROJECT_VERSION}"
        SWSB_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        SWSB_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        SWSB_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    )
    target_link_libraries(${target} PRIVATE scrap_with_same_button_core)
    set_target_properties(${target} PROPERTIES
        OUTPUT_NAME "ScrapWithSameButton-${output_suffix}"
        PREFIX ""
    )
endfunction()

add_swsb_variant(ScrapWithSameButtonOG SWSB_RUNTIME_OG OG)
add_swsb_variant(ScrapWithSameButtonNG SWSB_RUNTIME_NG NG)
add_swsb_variant(ScrapWithSameButtonAE SWSB_RUNTIME_AE AE)

set(PACKAGE_ROOT "${CMAKE_BINARY_DIR}/package")
set(PACKAGE_INSTALLER "${PACKAGE_ROOT}/installer")
set(PACKAGE_ARCHIVE
    "${CMAKE_BINARY_DIR}/${PROJECT_NAME}-${PROJECT_VERSION}-FOMOD.zip"
)

configure_file(
    "${CMAKE_SOURCE_DIR}/package/fomod/info.xml.in"
    "${CMAKE_BINARY_DIR}/generated-fomod/info.xml"
    @ONLY
)

add_custom_target(package_mod
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${PACKAGE_ROOT}"
    COMMAND "${CMAKE_COMMAND}" -E rm -f "${PACKAGE_ARCHIVE}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${PACKAGE_INSTALLER}/common"
            "${PACKAGE_INSTALLER}/fomod"
            "${PACKAGE_INSTALLER}/runtime/OG/F4SE/Plugins"
            "${PACKAGE_INSTALLER}/runtime/NG/F4SE/Plugins"
            "${PACKAGE_INSTALLER}/runtime/AE/F4SE/Plugins"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_SOURCE_DIR}/README.md"
            "${PACKAGE_INSTALLER}/common/README.md"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_SOURCE_DIR}/LICENSE"
            "${PACKAGE_INSTALLER}/common/LICENSE"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_SOURCE_DIR}/THIRD_PARTY_NOTICES.txt"
            "${PACKAGE_INSTALLER}/common/THIRD_PARTY_NOTICES.txt"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_BINARY_DIR}/generated-fomod/info.xml"
            "${PACKAGE_INSTALLER}/fomod/info.xml"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_SOURCE_DIR}/package/fomod/ModuleConfig.xml"
            "${PACKAGE_INSTALLER}/fomod/ModuleConfig.xml"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:ScrapWithSameButtonOG>"
            "${PACKAGE_INSTALLER}/runtime/OG/F4SE/Plugins/ScrapWithSameButton.dll"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:ScrapWithSameButtonNG>"
            "${PACKAGE_INSTALLER}/runtime/NG/F4SE/Plugins/ScrapWithSameButton.dll"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:ScrapWithSameButtonAE>"
            "${PACKAGE_INSTALLER}/runtime/AE/F4SE/Plugins/ScrapWithSameButton.dll"
    COMMAND "${CMAKE_COMMAND}" -E chdir "${PACKAGE_INSTALLER}"
            "${CMAKE_COMMAND}" -E tar cf "${PACKAGE_ARCHIVE}" --format=zip --
            fomod common runtime
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    BYPRODUCTS "${PACKAGE_ARCHIVE}"
    DEPENDS
        ScrapWithSameButtonOG
        ScrapWithSameButtonNG
        ScrapWithSameButtonAE
    COMMENT "Packaging the OG, NG, and AE DLLs in one FOMOD"
    VERBATIM
)
