CMAKE_MINIMUM_REQUIRED(VERSION 3.0.0)
PROJECT(perilune VERSION 0.1.0)

INCLUDE_DIRECTORIES(
    )
LINK_DIRECTORIES(
    )
ADD_DEFINITIONS(
    -DUNICODE
    -D_UNICODE
    -D_CRT_SECURE_NO_WARNINGS
    -DNOMINMAX
    )
ADD_COMPILE_OPTIONS(
    /std:c++latest 
    /EHsc
    )

SUBDIRS(lualib lua samples/value)
