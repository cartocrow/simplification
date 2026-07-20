set(SOURCES
    gui.cpp
    main.cpp
    persistent_settings.cpp
    read_graph_gdal.cpp
    region_set.cpp
    simplification_algorithm.cpp
    smoother.cpp
    vw.cpp
)
add_executable(simplification ${SOURCES})
target_link_libraries(
    simplification
    PRIVATE
    cartocrow::core
    cartocrow::data_structures
    cartocrow::reader
    cartocrow::renderer
    CGAL::CGAL
    Qt5::Widgets
    GDAL::GDAL
)
install(TARGETS simplification DESTINATION ${INSTALL_BINARY_DIR})