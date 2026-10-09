include(GNUInstallDirs)

install(TARGETS gatehaven-cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(GATEHAVEN_BUILD_APP)
    install(TARGETS gatehaven BUNDLE DESTINATION . RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
install(FILES README.md DESTINATION ${CMAKE_INSTALL_DATADIR}/gatehaven)
install(DIRECTORY docs samples third_party DESTINATION ${CMAKE_INSTALL_DATADIR}/gatehaven)

# Include the redistributable Microsoft runtime when built with MSVC.
set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ${CMAKE_INSTALL_BINDIR})
include(InstallRequiredSystemLibraries)
