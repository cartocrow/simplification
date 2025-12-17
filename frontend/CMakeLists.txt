set(SOURCES
    debug.cpp
    gui.cpp
    ksbb.cpp
    main.cpp
    simplification_algorithm.cpp
    vw.cpp
)
add_executable(simplification ${SOURCES})
target_link_libraries(
    simplification
    PRIVATE
    cartocrow::core
    cartocrow::reader
    cartocrow::renderer
    CGAL::CGAL
    Qt5::Widgets
)
install(TARGETS simplification DESTINATION ${INSTALL_BINARY_DIR})