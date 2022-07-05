# library: malleable TOHTN planner
set(TOHTN_SOURCES ${TOHTN_SOURCES}
        src/app/tohtn/tohtn_reader.hpp src/app/tohtn/tohtn_reader.hpp
        src/app/tohtn/tohtn_sysstate.hpp
        src/app/tohtn/tohtn_msg_tags.hpp
        src/app/tohtn/tohtn_simple_job.cpp src/app/tohtn/tohtn_simple_job.hpp
        src/app/tohtn/crowd/crowd_worker.hpp
        src/app/tohtn/tohtn_utils.cpp src/app/tohtn/tohtn_utils.hpp
        src/app/tohtn/tohtn_simple_job.cpp src/app/tohtn/tohtn_simple_job.hpp
        src/app/tohtn/tohtn_multi_job.cpp src/app/tohtn/tohtn_multi_job.hpp)

add_library(mallob_tohtn STATIC ${TOHTN_SOURCES})
set_target_properties(mallob_tohtn PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories(mallob_tohtn PRIVATE ${BASE_INCLUDES} ${PROJECT_SOURCE_DIR}/lib/pandaPIparser/include
        ${PROJECT_SOURCE_DIR}/src/app/tohtn/crowd)
target_compile_options(mallob_tohtn PRIVATE ${BASE_COMPILEFLAGS})
target_link_libraries(mallob_tohtn PRIVATE ${BASE_LIBS} mallob_commons
        ${PROJECT_SOURCE_DIR}/src/app/tohtn/crowd/libcrowd.a
        ${PROJECT_SOURCE_DIR}/lib/pandaPIparser/libpandaPIparser.a)
