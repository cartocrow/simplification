#include "smoother.h"

template class StraightGraph<VoidData, VoidData, Inexact>;

void smooth(SmoothGraph* graph, const Number<Inexact> radiusfrac, const int edges_on_semicircle) {

	using Vertex = SmoothGraph::Vertex;
	using Edge = SmoothGraph::Edge;
	using Pt = Point<SmoothGraph::Kernel>;
	using Vec = Vector<SmoothGraph::Kernel>;
	using Num = Number<SmoothGraph::Kernel>;

	// convert radius
	Rectangle<Inexact> bbox = utils::boxOf<Vertex, Inexact>(graph->getVertices());
	Num radius = radiusfrac * std::sqrt(bbox.area());


	int vtx_cnt = graph->getVertexCount();

	std::vector<Num> rads(vtx_cnt, 0);

	// determine smoothing radii
	for (int i = 0; i < vtx_cnt; i++) {

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
		Pt* ctr = std::get_if<Point<Inexact>>(&*is);
		if (ctr == nullptr) {
			// collinear
			continue;
		}

		// NB: angle is always less than 180 degrees by construction
		Num dotp = CGAL::scalar_product(inc_vec, out_vec);
		if (dotp > 1) {
			dotp = 1;
		}
		Num angle = std::acos(dotp);

		int samples = (int)std::floor(edges_on_semicircle * angle / std::numbers::pi);

		if (CGAL::right_turn(v->previous()->getPoint(), pt_v, v->next()->getPoint())) {
			// arc is clockwise
			angle *= -1;
		}

		// move to start of arc
		graph->shiftVertex(v, start);

		// create end of arc
		Edge* edge = graph->splitEdge(out, end)->incoming();

		// introduce intermediate samples

		Vec arm = start - *ctr;
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
		int i = 0;
		while (i < graph->getEdgeCount()) {
			Edge* e = graph->getEdges()[i];

			if (e->getSegment().squared_length() <= 0.00001) {
				graph->mergeVertex(e->getSource());
			}
			i++;
		}
	}

}