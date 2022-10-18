# library: malleable TOHTN planner
set(TOHTN_SOURCES
        src/app/tohtn/global_syncer.cpp
        src/app/tohtn/tohtn_reader.cpp
        src/app/tohtn/tohtn_simple_job.cpp
        src/app/tohtn/tohtn_utils.cpp
        src/app/tohtn/tohtn_multi_job.cpp)

# Add Tohtn sources to Mallob base sources
set(BASE_SOURCES ${BASE_SOURCES} ${TOHTN_SOURCES} CACHE INTERNAL "")

set(BASE_INCLUDES ${BASE_INCLUDES}
        ${PROJECT_SOURCE_DIR}/lib/pandaPIparser/include
        ${PROJECT_SOURCE_DIR}/src/app/tohtn/crowd
        CACHE INTERNAL "")
set(BASE_LIBS ${BASE_LIBS}
        ${PROJECT_SOURCE_DIR}/src/app/tohtn/crowd/libcrowd.a
        ${PROJECT_SOURCE_DIR}/lib/pandaPIparser/libpandaPIparser.a
        CACHE INTERNAL "")

#add_library(mallob_tohtn STATIC ${TOHTN_SOURCES})
#set_target_properties(mallob_tohtn PROPERTIES LINKER_LANGUAGE CXX)
#target_include_directories(mallob_tohtn PRIVATE ${BASE_INCLUDES} ${PROJECT_SOURCE_DIR}/lib/pandaPIparser/include
#        ${PROJECT_SOURCE_DIR}/src/app/tohtn/crowd)
#target_compile_options(mallob_tohtn PRIVATE ${BASE_COMPILEFLAGS})
#target_link_libraries(mallob_tohtn PRIVATE ${BASE_LIBS} mallob_commons
#        ${PROJECT_SOURCE_DIR}/src/app/tohtn/crowd/libcrowd.a
#        ${PROJECT_SOURCE_DIR}/lib/pandaPIparser/libpandaPIparser.a)
