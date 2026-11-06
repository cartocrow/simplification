set(HEADERS
	common.h
	edge_collapse.h
	edge_moves.h
	orientation_restriction.h
	orientation_restriction.hpp
	path_vertex_removal.h	
	precision_safe_tests.h
	utils.h
	vertex_removal.h	
)

install(FILES ${HEADERS} DESTINATION ${CARTOCROW_INSTALL_DIR}/simplification)