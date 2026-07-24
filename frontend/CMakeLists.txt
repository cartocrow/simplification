set(SOURCES
    gui.cpp
    main.cpp
    commandline.cpp
    cmd_arguments.cpp
    persistent_settings.cpp
    read_graph_gdal.cpp
    region_set.cpp
    simplification_algorithm.cpp
    smoother.cpp
    vw.cpp
    ksbb.cpp
    restrictor.cpp
    bmrs.cpp
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