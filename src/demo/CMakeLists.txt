# debugging
#set(SOURCES
#    debug.cpp
#)
#add_executable(simplification_debug ${SOURCES})
#target_link_libraries(
#    simplification_debug
#    PRIVATE
#    cartocrow::core
#    CGAL::CGAL
#)
#install(TARGETS simplification_debug DESTINATION ${INSTALL_BINARY_DIR})

# GUI program
set(SOURCES
    gui.cpp
)
add_executable(simplification_gui ${SOURCES})
target_link_libraries(
    simplification_gui
    PRIVATE
    cartocrow::core
    cartocrow::renderer
    CGAL::CGAL
    Qt5::Widgets
)
install(TARGETS simplification_gui DESTINATION ${INSTALL_BINARY_DIR})