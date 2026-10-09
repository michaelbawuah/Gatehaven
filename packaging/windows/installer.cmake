set(CPACK_PACKAGE_INSTALL_DIRECTORY Gatehaven)
set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY Gatehaven)
set(CPACK_NSIS_DISPLAY_NAME "Gatehaven ${PROJECT_VERSION}")
set(CPACK_NSIS_PACKAGE_NAME Gatehaven)
set(CPACK_NSIS_MANIFEST_DPI_AWARE ON)
set(CPACK_NSIS_MODIFY_PATH OFF)
set(CPACK_NSIS_IGNORE_LICENSE_PAGE ON)
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/michaelbawuah/Gatehaven")
set(CPACK_PACKAGE_EXECUTABLES "gatehaven" "Gatehaven")
set(CPACK_NSIS_MENU_LINKS "share/gatehaven/docs/manual.html" "Gatehaven Manual")

# Register an Open With choice without replacing the user's default application.
set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS [=[
  WriteRegStr HKLM "Software\Classes\Gatehaven.Circuit" "" "Gatehaven circuit"
  WriteRegStr HKLM "Software\Classes\Gatehaven.Circuit\shell\open\command" "" '$\"$INSTDIR\bin\gatehaven.exe$\" $\"%1$\"'
  WriteRegStr HKLM "Software\Classes\.ghv\OpenWithProgids" "Gatehaven.Circuit" ""
  WriteRegStr HKLM "Software\Classes\.ccsb\OpenWithProgids" "Gatehaven.Circuit" ""
  System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
]=])
set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS [=[
  ReadRegStr $0 HKLM "Software\Classes\Gatehaven.Circuit\shell\open\command" ""
  StrCmp $0 '$\"$INSTDIR\bin\gatehaven.exe$\" $\"%1$\"' 0 gatehaven_keep_association
  DeleteRegKey HKLM "Software\Classes\Gatehaven.Circuit"
  DeleteRegValue HKLM "Software\Classes\.ghv\OpenWithProgids" "Gatehaven.Circuit"
  DeleteRegValue HKLM "Software\Classes\.ccsb\OpenWithProgids" "Gatehaven.Circuit"
  DeleteRegKey /ifempty HKLM "Software\Classes\.ghv\OpenWithProgids"
  DeleteRegKey /ifempty HKLM "Software\Classes\.ccsb\OpenWithProgids"
  DeleteRegKey /ifempty HKLM "Software\Classes\.ghv"
  DeleteRegKey /ifempty HKLM "Software\Classes\.ccsb"
  gatehaven_keep_association:
  System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
]=])
