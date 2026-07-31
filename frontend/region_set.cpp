#include "region_set.h"

#include "library/utils.h"
#include "library/vertex_quad_tree.h"

using namespace cartocrow::simplification;

namespace cartocrow {

	bool ArcRegistration::validate(InputGraph* graph) {

		using Vtx = InputGraph::Vertex;

		// end of the last arc
		//Vtx* prev = back().reverse 
		//	? graph->getBoundaries()[back().boundary]->getFirstEdge()->getSource()
		//	: graph->getBoundaries()[back().boundary]->getLastEdge()->getTarget();
		//for (Arc a : *this) {

		//	Vtx* start = a.reverse
		//		? graph->getBoundaries()[a.boundary]->getLastEdge()->getTarget()
		//		: graph->getBoundaries()[a.boundary]->getFirstEdge()->getSource();
		//	// start of this arc: does it match the end of the previous?
		//	if (prev != start) {
		//		return false;
		//	}

		//	// end of last ast
		//	prev = a.reverse
		//		? graph->getBoundaries()[a.boundary]->getFirstEdge()->getSource()
		//		: graph->getBoundaries()[a.boundary]->getLastEdge()->getTarget();
		//}

		return true;
	}

	InputGraph* constructGraphAndRegisterBoundaries(RegionSet<Exact>& rs, const int depth) {

		using Vertex_handle = InputGraph::Vertex_handle;
		using Edge_handle = InputGraph::Edge_handle;

		InputGraph* graph = new InputGraph();

		// compute a bounding box
		std::vector<Point<Exact>> points;

		for (Region<Exact> r : rs) {

			for (Polygon<Exact> poly : r.rings) {
				for (Point<Exact> p : poly.vertices()) {
					points.push_back(p);
				}
			}
		}

		Rectangle<Exact> box = utils::boxOf<Exact>(points);

		// construct the graph
		VertexQuadTree<InputGraph> pqt(box, depth);

		auto findVtx = [&pqt, &graph](Point<Exact> pt) {
			Vertex_handle v = pqt.findElement(pt, M_EPSILON);
			if (v == nullptr) {
				v = graph->add_vertex(pt);
				pqt.insert(v);
			}
			return v;
			};

		for (Region<Exact>& r : rs) {
			for (Polygon<Exact> poly : r.rings) {
				Vertex_handle prev = nullptr;
				Vertex_handle first = nullptr;
				for (Point<Exact> p : poly.vertices()) {
					Vertex_handle curr = findVtx(p);

					if (prev == nullptr) {
						first = curr;
					}
					else if (prev != curr && !prev->is_neighbor_of(curr)) {
						graph->add_edge(prev, curr);
					}

					prev = curr;
				}
				if (prev != first && !prev->is_neighbor_of(first)) {
					graph->add_edge(prev, first);
				}
			}
		}

		// register boundaries

		graph->initialize();

		for (Region<Exact>& r : rs) {

			for (Polygon<Exact> poly : r.rings) {

				r.arcs.emplace_back();
				ArcRegistration& reg = r.arcs.back();

				Vertex_handle prev = nullptr;
				Vertex_handle first = nullptr;
				for (Point<Exact> p : poly.vertices()) {
					Vertex_handle curr = findVtx(p);

					if (prev == nullptr) {
						first = curr;
					}
					else if (prev != curr) {
						Edge_handle e = prev->find_edge_to(curr);

						int bi = e->path()->graph_index();
						bool brev = e->source() == curr;

						if (reg.empty() || reg.back().boundary != bi || reg.back().reverse != brev) {
							reg.emplace_back(bi, brev);
						}
					}

					prev = curr;
				}

				if (prev != first) {
					Edge_handle e = prev->find_edge_to(first);

					int bi = e->path()->graph_index();
					bool brev = e->source() == first;
					if (reg.empty() || reg.back().boundary != bi || reg.back().reverse != brev) {
						reg.emplace_back(bi, brev);
					}
				}

				// cleanup, in case we didnt start at a boundary start...
				while (reg.size() > 1
					&& reg.back().boundary == reg.front().boundary
					&& reg.back().reverse == reg.front().reverse) {
					reg.pop_back();
				}

				assert(reg.validate(graph));
			}
		}

		return graph;
	}



}