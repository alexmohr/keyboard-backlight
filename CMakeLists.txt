cmake_minimum_required(VERSION 3.12)
project(thinkpad_keyboard_backlight)
set(CMAKE_INSTALL_PREFIX /usr/local/bin)


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

# Configure C++ compiler flags
set(CMAKE_CXX_FLAGS "${OPTIMIZATION_LEVEL} \
    ${DEBUG_LEVEL} \
    -Wall \
    -Wpedantic \
    -Wextra")


find_package (Threads)



add_executable(keyboard_backlight kbd_backlight.cpp)
target_link_libraries (keyboard_backlight ${CMAKE_THREAD_LIBS_INIT})

install(TARGETS keyboard_backlight DESTINATION ${CMAKE_INSTALL_PREFIX})


# Add custom features to build hex, bin or lst files.
add_custom_target(service
        DEPENDS keyboard_backlight
        COMMAND sudo cp ${CMAKE_CURRENT_SOURCE_DIR}/keyboard_backlight.service /etc/systemd/system &&
        sudo systemctl enable --now keyboard_backlight.service
        )
