# Function to copy .dll files from vcpkg bin directory to the build output directory
function(copy_files _src_files _dest_dir)
    # Copy each DLL file to the build output directory
    foreach(FILE ${_src_files})
        add_custom_command(TARGET ${DELEGATE_APP} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${FILE}      # Copy files
            ${_dest_dir}  # Destination directory
            COMMENT "Copying ${FILE} to build output"
        )
    endforeach()
endfunction()





