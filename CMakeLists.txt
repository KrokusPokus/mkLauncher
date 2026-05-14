cmake_minimum_required(VERSION 3.19)
project(mkLauncher LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Concurrent LinguistTools Network)

qt_add_translations(mkLauncher
    TS_FILES i18n/mkLauncher_de.ts
    SOURCES ${PROJECT_SOURCES}
)

if(UNIX)
    find_package(ZLIB REQUIRED)
else()
    include(FetchContent)

    FetchContent_Declare(
    zlib
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG        v1.3.1 # Oder eine andere stabile Version
    )

    # Schaltet ZLIB-eigene Tests und Installation aus, damit es schneller geht
    set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(zlib)
endif()

qt_standard_project_setup()

# Diese Zeile ist entscheidend, damit Qt die Ressourcen kompiliert!
set(CMAKE_AUTORCC ON)

qt_add_executable(mkLauncher
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.h mainwindow.cpp
    resources.qrc
    searchworker.h searchworker.cpp
    helpers.h helpers.cpp
    settingsmanager.h settingsmanager.cpp
    filepropertiesdialog.h filepropertiesdialog.cpp

)

target_link_libraries(mkLauncher
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt6::Concurrent
        Qt6::Network
        $<$<PLATFORM_ID:Windows>:zlibstatic>
        $<$<PLATFORM_ID:Linux>:ZLIB::ZLIB>
)

install(TARGETS mkLauncher
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET mkLauncher
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
)
