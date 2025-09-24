# copy files from the pki directory
function(copy_pki_files)
    # argument parsing
	set(oneValueArgs TARGET SUBDIRECTORY)
    cmake_parse_arguments(ARG "" "${oneValueArgs}" "FILES" ${ARGN})
    
    if(NOT ARG_TARGET)
        message(FATAL_ERROR "You must provide a TARGET")
    endif(NOT ARG_TARGET)
    set(target ${ARG_TARGET})
	
	if(GENERATE_PKI)
		set(subdirectory ${ARG_SUBDIRECTORY})
		if(NOT subdirectory)
			set(subdirectory .)
		endif()
		
		set(files ${ARG_FILES})

		set(pki_dir ${PROJECT_BINARY_DIR}/pki)

		foreach(file ${files})
			get_executable_companion_destinations(destination_files ${subdirectory} ${pki_dir}/${file})
			FOREACH(destination ${destination_files})
				add_custom_command(
					OUTPUT ${destination}
					COMMAND ${CMAKE_COMMAND} -E copy ${pki_dir}/${file} ${destination}
					DEPENDS pki ${pki_dir}/${file}
				)
			ENDFOREACH()
			set(output_files ${output_files} ${destination_files})
		endforeach()
	endif(GENERATE_PKI)
	
    add_custom_target(
        ${target} ALL
        DEPENDS ${output_files}
    )
endfunction(copy_pki_files)
