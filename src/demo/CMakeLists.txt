set(SOURCES
    simplification.cpp
)

add_executable(simplification_demo ${SOURCES})

target_link_libraries(
    simplification_demo
    PRIVATE
    cartocrow::core
    cartocrow::renderer
    CGAL::CGAL
    Qt5::Widgets
)

install(TARGETS simplification_demo DESTINATION ${INSTALL_BINARY_DIR})