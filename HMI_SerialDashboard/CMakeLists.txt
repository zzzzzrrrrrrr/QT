cmake_minimum_required(VERSION 3.19)
project(HMI_SerialDashboard VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(HMI_USE_MANUAL_AUTOGEN "Use manually generated uic/moc files for restricted shells." OFF)

find_package(Qt6 COMPONENTS Core Widgets REQUIRED)
find_package(Qt6 COMPONENTS Network Xml LinguistTools REQUIRED)
find_package(Qt6 COMPONENTS Charts QUIET)
find_package(Qt6 COMPONENTS SerialPort QUIET)
find_package(Qt6 COMPONENTS SerialBus QUIET)

qt_standard_project_setup()

set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    datasource.cpp
    datasource.h
    alarmmanager.cpp
    alarmmanager.h
    datalogger.cpp
    datalogger.h
    serialmanager.cpp
    serialmanager.h
    dataprocessor.cpp
    dataprocessor.h
    workerthread.cpp
    workerthread.h
    configmanager.cpp
    configmanager.h
)

if(HMI_USE_MANUAL_AUTOGEN)
    set(MANUAL_AUTOGEN_DIR "${CMAKE_BINARY_DIR}/manual_autogen")
    set(MANUAL_MOC_SOURCES
        "${MANUAL_AUTOGEN_DIR}/moc/moc_mainwindow.cpp"
        "${MANUAL_AUTOGEN_DIR}/moc/moc_datasource.cpp"
        "${MANUAL_AUTOGEN_DIR}/moc/moc_alarmmanager.cpp"
        "${MANUAL_AUTOGEN_DIR}/moc/moc_datalogger.cpp"
        "${MANUAL_AUTOGEN_DIR}/moc/moc_serialmanager.cpp"
        "${MANUAL_AUTOGEN_DIR}/moc/moc_dataprocessor.cpp"
        "${MANUAL_AUTOGEN_DIR}/moc/moc_workerthread.cpp"
        "${MANUAL_AUTOGEN_DIR}/moc/moc_configmanager.cpp"
    )
    set_source_files_properties(${MANUAL_MOC_SOURCES} PROPERTIES GENERATED TRUE)
    list(APPEND PROJECT_SOURCES ${MANUAL_MOC_SOURCES})
else()
    list(APPEND PROJECT_SOURCES mainwindow.ui)
endif()

qt_add_executable(HMI_SerialDashboard
    WIN32 MACOSX_BUNDLE
    ${PROJECT_SOURCES}
)

if(HMI_USE_MANUAL_AUTOGEN)
    set_target_properties(HMI_SerialDashboard PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
    )
    target_include_directories(HMI_SerialDashboard
        PRIVATE
            "${MANUAL_AUTOGEN_DIR}/include"
    )
endif()

qt_add_translations(
    TARGETS HMI_SerialDashboard
    TS_FILES HMI_SerialDashboard_zh_CN.ts
)

set(PROJECT_LIBRARIES
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Xml
)

if(Qt6SerialPort_FOUND)
    list(APPEND PROJECT_LIBRARIES Qt6::SerialPort)
    target_compile_definitions(HMI_SerialDashboard PRIVATE HMI_HAS_QT_SERIALPORT=1)
else()
    target_compile_definitions(HMI_SerialDashboard PRIVATE HMI_HAS_QT_SERIALPORT=0)
endif()

if(Qt6SerialBus_FOUND)
    list(APPEND PROJECT_LIBRARIES Qt6::SerialBus)
    target_compile_definitions(HMI_SerialDashboard PRIVATE HMI_HAS_QT_SERIALBUS=1)
else()
    target_compile_definitions(HMI_SerialDashboard PRIVATE HMI_HAS_QT_SERIALBUS=0)
endif()

if(Qt6Charts_FOUND)
    list(APPEND PROJECT_LIBRARIES Qt6::Charts)
    target_compile_definitions(HMI_SerialDashboard PRIVATE HMI_HAS_QT_CHARTS=1)
else()
    target_compile_definitions(HMI_SerialDashboard PRIVATE HMI_HAS_QT_CHARTS=0)
endif()

target_link_libraries(HMI_SerialDashboard
    PRIVATE
        ${PROJECT_LIBRARIES}
)

enable_testing()

add_executable(hmi_unit_tests
    tests/hmi_unit_tests.cpp
    alarmmanager.cpp
    alarmmanager.h
    configmanager.cpp
    configmanager.h
    datalogger.cpp
    datalogger.h
    dataprocessor.cpp
    dataprocessor.h
)

set_target_properties(hmi_unit_tests PROPERTIES
    AUTOUIC OFF
)

if(HMI_USE_MANUAL_AUTOGEN)
    target_sources(hmi_unit_tests
        PRIVATE
            "${MANUAL_AUTOGEN_DIR}/moc/moc_alarmmanager.cpp"
            "${MANUAL_AUTOGEN_DIR}/moc/moc_configmanager.cpp"
            "${MANUAL_AUTOGEN_DIR}/moc/moc_datalogger.cpp"
            "${MANUAL_AUTOGEN_DIR}/moc/moc_dataprocessor.cpp"
    )
    set_target_properties(hmi_unit_tests PROPERTIES AUTOMOC OFF)
else()
    set_target_properties(hmi_unit_tests PROPERTIES AUTOMOC ON)
endif()

target_link_libraries(hmi_unit_tests
    PRIVATE
        Qt6::Core
        Qt6::Xml
)

target_include_directories(hmi_unit_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(NAME hmi_unit_tests COMMAND hmi_unit_tests)

install(TARGETS HMI_SerialDashboard
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET HMI_SerialDashboard
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
