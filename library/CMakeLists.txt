set(HEADERS
	common.h
	edge_collapse.h
	edge_collapse.hpp
	edge_moves.h
	edge_moves.hpp
	edge_quad_tree.h
	orientation_restriction.h
	orientation_restriction.hpp
	precision_safe_tests.h
	utils.h
	vertex_quad_tree.h
	vertex_removal.h	
	vertex_removal.hpp	
)

install(FILES ${HEADERS} DESTINATION ${CARTOCROW_INSTALL_DIR}/simplification)