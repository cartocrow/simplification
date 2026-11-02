#pragma once

#include "common.h"

namespace cartocrow::simplification::em {

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

	template<class EdgeHandle> 
	struct BaseMove {

		virtual ~BaseMove() {} // necessary for inheritance to work...

		using Kernel = EdgeHandle::Kernel;
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

	template<class EdgeHandle> struct SingleMove : public BaseMove<EdgeHandle> {

		using Kernel = EdgeHandle::Kernel;
		using Edge_handle = EdgeHandle;

		bool left;
		enum VertexType src_type, tar_type;
		bool remove_self, remove_previous, remove_next;
		bool merge_previous, merge_next;
		Polygon<Kernel> swept;
		bool contract_merges_highdegrees;
		int waiting_index = -1;

		bool is_single() const override {
			return true;
		}

		//bool is_blocked_by(Edge_handle candidate) const {
		//	return is_blocked_by(candidate, swept, true);
		//}

		//bool is_blocked_by(Edge_handle candidate, const Polygon<Kernel>& swept_polygon, const bool contract) const {
		//	if (this->edge->common_vertex(candidate) != nullptr) {
		//		// shares a vertex with the moving edge, part of configuration
		//		return false;
		//	}

		//	Edge_handle prev = this->edge->prev(left);
		//	Vertex_handle exclude = prev->common_vertex(candidate);

		//	Edge_handle next = this->edge->next(!left);
		//	Vertex_handle next_exclude = next->common_vertex(candidate);

		//	if (next_exclude != nullptr) {
		//		if (exclude != nullptr) {
		//			assert(contract);
		//			// convex quad, just make sure candidate is not aligned with edge itself
		//			Vector<K> dir_edge(this->edge->curve());
		//			Vector<K> dir_cand(candidate->curve());
		//			return safe_test::aligned_or_opposite_vectors(dir_edge, dir_cand);
		//		}
		//		else {
		//			exclude = next_exclude;
		//			// exclude comes from next, make sure it doesnt align with edge itself on contraction
		//			if (contract && this->remove_next) {
		//				// moving left: edge to next makes a left turn. test if next to candidate also makes a left turn
		//				// moving right: edge to next makes a right turn, test if next to candidate also makes a right turn
		//				if (left == CGAL::left_turn(this->edge->target()->point(), exclude->point(), candidate->other(exclude)->point())) {
		//					// same turns
		//					Vector<K> dir_edge(this->edge->curve());
		//					Vector<K> dir_cand(candidate->curve());
		//					return safe_test::aligned_or_opposite_vectors(dir_edge, dir_cand);
		//				}
		//			}
		//		}
		//	}
		//	else if (contract && exclude != nullptr && this->remove_previous) {
		//		// exclude comes from prev, make sure it doesnt align with edge itself on contraction
		//			// moving left: edge to prev makes a right turn. test if prev to candidate also makes a right turn
		//			// moving right: edge to prev makes a left turn, test if prev to candidate also makes a left turn
		//		if (left == CGAL::right_turn(this->edge->target()->point(), exclude->point(), candidate->other(exclude)->point())) {
		//			// same turns
		//			Vector<K> dir_edge(this->edge->curve());
		//			Vector<K> dir_cand(candidate->curve());
		//			return safe_test::aligned_or_opposite_vectors(dir_edge, dir_cand);
		//		}
		//	}

		//	if (exclude != candidate->source() && safe_test::convex_contains(candidate->source()->point(), swept_polygon)) {
		//		return true;
		//	}

		//	if (exclude != candidate->target() && safe_test::convex_contains(candidate->target()->point(), swept_polygon)) {
		//		return true;
		//	}

		//	const Segment<K> cc = candidate->curve();
		//	for (const Segment<K> e : swept_polygon.edges()) {
		//		auto is = CGAL::intersection(e, cc);
		//		if (is) {
		//			if (exclude != nullptr && std::holds_alternative<Point<K>>(is.value())) {
		//				// make sure its not the endpoint (we could have just touched the corner of swept)
		//				if (!safe_test::same_point(exclude->point(), std::get<Point<K>>(is.value()))) {
		//					return true;
		//				}
		//			}
		//			else {
		//				// anything beyond a point must be a proper intersect
		//				return true;
		//			}
		//		}
		//	}

		//	// no intersections were found, not blocking
		//	return false;
		//}

		//bool can_compensate_for(const SingleMove<Kernel>& contract, const int reduc, const Number<Kernel> area) const {
		//	if (this->is_blocked()) {
		//		return false;
		//	}

		//	if (!movable()) {
		//		// cannot move
		//		return false;
		//	}

		//	Number<Kernel> swept = swept_area();
		//	if (area > swept) {
		//		// too small
		//		return false;
		//	}
		//	else if (swept == area) {
		//		// contract
		//		if (!this->contract_merges_highdegrees) {
		//			// not allowed to contract
		//			return false;
		//		}

		//		if (reduc + decrease_on_contract() <= 0) {
		//			// no progress
		//			return false;
		//		}
		//	}
		//	else {
		//		// move only
		//		if (increase_on_move() >= reduc) {
		//			// no progress
		//			return false;
		//		}
		//	}

		//	if (contract.edge->common_vertex(this->edge) != nullptr) {
		//		// same or neighboring edges
		//		return false;
		//	}

		//	Edge_handle next = safe_next(this->edge);
		//	if (next != nullptr && next->common_vertex(contract.edge) != nullptr) {
		//		// sharing next edge (== prev edge of contract)
		//		if (contract.remove_previous) {
		//			// but contract is relying on its removal
		//			return false;
		//		}
		//		else {
		//			// make sure its not convex-reflex sequence
		//			bool left_at_source = CGAL::left_turn(this->edge->source()->point(), next->source()->point(), next->target()->point());
		//			bool right_at_target = CGAL::right_turn(next->source()->point(), next->target()->point(), contract.edge->target()->point());

		//			if (left_at_source == right_at_target) {
		//				// different turns, disallow, and leave to ComboMove
		//				return false;
		//			}
		//		}
		//	}

		//	Edge_handle prev = safe_prev(this->edge);
		//	if (prev != nullptr && prev->common_vertex(contract.edge) != nullptr) {
		//		// sharing previous edge (== next edge of contract)
		//		if (contract.remove_next) {
		//			// but contract is relying on its removal
		//			return false;
		//		}
		//		else {
		//			// make sure its not convex-reflex sequence
		//			bool left_at_source = CGAL::left_turn(contract.edge->source()->point(), prev->source()->point(), prev->target()->point());
		//			bool right_at_target = CGAL::right_turn(prev->source()->point(), prev->target()->point(), this->edge->target()->point());

		//			if (left_at_source == right_at_target) {
		//				// different turns, disallow, and leave to CombinedMove
		//				return false;
		//			}
		//		}
		//	}

		//	return true;
		//}

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

		//void update() {
		//	swept.clear();

		//	Vertex_handle a = nullptr, d = nullptr;
		//	Vertex_handle a_alt = nullptr, d_alt = nullptr;

		//	Vertex_handle b = this->edge->source();
		//	Vertex_handle c = this->edge->target();

		//	Line<K> baseline = Line<K>(b->point(), c->point());

		//	// determine start type
		//	switch (b->degree()) {
		//	case 2: {
		//		a = b->prev();
		//		if (left == CGAL::left_turn(a->point(), b->point(), c->point())) {
		//			src_type = detail::DEG_TWO_SUPPORT;
		//		}
		//		else {
		//			src_type = detail::DEG_TWO_NO_SUPPORT;
		//		}
		//		break;
		//	}
		//	case 3: {
		//		size_t edge_index = b->find_incident_index(this->edge);
		//		a = b->neighboring_incident_edge(edge_index, left)->other(b);
		//		a_alt = b->neighboring_incident_edge(edge_index, !left)->other(b);

		//		if (safe_test::point_on_line(a->point(), baseline)) {
		//			src_type = detail::DEG_THREE_ALIGNED;
		//		}
		//		else if (left == CGAL::left_turn(a->point(), b->point(), c->point())) {
		//			if (safe_test::collinear(a->point(), b->point(), a_alt->point())) {
		//				src_type = detail::DEG_THREE_SMOOTH;
		//			}
		//			else {
		//				src_type = detail::DEG_THREE_SUPPORT;
		//			}
		//		}
		//		else {
		//			src_type = detail::DEG_THREE_NO_SUPPORT;
		//		}
		//		break;
		//	}
		//	default:
		//		src_type = detail::UNMOVABLE;
		//		break;
		//	}

		//	// determine end type
		//	switch (c->degree()) {
		//	case 2: {
		//		d = c->next();
		//		if (left == CGAL::left_turn(b->point(), c->point(), d->point())) {
		//			tar_type = detail::DEG_TWO_SUPPORT;
		//		}
		//		else {
		//			tar_type = detail::DEG_TWO_NO_SUPPORT;
		//		}
		//		break;
		//	}
		//	case 3: {
		//		size_t edge_index = c->find_incident_index(this->edge);
		//		d = c->neighboring_incident_edge(edge_index, !left)->other(c);
		//		d_alt = c->neighboring_incident_edge(edge_index, left)->other(c);
		//		if (safe_test::point_on_line(d->point(), baseline)) {
		//			tar_type = detail::DEG_THREE_ALIGNED;
		//		}
		//		else if (left == CGAL::left_turn(b->point(), c->point(), d->point())) {
		//			if (safe_test::collinear(c->point(), d->point(), d_alt->point())) {
		//				tar_type = detail::DEG_THREE_SMOOTH;
		//			}
		//			else {
		//				tar_type = detail::DEG_THREE_SUPPORT;
		//			}
		//		}
		//		else {
		//			tar_type = detail::DEG_THREE_NO_SUPPORT;
		//		}
		//		break;
		//	}
		//	default:
		//		tar_type = detail::UNMOVABLE;
		//		break;
		//	}

		//	if (!movable()) {
		//		return;
		//	}

		//	// how far can we move along the previous track before the previous edge vanishes
		//	Number<K> prev_dist_squared;
		//	switch (src_type) {
		//	case DEG_TWO_SUPPORT:
		//	case DEG_THREE_SMOOTH:
		//	case DEG_THREE_SUPPORT:
		//		prev_dist_squared = CGAL::squared_distance(baseline, a->point());
		//		break;
		//	default:
		//		prev_dist_squared = -1;
		//		break;
		//	}

		//	// how far can we move along the next track before the next edge vanishes
		//	Number<K> next_dist_squared;
		//	switch (tar_type) {
		//	case DEG_TWO_SUPPORT:
		//	case DEG_THREE_SMOOTH:
		//	case DEG_THREE_SUPPORT:
		//		next_dist_squared = CGAL::squared_distance(baseline, d->point());
		//		break;
		//	default:
		//		next_dist_squared = -1;
		//		break;
		//	}

		//	// how far can we move until the edge itself vanishes
		//	Line<K> prev_track = Line<K>(b->point(), (src_type == detail::DEG_THREE_ALIGNED ? a_alt : a)->point());
		//	Line<K> next_track = Line<K>(c->point(), (tar_type == detail::DEG_THREE_ALIGNED ? d_alt : d)->point());

		//	Number<K> edge_dist_squared = -1;
		//	Point<K> edge_lim;

		//	{
		//		const auto is = CGAL::intersection(prev_track, next_track);
		//		if (is) {
		//			if (const Point<K>* pt = std::get_if<Point<K>>(&*is)) {
		//				edge_lim = *pt;
		//				if (left && baseline.oriented_side(edge_lim) != CGAL::ON_NEGATIVE_SIDE) {
		//					edge_dist_squared = CGAL::squared_distance(baseline, edge_lim);
		//				}
		//				else if (!left && baseline.oriented_side(edge_lim) != CGAL::ON_POSITIVE_SIDE) {
		//					edge_dist_squared = CGAL::squared_distance(baseline, edge_lim);
		//				}
		//			}
		//		}
		//	}

		//	// which is the limiting factor? and which edges then get removed?
		//	if (prev_dist_squared < 0 && next_dist_squared < 0) {
		//		// no support at all
		//		// TODO: refine, to allow converging unsupported moves?
		//		src_type = detail::UNMOVABLE;
		//		tar_type = detail::UNMOVABLE;
		//		return;
		//	}

		//	assert(prev_dist_squared >= 0 || next_dist_squared >= 0); // NB: if TODO above is resolved, this may fail

		//	Number<K> min_dist_squared;
		//	if (prev_dist_squared < 0) {
		//		min_dist_squared = next_dist_squared;
		//	}
		//	else if (next_dist_squared < 0) {
		//		min_dist_squared = prev_dist_squared;
		//	}
		//	else {
		//		min_dist_squared = CGAL::min(prev_dist_squared, next_dist_squared);
		//	}

		//	if (edge_dist_squared >= 0) {
		//		min_dist_squared = CGAL::min(min_dist_squared, edge_dist_squared);
		//	}

		//	remove_self = safe_test::close(edge_dist_squared, min_dist_squared);
		//	remove_previous = safe_test::close(prev_dist_squared, min_dist_squared);
		//	remove_next = safe_test::close(next_dist_squared, min_dist_squared);

		//	// determine if the previous edge allows merging
		//	if (remove_previous && b->degree() == 2 && a->degree() == 2) {
		//		if (remove_self) {
		//			// check if double-prev can merge with next
		//			merge_previous = safe_test::aligned_vectors(a->point() - a->prev()->point(),
		//				d->point() - c->point());
		//		}
		//		else {
		//			// check alignment with self
		//			merge_previous = safe_test::aligned_vectors(a->point() - a->prev()->point(),
		//				c->point() - b->point());
		//		}
		//	}
		//	else {
		//		merge_previous = false;
		//	}

		//	// determine if the next edge allows merging
		//	if (remove_next && c->degree() == 2 && d->degree() == 2) {

		//		if (remove_self) {
		//			// check if double-next can merge with prev
		//			merge_next = safe_test::aligned_vectors(d->next()->point() - d->point(),
		//				b->point() - a->point());
		//		}
		//		else {
		//			// check alignment with self
		//			merge_next = safe_test::aligned_vectors(d->next()->point() - d->point(),
		//				c->point() - b->point());
		//		}
		//	}
		//	else {
		//		merge_next = false;
		//	}

		//	// construct the swept area
		//	if (remove_self) {
		//		swept.push_back(edge_lim);
		//	}
		//	else if (remove_previous) {
		//		swept.push_back(a->point());
		//	}
		//	else {
		//		assert(remove_next);
		//		// force using the actual intersection, we (should) have eliminated exactly equal directions
		//		const auto is = CGAL::intersection(Line<K>(d->point(), c->point() - b->point()), prev_track);
		//		swept.push_back(*std::get_if<Point<K>>(&*is));
		//	}
		//	swept.push_back(b->point());
		//	swept.push_back(c->point());
		//	if (remove_self) {
		//		// do nothing, triangular area
		//	}
		//	else if (remove_next) {
		//		swept.push_back(d->point());
		//	}
		//	else {
		//		assert(remove_previous);
		//		// force using the actual intersection, we (should) have eliminated exactly equal directions
		//		const auto is = CGAL::intersection(Line<K>(a->point(), c->point() - b->point()), next_track);
		//		swept.push_back(*std::get_if<Point<K>>(&*is));
		//	}

		//	// precision, be sure we dont want to remove self...  
		//	if (!remove_self && safe_test::same_point(source_destination(), target_destination())) {
		//		remove_self = true;
		//		swept.erase(--swept.vertices_end());
		//	}

		//	if (remove_self && b->degree() != 2 && c->degree() != 2) {
		//		contract_merges_highdegrees = true;
		//	}
		//	else if (remove_previous && a->degree() != 2 && b->degree() != 2) {
		//		contract_merges_highdegrees = true;
		//	}
		//	else if (remove_next && c->degree() != 2 && d->degree() != 2) {
		//		contract_merges_highdegrees = true;
		//	}
		//	else {
		//		contract_merges_highdegrees = false;
		//	}
		//}

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

	template<class EdgeHandle> struct ComboMove : public BaseMove<EdgeHandle> {

		using Kernel = EdgeHandle::Kernel;
		using Edge_handle = EdgeHandle;
		using Single = SingleMove<EdgeHandle>;

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

		//bool is_blocked_by(Edge_handle candidate) const {

		//	return prev_move->is_blocked_by(candidate, prev_swept, !prev_partial)
		//		|| next_move->is_blocked_by(candidate, next_swept, !next_partial);
		//}

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

		//void update() {

		//	size_t edgeindex = this->edge->graph_index();

		//	prev_swept.clear();
		//	next_swept.clear();

		//	if (this->edge->source()->degree() != 2 || this->edge->target()->degree() != 2) {
		//		executable = false;
		//		return;
		//	}

		//	Edge_handle prev = this->edge->prev();
		//	Edge_handle next = this->edge->next();

		//	bool left_at_source = CGAL::left_turn(prev->source()->point(), this->edge->source()->point(), this->edge->target()->point());
		//	bool right_at_target = CGAL::right_turn(this->edge->source()->point(), this->edge->target()->point(), next->target()->point());

		//	if (left_at_source != right_at_target) {
		//		// same turn
		//		executable = false;
		//		return;
		//	}

		//	// TODO: use traits
		//	if (left_at_source) {
		//		prev_move = &(prev->data().left);
		//		next_move = &(next->data().right);
		//	}
		//	else {
		//		prev_move = &(prev->data().right);
		//		next_move = &(next->data().left);
		//	}

		//	if (!prev_move->movable() || !next_move->movable()) {
		//		executable = false;
		//		return;
		//	}

		//	// TODO: reduce approximate use?

		//	// now, determine how far both need to move until the common edge vanishes
		//	Vector<Inexact> edgedir = approximate(this->edge->target()->point() - this->edge->source()->point());
		//	Number<Inexact> edge_length = std::sqrt(edgedir.squared_length());
		//	edgedir /= edge_length;

		//	Vector<Inexact> prev_movedir = prev_move->move_direction();
		//	Number<Inexact> prev_len = prev_move->base_length();
		//	Number<Inexact> prev_len_end = prev_move->end_length();
		//	Vector<Inexact> prev_common_vector = approximate(prev_move->target_move());
		//	Number<Inexact> prev_s = 1.0 / CGAL::scalar_product(edgedir, prev_movedir); // NB: signswap between sides
		//	Number<Inexact> prev_lim_dist = CGAL::scalar_product(prev_movedir, prev_common_vector);
		//	Number<Inexact> prev_r = (prev_len_end - prev_len) / prev_lim_dist;

		//	Vector<Inexact> next_movedir = next_move->move_direction();
		//	Number<Inexact> next_len = next_move->base_length();
		//	Number<Inexact> next_len_end = next_move->end_length();
		//	Vector<Inexact> next_common_vector = approximate(next_move->source_move());
		//	Number<Inexact> next_s = -1.0 / CGAL::scalar_product(edgedir, next_movedir); // NB: signswap between sides
		//	Number<Inexact> next_lim_dist = CGAL::scalar_product(next_movedir, next_common_vector);
		//	Number<Inexact> next_r = (next_len_end - next_len) / next_lim_dist;

		//	// edge left/right of length _len expands at rate _r, covering the common edge at rate _s
		//	// so, moving a distance _d sweeps area
		//	//    _d * (_len + _d * _r / 2)
		//	// and covers the common edge for
		//	//    _d * _s;
		//	//
		//	// we need the swept areas to match, and the common edge to be fully covered
		//	//    p_d * p_s + n_d * n_s = e_len
		//	//    p_d * (p_len + p_d * p_r / 2) = n_d * (n_len + n_d * n_r / 2)
		//	// the former gives
		//	//    p_d = (e_len - n_d * n_s) / p_s
		//	// substituting in the latter and rewriting to quadratic form
		//	//     n_d^2 * (n_s ^ 2 * p_r / (2 * p_s ^ 2) - n_r / 2)
		//	//     + n_d (- n_s * p_len / p_s - e_len *  n_s * p_r / p_s^2 - n_len)
		//	//     + (e_len * p_len / p_s + e_len^2 * p_r / (2 * p_s^2)  )
		//	//     = 0
		//	// find smallest positive solution for n_d
		//	Number<Inexact> a = prev_r * next_s * next_s / (2 * prev_s * prev_s) - next_r / 2.0;
		//	Number<Inexact> b = -prev_len * next_s / prev_s - next_len - prev_r * next_s * edge_length / (prev_s * prev_s);
		//	Number<Inexact> c = prev_len * edge_length / prev_s + prev_r * edge_length * edge_length / (2 * prev_s * prev_s);

		//	Number<Inexact> next_d;
		//	if (safe_test::close(a, 0) && safe_test::close(b, 0) && safe_test::close(c, 0)) {
		//		// we'll get division by zero / NaNs... but if c is also zero, then things just line up perfectly...
		//		next_d = next_lim_dist;
		//	}
		//	else {
		//		next_d = utils::solveQuadraticEquationForSmallestPositive(a, b, c); // NB: will be <= 0 if there is no positive solution
		//	}
		//	Number<Inexact> prev_d = (edge_length - next_d * next_s) / prev_s;

		//	// we need to make sure that after the move, no (approximately) zero-length edges remain
		//	// there are a couple of cases to consider:
		//	// if _d > _lim_dist, then the move contracts before the edges meet
		//	// if _d = _lim_dist, then the move contracts as the edges meet
		//	// if _d < _lim_dist, then the move only has to do a partial move, for the edges to meet
		//	// now, if ONE of the edges contracts before meeting, then this effectively is just a pair of edge moves
		//	// otherwise, we need to account for the meeting point, and the possibility of either move still contracting
		//	prev_swept = prev_move->swept;
		//	next_swept = next_move->swept;

		//	if (next_d <= 0 || prev_d > prev_lim_dist + M_EPSILON || next_d > next_lim_dist + M_EPSILON) {
		//		// just a pair

		//		Number<K> prev_area = CGAL::abs(prev_move->swept.area());
		//		Number<K> next_area = CGAL::abs(next_move->swept.area());
		//		if (prev_area < next_area) {
		//			// prev contracts
		//			if (!prev_move->contractable()) {
		//				executable = false;
		//				return;
		//			}
		//			prev_partial = false;

		//			// and possibly next
		//			auto startend = next_move->end_positions_for(prev_area);
		//			if (!startend || safe_test::same_point(next_swept[0], startend->first)) {
		//				if (!next_move->contractable()) {
		//					executable = false;
		//					return;
		//				}
		//				next_partial = false;
		//			}
		//			else {
		//				next_partial = true;
		//				next_swept[0] = startend->first;
		//				if (next_swept.size() < 4) {
		//					next_swept.push_back(startend->second);
		//				}
		//				else {
		//					next_swept[3] = startend->second;
		//				}
		//			}

		//		}
		//		else {
		//			// next contract
		//			if (!next_move->contractable()) {
		//				executable = false;
		//				return;
		//			}
		//			next_partial = false;

		//			// and possibly prev
		//			auto startend = prev_move->end_positions_for(next_area);
		//			if (!startend || safe_test::same_point(prev_swept[0], startend->first)) {
		//				if (!prev_move->contractable()) {
		//					executable = false;
		//					return;
		//				}
		//				prev_partial = false;
		//			}
		//			else {
		//				prev_partial = true;
		//				prev_swept[0] = startend->first;
		//				if (prev_swept.size() < 4) {
		//					prev_swept.push_back(startend->second);
		//				}
		//				else {
		//					prev_swept[3] = startend->second;
		//				}
		//			}
		//		}

		//		// handling precision issues: if one is trying to remove the common edge, we should just let it be handled by the combined move
		//		remove_self = (!prev_partial && prev_move->remove_next) || (!next_partial && next_move->remove_previous);
		//	}
		//	else {
		//		// construct the meeting point
		//		remove_self = true;

		//		Number<K> prev_ratio = convert_kernel<K>(CGAL::min(1.0, prev_d / prev_lim_dist));
		//		Point<K> merge_point = this->edge->source()->point() + prev_ratio * convert_kernel<K>(prev_common_vector);
		//		Point<K> prev_other_end = prev_move->swept[1] + prev_ratio * prev_move->source_move();

		//		//std::cout << " meeting point " << merge_point << std::endl;
		//		prev_partial = !safe_test::same_point(merge_point, prev_swept[prev_swept.size() == 3 ? 0 : 3])
		//			&& !safe_test::same_point(merge_point, prev_other_end)
		//			&& !safe_test::same_point(prev_other_end, prev_move->source_destination());
		//		if (prev_partial) {
		//			prev_swept[0] = prev_other_end;
		//			if (prev_swept.size() < 4) {
		//				prev_swept.push_back(merge_point);
		//			}
		//			else {
		//				prev_swept[3] = merge_point;
		//			}
		//		}
		//		else if (!prev_move->contractable()) {
		//			executable = false;
		//			return;
		//		}

		//		Number<K> next_ratio = convert_kernel<K>(CGAL::min(1.0, next_d / next_lim_dist));
		//		Point<K> next_other_end = next_move->swept[2] + next_ratio * next_move->target_move();

		//		next_partial = !safe_test::same_point(merge_point, next_swept[0])
		//			&& !safe_test::same_point(merge_point, next_other_end)
		//			&& !safe_test::same_point(next_other_end, next_move->target_destination());
		//		if (next_partial) {

		//			next_swept[0] = merge_point;
		//			if (next_swept.size() < 4) {
		//				next_swept.push_back(next_other_end);
		//			}
		//			else {
		//				next_swept[3] = next_other_end;
		//			}
		//		}
		//		else if (!next_move->contractable()) {
		//			executable = false;
		//			return;
		//		}
		//	}

		//	if (remove_self) {
		//		Vector<K> prevdir_at_join;
		//		if (prev_swept.size() == 3) {
		//			prevdir_at_join = prev_swept[0] - prev_swept[1];
		//		}
		//		else {
		//			prevdir_at_join = prev_swept[2] - prev_swept[1];
		//		}

		//		Vector<K> nextdir_at_join;
		//		if (next_swept.size() == 3) {
		//			nextdir_at_join = next_swept[0] - next_swept[2];
		//		}
		//		else {
		//			nextdir_at_join = next_swept[2] - next_swept[1];
		//		}

		//		//std::cout << prevdir_at_join << "  ||  " << nextdir_at_join << std::endl;
		//		//std::cout << " -> det = " << CGAL::determinant(prevdir_at_join, nextdir_at_join) << std::endl;
		//		merge_across = safe_test::aligned_vectors(prevdir_at_join, nextdir_at_join);
		//		//std::cout << " merge_across = " << merge_across << std::endl;
		//	}
		//	else {
		//		merge_across = false;
		//	}

		//	int reduc = prev_partial ? -prev_move->increase_on_move() : prev_move->decrease_on_contract();
		//	reduc += next_partial ? -next_move->increase_on_move() : next_move->decrease_on_contract();
		//	if (remove_self) reduc++;
		//	if (merge_across) reduc++;

		//	executable = reduc > 0;
		//}

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
		using Edge_handle = Graph_2_edge<VertexData, Graph_edge_data, Graph_path_data, Graph_segment_curve_traits_2<K>, GraphTraits>*; // needed to avoid co-dependent templates for dedicated graphs

		SingleMove<Edge_handle> left, right;
		ComboMove<Edge_handle> combo;
		std::vector<BaseMove<Edge_handle>*> blocking;
	};


	template<bool UseSelfEdgeData, class VertexData, class ActualEdgeData, bool UseSelfPathData, class ActualPathData, KernelType K, class GraphTraits>
	struct PathData {
		using Graph_edge_data = EdgeData<UseSelfEdgeData, VertexData, ActualEdgeData, UseSelfPathData, ActualPathData, K, GraphTraits>::Graph_edge_data;
		using Graph_path_data = std::conditional<UseSelfPathData, PathData, ActualPathData>::type;

		using Kernel = K;
		using Edge_handle = Graph_2_edge<VertexData, Graph_edge_data, Graph_path_data, Graph_segment_curve_traits_2<K>, GraphTraits>*; // needed to avoid co-dependent templates for dedicated graphs

		using Single = SingleMove<Edge_handle>;

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
		requires(ST score, SingleMove<typename ST::Kernel>& single, ComboMove<typename ST::Kernel>& combo) {

		typename ST::Kernel;

		{ score.compute_cost(single) };
		{ score.compute_cost(combo) };
	};

	template<class DT, class ST>
	concept Setup =
		DataTraits<DT> && ScoreTraits<ST> &&
		std::is_same<typename ST::Kernel, typename DT::Kernel>::value;

	template<KernelType K, bool Historic>
	class DataTraits_Dedicated {
	public:
		using Kernel = K;
		using Edge_data = EdgeData<true, std::monostate, std::monostate, true, std::monostate, Kernel, DecomposedGraph<Historic>>;
		using Path_data = PathData<true, std::monostate, std::monostate, true, std::monostate, Kernel, DecomposedGraph<Historic>>;
		using Graph = Straight_graph_2<std::monostate, Edge_data, Path_data, Kernel, DecomposedGraph<Historic>>;

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

	template<KernelType Kernel>
	struct MoveQueueTraits {

		using Element_handle = BaseMove<Kernel>*;

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
	class EdgeMove {
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
		using Move = BaseMove<Kernel>;
		using Single = SingleMove<Kernel>;
		using Combo = ComboMove<Kernel>;
		using Queue = cartocrow::IndexedPriorityQueue<MoveQueueTraits<Kernel>>;

		using PairedSingles = std::pair<Single*, Single*>;
		using Operation = std::variant<Combo*, PairedSingles, Single*>;

		using VertexTree = VertexQuadTree<Graph>;
		using EdgeTree = EdgeQuadTree<Graph>;

		Graph& graph;
		EdgeTree* sqt = nullptr;
		VertexTree* pqt = nullptr;
		Data_traits data_traits;
		Score_traits score_traits;
		Queue queue = Queue(data_traits);
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
			for (void* bvoid : single.blocked_by) {
				Edge_handle b = static_cast<Edge_handle>(bvoid);
				utils::listRemove<Move>(&single, data_traits.data(b).blocking);
			}
			single.blocked_by.clear();
			single.left = LEFT;

			if (single.waiting_index >= 0) {
				data_traits.data(e->path()).remove_waiting(&single);
				single.waiting_index = -1;
			}

			single.update();

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

		void update_singles(Edge_handle e);
		void update_combo(Edge_handle e);
		bool test_topology(Single& single);
		bool test_topology(Combo& combo);
		Single* find_compensate_move(Single& contract, Number<Kernel> area);

		std::optional<Operation> findNextStep();
		void start_move(Single& move);
		void contract(Single& move);
		void move(Single& move, const Number<Kernel> area);
		void move(Single& move, const Point<Kernel> source_dest, const Point<Kernel> target_dest);
		void performStep(Single& contract, Single& compensate);
		void performStep(Combo& combo);
		void performStep(Single& contact);

		void determineCheckout(Single& move, const bool contract);
		void checkOut();
		void postProcess();

	public:
		EdgeMove(Graph& g) requires std::is_default_constructible<Data_traits>::value&& std::is_default_constructible<Score_traits>::value : graph(g) {
		}

		EdgeMove(Graph& g, Score_traits score) requires std::is_default_constructible<Data_traits>::value : graph(g), score_traits(score) {
		}

		EdgeMove(Graph& g, Data_traits data) requires std::is_default_constructible<Score_traits>::value : graph(g), data_traits(data) {
		}

		EdgeMove(Graph& g, Data_traits data, Score_traits score) : graph(g), data_traits(data), score_traits(score) {
		}

		~EdgeMove() {
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
}

namespace cartocrow::simplification {

	namespace detail {

		template<class G> class EdgeSet;

		template<class G> struct BaseMove;
		template<class G> struct SingleMove;
		template<class G> struct ComboMove;

		template <class K, bool H> struct EMData;
		template <class K, bool H> struct EMPathData;

		template<class K, bool H>
		using EMGraphTraits = CustomGraphTraits<H, true, true, true>;

		template <class EMT>
		concept EMTraits = requires(typename EMT::Graph::Edge_handle e, typename EMT::Graph::Path_handle p, SingleMove<typename EMT::Graph>&sm, ComboMove<typename EMT::Graph>&cm) {

			typename EMT::Graph;

			{
				EMT::data(e)
			} -> std::same_as<EMData<typename EMT::Kernel, EMT::Graph::Graph_traits::historic>&>;

			{
				EMT::data(p)
			} -> std::same_as<EMPathData<typename EMT::Kernel, EMT::Graph::Graph_traits::historic>&>;

			{ EMT::determineSingleCost(sm) } -> std::same_as< Number<typename EMT::Kernel>>;
			{ EMT::determineComboCost(cm) } -> std::same_as< Number<typename EMT::Kernel>>;
		};

		template<class G>
		struct MoveQueueTraits;
	}

	template<typename K, bool H>
	using EdgeMovesGraph = Straight_graph_2<K, detail::EMGraphTraits<K, H>, std::monostate, detail::EMData<K, H>, detail::EMPathData<K, H>>;

	template <detail::EMTraits EMT> class EdgeMoves {
	public:
		using Graph = EMT::Graph;
		using Vertex_handle = Graph::Vertex_handle;
		using Edge_handle = Graph::Edge_handle;
		using Kernel = Graph::Kernel;
		using VertexTree = VertexQuadTree<Graph>;
		using EdgeTree = EdgeQuadTree<Graph>;

	private:
		using Data = detail::EMData<Kernel, Graph::Graph_traits::historic>;
		using Move = detail::BaseMove<Graph>;
		using Single = detail::SingleMove<Graph>;
		using Combo = detail::ComboMove<Graph>;
		using Queue = cartocrow::IndexedPriorityQueue<detail::MoveQueueTraits<Graph>>;
		
		using PairedSingles = std::pair<Single*, Single*>;
		using Operation = std::variant<Combo*, PairedSingles, Single*>;

		Graph& graph;
		EdgeTree& sqt;
		VertexTree& pqt;
		Queue queue;
		Graph_edge_set<Graph> edgeset;

		bool validate_state();

		template<bool LEFT>
		void update_single(Edge_handle e, Single& single);
		void update_singles(Edge_handle e);
		void update_combo(Edge_handle e);
		bool test_topology(Single& single);
		bool test_topology(Combo& combo);
		Single* find_compensate_move(Single& contract, Number<Kernel> area);

		std::optional<Operation> findNextStep();
		void start_move(Single& move);
		void contract(Single& move);
		void move(Single& move, const Number<Kernel> area);
		void move(Single& move, const Point<Kernel> source_dest, const Point<Kernel> target_dest);
		void performStep(Single& contract, Single& compensate);
		void performStep(Combo& combo);
		void performStep(Single& contact);

		void determineCheckout(Single& move, const bool contract);
		void checkOut();
		void postProcess();

	public:
		EdgeMoves(Graph& g, EdgeTree& sqt, VertexTree& pqt);

		void initialize(bool initSQT, bool initPQT);
		bool run(std::optional<std::function<bool(int, Number<Kernel>)>> stop = std::nullopt);
		bool step();
	};


	template <typename G> struct BuchinEtAlTraits {
		using Graph = G;
		using Kernel = Graph::Kernel;

		static detail::EMData<Kernel, Graph::Graph_traits::historic>& data(typename Graph::Edge_handle e) {
			return e->data();
		}
		static detail::EMPathData<Kernel, Graph::Graph_traits::historic>& data(typename Graph::Path_handle p) {
			return p->data();
		}

		static Number<Kernel> determineSingleCost(detail::SingleMove<Graph>& sm);
		static Number<Kernel> determineComboCost(detail::ComboMove<Graph>& cm);
	};

	template <typename G> using BuchinEtAl = EdgeMoves<BuchinEtAlTraits<G>>;

} // namespace cartocrow::simplification

#include "edge_moves.hpp"