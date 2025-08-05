cmake_minimum_required(VERSION 3.10.0)
project(test_slmp VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)

find_package(ament_cmake REQUIRED)

find_package(libslmp REQUIRED)
find_package(libmelcli REQUIRED)

find_package(spdlog REQUIRED)


include_directories(include)

# Original test executable
add_executable(test_slmp main.cpp)
target_link_libraries(test_slmp spdlog::spdlog)
ament_target_dependencies(test_slmp
    libslmp
    libmelcli
)

# New scattered access test executable
add_executable(test_scattered_access test_scattered_access.cpp)
target_link_libraries(test_scattered_access spdlog::spdlog)
ament_target_dependencies(test_scattered_access
    libslmp
    libmelcli
)

# Install executables
install(TARGETS test_slmp test_scattered_access
    DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY include/test_slmp DESTINATION include)

ament_package()






