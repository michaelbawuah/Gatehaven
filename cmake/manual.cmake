find_package(Python3 REQUIRED COMPONENTS Interpreter)
file(GLOB GATEHAVEN_GUIDES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/docs/*.md")
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/manual/manual.html"
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/build_manual.py"
        "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/manual"
    DEPENDS ${GATEHAVEN_GUIDES} "${CMAKE_CURRENT_SOURCE_DIR}/samples/README.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/build_manual.py"
    VERBATIM)
add_custom_target(gatehaven_manual ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/manual/manual.html")
install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/manual/" DESTINATION "${CMAKE_INSTALL_DATADIR}/gatehaven/docs")
if(APPLE AND GATEHAVEN_BUILD_APP)
    install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/manual/" DESTINATION gatehaven.app/Contents/Resources/docs)
endif()
