cmake_minimum_required(VERSION 3.25)

project(MyLiteInstallConsumer LANGUAGES C)

find_package(MyLite 0.1 CONFIG REQUIRED)

add_executable(mylite_install_consumer main.c)
target_link_libraries(mylite_install_consumer PRIVATE MyLite::mylite)
set_target_properties(mylite_install_consumer PROPERTIES
  C_STANDARD 17
  C_STANDARD_REQUIRED ON
  C_EXTENSIONS OFF
)
