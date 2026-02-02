#include "region_set.h"

namespace cartocrow {

	bool ArcRegistration::validate(InputGraph* graph) {

		using Vtx = InputGraph::Vertex;

		// end of the last arc
		Vtx* prev = back().reverse 
			? graph->getBoundaries()[back().boundary]->getFirstEdge()->getSource()
			: graph->getBoundaries()[back().boundary]->getLastEdge()->getTarget();
		for (Arc a : *this) {

			Vtx* start = a.reverse
				? graph->getBoundaries()[a.boundary]->getLastEdge()->getTarget()
				: graph->getBoundaries()[a.boundary]->getFirstEdge()->getSource();
			// start of this arc: does it match the end of the previous?
			if (prev != start) {
				return false;
			}

			// end of last ast
			prev = a.reverse
				? graph->getBoundaries()[a.boundary]->getFirstEdge()->getSource()
				: graph->getBoundaries()[a.boundary]->getLastEdge()->getTarget();
		}

		return true;
	}

	InputGraph* constructGraphAndRegisterBoundaries(RegionSet<Exact>& rs, const int depth) {

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
			InputGraph::Vertex* v = pqt.findElement(pt, 0.00001);
			if (v == nullptr) {
				v = graph->addVertex(pt);
				pqt.insert(*v);
			}
			return v;
			};

		for (Region<Exact>& r : rs) {
			for (Polygon<Exact> poly : r.rings) {
				InputGraph::Vertex* prev = nullptr;
				InputGraph::Vertex* first = nullptr;
				for (Point<Exact> p : poly.vertices()) {
					InputGraph::Vertex* curr = findVtx(p);

					if (prev == nullptr) {
						first = curr;
					}
					else if (prev != curr && !prev->isNeighborOf(curr)) {
						graph->addEdge(prev, curr);
					}

					prev = curr;
				}
				if (prev != first && !prev->isNeighborOf(first)) {
					graph->addEdge(prev, first);
				}
			}
		}

		// register boundaries
		graph->orient();

		for (Region<Exact>& r : rs) {

			for (Polygon<Exact> poly : r.rings) {

				ArcRegistration reg;

				InputGraph::Vertex* prev = nullptr;
				InputGraph::Vertex* first = nullptr;
				for (Point<Exact> p : poly.vertices()) {
					InputGraph::Vertex* curr = findVtx(p);

					if (prev == nullptr) {
						first = curr;
					}
					else if (prev != curr) {
						InputGraph::Edge* e = prev->edgeTo(curr);

						int bi = e->getBoundary()->graphIndex();
						bool brev = e->getSource() == curr;

						if (reg.empty() || reg.back().boundary != bi || reg.back().reverse != brev) {
							reg.push_back(Arc(bi, brev));
						}
					}

					prev = curr;
				}

				if (prev != first) {
					InputGraph::Edge* e = prev->edgeTo(first);

					int bi = e->getBoundary()->graphIndex();
					bool brev = e->getSource() == first;
					if (reg.empty() || reg.back().boundary != bi || reg.back().reverse != brev) {
						reg.push_back(Arc(bi, brev));
					}
				}

				// cleanup, in case we didnt start at a boundary start...
				while (reg.size() > 1 
					&& reg.back().boundary == reg.front().boundary 
					&& reg.back().reverse == reg.front().reverse) {
					reg.pop_back();
				}

				r.arcs.push_back(reg);

				assert(reg.validate(graph));
			}
		}

		return graph;
	}

	

}