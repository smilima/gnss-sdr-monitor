find_package(Boost REQUIRED COMPONENTS system)
if(NOT Boost_FOUND)
     message(FATAL_ERROR "Fatal error: Boost required.")
endif()

find_package(Protobuf REQUIRED)
if(${Protobuf_VERSION} VERSION_LESS "3.0.0")
     message(FATAL_ERROR "Fatal error: Protocol Buffers > v3.0.0 required.")
endif()
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${CMAKE_SOURCE_DIR}/src/protobuf/gnss_synchro.proto)
set_property(SOURCE ${PROTO_SRCS} PROPERTY SKIP_AUTOGEN ON)
set_property(SOURCE ${PROTO_HDRS} PROPERTY SKIP_AUTOGEN ON)

protobuf_generate_cpp(PROTO_SRCS2 PROTO_HDRS2 ${CMAKE_SOURCE_DIR}/src/protobuf/monitor_pvt.proto)
set_property(SOURCE ${PROTO_SRCS2} PROPERTY SKIP_AUTOGEN ON)
set_property(SOURCE ${PROTO_HDRS2} PROPERTY SKIP_AUTOGEN ON)

protobuf_generate_cpp(PROTO_SRCS3 PROTO_HDRS3 ${CMAKE_SOURCE_DIR}/src/protobuf/gps_ephemeris.proto)
set_property(SOURCE ${PROTO_SRCS3} PROPERTY SKIP_AUTOGEN ON)
set_property(SOURCE ${PROTO_HDRS3} PROPERTY SKIP_AUTOGEN ON)

find_package(Qt5 COMPONENTS Core Gui Widgets Network PrintSupport Quick QuickWidgets Positioning Charts REQUIRED)
find_package(Qt5 REQUIRED COMPONENTS Core)
if(NOT Qt5_FOUND)
     message(FATAL_ERROR "Fatal error: Qt5 required.")
endif(NOT Qt5_FOUND)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(QT5_LIBRARIES
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Network
    Qt5::PrintSupport
    Qt5::Quick
    Qt5::QuickWidgets
    Qt5::Positioning
    Qt5::Charts
)

set(TARGET ${CMAKE_PROJECT_NAME})

set(HEADERS
    altitude_widget.h
    channel_table_model.h
    cn0_delegate.h
    constellation_delegate.h
    doppler_delegate.h
    dop_widget.h
    ephemeris_widget.h
    led_delegate.h
    main_window.h
    monitor_pvt_wrapper.h
    gps_ephemeris_wrapper.h
    preferences_dialog.h
    skyplot_widget.h
    telecommand_widget.h
    telnet_manager.h
    protobuf/gnss_synchro.proto
    protobuf/monitor_pvt.proto
    protobuf/gps_ephemeris.proto
)

set(SOURCES
    channel_table_model.cpp
    cn0_delegate.cpp
    constellation_delegate.cpp
    doppler_delegate.cpp
    ephemeris_widget.cpp
    led_delegate.cpp
    main.cpp
    main_window.cpp
    monitor_pvt_wrapper.cpp
    gps_ephemeris_wrapper.cpp
    preferences_dialog.cpp
    telecommand_widget.cpp
    telnet_manager.cpp
    altitude_widget.cpp
    dop_widget.cpp
    skyplot_widget.cpp
    ${PROTO_SRCS}
    ${PROTO_SRCS2}
    ${PROTO_SRCS3}
)

set(UI_SOURCES
    main_window.ui
    preferences_dialog.ui
    telecommand_widget.ui
)

set(RESOURCES
    resources.qrc
    ../screenshots/gnss-sdr-monitor-skyplot.png
)

add_executable(${TARGET} ${HEADERS} ${SOURCES} ${UI_SOURCES} ${RESOURCES}
     )


target_link_libraries(gnss-sdr-monitor PRIVATE Qt5::Core)
target_link_libraries(${TARGET} PUBLIC ${QT5_LIBRARIES} Boost::boost protobuf::libprotobuf)

install(TARGETS ${TARGET} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
