set(SOURCES
    main.cpp
)
add_executable(cgal_api_test ${SOURCES})
target_link_libraries(
    cgal_api_test
    PRIVATE
    cartocrow::core
    cartocrow::data_structures
    cartocrow::renderer;
    CGAL::CGAL
)
install(TARGETS cgal_api_test DESTINATION ${INSTALL_BINARY_DIR})