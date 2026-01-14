#include "smoother.h"

template class StraightGraph<std::monostate, std::monostate, Inexact>;

void smooth(SmoothGraph* graph, const Number<Inexact> radiusfrac, const int edges_on_semicircle, std::optional<std::function<void(std::string, int, int)>> progress) {

	using Vertex = SmoothGraph::Vertex;
	using Edge = SmoothGraph::Edge;
	using Pt = Point<SmoothGraph::Kernel>;
	using Vec = Vector<SmoothGraph::Kernel>;
	using Num = Number<SmoothGraph::Kernel>;

	// convert radius
	Num sqrLongest = 0;
	for (Edge* e : graph->getEdges()) {
		Num sqrL = e->getSegment().squared_length();
		if (sqrL > sqrLongest) {
			sqrLongest = sqrL;
		}
	}
	Num radius = radiusfrac * std::sqrt(sqrLongest) / 2.0;

	int vtx_cnt = graph->getVertexCount();

	std::vector<Num> rads(vtx_cnt, 0);

	// determine smoothing radii
	for (int i = 0; i < vtx_cnt; i++) {
		if (progress.has_value()) {
			(*progress)("Determining radii", i, vtx_cnt);
		}

		Vertex* v = graph->getVertices()[i];

		if (v->degree() != 2) continue;

		Edge* inc = v->incoming();
		Edge* out = v->outgoing();

		Num in_len = std::sqrt(inc->getSegment().squared_length());
		Num out_len = std::sqrt(out->getSegment().squared_length());

		Num v_rad = std::min(radius, std::min(in_len, out_len) / 2.0);

		rads[i] = v_rad;
	}

	// apply smoothing
	for (int i = 0; i < vtx_cnt; i++) {
		if (progress.has_value()) {
			(*progress)("Applying smoothing", i, vtx_cnt);
		}

		Vertex* v = graph->getVertices()[i];
		if (v->degree() != 2) continue;


		Num v_rad = rads[i];

		Edge* inc = v->incoming();
		Edge* out = v->outgoing();

		Num in_len = std::sqrt(inc->getSegment().squared_length());
		Num out_len = std::sqrt(out->getSegment().squared_length());

		Pt pt_v = v->getPoint();

		Vec inc_vec = v->previous()->getPoint() - pt_v;
		inc_vec /= in_len;
		Pt start = pt_v + v_rad * inc_vec;

		Vec out_vec = v->next()->getPoint() - pt_v;
		out_vec /= out_len;
		Pt end = pt_v + v_rad * out_vec;

		auto is = CGAL::intersection(Line<Inexact>(start, inc_vec.perpendicular(CGAL::COUNTERCLOCKWISE)), Line<Inexact>(end, out_vec.perpendicular(CGAL::COUNTERCLOCKWISE)));
		if (!is.has_value()) {
			// collinear
			continue;
		}
		Pt* ctr = std::get_if<Point<Inexact>>(&*is);
		if (ctr == nullptr) {
			// collinear
			continue;
		}

		bool ccw = CGAL::right_turn(v->previous()->getPoint(), pt_v, v->next()->getPoint());

		// move to start of arc
		graph->shiftVertex(v, start);

		// create end of arc
		Edge* edge = graph->splitEdge(out, end)->incoming();

		// introduce intermediate samples

		Vec arm = start - *ctr;
		Vec endarm = end - *ctr;

		// NB: angle is always less than 180 degrees by construction
		Num dotp = CGAL::scalar_product(arm, endarm) / std::sqrt(arm.squared_length() * endarm.squared_length());
		if (dotp > 1) {
			dotp = 1;
		}
		Num angle = std::acos(dotp);

		int samples = (int)std::floor(edges_on_semicircle * angle / std::numbers::pi);

		if (ccw) {
			// arc is clockwise
			angle *= -1;
		}

		CGAL::Aff_transformation_2<Inexact> rot(CGAL::ROTATION, std::sin(angle / samples), std::cos(angle / samples));

		while (samples > 1) {
			arm = arm.transform(rot);
			Pt pt = *ctr + arm;
			edge = graph->splitEdge(edge, pt)->outgoing();
			samples--;
		}
	}

	// erase zero-length edges (if two adjacent vertices are both constrained by their shared edge
	{
		int init_edge_count = graph->getEdgeCount();
		int i = 0;
		int ii = 0;
		// invariant: all edges with index < i have sufficient length, except for indices in "revisit"
		std::vector<int> revisit;
		while (i < graph->getEdgeCount()) {
			if (progress.has_value()) {
				(*progress)("Cleaning up geometry", ii, init_edge_count);
				// number of edges processed
				ii++;
			}

			int next_i;
			if (revisit.empty()) {
				next_i = i;
			}
			else {
				next_i = revisit.back();
				revisit.pop_back();
			}

			Edge* e = graph->getEdges()[next_i];

			if (e->getSegment().squared_length() <= 0.00001) {
				// remove unless between degree-3 vertices
				if (e->getSource()->degree() == 2) {
					graph->mergeVertex(e->getSource());
					// this removes e itself
				}
				else if (e->getSource()->degree() == 2) {
					Edge* nxt = e->next();
					if (nxt->graphIndex() < i) {
						revisit.push_back(nxt->graphIndex());
					}
					graph->mergeVertex(e->getTarget());
					// this removes the next edge					
					if (next_i == i)
						i++;
				}
				else {
					// cannot remove, just go to next edge
					if (next_i == i)
						i++;
				}
			}
			else {
				// keep it
				if (next_i == i)
					i++;
			}
		}
	}

}