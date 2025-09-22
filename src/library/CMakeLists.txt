set(SOURCES

)
set(HEADERS
	utils.h
	indexed_priority_queue.h
	straight_graph.h
	point_quad_tree.h
	segment_quad_tree.h

	modifiable_graph.h
	oblivious_graph.h
	historic_graph.h
	
	vertex_removal.h
	vw_simplification.h
	
	edge_collapse.h
	ksbb_simplification.h
)

#add_library(simplification ${SOURCES})
#target_link_libraries(simplification
#	PUBLIC 
#    CartoCrow::core
#)

install(FILES ${HEADERS} DESTINATION ${CARTOCROW_INSTALL_DIR}/simplification)