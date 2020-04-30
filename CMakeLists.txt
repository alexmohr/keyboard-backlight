cmake_minimum_required(VERSION 3.12)
project(thinkpad_keyboard_backlight)
set(PROJECT_VERSION_MAJOR  1)
set(PROJECT_VERSION_MINOR  3)
set(PROJECT_VERSION_PATCH  0)

# Add version compile definition
add_compile_definitions(VERSION="${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")


set(CMAKE_INSTALL_PREFIX /usr/bin)
set(CMAKE_CXX_STANDARD 17)

# Configure optimization
# Available
# -O0   None
# -O1   Optimize
# -O2   Optimize more
# -O3   Optimize most
# -Os   Optimize size
# -Og   Optimize debug
set(OPTIMIZATION_LEVEL "-O3")

# Configure debug
# Available
# -g3   Maximum
# -g    Default
# -g1   Minimal
# ""    None
set(DEBUG_LEVEL "")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG=1")

# Configure C++ compiler flags
set(CMAKE_CXX_FLAGS "${OPTIMIZATION_LEVEL} \
    ${DEBUG_LEVEL} \
    -Wall \
    -Wpedantic \
    -Wextra")

set(SERVICE_TARGET_PATH  /etc/systemd/system/keyboard_backlight.service)
set(APP_TARGET_PATH ${CMAKE_INSTALL_PREFIX}/keyboard_backlight)
set(APP_NAME keyboard_backlight)

find_package (Threads)
# Some versions of gcc need to link filesystem lib
if (CMAKE_CXX_COMPILER_ID STREQUAL GNU)
    set(CXX_FILESYSTEM_LIBRARIES stdc++fs)
else()
    set(CXX_FILESYSTEM_LIBRARIES)
endif()


add_executable(${APP_NAME} kbd_backlight.cpp)
target_link_libraries (keyboard_backlight ${CMAKE_THREAD_LIBS_INIT} ${CXX_FILESYSTEM_LIBRARIES})

install(TARGETS keyboard_backlight DESTINATION ${CMAKE_INSTALL_PREFIX})

add_custom_target(service
        DEPENDS ${APP_NAME}
        COMMAND sudo cp ${CMAKE_CURRENT_SOURCE_DIR}/keyboard_backlight.service /etc/systemd/system &&
        sudo systemctl enable --now keyboard_backlight.service
)

add_custom_target(uninstall
        COMMAND sudo rm -f ${SERVICE_TARGET_PATH}  ${APP_TARGET_PATH}
)

# Write version to PKGBUILD
add_custom_command(TARGET ${APP_NAME} POST_BUILD
        COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/PKGBUILD ${CMAKE_BINARY_DIR} &&
        sed -i s/VERSION/${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}/g  ${CMAKE_BINARY_DIR}/PKGBUILD)