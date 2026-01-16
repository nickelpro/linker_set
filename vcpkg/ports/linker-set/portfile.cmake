vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO nickelpro/linker_set
    REF 9cfa3ee1f72a7953d244f47671be7282ad74337c
    SHA512 64a39cd5ea42cdf52d8893ea9e3da36074049da7ff6013b18fe002ed176c8b0feac78000df278e953e8f379a7fe2ef0fe39236db0acb1bc2f14c4a1f344ec740
    HEAD_REF main
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DLINKER_SET_BUILD_TESTS=OFF
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(
    PACKAGE_NAME linker_set
    CONFIG_PATH lib/cmake/linker_set
)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/License")
file(
  INSTALL ${CMAKE_CURRENT_LIST_DIR}/usage
  DESTINATION ${CURRENT_PACKAGES_DIR}/share/${PORT}
)
