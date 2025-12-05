
function(copyOutputs TARGET_FOLDER)
    set(DLL_FOLDER "${TARGET_FOLDER}/SKSE/Plugins")
    set(TRANS_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${TRANS_DIR}")
    set(TRANS_TARGET "${TARGET_FOLDER}/SKSE/Plugins/${PRODUCT_NAME}/${TRANS_DIR}")

    message(STATUS "SKSE plugin output folder: ${DLL_FOLDER}")
    message(STATUS "Translation source folder: ${TRANS_SOURCE}")
    message(STATUS "Translation target folder: ${TRANS_TARGET}")

    add_custom_command(
        TARGET "${PROJECT_NAME}"
        POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${DLL_FOLDER}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${PROJECT_NAME}>" "${DLL_FOLDER}/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${TRANS_SOURCE}" "${TRANS_TARGET}"
        VERBATIM
    )

    add_custom_command(
        TARGET "${PROJECT_NAME}"
        POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_PDB_FILE:${PROJECT_NAME}>" "${DLL_FOLDER}/$<TARGET_PDB_FILE_NAME:${PROJECT_NAME}>"
        VERBATIM
    )
endfunction()
