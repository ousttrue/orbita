SET(SUB_NAME sample_pointer)
SET(DEPENDENCIES_DIR ${CMAKE_CURRENT_LIST_DIR}/../../dependencies)
SET(LUA_DIR ${DEPENDENCIES_DIR}/lua)

FILE(GLOB SRC
    *.cpp
    )

ADD_EXECUTABLE(${SUB_NAME}
    ${SRC}
    )
TARGET_COMPILE_DEFINITIONS(${SUB_NAME} PUBLIC
    )
TARGET_INCLUDE_DIRECTORIES(${SUB_NAME} PUBLIC
    ${LUA_DIR}
    ${DEPENDENCIES_DIR}/nlohmann-json/include
    ${DEPENDENCIES_DIR}/plog/include
    ../../include
    )
TARGET_LINK_LIBRARIES(${SUB_NAME}
    winmm
    d3d11
    lualib
    )
