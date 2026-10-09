# Set the system prefix only for the package manager artifact, not the archive.
if(CPACK_GENERATOR STREQUAL "DEB")
    set(CPACK_PACKAGING_INSTALL_PREFIX /usr)
endif()
