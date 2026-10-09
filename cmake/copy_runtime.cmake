# A static SDL build has no runtime DLLs; skip the copy in that case.
string(REPLACE "|" ";" gatehaven_runtime_files "${GH_RUNTIME_DLLS}")
foreach(gatehaven_runtime_file IN LISTS gatehaven_runtime_files)
    file(COPY "${gatehaven_runtime_file}" DESTINATION "${GH_DEST}")
endforeach()
