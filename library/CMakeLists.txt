set(HEADERS
	common.h
	edge_collapse.h
	edge_collapse.hpp
	edge_moves.h
	edge_moves.hpp
	historic_graph.h
	historic_graph.hpp	
	indexed_priority_queue.h
	indexed_priority_queue.hpp
	modifiable_graph.h
	point_quad_tree.h
	point_quad_tree.hpp
	segment_quad_tree.h
	segment_quad_tree.hpp
	straight_graph.h
	straight_graph.hpp
	utils.h
	vertex_removal.h	
	vertex_removal.hpp	
)

install(FILES ${HEADERS} DESTINATION ${CARTOCROW_INSTALL_DIR}/simplification)