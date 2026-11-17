cmake_minimum_required(VERSION 3.16)
project(metro_medellin CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

if(MINGW)
  # MinGW enlaza la biblioteca estándar como DLL (libstdc++-6.dll,
  # libgcc_s_seh-1.dll). Sin -static, navegador.exe solo arranca si el PATH del
  # proceso incluye la carpeta bin de MinGW; con doble clic no la incluye.
  add_link_options(-static)
endif()

# Núcleo compartido entre el programa y las pruebas.
add_library(nucleo STATIC
  src/graph.cpp
  src/algorithms.cpp
  src/io.cpp
  src/servidor.cpp
)
target_include_directories(nucleo PUBLIC src)
if(WIN32)
  # Winsock: el servidor web usa sockets en Windows.
  target_link_libraries(nucleo PUBLIC ws2_32)
endif()

add_executable(navegador src/main.cpp)
target_link_libraries(navegador PRIVATE nucleo)

add_executable(pruebas tests/test_main.cpp)
target_link_libraries(pruebas PRIVATE nucleo)
target_compile_definitions(pruebas PRIVATE CARPETA_DATOS="${CMAKE_CURRENT_SOURCE_DIR}/data")
# Las pruebas usan assert: hay que anular el NDEBUG que agrega Release.
if(MSVC)
  target_compile_options(pruebas PRIVATE /UNDEBUG)
else()
  target_compile_options(pruebas PRIVATE -UNDEBUG)
endif()

enable_testing()
add_test(NAME pruebas COMMAND pruebas)

# El SVG y los demás archivos de salida quedan en build/out.
add_custom_command(TARGET navegador POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/out)
