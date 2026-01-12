#include "region_set.h"

namespace cartocrow {

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

		Rectangle<MyKernel> box = utils::boxOf<MyKernel>(points);

		// construct the graph
		PointQuadTree<InputGraph::Vertex, Exact> pqt(box, depth);

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

						if (reg.empty() || reg.back().boundary != e->getBoundary()->graphIndex()) {
							reg.push_back(Arc(e->getBoundary()->graphIndex(), e->getSource() == curr));
						}
					}

					prev = curr;
				}

				if (prev != first) {
					InputGraph::Edge* e = prev->edgeTo(first);

					if (reg.empty() || reg.back().boundary != e->getBoundary()->graphIndex()) {
						reg.push_back(Arc(e->getBoundary()->graphIndex(), e->getSource() == first));
					}
				}

				r.arcs.push_back(reg);

			}
		}

		return graph;

	}

}