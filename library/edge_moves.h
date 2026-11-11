#pragma once

#include "common.h"

namespace cartocrow::simplification::em {

	inline Debug<-1> debug; // disabled debugging output

	template<typename Edge_handle>
	inline Edge_handle safe_next(Edge_handle e) {
		if (e->target()->degree() == 2) {
			return e->next();
		}
		else {
			return nullptr;
		}
	}

	template<typename Edge_handle>
	inline Edge_handle safe_prev(Edge_handle e) {
		if (e->source()->degree() == 2) {
			return e->prev();
		}
		else {
			return nullptr;
		}
	}

	enum VertexType {
		// unmovable vertex (degree != 2 and != 3)
		UNMOVABLE,

		// degree 2, move shortens adjacent edge
		DEG_TWO_SUPPORT,

		// degree 2, move lengthens adjacent edge
		DEG_TWO_NO_SUPPORT,

		// degree 3, the two other adjacent edges are aligned, so vertex can just shift: they define identical tracks
		DEG_THREE_SMOOTH,

		// degree 3, needs extra vertex of deg-2 that stays at the old location, track defined by immediate adjacent edge
		DEG_THREE_SUPPORT,

		// degree 3, needs extra vertex of deg-2 that shifts, track defined by immediate adjacent edge
		DEG_THREE_NO_SUPPORT,

		// degree 3, as previous case, but the immediate adjacent edge is aligned, track defined by other adjacent edge
		DEG_THREE_ALIGNED
	};

	template<class K, typename EdgeHandle>
	struct BaseMove {

		virtual ~BaseMove() {} // necessary for inheritance to work...

		using Kernel = K;
		using Edge_handle = EdgeHandle;

		Edge_handle edge;

		Number<Kernel> cost;
		int queue_index = -1;

		bool blocked_by_degzero = false;
		std::vector<Edge_handle> blocked_by;

		virtual bool is_single() const = 0;

		bool is_blocked() const {
			return blocked_by_degzero || !blocked_by.empty();
		}
	};

	template<class K, typename EdgeHandle, typename VertexHandle>
	struct ComboMove;

	template<class K, typename EdgeHandle, typename VertexHandle>
	struct SingleMove : public BaseMove<K, EdgeHandle> {

		using Kernel = K;
		using Edge_handle = EdgeHandle;
		using Vertex_handle = VertexHandle;

		using Combo = ComboMove<K, EdgeHandle, VertexHandle>;

		SingleMove(const bool left) : left(left) {}

		const bool left;
		enum VertexType src_type, tar_type;
		bool remove_self, remove_previous, remove_next;
		bool merge_previous, merge_next;
		Polygon<Kernel> swept;
		bool contract_merges_highdegrees;
		int waiting_index = -1;

		bool is_single() const override {
			return true;
		}

		bool is_blocked_by(Edge_handle candidate) const {
			return is_blocked_by(candidate, swept, true);
		}

		int supported_count() {
			int support = 0;
			switch (src_type) {
			case DEG_TWO_SUPPORT:
			case DEG_THREE_SUPPORT:
			case DEG_THREE_ALIGNED:
				support++;
				break;
			default:
				break;
			}
			switch (tar_type) {
			case DEG_TWO_SUPPORT:
			case DEG_THREE_SUPPORT:
			case DEG_THREE_ALIGNED:
				support++;
				break;
			default:
				break;
			}
			return support;
		}

		bool is_blocked_by(Edge_handle candidate, const Polygon<Kernel>& swept_polygon, const bool contract) const {
			if (this->edge->common_vertex(candidate) != nullptr) {
				// shares a vertex with the moving edge, part of configuration
				return false;
			}

			Edge_handle prev = this->edge->prev(left);
			Vertex_handle exclude = prev->common_vertex(candidate);

			Edge_handle next = this->edge->next(!left);
			Vertex_handle next_exclude = next->common_vertex(candidate);

			if (next_exclude != nullptr) {
				if (exclude != nullptr) {
					assert(contract);
					// convex quad, just make sure candidate is not aligned with edge itself
					Vector<Kernel> dir_edge(this->edge->curve());
					Vector<Kernel> dir_cand(candidate->curve());
					return safe_test::aligned_or_opposite_vectors(dir_edge, dir_cand);
				}
				else {
					exclude = next_exclude;
					// exclude comes from next, make sure it doesnt align with edge itself on contraction
					if (contract && this->remove_next) {
						// moving left: edge to next makes a left turn. test if next to candidate also makes a left turn
						// moving right: edge to next makes a right turn, test if next to candidate also makes a right turn
						if (left == CGAL::left_turn(this->edge->target()->point(), exclude->point(), candidate->other(exclude)->point())) {
							// same turns
							Vector<Kernel> dir_edge(this->edge->curve());
							Vector<Kernel> dir_cand(candidate->curve());
							return safe_test::aligned_or_opposite_vectors(dir_edge, dir_cand);
						}
					}
				}
			}
			else if (contract && exclude != nullptr && this->remove_previous) {
				// exclude comes from prev, make sure it doesnt align with edge itself on contraction
					// moving left: edge to prev makes a right turn. test if prev to candidate also makes a right turn
					// moving right: edge to prev makes a left turn, test if prev to candidate also makes a left turn
				if (left == CGAL::right_turn(this->edge->target()->point(), exclude->point(), candidate->other(exclude)->point())) {
					// same turns
					Vector<Kernel> dir_edge(this->edge->curve());
					Vector<Kernel> dir_cand(candidate->curve());
					return safe_test::aligned_or_opposite_vectors(dir_edge, dir_cand);
				}
			}

			if (exclude != candidate->source() && safe_test::convex_contains(candidate->source()->point(), swept_polygon)) {
				return true;
			}

			if (exclude != candidate->target() && safe_test::convex_contains(candidate->target()->point(), swept_polygon)) {
				return true;
			}

			const Segment<Kernel> cc = candidate->curve();
			for (const Segment<Kernel> e : swept_polygon.edges()) {
				auto is = CGAL::intersection(e, cc);
				if (is) {
					if (exclude != nullptr && std::holds_alternative<Point<Kernel>>(is.value())) {
						// make sure its not the endpoint (we could have just touched the corner of swept)
						if (!safe_test::same_point(exclude->point(), std::get<Point<Kernel>>(is.value()))) {
							return true;
						}
					}
					else {
						// anything beyond a point must be a proper intersect
						return true;
					}
				}
			}

			// no intersections were found, not blocking
			return false;
		}

		template<class DT>
		bool can_compensate_for(const SingleMove& contract, const int reduc, const Number<Kernel> area, DT& data_traits) const {
			if (this->is_blocked()) {
				return false;
			}

			if (!movable()) {
				// cannot move
				return false;
			}

			Number<Kernel> swept = swept_area();
			if (area > swept) {
				// too small
				return false;
			}
			else if (swept == area) {
				// contract
				if (!this->contract_merges_highdegrees) {
					// not allowed to contract
					return false;
				}

				if (reduc + decrease_on_contract() <= 0) {
					// no progress
					return false;
				}
			}
			else {
				// move only
				if (increase_on_move() >= reduc) {
					// no progress
					return false;
				}
			}

			if (contract.edge->common_vertex(this->edge) != nullptr) {
				// same or neighboring edges
				return false;
			}

			Edge_handle next = safe_next(this->edge);
			if (next != nullptr && next->common_vertex(contract.edge) != nullptr) {
				// sharing next edge (== prev edge of contract)
				if (contract.remove_previous) {
					// but contract is relying on its removal
					return false;
				}
				else {
					Combo* combo = data_traits.data(next).combo;
					if (combo->prev_move == this) {
						// leave to ComboMove
						return false;
					}

					if (area == swept && remove_next) {
						// this move is to remove the shared edge if compensating but the contracting move will lengthen this shared edge
						return false;
					}
				}
			}

			Edge_handle prev = safe_prev(this->edge);
			if (prev != nullptr && prev->common_vertex(contract.edge) != nullptr) {
				// sharing previous edge (== next edge of contract)
				if (contract.remove_next) {
					// but contract is relying on its removal
					return false;
				}
				else {
					Combo* combo = data_traits.data(prev).combo;
					if (combo->next_move == this) {
						// leave to ComboMove
						return false;
					}

					if (area == swept && remove_previous) {
						// this move is to remove the shared edge if compensating but the contracting move will lengthen this shared edge
						return false;
					}
				}
			}

			return true;
		}

		bool movable() const {
			return src_type != UNMOVABLE && tar_type != UNMOVABLE;
		}

		bool contractable() const {
			return movable() && !contract_merges_highdegrees && decrease_on_contract() > 0;
		}

		int increase_on_move() const {
			int inc = 0;
			switch (src_type) {
			case DEG_THREE_NO_SUPPORT:
			case DEG_THREE_SUPPORT:
			case DEG_THREE_ALIGNED:
				inc++;
				break;
			}
			switch (tar_type) {
			case DEG_THREE_NO_SUPPORT:
			case DEG_THREE_SUPPORT:
			case DEG_THREE_ALIGNED:
				inc++;
				break;
			}
			return inc;
		}

		int decrease_on_contract() const {
			int dec = 0;
			if (remove_next) {
				dec++;
			}
			if (remove_self) {
				dec++;
			}
			if (remove_previous) {
				dec++;
			}
			if (merge_next) {
				dec++;
			}
			if (merge_previous) {
				dec++;
			}
			return dec - increase_on_move();
		}

		template<class ST>
		void update(ST& score_traits) {
			swept.clear();

			Vertex_handle a = nullptr, d = nullptr;
			Vertex_handle a_alt = nullptr, d_alt = nullptr;

			Vertex_handle b = this->edge->source();
			Vertex_handle c = this->edge->target();

			Line<Kernel> baseline = Line<Kernel>(b->point(), c->point());

			// determine start type
			switch (b->degree()) {
			case 2: {
				a = b->prev();
				if (left == CGAL::left_turn(a->point(), b->point(), c->point())) {
					src_type = DEG_TWO_SUPPORT;
				}
				else {
					src_type = DEG_TWO_NO_SUPPORT;
				}
				break;
			}
			case 3: {
				size_t edge_index = b->find_incident_index(this->edge);
				a = b->neighboring_incident_edge(edge_index, left)->other(b);
				a_alt = b->neighboring_incident_edge(edge_index, !left)->other(b);

				if (safe_test::point_on_line(a->point(), baseline)) {
					src_type = DEG_THREE_ALIGNED;
				}
				else if (left == CGAL::left_turn(a->point(), b->point(), c->point())) {
					if (safe_test::collinear(a->point(), b->point(), a_alt->point())) {
						src_type = DEG_THREE_SMOOTH;
					}
					else {
						src_type = DEG_THREE_SUPPORT;
					}
				}
				else {
					src_type = DEG_THREE_NO_SUPPORT;
				}
				break;
			}
			default:
				src_type = UNMOVABLE;
				break;
			}

			// determine end type
			switch (c->degree()) {
			case 2: {
				d = c->next();
				if (left == CGAL::left_turn(b->point(), c->point(), d->point())) {
					tar_type = DEG_TWO_SUPPORT;
				}
				else {
					tar_type = DEG_TWO_NO_SUPPORT;
				}
				break;
			}
			case 3: {
				size_t edge_index = c->find_incident_index(this->edge);
				d = c->neighboring_incident_edge(edge_index, !left)->other(c);
				d_alt = c->neighboring_incident_edge(edge_index, left)->other(c);
				if (safe_test::point_on_line(d->point(), baseline)) {
					tar_type = DEG_THREE_ALIGNED;
				}
				else if (left == CGAL::left_turn(b->point(), c->point(), d->point())) {
					if (safe_test::collinear(c->point(), d->point(), d_alt->point())) {
						tar_type = DEG_THREE_SMOOTH;
					}
					else {
						tar_type = DEG_THREE_SUPPORT;
					}
				}
				else {
					tar_type = DEG_THREE_NO_SUPPORT;
				}
				break;
			}
			default:
				tar_type = UNMOVABLE;
				break;
			}

			if (!movable()) {
				return;
			}

			// how far can we move along the previous track before the previous edge vanishes
			Number<Kernel> prev_dist_squared;
			switch (src_type) {
			case DEG_TWO_SUPPORT:
			case DEG_THREE_SMOOTH:
			case DEG_THREE_SUPPORT:
				prev_dist_squared = CGAL::squared_distance(baseline, a->point());
				break;
			default:
				prev_dist_squared = -1;
				break;
			}

			// how far can we move along the next track before the next edge vanishes
			Number<Kernel> next_dist_squared;
			switch (tar_type) {
			case DEG_TWO_SUPPORT:
			case DEG_THREE_SMOOTH:
			case DEG_THREE_SUPPORT:
				next_dist_squared = CGAL::squared_distance(baseline, d->point());
				break;
			default:
				next_dist_squared = -1;
				break;
			}

			// how far can we move until the edge itself vanishes
			Line<Kernel> prev_track = Line<Kernel>(b->point(), (src_type == DEG_THREE_ALIGNED ? a_alt : a)->point());
			Line<Kernel> next_track = Line<Kernel>(c->point(), (tar_type == DEG_THREE_ALIGNED ? d_alt : d)->point());

			Number<Kernel> edge_dist_squared = -1;
			Point<Kernel> edge_lim;

			{
				const auto is = CGAL::intersection(prev_track, next_track);
				if (is) {
					if (const Point<Kernel>* pt = std::get_if<Point<Kernel>>(&*is)) {
						edge_lim = *pt;
						if (left && baseline.oriented_side(edge_lim) != CGAL::ON_NEGATIVE_SIDE) {
							edge_dist_squared = CGAL::squared_distance(baseline, edge_lim);
						}
						else if (!left && baseline.oriented_side(edge_lim) != CGAL::ON_POSITIVE_SIDE) {
							edge_dist_squared = CGAL::squared_distance(baseline, edge_lim);
						}
					}
				}
			}

			// which is the limiting factor? and which edges then get removed?
			if (prev_dist_squared < 0 && next_dist_squared < 0 && (!score_traits.allow_finite_unsupported() || edge_dist_squared < 0)) {
				// no support at all
				src_type = UNMOVABLE;
				tar_type = UNMOVABLE;
				return;
			}

			Number<Kernel> min_dist_squared;
			if (prev_dist_squared >= 0) {
				min_dist_squared = prev_dist_squared;
			}
			else {
				min_dist_squared = -1;
			}
			if (next_dist_squared >= 0) {
				if (min_dist_squared < 0) {
					min_dist_squared = next_dist_squared;
				}
				else {
					min_dist_squared = CGAL::min(min_dist_squared, next_dist_squared);
				}
			}
			if (edge_dist_squared >= 0) {
				if (min_dist_squared < 0) {
					min_dist_squared = edge_dist_squared;
				}
				else {
					min_dist_squared = CGAL::min(min_dist_squared, edge_dist_squared);
				}
			}

			remove_self = safe_test::close(edge_dist_squared, min_dist_squared);
			remove_previous = safe_test::close(prev_dist_squared, min_dist_squared);
			remove_next = safe_test::close(next_dist_squared, min_dist_squared);

			// determine if the previous edge allows merging
			if (remove_previous && b->degree() == 2 && a->degree() == 2) {
				if (remove_self) {
					// check if double-prev can merge with next
					merge_previous = safe_test::aligned_vectors(a->point() - a->prev()->point(),
						d->point() - c->point());
				}
				else {
					// check alignment with self
					merge_previous = safe_test::aligned_vectors(a->point() - a->prev()->point(),
						c->point() - b->point());
				}
			}
			else {
				merge_previous = false;
			}

			// determine if the next edge allows merging
			if (remove_next && c->degree() == 2 && d->degree() == 2) {

				if (remove_self) {
					// check if double-next can merge with prev
					merge_next = safe_test::aligned_vectors(d->next()->point() - d->point(),
						b->point() - a->point());
				}
				else {
					// check alignment with self
					merge_next = safe_test::aligned_vectors(d->next()->point() - d->point(),
						c->point() - b->point());
				}
			}
			else {
				merge_next = false;
			}

			// construct the swept area
			if (remove_self) {
				swept.push_back(edge_lim);
			}
			else if (remove_previous) {
				swept.push_back(a->point());
			}
			else {
				assert(remove_next);
				// force using the actual intersection, we (should) have eliminated exactly equal directions
				const auto is = CGAL::intersection(Line<Kernel>(d->point(), c->point() - b->point()), prev_track);
				swept.push_back(*std::get_if<Point<Kernel>>(&*is));
			}
			swept.push_back(b->point());
			swept.push_back(c->point());
			if (remove_self) {
				// do nothing, triangular area
			}
			else if (remove_next) {
				swept.push_back(d->point());
			}
			else {
				assert(remove_previous);
				// force using the actual intersection, we (should) have eliminated exactly equal directions
				const auto is = CGAL::intersection(Line<Kernel>(a->point(), c->point() - b->point()), next_track);
				swept.push_back(*std::get_if<Point<Kernel>>(&*is));
			}

			// precision, be sure we dont want to remove self...  
			if (!remove_self && safe_test::same_point(source_destination(), target_destination())) {
				remove_self = true;
				swept.erase(--swept.vertices_end());
			}

			if (remove_self && b->degree() != 2 && c->degree() != 2) {
				contract_merges_highdegrees = true;
			}
			else if (remove_previous && a->degree() != 2 && b->degree() != 2) {
				contract_merges_highdegrees = true;
			}
			else if (remove_next && c->degree() != 2 && d->degree() != 2) {
				contract_merges_highdegrees = true;
			}
			else {
				contract_merges_highdegrees = false;
			}
		}

		Number<Inexact> base_length() const {
			return std::sqrt(approximate(CGAL::squared_distance(source(), target())));
		}
		Number<Inexact> end_length() const {
			return swept.size() == 3 ? 0 : std::sqrt(approximate(CGAL::squared_distance(source_destination(), target_destination())));
		}
		Vector<Inexact> move_direction() const {
			Vector<Kernel> vec = edge_vector();
			Vector<Inexact> dir = approximate(left ? vec.perpendicular(CGAL::COUNTERCLOCKWISE) : vec.perpendicular(CGAL::CLOCKWISE));
			return dir / std::sqrt(dir.squared_length());
		}
		Vector<Kernel> edge_vector() const {
			return this->edge->target()->point() - this->edge->source()->point();
		}
		Point<Kernel> source() const {
			return swept[1];
		}
		Point<Kernel> source_destination() const {
			return swept[0];
		}
		Point<Kernel> target() const {
			return swept[2];
		}
		Point<Kernel> target_destination() const {
			return swept[swept.size() == 3 ? 0 : 3];
		}
		Vector<Kernel> source_move() const {
			return source_destination() - source();
		}
		Vector<Kernel> target_move() const {
			return target_destination() - target();
		}
		Number<Kernel> swept_area() const {
			return CGAL::abs(swept.area());
		}

		std::optional<std::pair<Point<Kernel>, Point<Kernel>>> end_positions_for(const Number<Kernel> area) const {

			Vector<Inexact> movedir = move_direction();
			Number<Inexact> len = base_length();
			Number<Inexact> end_len = end_length();
			Vector<Inexact> startTrackVec = approximate(source_move());
			Vector<Inexact> endTrackVec = approximate(target_move());
			Number<Inexact> lim_dist = CGAL::scalar_product(movedir, endTrackVec);
			Number<Inexact> r = (end_len - len) / lim_dist;

			// movearea = d * (len + d * r / 2)
			//          = d * len + d^2 * r/2
			Number<Inexact> d = utils::solveQuadraticEquationForSmallestPositive(r / 2.0, len, -approximate(area));
			if (d <= 0) {
				return std::nullopt;
			}
			Number<Inexact> ratio = CGAL::min(1.0, d / lim_dist);
			return std::pair<Point<Kernel>, Point<Kernel>>(source() + convert_kernel<Kernel>(ratio * startTrackVec), target() + convert_kernel<Kernel>(ratio * endTrackVec));
		}
	};

	template<class K, typename EdgeHandle, typename VertexHandle>
	struct ComboMove : public BaseMove<K, EdgeHandle> {

		using Kernel = K;
		using Edge_handle = EdgeHandle;
		using Vertex_handle = VertexHandle;
		using Single = SingleMove<K, EdgeHandle, VertexHandle>;

		bool executable;
		Single* prev_move;
		Single* next_move;
		Polygon<Kernel> prev_swept;
		Polygon<Kernel> next_swept;
		bool remove_self, merge_across;
		bool prev_partial, next_partial;

		bool is_single() const override {
			return false;
		}

		bool is_blocked_by(Edge_handle candidate) const {

			return prev_move->is_blocked_by(candidate, prev_swept, !prev_partial)
				|| next_move->is_blocked_by(candidate, next_swept, !next_partial);
		}

		Point<Kernel> prev_source_destination() {
			return prev_swept[0];
		}

		Point<Kernel> prev_target_destination() {
			return prev_swept[prev_swept.size() == 3 ? 0 : 3];
		}

		Point<Kernel> next_source_destination() {
			return next_swept[0];
		}

		Point<Kernel> next_target_destination() {
			return next_swept[next_swept.size() == 3 ? 0 : 3];
		}

		template<class DT>
		void update(DT& data_traits) {

			size_t edgeindex = this->edge->graph_index();

			prev_swept.clear();
			next_swept.clear();

			if (this->edge->source()->degree() != 2 || this->edge->target()->degree() != 2) {
				executable = false;
				return;
			}

			Edge_handle prev = this->edge->prev();
			Edge_handle next = this->edge->next();

			bool left_at_source = CGAL::left_turn(prev->source()->point(), this->edge->source()->point(), this->edge->target()->point());
			bool right_at_target = CGAL::right_turn(this->edge->source()->point(), this->edge->target()->point(), next->target()->point());

			if (left_at_source != right_at_target) {
				// same turn
				prev_move = nullptr;
				next_move = nullptr;
				executable = false;
				return;
			}

			if (left_at_source) {
				prev_move = data_traits.data(prev).left;
				next_move = data_traits.data(next).right;
			}
			else {
				prev_move = data_traits.data(prev).right;
				next_move = data_traits.data(next).left;
			}

			if (!prev_move->movable() || !next_move->movable()) {
				executable = false;
				return;
			}

			// TODO: reduce approximate use?

			// now, determine how far both need to move until the common edge vanishes
			Vector<Inexact> edgedir = approximate(this->edge->target()->point() - this->edge->source()->point());
			Number<Inexact> edge_length = std::sqrt(edgedir.squared_length());
			edgedir /= edge_length;

			Vector<Inexact> prev_movedir = prev_move->move_direction();
			Number<Inexact> prev_len = prev_move->base_length();
			Number<Inexact> prev_len_end = prev_move->end_length();
			Vector<Inexact> prev_common_vector = approximate(prev_move->target_move());
			Number<Inexact> prev_s = 1.0 / CGAL::scalar_product(edgedir, prev_movedir); // NB: signswap between sides
			Number<Inexact> prev_lim_dist = CGAL::scalar_product(prev_movedir, prev_common_vector);
			Number<Inexact> prev_r = (prev_len_end - prev_len) / prev_lim_dist;

			Vector<Inexact> next_movedir = next_move->move_direction();
			Number<Inexact> next_len = next_move->base_length();
			Number<Inexact> next_len_end = next_move->end_length();
			Vector<Inexact> next_common_vector = approximate(next_move->source_move());
			Number<Inexact> next_s = -1.0 / CGAL::scalar_product(edgedir, next_movedir); // NB: signswap between sides
			Number<Inexact> next_lim_dist = CGAL::scalar_product(next_movedir, next_common_vector);
			Number<Inexact> next_r = (next_len_end - next_len) / next_lim_dist;

			// edge left/right of length _len expands at rate _r, covering the common edge at rate _s
			// so, moving a distance _d sweeps area
			//    _d * (_len + _d * _r / 2)
			// and covers the common edge for
			//    _d * _s;
			//
			// we need the swept areas to match, and the common edge to be fully covered
			//    p_d * p_s + n_d * n_s = e_len
			//    p_d * (p_len + p_d * p_r / 2) = n_d * (n_len + n_d * n_r / 2)
			// the former gives
			//    p_d = (e_len - n_d * n_s) / p_s
			// substituting in the latter and rewriting to quadratic form
			//     n_d^2 * (n_s ^ 2 * p_r / (2 * p_s ^ 2) - n_r / 2)
			//     + n_d (- n_s * p_len / p_s - e_len *  n_s * p_r / p_s^2 - n_len)
			//     + (e_len * p_len / p_s + e_len^2 * p_r / (2 * p_s^2)  )
			//     = 0
			// find smallest positive solution for n_d
			Number<Inexact> a = prev_r * next_s * next_s / (2 * prev_s * prev_s) - next_r / 2.0;
			Number<Inexact> b = -prev_len * next_s / prev_s - next_len - prev_r * next_s * edge_length / (prev_s * prev_s);
			Number<Inexact> c = prev_len * edge_length / prev_s + prev_r * edge_length * edge_length / (2 * prev_s * prev_s);

			Number<Inexact> next_d;
			if (safe_test::close(a, 0) && safe_test::close(b, 0) && safe_test::close(c, 0)) {
				// we'll get division by zero / NaNs... but if c is also zero, then things just line up perfectly...
				next_d = next_lim_dist;
			}
			else {
				next_d = utils::solveQuadraticEquationForSmallestPositive(a, b, c); // NB: will be <= 0 if there is no positive solution
			}
			Number<Inexact> prev_d = (edge_length - next_d * next_s) / prev_s;

			// we need to make sure that after the move, no (approximately) zero-length edges remain
			// there are a couple of cases to consider:
			// if _d > _lim_dist, then the move contracts before the edges meet
			// if _d = _lim_dist, then the move contracts as the edges meet
			// if _d < _lim_dist, then the move only has to do a partial move, for the edges to meet
			// now, if ONE of the edges contracts before meeting, then this effectively is just a pair of edge moves
			// otherwise, we need to account for the meeting point, and the possibility of either move still contracting
			prev_swept = prev_move->swept;
			next_swept = next_move->swept;

			if (next_d <= 0 || prev_d > prev_lim_dist + M_EPSILON || next_d > next_lim_dist + M_EPSILON) {
				// just a pair

				Number<Kernel> prev_area = CGAL::abs(prev_move->swept.area());
				Number<Kernel> next_area = CGAL::abs(next_move->swept.area());
				if (prev_area < next_area) {
					// prev contracts
					if (!prev_move->contractable()) {
						executable = false;
						return;
					}
					prev_partial = false;

					// and possibly next
					auto startend = next_move->end_positions_for(prev_area);
					if (!startend || safe_test::same_point(next_swept[0], startend->first)) {
						if (!next_move->contractable()) {
							executable = false;
							return;
						}
						next_partial = false;
					}
					else {
						next_partial = true;
						next_swept[0] = startend->first;
						if (next_swept.size() < 4) {
							next_swept.push_back(startend->second);
						}
						else {
							next_swept[3] = startend->second;
						}
					}

				}
				else {
					// next contract
					if (!next_move->contractable()) {
						executable = false;
						return;
					}
					next_partial = false;

					// and possibly prev
					auto startend = prev_move->end_positions_for(next_area);
					if (!startend || safe_test::same_point(prev_swept[0], startend->first)) {
						if (!prev_move->contractable()) {
							executable = false;
							return;
						}
						prev_partial = false;
					}
					else {
						prev_partial = true;
						prev_swept[0] = startend->first;
						if (prev_swept.size() < 4) {
							prev_swept.push_back(startend->second);
						}
						else {
							prev_swept[3] = startend->second;
						}
					}
				}

				// handling precision issues: if one is trying to remove the common edge, we should just let it be handled by the combined move
				remove_self = (!prev_partial && prev_move->remove_next) || (!next_partial && next_move->remove_previous);
			}
			else {
				// construct the meeting point
				remove_self = true;

				Number<Kernel> prev_ratio = convert_kernel<Kernel>(CGAL::min(1.0, prev_d / prev_lim_dist));
				Point<Kernel> merge_point = this->edge->source()->point() + prev_ratio * convert_kernel<Kernel>(prev_common_vector);
				Point<Kernel> prev_other_end = prev_move->swept[1] + prev_ratio * prev_move->source_move();

				//std::cout << " meeting point " << merge_point << std::endl;
				prev_partial = !safe_test::same_point(merge_point, prev_swept[prev_swept.size() == 3 ? 0 : 3])
					&& !safe_test::same_point(merge_point, prev_other_end)
					&& !safe_test::same_point(prev_other_end, prev_move->source_destination());
				if (prev_partial) {
					prev_swept[0] = prev_other_end;
					if (prev_swept.size() < 4) {
						prev_swept.push_back(merge_point);
					}
					else {
						prev_swept[3] = merge_point;
					}
				}
				else if (!prev_move->contractable()) {
					executable = false;
					return;
				}

				Number<Kernel> next_ratio = convert_kernel<Kernel>(CGAL::min(1.0, next_d / next_lim_dist));
				Point<Kernel> next_other_end = next_move->swept[2] + next_ratio * next_move->target_move();

				next_partial = !safe_test::same_point(merge_point, next_swept[0])
					&& !safe_test::same_point(merge_point, next_other_end)
					&& !safe_test::same_point(next_other_end, next_move->target_destination());
				if (next_partial) {

					next_swept[0] = merge_point;
					if (next_swept.size() < 4) {
						next_swept.push_back(next_other_end);
					}
					else {
						next_swept[3] = next_other_end;
					}
				}
				else if (!next_move->contractable()) {
					executable = false;
					return;
				}
			}

			if (remove_self) {
				Vector<Kernel> prevdir_at_join;
				if (prev_swept.size() == 3) {
					prevdir_at_join = prev_swept[0] - prev_swept[1];
				}
				else {
					prevdir_at_join = prev_swept[2] - prev_swept[1];
				}

				Vector<Kernel> nextdir_at_join;
				if (next_swept.size() == 3) {
					nextdir_at_join = next_swept[0] - next_swept[2];
				}
				else {
					nextdir_at_join = next_swept[2] - next_swept[1];
				}

				//std::cout << prevdir_at_join << "  ||  " << nextdir_at_join << std::endl;
				//std::cout << " -> det = " << CGAL::determinant(prevdir_at_join, nextdir_at_join) << std::endl;
				merge_across = safe_test::aligned_vectors(prevdir_at_join, nextdir_at_join);
				//std::cout << " merge_across = " << merge_across << std::endl;
			}
			else {
				merge_across = false;
			}

			int reduc = prev_partial ? -prev_move->increase_on_move() : prev_move->decrease_on_contract();
			reduc += next_partial ? -next_move->increase_on_move() : next_move->decrease_on_contract();
			if (remove_self) reduc++;
			if (merge_across) reduc++;

			executable = reduc > 0;
		}

		bool is_executable() const {
			return executable;
		}

		Number<Kernel> swept_area() const {
			return CGAL::abs(prev_swept.area());
		}
	};

	template<bool UseSelfEdgeData, class VertexData, class ActualEdgeData, bool UseSelfPathData, class ActualPathData, KernelType K, class GraphTraits>
	struct PathData;

	template<bool UseSelfEdgeData, class VertexData, class ActualEdgeData, bool UseSelfPathData, class ActualPathData, KernelType K, class GraphTraits>
	struct EdgeData {
		using Graph_edge_data = std::conditional<UseSelfEdgeData, EdgeData, ActualEdgeData>::type;
		using Graph_path_data = PathData<UseSelfEdgeData, VertexData, ActualEdgeData, UseSelfPathData, ActualPathData, K, GraphTraits>::Graph_path_data;

		using Kernel = K;
		using Vertex_handle = Graph_2_vertex<Graph_segment_curve_traits_2<Kernel>, GraphTraits, VertexData, Graph_edge_data, Graph_path_data>*; // needed to avoid co-dependent templates for dedicated graphs
		using Edge_handle = Graph_2_edge<Graph_segment_curve_traits_2<Kernel>, GraphTraits, VertexData, Graph_edge_data, Graph_path_data>*; // needed to avoid co-dependent templates for dedicated graphs

		using Move = BaseMove<K, Edge_handle>;
		using Single = SingleMove<K, Edge_handle, Vertex_handle>;
		using Combo = ComboMove<K, Edge_handle, Vertex_handle>;

		// NB pointers instead of value types, s.t. we can store pointers to these while the edgedata may be relocated
		Single* left = new Single(true);
		Single* right = new Single(false);
		Combo* combo = new Combo();
		std::vector<Move*> blocking;

		~EdgeData() {
			delete left;
			delete right;
			delete combo;
		}
	};


	template<bool UseSelfEdgeData, class VertexData, class ActualEdgeData, bool UseSelfPathData, class ActualPathData, KernelType K, class GraphTraits>
	struct PathData {
		using Graph_edge_data = EdgeData<UseSelfEdgeData, VertexData, ActualEdgeData, UseSelfPathData, ActualPathData, K, GraphTraits>::Graph_edge_data;
		using Graph_path_data = std::conditional<UseSelfPathData, PathData, ActualPathData>::type;

		using Kernel = K;
		using Vertex_handle = Graph_2_vertex<Graph_segment_curve_traits_2<Kernel>, GraphTraits, VertexData, Graph_edge_data, Graph_path_data>*; // needed to avoid co-dependent templates for dedicated graphs
		using Edge_handle = Graph_2_edge<Graph_segment_curve_traits_2<Kernel>, GraphTraits, VertexData, Graph_edge_data, Graph_path_data>*; // needed to avoid co-dependent templates for dedicated graphs

		using Move = BaseMove<K, Edge_handle>;
		using Single = SingleMove<K, Edge_handle, Vertex_handle>;
		using Combo = ComboMove<K, Edge_handle, Vertex_handle>;

		std::vector<Single*> left_waiting;
		std::vector<Single*> right_waiting;

		void add_waiting(Single* single) {
			if (single->left) {
				//std::cout << ">> left move waiting for edge " << *single->edge << ", index = " << left_waiting.size() << std::endl;
				single->waiting_index = left_waiting.size();
				left_waiting.push_back(single);
			}
			else {
				//std::cout << ">> right move waiting for edge " << *single->edge << ", index = " << right_waiting.size() << std::endl;
				single->waiting_index = right_waiting.size();
				right_waiting.push_back(single);
			}
		}

		void remove_waiting(Single* single) {
			if (single->left) {
				//std::cout << ">> left move no longer waiting for edge " << *single->edge << std::endl;
				Single* other = utils::swapRemove(single->waiting_index, left_waiting);
				if (other != nullptr) {
					other->waiting_index = single->waiting_index;
				}
				single->waiting_index = -1;
			}
			else {
				//std::cout << ">> right move no longer waiting for edge " << *single->edge << std::endl;
				Single* other = utils::swapRemove(single->waiting_index, right_waiting);
				if (other != nullptr) {
					other->waiting_index = single->waiting_index;
				}
				single->waiting_index = -1;
			}
		}
	};

	template<class DT>
	concept DataTraits =
		KernelType<typename DT::Kernel> &&
		GraphType<typename DT::Graph> &&
		DT::Graph::Graph_traits::oriented &&
		DT::Graph::Graph_traits::decomposed &&
		DT::Graph::Graph_traits::sorted &&
		std::is_default_constructible<typename DT::Edge_data>::value &&
		std::is_default_constructible<typename DT::Path_data>::value &&
		requires(DT data, typename DT::Graph graph, typename DT::Graph::Edge_handle e, typename DT::Graph::Path_handle p) {

		typename DT::Kernel;
		typename DT::Graph;
		typename DT::Edge_data;
		typename DT::Path_data;

		{ data.initialize(graph) };
		{ data.data(e) } -> std::same_as<typename DT::Edge_data&>;
		{ data.data(p) } -> std::same_as<typename DT::Path_data&>;
	};

	template<class ST>
	concept ScoreTraits =
		KernelType<typename ST::Kernel> &&
		requires(ST score, typename ST::Single & single, typename ST::Combo & combo) {

		typename ST::Kernel;
		typename ST::Graph;
		typename ST::Single;
		typename ST::Combo;

		{ score.allow_finite_unsupported() } -> std::same_as<bool>;
		{ score.tiny_move() } -> std::same_as<Number<typename ST::Kernel>>;
		{ score.compute_cost(single) } -> std::same_as<Number<typename ST::Kernel>>;
		{ score.compute_cost(combo) } -> std::same_as<Number<typename ST::Kernel>>;
	};

	template<class DT, class ST>
	concept Setup =
		DataTraits<DT> && ScoreTraits<ST> &&
		std::is_same<typename ST::Kernel, typename DT::Kernel>::value;

	template<KernelType K, bool Historic>
	class DataTraits_Dedicated {
	public:
		using Kernel = K;
		using Edge_data = EdgeData<true, std::monostate, std::monostate, true, std::monostate, Kernel, CustomGraphTraits<Historic, true, true, true>>;
		using Path_data = PathData<true, std::monostate, std::monostate, true, std::monostate, Kernel, CustomGraphTraits<Historic, true, true, true>>;
		using Graph = Straight_graph_2<Kernel, CustomGraphTraits<Historic, true, true, true>, std::monostate, Edge_data, Path_data>;

		static void initialize(Graph& graph) {
			// skip
		}

		static Edge_data& data(typename Graph::Edge_handle e) {
			return e->data();
		}
		static Path_data& data(typename Graph::Path_handle p) {
			return p->data();
		}
	};



	template<GraphType G>
	class DataTraits_Generic {
	public:
		using Graph = G;
		using Kernel = Graph::Kernel;
		using Edge_data = EdgeData<false, typename G::Vertex_data, typename G::Edge_date, false, typename G::Path_data, Kernel, typename G::Graph_traits>;
		using Path_data = PathData<false, typename G::Vertex_data, typename G::Edge_date, false, typename G::Path_data, Kernel, typename G::Graph_traits>;

		void initialize(Graph& graph) {
			delete m_edge_data;
			m_edge_data = new Graph_edge_map<Graph, std::shared_ptr<Edge_data>>(graph, nullptr);
			delete m_path_data;
			m_path_data = new Graph_path_map<Graph, std::shared_ptr<Path_data>>(graph, nullptr);
		}

		~DataTraits_Generic() {
			delete m_edge_data;
		}

		Edge_data& data(typename Graph::Edge_handle e) {
			return *(*m_edge_data)[e];
		}

		Path_data& data(typename Graph::Path_handle p) {
			return *(*m_path_data)[p];
		}
	private:
		Graph_edge_map<Graph, std::shared_ptr<Edge_data>>* m_edge_data = nullptr;
		Graph_path_map<Graph, std::shared_ptr<Path_data>>* m_path_data = nullptr;
	};

	template<typename Elt>
	struct MoveQueueTraits {

		using Element_handle = Elt;

		static void setIndex(Element_handle m, int id) {
			m->queue_index = id;
		}

		static int getIndex(Element_handle m) {
			return m->queue_index;
		}

		static int compare(Element_handle a, Element_handle b) {
			const auto ac = a->cost;
			const auto bc = b->cost;
			if (ac < bc) {
				return -1;
			}
			else if (ac > bc) {
				return 1;
			}
			else {
				return 0;
			}
		}
	};

	template <DataTraits DT, ScoreTraits ST>
		requires Setup<DT, ST>
	class EdgeMoves {
	public:
		using Data_traits = DT;
		using Score_traits = ST;

		using Graph = Data_traits::Graph;
		using Vertex_handle = Graph::Vertex_handle;
		using Edge_handle = Graph::Edge_handle;
		using Edge_data = Data_traits::Edge_data;
		using Path_handle = Graph::Path_handle;
		using Path_data = Data_traits::Path_data;
		using Kernel = Graph::Kernel;

	private:
		using Move = Edge_data::Move;
		using Single = Edge_data::Single;
		using Combo = Edge_data::Combo;
		//using Move = BaseMove<Kernel, Edge_handle>;
		//using Single = SingleMove<Kernel, Edge_handle, Vertex_handle>;
		//using Combo = ComboMove<Kernel, Edge_handle, Vertex_handle>;
		using Queue = cartocrow::IndexedPriorityQueue<MoveQueueTraits<Move*>>;

		using PairedSingles = std::pair<Single*, Single*>;
		using Operation = std::variant<Combo*, PairedSingles, Single*>;

		using VertexTree = VertexQuadTree<Graph>;
		using EdgeTree = EdgeQuadTree<Graph>;

		Graph& graph;
		EdgeTree* sqt = nullptr;
		VertexTree* pqt = nullptr;
		Data_traits data_traits;
		Score_traits score_traits;
		Queue queue;
		Graph_edge_set<Graph> edgeset = Graph_edge_set<Graph>(graph);

		bool validate_state() {
			bool correct_state = true;

			for (Move* m : queue.content()) {
				if (graph.edge(m->edge->graph_index()) != m->edge) {
					std::cout << "! move in queue for edge " << *m->edge << "  that is not in graph: ";
					if (m->is_single()) {
						if (static_cast<Single*>(m)->left) {
							std::cout << "left move";
						}
						else {
							std::cout << "right move";
						}
					}
					else {
						std::cout << "combo move";
					}
					std::cout << std::endl;
					correct_state = false;
				}
			}

			Graph_static_edge_map<Graph, int> emap(graph, 0);

			Rectangle<Kernel> rect = sqt->root_box();
			sqt->findOverlapped(rect, [this, &emap](Edge_handle e) {
				if (this->graph.edge(e->graph_index()) != e) {
					std::cout << "! edge in SQT that is not in graph: " << *e << std::endl;
				}
				else {
					emap[e] = emap[e] + 1;
				}
				});
			for (Edge_handle e : graph.edges()) {
				if (emap[e] != 1) {
					std::cout << "! edge not exactly once in SQT: " << *e << " occurs " << emap[e] << " times" << std::endl;
				}
			}


			for (Edge_handle e : graph.edges()) {

				Edge_data& edata = data_traits.data(e);
				Path_data& pdata = data_traits.data(e->path());

				// left check
				if (edata.left.contractable()) {
					if (edata.left.is_blocked()) {
						if (queue.contains(&edata.left)) {
							std::cout << "! move in queue that is blocked: left move for " << *e << std::endl;
							correct_state = false;
						}

						for (Edge_handle b : edata.left.blocked_by) {
							Edge_data& bdata = data_traits.data(b);
							if (std::find(bdata.blocking.begin(), bdata.blocking.end(), &edata.left) == bdata.blocking.end()) {
								std::cout << "! left move for edge " << *e << " says its blocked by edge" << *b << ", but is not in its blocking list" << std::endl;
								correct_state = false;
							}
						}
					}
					else if (edata.left.waiting_index < 0) {
						if (!queue.contains(&edata.left)) {
							std::cout << "! move not in queue that is contractable, not blocked and not waiting: left move for " << *e << std::endl;
							correct_state = false;
						}
					}
					else {
						if (pdata.left_waiting[edata.left.waiting_index] != &edata.left) {
							std::cout << "! move is waiting, but not in waiting list: left move for " << *e << std::endl;
							correct_state = false;
						}
					}
				}
				else {
					if (queue.contains(&edata.left)) {
						std::cout << "! move in queue that is not contractable: left move for " << *e << std::endl;
						correct_state = false;
					}
				}

				// right check
				if (edata.right.contractable()) {
					if (edata.right.is_blocked()) {
						if (queue.contains(&edata.right)) {
							std::cout << "! move in queue that is blocked: right move for " << *e << std::endl;
							correct_state = false;
						}

						for (Edge_handle b : edata.right.blocked_by) {
							Edge_data& bdata = data_traits.data(b);
							if (std::find(bdata.blocking.begin(), bdata.blocking.end(), &edata.right) == bdata.blocking.end()) {
								std::cout << "! right move for edge " << *e << " says its blocked by edge" << *b << ", but is not in its blocking list" << std::endl;
								correct_state = false;
							}
						}
					}
					else if (edata.right.waiting_index < 0) {
						if (!queue.contains(&edata.right)) {
							std::cout << "! move not in queue that is contractable, not blocked and not waiting: right move for " << *e << std::endl;
							correct_state = false;
						}
					}
					else {
						if (pdata.right_waiting[edata.right.waiting_index] != &edata.right) {
							std::cout << "! move is waiting, but not in waiting list: right move for " << *e << std::endl;
							correct_state = false;
						}
					}
				}
				else {
					if (queue.contains(&edata.right)) {
						std::cout << "! move in queue that is not contractable: right move for " << *e << std::endl;
						correct_state = false;
					}
				}

				// combo check
				if (edata.combo.is_executable()) {
					if (edata.combo.is_blocked()) {
						if (queue.contains(&edata.combo)) {
							std::cout << "! move in queue that is blocked: combo move for " << *e << std::endl;
							correct_state = false;
						}

						for (Edge_handle b : edata.combo.blocked_by) {
							Edge_data& bdata = data_traits.data(b);
							if (std::find(bdata.blocking.begin(), bdata.blocking.end(), &edata.combo) == bdata.blocking.end()) {
								std::cout << "! combo move for edge " << *e << " says its blocked by edge" << *b << ", but is not in its blocking list" << std::endl;
								correct_state = false;
							}
						}
					}
					else {
						if (!queue.contains(&edata.combo)) {
							std::cout << "! move not in queue that is movable and not blocked: combo move for " << *e << std::endl;
							correct_state = false;
						}
					}
				}
				else {
					if (queue.contains(&edata.combo)) {
						std::cout << "! move in queue that is not movable: combo move for " << *e << std::endl;
						correct_state = false;
					}
				}


				for (Move* m : edata.blocking) {
					if (std::find(m->blocked_by.begin(), m->blocked_by.end(), e) == m->blocked_by.end()) {
						std::cout << "! edge " << *e << " says its blocking a move for " << *m->edge << ", but is not in its blocked_by list" << std::endl;
						correct_state = false;
					}
				}

			}

			return correct_state;
		}

		template<bool LEFT>
		void update_single(Edge_handle e, Single& single) {
			single.edge = e;
			single.blocked_by_degzero = false;
			for (Edge_handle b : single.blocked_by) {
				utils::listRemove<Move>(&single, data_traits.data(b).blocking);
			}
			single.blocked_by.clear();

			if (single.waiting_index >= 0) {
				data_traits.data(e->path()).remove_waiting(&single);
				single.waiting_index = -1;
			}

			single.update(score_traits);

			if (single.movable()) {

				std::vector<Single*>& waiting = LEFT ? data_traits.data(e->path()).right_waiting : data_traits.data(e->path()).left_waiting;
				for (Single* other : waiting) {
					other->waiting_index = -1;
					queue.push(other);
				}
				waiting.clear();
			}

			if (single.contractable()) {
				single.cost = score_traits.compute_cost(single);

				if (queue.contains(&single)) {
					queue.update(&single);
				}
				else {
					queue.push(&single);
				}
			}
			else {
				queue.remove(&single);
			}
		}

		void update_singles(Edge_handle e) {
			assert(graph.edge(e->graph_index()) == e);

			Edge_data& data = data_traits.data(e);
			update_single<true>(e, *data.left);
			update_single<false>(e, *data.right);
		}

		void update_combo(Edge_handle e) {
			Edge_data& data = data_traits.data(e);
			Combo& combo = *data.combo;

			combo.edge = e;
			combo.blocked_by_degzero = false;
			for (Edge_handle b : combo.blocked_by) {
				utils::listRemove<Move>(&combo, data_traits.data(b).blocking);
			}
			combo.blocked_by.clear();

			combo.update(data_traits);

			if (combo.is_executable()) {
				combo.cost = score_traits.compute_cost(combo);

				if (queue.contains(&combo)) {
					queue.update(&combo);
				}
				else {
					queue.push(&combo);
				}
			}
			else {
				queue.remove(&combo);
			}
		}

		bool test_topology(Single& single) {
			// test if its blocked
			Rectangle<Kernel> rect = utils::boxOf(single.swept);

			single.blocked_by_degzero = false;
			pqt->findContained(rect, [&single](Vertex_handle b) {
				if (!single.swept.has_on_unbounded_side(b->point())) {
					single.blocked_by_degzero = true;
				}
				});

			if (single.blocked_by_degzero)
				return false;

			sqt->findOverlapped(rect, [this, &single](Edge_handle b) {
				if (single.is_blocked_by(b)) {
					data_traits.data(b).blocking.push_back(&single);
					single.blocked_by.push_back(b);
				}
				});

			return single.blocked_by.empty();
		}

		bool test_topology(Combo& combo) {
			// test if its blocked
			Rectangle<Kernel> rect = utils::boxOf(utils::boxOf(combo.prev_swept), utils::boxOf(combo.next_swept));

			combo.blocked_by_degzero = false;
			pqt->findContained(rect, [&combo](Vertex_handle b) {
				if (!combo.prev_swept.has_on_unbounded_side(b->point())
					|| !combo.next_swept.has_on_unbounded_side(b->point())) {
					combo.blocked_by_degzero = true;
				}
				});

			if (combo.blocked_by_degzero)
				return false;

			sqt->findOverlapped(rect, [this, &combo](Edge_handle b) {
				if (combo.is_blocked_by(b)) {
					data_traits.data(b).blocking.push_back(&combo);
					combo.blocked_by.push_back(b);
				}
				});

			return combo.blocked_by.empty();
		}

		Single* find_compensate_move(Single& contract, Number<Kernel> area) {
			// find compensating move		

			using namespace detail;

			Edge_handle walkBck = safe_prev(contract.edge);
			if (walkBck != nullptr) {
				walkBck = safe_prev(walkBck);
			}
			Edge_handle walkFwd = safe_next(contract.edge);
			if (walkFwd != nullptr) {
				walkFwd = safe_next(walkFwd);
			}

			const int reduc = contract.decrease_on_contract();

			// TODO: partial blocked...?
			while (walkBck != nullptr || walkFwd != nullptr) {
				if (walkBck != nullptr) {
					// take a step back and test
					Single* candidate = contract.left ? data_traits.data(walkBck).right : data_traits.data(walkBck).left;
					if (candidate->can_compensate_for(contract, reduc, area, data_traits)) {

						if (test_topology(*candidate)) {
							return candidate;
						}
						else {
							queue.remove(candidate);
						}
					}

					walkBck = safe_prev(walkBck);

					if (walkBck == walkFwd) {
						// cycled around, stop
						break;
					}
				}

				if (walkFwd != nullptr) {
					// take a step forward and test
					Single* candidate = contract.left ? data_traits.data(walkFwd).right : data_traits.data(walkFwd).left;
					if (candidate->can_compensate_for(contract, reduc, area, data_traits)) {

						if (test_topology(*candidate)) {
							return candidate;
						}
						else {
							queue.remove(candidate);
						}
					}
					walkFwd = safe_next(walkFwd);

					if (walkBck == walkFwd) {
						// cycled around, stop
						break;
					}
				}

			}
			return nullptr;
		}

		std::optional<Operation> findNextStep() {

			assert(graph.can_perform_operation());
			while (!queue.empty()) {
				Move* m = queue.peek();

				if (m->is_single()) {

					Single* sm = static_cast<Single*>(m);
					if (test_topology(*sm)) {

						Number<Kernel> area = sm->swept_area();

						if (area < score_traits.tiny_move()) { // TODO: can we avoid this check for exact mode?
							// tiny move, no compensation used
							return Operation(sm);
						}
						else {
							Single* compensate = find_compensate_move(*sm, area);
							if (compensate != nullptr) {
								return Operation(PairedSingles(sm, compensate));
							}
							else {
								data_traits.data(sm->edge->path()).add_waiting(sm);
							}
						}
					}
					// if it arrived here: not a valid operation, pop it
					queue.pop();
				}
				else {

					Combo* combo = static_cast<Combo*>(m);

					if (test_topology(*combo)) {
						return Operation(combo);
					}
					else {
						// not a valid operation, pop it
						queue.pop();
					}
				}
			}

			// no move exists
			return std::nullopt;
		}

		void start_move(Single& move) {
			using namespace detail;

			switch (move.src_type) {
			case DEG_TWO_SUPPORT:
			case DEG_TWO_NO_SUPPORT: {
				// skip
				break;
			}
			case DEG_THREE_SMOOTH: {
				// be sure to notice that the other edge will change
				//edgeset.add(move.edge->prev(!move.left));
				assert(edgeset.contains(move.edge->prev(!move.left)));
				break;
			}
			case DEG_THREE_SUPPORT: {
				Edge_handle other_prev = move.edge->prev(!move.left);
				bool add_inc = other_prev->source() == move.edge->source();
				Vertex_handle v = graph.subdivide_edge(other_prev, move.edge->source()->point(), !add_inc);
				edgeset.add(add_inc ? v->incoming() : v->outgoing());
				// NB: prev-edge on same face will be handled later
				break;
			}
			case DEG_THREE_ALIGNED:
			case DEG_THREE_NO_SUPPORT: {
				graph.subdivide_edge(move.edge, move.edge->source()->point(), false);
				// newedge is the prev edge, will be added later to edgeset
				edgeset.add(move.edge->prev());
				break;
			}
			default:
				assert(false); // unsupported vertex-moving type
			}

			switch (move.tar_type) {
			case DEG_TWO_SUPPORT:
			case DEG_TWO_NO_SUPPORT: {
				// skip
				break;
			}
			case DEG_THREE_SMOOTH: {
				// be sure to notice that the other edge will change
				//edgeset.add(move.edge->next(move.left));
				assert(edgeset.contains(move.edge->next(move.left)));
				break;
			}
			case DEG_THREE_SUPPORT: {
				Edge_handle other_next = move.edge->next(move.left);
				bool add_inc = other_next->source() == move.edge->target();
				Vertex_handle v = graph.subdivide_edge(other_next, move.edge->target()->point(), !add_inc);
				edgeset.add(add_inc ? v->incoming() : v->outgoing());
				// NB: next-edge on same face will be handled later
				break;
			}
			case DEG_THREE_ALIGNED:
			case DEG_THREE_NO_SUPPORT: {
				graph.subdivide_edge(move.edge, move.edge->target()->point(), true);
				// newedge is the next edge, will be added later to edgeset
				edgeset.add(move.edge->next());
				break;
			}
			default:
				assert(false); // unsupported vertex-moving type
			}
		}

		void contract_move(Single& move) {
			debug[1] << "Contracting " << *move.edge << (move.left ? " left" : " right") << " for area " << move.swept_area() << std::endl;

			start_move(move);

			Edge_handle prev = move.edge->prev(move.left);
			Edge_handle next = move.edge->next(!move.left);

			if (move.remove_self) {
				debug[2] << "  remove self" << std::endl;
				Vertex_handle v;
				if (move.edge->source()->degree() != 2) {
					// target must be degree-2
					v = move.edge->source();
					graph.merge_with_next(move.edge);
				}
				else {
					// source is degree-2
					v = move.edge->target();
					graph.merge_with_prev(move.edge);
				}
				graph.move_vertex(v, move.source_destination());

				assert(!move.remove_previous || !move.remove_next);

				if (move.remove_previous) {
					if (v->degree() == 2) {
						debug[2] << "  remove prev" << std::endl;
						graph.merge_with_next(prev);

						if (move.merge_previous) {
							debug[2] << "  merge prev" << std::endl;
							graph.merge_with_next(next->prev());
						}
					}
					else {
						debug[2] << "  remove prev, deg != 2" << std::endl;
						Vertex_handle pv = prev->other(v);
						assert(!move.merge_previous);
						assert(pv->degree() == 2);
						Edge_handle pe = graph.merge_vertex(pv, pv->outgoing() == prev); // make sure to erase prev. NB: combined with the move, the other edge at pv does not change
						graph.move_vertex(v, move.source_destination());
						//edgeset.add(pe);
					}
					//edgeset.add(next);
				}
				else if (move.remove_next) {
					if (v->degree() == 2) {
						debug[2] << "  remove next" << std::endl;
						graph.merge_with_prev(next);

						if (move.merge_next) {
							debug[2] << "  merge next" << std::endl;
							graph.merge_with_prev(prev->next());
						}
					}
					else {
						debug[2] << "  remove next, deg != 2" << std::endl;
						Vertex_handle nv = next->other(v);
						assert(!move.merge_next);
						assert(nv->degree() == 2);
						Edge_handle ne = graph.merge_vertex(nv, nv->outgoing() == next); // make sure to erase next. NB: combined with the move, the other edge at nv does not change
						graph.move_vertex(v, move.target_destination());
						//edgeset.add(ne);
					}
					//edgeset.add(prev);
				}
				else {
					//edgeset.addAll(v->incident_edges());
				}
			}
			else {
				//edgeset.add(move.edge);

				if (move.remove_previous) {
					Vertex_handle v = move.edge->source();
					if (v->degree() == 2) {
						debug[2] << "  remove prev" << std::endl;
						graph.merge_with_next(prev);
						if (move.merge_previous) {
							debug[2] << "  merge prev" << std::endl;
							graph.merge_with_next(move.edge->prev());
						}
					}
					else {
						debug[2] << "  remove prev, deg != 2" << std::endl;
						Vertex_handle pv = prev->other(v);
						assert(!move.merge_previous);
						assert(pv->degree() == 2);
						Edge_handle pe = graph.merge_vertex(pv, pv->outgoing() == prev); // make sure to erase prev. NB: combined with the move, the other edge at pv does not change
						graph.move_vertex(v, move.source_destination());
						//edgeset.add(pe);
					}
				}
				else {
					graph.move_vertex(move.edge->source(), move.source_destination());
					//edgeset.add(prev);
				}

				if (move.remove_next) {
					Vertex_handle v = move.edge->target();
					if (v->degree() == 2) {
						debug[2] << "  remove next" << std::endl;
						graph.merge_with_prev(next);
						if (move.merge_next) {
							debug[2] << "  merge next" << std::endl;
							graph.merge_with_prev(move.edge->next());
						}
					}
					else {
						debug[2] << "  remove next, deg != 2" << std::endl;
						Vertex_handle nv = next->other(v);
						assert(!move.merge_next);
						assert(nv->degree() == 2);
						Edge_handle ne = graph.merge_vertex(nv, nv->outgoing() == next); // make sure to erase next. NB: combined with the move, the other edge at nv does not change
						graph.move_vertex(v, move.target_destination());
						//edgeset.add(ne);
					}
				}
				else {
					graph.move_vertex(move.edge->target(), move.target_destination());
					//edgeset.add(next);
				}
			}
		}

		void move(Single& move, const Number<Kernel> area) {
			assert(area > 0);
			auto [s, t] = move.end_positions_for(area).value();
			this->move(move, s, t);
		}

		void move(Single& move, const Point<Kernel> source_dest, const Point<Kernel> target_dest) {
			debug[1] << "Moving " << *move.edge << (move.left ? " left" : " right") << std::endl;

			start_move(move);

			graph.move_vertex(move.edge->source(), source_dest);
			graph.move_vertex(move.edge->target(), target_dest);
			//edgeset.add(prev);
			//edgeset.add(move.edge);
			//edgeset.add(next);
		}

		void performStep(Single& contract, Single& compensate) {
			assert(graph.can_perform_operation());

			debug[1] << "Paired move" << std::endl;

			Number<Kernel> area = contract.swept_area();
			const bool contract_compensate = safe_test::leq(compensate.swept_area(), area);

			determineCheckout(contract, true);
			determineCheckout(compensate, contract_compensate);
			checkOut();

			graph.start_operation_group();
			this->contract_move(contract);
			if (contract_compensate) {
				this->contract_move(compensate);
			}
			else {
				this->move(compensate, area);
			}
			graph.end_operation_group();

			postProcess();
		}

		void performStep(Combo& combo) {
			assert(graph.can_perform_operation());

			debug[1] << "Combo move " << *combo.edge << " for area " << combo.swept_area() << std::endl;

			determineCheckout(*combo.prev_move, !combo.prev_partial);
			determineCheckout(*combo.next_move, !combo.next_partial);
			checkOut();

			// perform

			graph.start_operation_group();
			if (combo.remove_self) {
				// degeneracy handling
				combo.prev_move->remove_next = false;
				combo.next_move->remove_previous = false;
			}
			assert(combo.prev_partial || combo.next_partial || !combo.prev_move->remove_next || !combo.next_move->remove_previous); // cannot contract both ends and both remove the common edge?

			if (combo.prev_partial) {
				move(*combo.prev_move, combo.prev_source_destination(), combo.prev_target_destination());
			}
			else {
				contract_move(*combo.prev_move);
			}
			if (combo.next_partial) {
				move(*combo.next_move, combo.next_source_destination(), combo.next_target_destination());
			}
			else {
				contract_move(*combo.next_move);
			}
			assert(graph.edge(combo.edge->graph_index()) == combo.edge); // if violated, something goes wrong with degeneracy handling above?

			if (combo.remove_self) {
				debug[2] << "  combo: remove self" << std::endl;
				// NB: the two points should already be on top of eachother by the two moves
				Vertex_handle src = combo.edge->source();
				Vertex_handle tar = combo.edge->target();
				edgeset.remove(combo.edge);
				graph.merge_vertex(src, true); // removes combo.edge
				if (combo.merge_across) {
					debug[2] << "  combo: merge across" << std::endl;
					edgeset.remove(tar->outgoing());
					graph.merge_vertex(tar, true); // removes tar.outgoing
				}
			}
			graph.end_operation_group();

			postProcess();
		}

		void performStep(Single& contract) {
			assert(graph.can_perform_operation());

			debug[1] << "Tiny move" << std::endl;

			determineCheckout(contract, true);
			checkOut();

			graph.start_operation_group();
			contract_move(contract);
			graph.end_operation_group();

			postProcess();
		}

		void determineCheckout(Single& move, const bool contract) {
			// TODO: refine what actually changes around high-degree vertices? (is that possible...?)
			edgeset.addAll(move.edge->source()->incident_edges());
			edgeset.addAll(move.edge->target()->incident_edges());
			if (contract) {
				if (move.merge_previous) {
					edgeset.add(move.edge->prev()->prev());
				}
				else if (move.remove_previous && move.edge->source()->degree() != 2) {
					// vertex of deg 2 will be replaced by vertex of degree != 2
					Edge_handle pe = move.edge->prev(move.left);
					edgeset.add(pe->source() == move.edge->source() ? pe->next() : pe->prev());
				}

				if (move.merge_next) {
					edgeset.add(move.edge->next()->next());
				}
				else if (move.remove_next && move.edge->target()->degree() != 2) {
					// vertex of deg 2 will be replaced by vertex of degree != 2
					Edge_handle ne = move.edge->next(!move.left);
					edgeset.add(ne->source() == move.edge->target() ? ne->next() : ne->prev());
				}
			}
		}

		void checkOut() {
			//std::cout << "checking out..." << std::endl;
			for (Edge_handle e : edgeset) {
				//std::cout << "  " << *e << std::endl;
				sqt->remove(e);

				Edge_data& edata = data_traits.data(e);

				if (edata.left->movable()) {
					queue.remove(edata.left);
					if (edata.left->waiting_index >= 0) {
						data_traits.data(e->path()).remove_waiting(edata.left);
					}

					for (Edge_handle b : edata.left->blocked_by) {
						utils::listRemove<Move>(edata.left, data_traits.data(b).blocking);
					}
					edata.left->blocked_by.clear();
				}
				assert(!queue.contains(edata.left));

				if (edata.right->movable()) {
					queue.remove(edata.right);
					if (edata.right->waiting_index >= 0) {
						data_traits.data(e->path()).remove_waiting(edata.right);
					}

					for (Edge_handle b : edata.right->blocked_by) {
						utils::listRemove<Move>(edata.right, data_traits.data(b).blocking);
					}
					edata.right->blocked_by.clear();
				}
				assert(!queue.contains(edata.right));

				if (edata.combo->is_executable()) {
					queue.remove(edata.combo);

					for (Edge_handle b : edata.combo->blocked_by) {
						utils::listRemove<Move>(edata.combo, data_traits.data(b).blocking);
					}
					edata.combo->blocked_by.clear();
				}
				assert(!queue.contains(edata.combo));

				for (Move* move : edata.blocking) {
					assert(!queue.contains(move));

					utils::listRemove(e, move->blocked_by);

					if (!move->is_blocked() && !edgeset.contains(move->edge)) {

						if (move->is_single()) {
							Single* single = static_cast<Single*>(move);
							assert(single->waiting_index < 0);
							// NB: single move can also be blocked because it was checked as a compensating move, but not be actually contractable itself
							if (single->contractable())
								queue.push(move);

							// but it's movable in any case (otherwise, it would not have been blocked)
							// so it may be movable now, pop waiting from the other side
							auto& pdata = data_traits.data(single->edge->path());
							std::vector<Single*>& waiting = single->left ? pdata.right_waiting : pdata.left_waiting;
							for (Single* other : waiting) {
								if (!edgeset.contains(other->edge)) {
									queue.push(other);
								}
								other->waiting_index = -1;
							}
							waiting.clear();

						}
						else {
							// Combo move: would only have been blocked if it was indeed executable
							queue.push(move);
						}
					}
				}
				edata.blocking.clear();

			}
			//edgeset.removeAll();
		}

		void postProcess() {
			size_t changed = edgeset.size();

			for (size_t i = 0; i < changed; ++i) {

				Edge_handle e = edgeset[i];
				assert(graph.edge(e->graph_index()) == e);
				sqt->insert(e);
				edgeset.addAll(e->source()->incident_edges());
				edgeset.addAll(e->target()->incident_edges());
			}

			size_t movechanged = edgeset.size();
			for (size_t i = 0; i < movechanged; ++i) {
				Edge_handle e = edgeset[i];
				update_singles(e);
				edgeset.addAll(e->source()->incident_edges());
				edgeset.addAll(e->target()->incident_edges());
			}

			for (Edge_handle e : edgeset) {
				update_combo(e);
			}

			edgeset.removeAll();

			debug[0] << "--- reached " << graph.number_of_edges() << " edges -------------------------" << std::endl;
		}

	public:
		EdgeMoves(Graph& g) requires std::is_default_constructible<Data_traits>::value&& std::is_default_constructible<Score_traits>::value : graph(g) {
		}

		EdgeMoves(Graph& g, Score_traits score) requires std::is_default_constructible<Data_traits>::value : graph(g), score_traits(score) {
		}

		EdgeMoves(Graph& g, Data_traits data) requires std::is_default_constructible<Score_traits>::value : graph(g), data_traits(data) {
		}

		EdgeMoves(Graph& g, Data_traits data, Score_traits score) : graph(g), data_traits(data), score_traits(score) {
		}

		~EdgeMoves() {
			delete sqt;
			delete pqt;
		}

		void initialize(const int& treedepth, const Number<Kernel> fuzziness) {
			data_traits.initialize(graph);

			// erase deg-2 aligned vertices
			assert(graph.is_initialized());
			assert(graph.can_perform_operation());

			graph.start_operation_group();
			size_t i = 0;
			while (i < graph.number_of_vertices()) {
				Vertex_handle v = graph.vertex(i);

				if (v->degree() == 2 && safe_test::collinear(v->prev()->point(), v->point(), v->next()->point())) {
					graph.merge_vertex(v);
				}
				else {
					++i;
				}
			}
			graph.end_operation_group();

			// initialize datastructures
			Rectangle<Kernel> box = graph.bounding_rectangle();
			sqt = new EdgeTree(box, treedepth, fuzziness);
			for (Edge_handle e : graph.edges()) {
				sqt->insert(e);
			}

			pqt = new VertexTree(box, treedepth);
			for (Vertex_handle v : graph.vertices()) {
				if (v->degree() == 0) {
					pqt->insert(v);
				}
			}

			// initialize move data
			for (Edge_handle e : graph.edges()) {
				update_singles(e);
			}
			for (Edge_handle e : graph.edges()) {
				update_combo(e);
			}

			assert(validate_state());
		}

		bool run(std::function<bool(int, Number<Kernel>)> stop) {
			assert(graph.can_perform_operation());
			assert(validate_state());

			while (true) {

				std::optional<Operation> next = findNextStep();
				if (!next) {
					return false;
				}

				Operation op = next.value();

				if (std::holds_alternative<Combo*>(op)) {
					Combo* combo = std::get<Combo*>(op);
					if (stop(graph.number_of_edges(), combo->cost)) {
						return true;
					}
					performStep(*combo);
				}
				else if (std::holds_alternative<PairedSingles>(op)) {
					auto [contract, compensate] = std::get<PairedSingles>(op);
					if (stop(graph.number_of_edges(), contract->cost)) {
						return true;
					}
					performStep(*contract, *compensate);
				}
				else {
					Single* contract = std::get<Single*>(op);
					if (stop(graph.number_of_edges(), contract->cost)) {
						return true;
					}
					performStep(*contract);
				}

				assert(validate_state());
			}
		}

		bool step() {
			assert(graph.can_perform_operation());
			assert(validate_state());

			std::optional<Operation> next = findNextStep();
			if (!next) {
				return false;
			}

			Operation op = next.value();

			if (std::holds_alternative<Combo*>(op)) {
				Combo* combo = std::get<Combo*>(op);
				performStep(*combo);
			}
			else if (std::holds_alternative<PairedSingles>(op)) {
				auto [contract, compensate] = std::get<PairedSingles>(op);
				performStep(*contract, *compensate);
			}
			else {
				Single* contract = std::get<Single*>(op);
				performStep(*contract);
			}

			assert(validate_state());
			return true;
		}
	};

	template<DataTraits DT, bool AllowFiniteNoSupport>
	class SweptAreaScore {
	public:
		using Graph = DT::Graph;
		using Edge_handle = Graph::Vertex_handle;
		using Kernel = Graph::Kernel;

		using Single = DT::Edge_data::Single;
		using Combo = DT::Edge_data::Combo;

		static void initialize(Graph& graph, DT& data) {
			// skip
		}

		constexpr static bool allow_finite_unsupported() {
			return AllowFiniteNoSupport;
		}

		static Number<Kernel> tiny_move() {
			return M_EPSILON;
		}

		static Number<Kernel> compute_cost(Single& single) {
			return CGAL::abs(single.swept.area());
		}

		static Number<Kernel> compute_cost(Combo& combo) {
			return CGAL::abs(combo.prev_swept.area());
		}
	};
}