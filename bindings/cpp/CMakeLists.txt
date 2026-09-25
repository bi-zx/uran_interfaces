cmake_minimum_required(VERSION 3.16)
project(uran_interfaces VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
find_package(nlohmann_json 3.10 REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(generated_include ${CMAKE_CURRENT_BINARY_DIR}/generated/uran_interfaces/generated_catalog.hpp)
add_custom_command(OUTPUT ${generated_include}
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../../generators/generate_catalog.py
    --schemas ${CMAKE_CURRENT_SOURCE_DIR}/../../schemas --cpp ${generated_include}
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../../generators/generate_catalog.py
  COMMENT "Generating URAN schema catalog")
add_custom_target(uran_interfaces_generate_catalog DEPENDS ${generated_include})

add_library(${PROJECT_NAME} SHARED src/contracts.cpp)
add_dependencies(${PROJECT_NAME} uran_interfaces_generate_catalog)
add_library(uran_interfaces::uran_interfaces ALIAS ${PROJECT_NAME})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
target_include_directories(${PROJECT_NAME} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(${PROJECT_NAME} PUBLIC nlohmann_json::nlohmann_json PRIVATE ${CMAKE_DL_LIBS})
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8)
  target_link_libraries(${PROJECT_NAME} PRIVATE stdc++fs)
endif()
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION 1)

install(TARGETS ${PROJECT_NAME} EXPORT uran_interfacesTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES ${generated_include} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/uran_interfaces)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../schemas/ DESTINATION ${CMAKE_INSTALL_DATADIR}/uran_interfaces/schemas)
install(EXPORT uran_interfacesTargets NAMESPACE uran_interfaces:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/uran_interfaces)
configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/uran_interfacesConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/uran_interfacesConfig.cmake INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/uran_interfaces)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/uran_interfacesConfigVersion.cmake
  VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/uran_interfacesConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/uran_interfacesConfigVersion.cmake DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/uran_interfaces)

include(CTest)
if(BUILD_TESTING)
  add_executable(test_contracts ../../tests/test_contracts.cpp)
  target_compile_definitions(test_contracts PRIVATE
    URAN_INTERFACES_TEST_SCHEMA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../schemas"
    URAN_INTERFACES_TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../fixtures")
  target_link_libraries(test_contracts PRIVATE uran_interfaces::uran_interfaces)
  add_test(NAME uran_interfaces_contracts COMMAND test_contracts)
endif()
