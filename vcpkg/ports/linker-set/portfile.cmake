vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO nickelpro/linker_set
    REF 8fc85e3e94bc9b920f3688e203d6a855bab269fe
    SHA512 45db2e3d2777b6cb86bc7571ec06a6931c526cf200ee3898cbe7e818f35b234e6594c5f04704b05cdb04a741c730d6a6486adde9e485e2a002b058f15c1ceee4
    HEAD_REF main
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DLINKER_SET_BUILD_TESTS=OFF
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/linker_set)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/License")
file(
  INSTALL ${CMAKE_CURRENT_LIST_DIR}/usage
  DESTINATION ${CURRENT_PACKAGES_DIR}/share/${PORT}
)
