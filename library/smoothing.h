#pragma once

#include <cartocrow/core/core.h>
#include <cartocrow/data_structures/graph_map_2.h>

#include <functional>
#include <numbers>
#include <optional>
#include <string>

namespace cartocrow::simplification {

/// For graphs with an inexact kernel, such as the output of a simplification.
/// Replaces the corners at degree-2 vertices of the graph by circular arcs, approximated by line segments.
/// The radius at a vertex is half the length of its shorter incident edge, but at most radiusPercent percent of the
/// largest such radius. A semicircle is approximated by samples_per_semicircle edges.
/// If progress is set, it is called with the label of the current phase, the progress in that phase and its maximum.
template <class Graph>
void smooth(Graph& graph, int radiusPercent, int samples_per_semicircle,
            std::optional<std::function<void(std::string, int, int)>> progress = std::nullopt) {
	std::string phaseLabel;
	int phaseMax = 0;
	auto phase = [&](const std::string& label, int max) {
		phaseLabel = label;
		phaseMax = max;
		if (progress) (*progress)(phaseLabel, 0, phaseMax);
	};
	auto update = [&](int current) {
		if (progress) (*progress)(phaseLabel, current, phaseMax);
	};

	using Kernel = typename Graph::Kernel;
	using Vertex_handle = typename Graph::Vertex_handle;
	using Edge_handle = typename Graph::Edge_handle;
	using Pt = Point<Kernel>;
	using Vec = Vector<Kernel>;
	using Num = Number<Kernel>;

	size_t vtx_cnt = graph.number_of_vertices();

	Graph_static_vertex_map<Graph, Num> rads(graph, 0.0);

	// determine smoothing radii
	Num max_rad = 0;
	phase("Determining radii", vtx_cnt);
	for (size_t i = 0; i < vtx_cnt; ++i) {
		update(i);

		Vertex_handle v = graph.vertex(i);

		if (v->degree() != 2) continue;

		Edge_handle inc = v->incoming();
		Edge_handle out = v->outgoing();

		Num in_len = std::sqrt(inc->curve().squared_length());
		Num out_len = std::sqrt(out->curve().squared_length());

		Num v_rad = std::min(in_len, out_len) / 2.0;
		if (v_rad > max_rad) max_rad = v_rad;

		rads[v] = v_rad;
	}

	// impose max
	max_rad *= (radiusPercent / 100.0);

	// apply smoothing
	phase("Applying smoothing", vtx_cnt);
	for (int i = 0; i < vtx_cnt; i++) {
			update(i);

		Vertex_handle v = graph.vertex(i);
		if (v->degree() != 2) continue;

		Num v_rad = std::min(rads[v], max_rad);

		Edge_handle inc = v->incoming();
		Edge_handle out = v->outgoing();

		Num in_len = std::sqrt(inc->curve().squared_length());
		Num out_len = std::sqrt(out->curve().squared_length());

		Pt pt_v = v->point();

		Vec inc_vec = v->prev()->point() - pt_v;
		inc_vec /= in_len;
		Pt start = pt_v + v_rad * inc_vec;

		Vec out_vec = v->next()->point() - pt_v;
		out_vec /= out_len;
		Pt end = pt_v + v_rad * out_vec;

		auto is = CGAL::intersection(Line<Kernel>(start, inc_vec.perpendicular(CGAL::COUNTERCLOCKWISE)), Line<Kernel>(end, out_vec.perpendicular(CGAL::COUNTERCLOCKWISE)));
		if (!is.has_value()) {
			// collinear
			continue;
		}
		Pt* ctr = std::get_if<Pt>(&*is);
		if (ctr == nullptr) {
			// collinear
			continue;
		}

		bool ccw = CGAL::right_turn(v->prev()->point(), pt_v, v->next()->point());

		// move to start of arc
		graph.move_vertex(v, start);

		// create end of arc
		Edge_handle edge = graph.subdivide_edge(out, end)->incoming();

		// introduce intermediate samples

		Vec arm = start - *ctr;
		Vec endarm = end - *ctr;

		// NB: angle is always less than 180 degrees by construction
		Num dotp = CGAL::scalar_product(arm, endarm) / std::sqrt(arm.squared_length() * endarm.squared_length());
		if (dotp > 1) {
			dotp = 1;
		}
		Num angle = std::acos(dotp);

		int samples = (int)std::floor(samples_per_semicircle * angle / std::numbers::pi);

		if (ccw) {
			// arc is clockwise
			angle *= -1;
		}

		CGAL::Aff_transformation_2<Kernel> rot(CGAL::ROTATION, std::sin(angle / samples), std::cos(angle / samples));

		while (samples > 1) {
			arm = arm.transform(rot);
			Pt pt = *ctr + arm;
			edge = graph.subdivide_edge(edge, pt)->outgoing();
			samples--;
		}
	}

	// erase zero-length edges (if two adjacent vertices are both constrained by their shared edge)
	{
		int init_edge_count = graph.number_of_edges();
		int i = 0;
		int ii = 0;
		// invariant: all edges with index < i have sufficient length, except for indices in "revisit"
		std::vector<int> revisit;

		phase("Cleaning up geometry", init_edge_count);
		while (i < graph.number_of_edges()) {
			update(ii++);

			int next_i;
			if (revisit.empty()) {
				next_i = i;
			}
			else {
				next_i = revisit.back();
				revisit.pop_back();
			}

			Edge_handle e = graph.edge(next_i);

			if (e->curve().squared_length() <= 0.00001) {
				// remove unless between degree-3 vertices
				if (e->source()->degree() == 2) {
					graph.merge_vertex(e->source());
					// this removes e itself
				}
				else if (e->target()->degree() == 2) {
					Edge_handle nxt = e->next();
					if (nxt->graph_index() < i) {
						revisit.push_back(nxt->graph_index());
					}
					graph.merge_vertex(e->target());
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

} // namespace cartocrow::simplification
