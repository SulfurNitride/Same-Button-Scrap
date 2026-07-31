vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO TsudaKageyu/minhook
    REF v1.3.4
    SHA512 8a33233598b56ad9da44d22d470c2432f68364dac31bc719fcd6b085e681fa10ddd41865fbde056ee7f4e7a075cc135344b6bf444eadbd7e7314ee1bedfd89b5
    HEAD_REF master
    PATCHES fix-usage.patch
)

vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/minhook)
vcpkg_copy_pdbs()

file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug/include"
    "${CURRENT_PACKAGES_DIR}/debug/share"
)

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE.txt")
