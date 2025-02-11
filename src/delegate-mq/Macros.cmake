# Function to copy .dll files from vcpkg bin directory to the build output directory
function(copy_vcpkg_dlls VCPKG_ROOT_DIR DELEGATE_APP)
    # Get .dll and .pdb files in the vcpkg bin directory
    file(GLOB ZMQ_BIN_FILES "${VCPKG_ROOT_DIR}/bin/libzmq*.dll" "${VCPKG_ROOT_DIR}/bin/libzmq*.pdb")

    # Copy each DLL file to the build output directory
    foreach(DLL_FILE ${ZMQ_BIN_FILES})
        add_custom_command(TARGET ${DELEGATE_APP} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${DLL_FILE}  # Copy the DLL file
            "${CMAKE_BINARY_DIR}/Debug"  # Destination directory
            COMMENT "Copying ${DLL_FILE} to build output"
        )
    endforeach()
endfunction()



