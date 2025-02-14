# Function to copy .dll files from vcpkg bin directory to the build output directory
function(copy_files SRC_FILES DEST_DIR)
    # Copy each DLL file to the build output directory
    foreach(FILE ${SRC_FILES})
        add_custom_command(TARGET ${DELEGATE_APP} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${FILE}      # Copy files
            ${DEST_DIR}  # Destination directory
            COMMENT "Copying ${FILE} to build output"
        )
    endforeach()
endfunction()



