set(SOURCES
    debug.cpp
    gui.cpp
    main.cpp
)
add_executable(simplification ${SOURCES})
target_link_libraries(
    simplification
    PRIVATE
    cartocrow::core
    cartocrow::renderer
    CGAL::CGAL
    Qt5::Widgets
)
install(TARGETS simplification DESTINATION ${INSTALL_BINARY_DIR})