set(HEADERS
	common.h
	edge_collapse.h
	edge_collapse.hpp
	edge_moves.h
	edge_moves.hpp
	edge_quad_tree.h
	historic_graph.h
	historic_graph.hpp	
	modifiable_graph.h
	orientation_restriction.h
	orientation_restriction.hpp
	point_quad_tree.h
	point_quad_tree.hpp
	straight_graph.h
	straight_graph.hpp
	utils.h
	vertex_quad_tree.h
	vertex_removal.h	
	vertex_removal.hpp	
)

install(FILES ${HEADERS} DESTINATION ${CARTOCROW_INSTALL_DIR}/simplification)