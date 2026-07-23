// -----------------------------------------------------------------------------
// IMPLEMENTATION OF TEMPLATE FUNCTIONS
// Do not include this file, but the .h file instead
// -----------------------------------------------------------------------------

#include "utils.h"
#include "precision_safe_tests.h"

namespace cartocrow::simplification {

	namespace detail {

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

		template<class G> struct BaseMove {

			virtual ~BaseMove() {} // necessary for inheritance to work...

			G::Edge_handle edge;

			Number<typename G::Kernel> cost;
			int queue_index;

			bool blocked_by_degzero;
			std::vector<typename G::Edge_handle> blocked_by;
		};

		template<class G> struct SingleMove : public BaseMove<G> {

			using K = G::Kernel;
			using Vertex_handle = G::Vertex_handle;
			using Edge_handle = G::Edge_handle;

			bool left;
			enum VertexType src_type, tar_type;
			bool remove_self, remove_previous, remove_next;
			bool merge_previous, merge_next;
			Polygon<K> swept;
			bool contract_merges_highdegrees;
			int waiting_index = -1;

			bool movable() {
				return src_type != UNMOVABLE && tar_type != UNMOVABLE;
			}

			bool contractable() {
				return movable() && !contract_merges_highdegrees;
			}

			void update() {
				swept.clear();

				Vertex_handle a = nullptr, d = nullptr;
				Vertex_handle a_alt = nullptr, d_alt = nullptr;

				Vertex_handle b = this->edge->source();
				Vertex_handle c = this->edge->target();

				Line<K> baseline = Line<K>(b->point(), c->point());

				// determine start type
				switch (b->degree()) {
				case 2: {
					a = b->prev();
					if (left == CGAL::left_turn(a->point(), b->point(), c->point())) {
						src_type = detail::DEG_TWO_SUPPORT;
					}
					else {
						src_type = detail::DEG_TWO_NO_SUPPORT;
					}
					break;
				}
				case 3: {
					size_t edge_index = b->find_incident_index(this->edge);
					a = b->neighboring_incident_edge(edge_index, left)->other(b);
					a_alt = b->neighboring_incident_edge(edge_index, !left)->other(b);

					if (safe_test::point_on_line(a->point(), baseline)) {
						src_type = detail::DEG_THREE_ALIGNED;
					}
					else if (left == CGAL::left_turn(a->point(), b->point(), c->point())) {
						if (safe_test::collinear(a->point(), b->point(), a_alt->point())) {
							src_type = detail::DEG_THREE_SMOOTH;
						}
						else {
							src_type = detail::DEG_THREE_SUPPORT;
						}
					}
					else {
						src_type = detail::DEG_THREE_NO_SUPPORT;
					}
					break;
				}
				default:
					src_type = detail::UNMOVABLE;
					break;
				}

				// determine end type
				switch (c->degree()) {
				case 2: {
					d = c->next();
					if (left == CGAL::left_turn(b->point(), c->point(), d->point())) {
						tar_type = detail::DEG_TWO_SUPPORT;
					}
					else {
						tar_type = detail::DEG_TWO_NO_SUPPORT;
					}
					break;
				}
				case 3: {
					size_t edge_index = c->find_incident_index(this->edge);
					d = c->neighboring_incident_edge(edge_index, !left)->other(c);
					d_alt = c->neighboring_incident_edge(edge_index, left)->other(c);
					if (safe_test::point_on_line(d->point(), baseline)) {
						tar_type = detail::DEG_THREE_ALIGNED;
					}
					else if (left == CGAL::left_turn(b->point(), c->point(), d->point())) {
						if (safe_test::collinear(c->point(), d->point(), d_alt->point())) {
							tar_type = detail::DEG_THREE_SMOOTH;
						}
						else {
							tar_type = detail::DEG_THREE_SUPPORT;
						}
					}
					else {
						tar_type = detail::DEG_THREE_NO_SUPPORT;
					}
					break;
				}
				default:
					tar_type = detail::UNMOVABLE;
					break;
				}

				if (!movable()) {
					return;
				}

				// how far can we move along the previous track before the previous edge vanishes
				Number<K> prev_dist_squared;
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
				Number<K> next_dist_squared;
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
				Line<K> prev_track = Line<K>(b->point(), (src_type == detail::DEG_THREE_ALIGNED ? a_alt : a)->point());
				Line<K> next_track = Line<K>(c->point(), (tar_type == detail::DEG_THREE_ALIGNED ? d_alt : d)->point());

				Number<K> edge_dist_squared = -1;
				Point<K> edge_lim;

				{
					const auto is = CGAL::intersection(prev_track, next_track);
					if (is) {
						if (const Point<K>* pt = std::get_if<Point<K>>(&*is)) {
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
				if (prev_dist_squared < 0 && next_dist_squared < 0) {
					// no support at all
					// TODO: refine, to allow converging unsupported moves?
					src_type = detail::UNMOVABLE;
					tar_type = detail::UNMOVABLE;
					return;
				}

				assert(prev_dist_squared >= 0 || next_dist_squared >= 0); // NB: if TODO above is resolved, this may fail

				Number<K> min_dist_squared;
				if (prev_dist_squared < 0) {
					min_dist_squared = next_dist_squared;
				}
				else if (next_dist_squared < 0) {
					min_dist_squared = prev_dist_squared;
				}
				else {
					min_dist_squared = CGAL::min(prev_dist_squared, next_dist_squared);
				}

				if (edge_dist_squared >= 0) {
					min_dist_squared = CGAL::min(min_dist_squared, edge_dist_squared);
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
							c->point() - d->point());
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
					const auto is = CGAL::intersection(Line<K>(d->point(), c->point() - b->point()), prev_track);
					swept.push_back(*std::get_if<Point<K>>(&*is));
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
					const auto is = CGAL::intersection(Line<K>(a->point(), c->point() - b->point()), next_track);
					swept.push_back(*std::get_if<Point<K>>(&*is));
				}

				// precision, be sure we dont want to remove self...  
				if (!remove_self && safe_test::same_point(swept[0], swept[3])) {
					remove_self = true;
					swept.erase(swept.vertices_end()--);
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

			Number<Inexact> base_length() {
				return std::sqrt(approximate(CGAL::squared_distance(swept[1], swept[2])));
			}
			Number<Inexact> end_length() {
				return swept.size() == 3 ? 0 : std::sqrt(approximate(CGAL::squared_distance(swept[0], swept[3])));
			}
			Vector<Inexact> move_direction() {
				Vector<K> vec = edge_vector();
				Vector<Inexact> dir = approximate(left ? vec.perpendicular(CGAL::COUNTERCLOCKWISE) : vec.perpendicular(CGAL::CLOCKWISE));
				return dir / std::sqrt(dir.squared_length());
			}
			Vector<K> edge_vector() {
				return this->edge->target()->point() - this->edge->source()->point();
			}
			Vector<K> source_move() {
				return swept[0] - swept[1];
			}
			Vector<K> target_move() {
				return swept[swept.size() == 3 ? 0 : 3] - swept[2];
			}

			std::optional<std::pair<Point<K>, Point<K>>> end_positions_for(Number<K> area) {

				Vector<Inexact> edgedir = approximate(edge_vector());
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
				return std::pair<Point<K>, Point<K>>(swept[1] + convert_kernel<K>(ratio * startTrackVec), swept[2] + convert_kernel<K>(ratio * endTrackVec));
			}
		};

		template<class G> struct ComboMove : public BaseMove<G> {

			using K = G::Kernel;
			using Vertex_handle = G::Vertex_handle;
			using Edge_handle = G::Edge_handle;
			using Single = SingleMove<G>;

			bool executable;
			Single* prev_move;
			Single* next_move;
			Polygon<K> prev_swept;
			Polygon<K> next_swept;
			bool remove_self, merge_across;
			bool prev_partial, next_partial;

			void update() {
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
					executable = false;
					return;
				}

				// TODO: use traits!
				if (left_at_source) {
					prev_move = &(prev->data().left);
					next_move = &(next->data().right);
				}
				else {
					prev_move = &(prev->data().right);
					next_move = &(next->data().left);
				}

				if (!prev_move->movable() || !next_move->movable()) {
					executable = false;
					return;
				}

				// TODO: reduce approximate use?

				// now, determine how far both need to move until the common edge vanishes
				Vector<Inexact> edgedir = approximate(this->edge->target()->point() - this->edge->source()->point());
				Number<Inexact> edge_length = std::sqrt(edgedir.squared_length());

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

					Number<K> prev_area = CGAL::abs(prev_move->swept.area());
					Number<K> next_area = CGAL::abs(next_move->swept.area());
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

					Number<K> prev_ratio = convert_kernel<K>(CGAL::min(1.0, prev_d / prev_lim_dist));
					Point<K> merge_point = this->edge->source()->point() + prev_ratio * convert_kernel<K>(prev_common_vector);
					Point<K> prev_other_end = prev_move->swept[1] + prev_ratio * prev_move->source_move();

					prev_partial = !safe_test::same_point(merge_point, prev_swept[prev_swept.size() == 3 ? 0 : 3])
						&& !safe_test::same_point(merge_point, prev_other_end)
						&& !safe_test::same_point(prev_other_end, prev_move->swept[0]);
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

					Number<K> next_ratio = convert_kernel<K>(CGAL::min(1.0, next_d / next_lim_dist));
					Point<K> next_other_end = next_move->swept[2] + next_ratio * next_move->target_move();

					next_partial = !safe_test::same_point(merge_point, next_swept[0])
						&& !safe_test::same_point(merge_point, next_other_end)
						&& !safe_test::same_point(next_other_end, next_move->swept[next_move->swept.size() == 3 ? 0 : 3]);
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
					Vector<K> prevdir_at_join;
					if (prev_swept.size() == 3) {
						prevdir_at_join = prev_move->source_move();
					}
					else {
						prevdir_at_join = prev->target()->point() - prev->source()->point();
					}

					Vector<K> nextdir_at_join;
					if (next_swept.size() == 3) {
						nextdir_at_join = -1 * next_move->target_move();
					}
					else {
						nextdir_at_join = next->target()->point() - next->source()->point();
					}

					merge_across = safe_test::aligned_vectors(prevdir_at_join, nextdir_at_join);
				}
				else {
					merge_across = false;
				}
			}

			bool is_executable() {
				return executable;
			}
		};


		template <class K, bool H> struct EMData {

			SingleMove<EdgeMovesGraph<K, H>> left, right;
			ComboMove<EdgeMovesGraph<K, H>> combo;
			std::vector<BaseMove<EdgeMovesGraph<K, H>>*> blocking;
		};


		template <class K, bool H> struct EMPathData {
			using Single = SingleMove<EdgeMovesGraph<K, H>>;
			std::vector<Single*> left_waiting;
			std::vector<Single*> right_waiting;

			void add_waiting(Single* single) {
				if (single->left) {
					single->waiting_index = left_waiting.size();
					left_waiting.push_back(single);
				}
				else {
					single->waiting_index = right_waiting.size();
					right_waiting.push_back(single);
				}
			}

			void remove_waiting(Single* single) {
				if (single->left) {
					Single* other = utils::swapRemove(single->waiting_index, left_waiting);
					if (other != nullptr) {
						other->waiting_index = single->waiting_index;
						single->waiting_index = -1;
					}
				}
				else {
					Single* other = utils::swapRemove(single->waiting_index, right_waiting);
					if (other != nullptr) {
						other->waiting_index = single->waiting_index;
						single->waiting_index = -1;
					}
				}
			}
		};

		template<class G>
		struct MoveQueueTraits {

			using Element_handle = BaseMove<G>*;

			static void setIndex(Element_handle m, int id) {
				m->queue_index = id;
			}

			static int getIndex(Element_handle m) {
				return m->queue_index;
			}

			static int compare(Element_handle a, Element_handle b) {
				Number<G::Kernel> ac = a->cost;
				Number<G::Kernel> bc = b->cost;
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
	} // namespace detail

	template <detail::EMTraits EMT>
	void EdgeMoves<EMT>::update(Edge_handle e) {

		Data& data = EMT::data(e);
		{// left
			data.left.edge = e;
			data.left.blocked_by_degzero = false;
			data.left.blocked_by.clear();
			data.left.left = true;

			data.left.update();

			if (data.left.contractable()) {
				data.left.cost = EMT::determineSingleCost(data.left);

				if (queue.contains(&data.left)) {
					queue.update(&data.left);
				}
				else {
					queue.push(&data.left);
				}
			}
			else {
				queue.remove(&data.left);
			}
		}
		{// right
			data.right.edge = e;
			data.right.blocked_by_degzero = false;
			data.right.blocked_by.clear();
			data.right.left = false;

			data.right.update();

			if (data.right.contractable()) {
				data.right.cost = EMT::determineSingleCost(data.right);

				if (queue.contains(&data.right)) {
					queue.update(&data.right);
				}
				else {
					queue.push(&data.right);
				}
			}
			else {
				queue.remove(&data.right);
			}
		}
		{// combo
			data.combo.edge = e;
			data.combo.blocked_by_degzero = false;
			data.combo.blocked_by.clear();

			data.combo.update();

			if (data.combo.is_executable()) {
				data.combo.cost = EMT::determineComboCost(data.combo);

				if (queue.contains(&data.combo)) {
					queue.update(&data.combo);
				}
				else {
					queue.push(&data.combo);
				}
			}
			else {
				queue.remove(&data.combo);
			}
		}
	}

	template <detail::EMTraits EMT>
	bool EdgeMoves<EMT>::blocks(Edge_handle e, Move& move) {
		// TODO
		return false;
	}

	template <detail::EMTraits EMT>
	EdgeMoves<EMT>::EdgeMoves(Graph& g, EdgeTree& sqt, VertexTree& pqt)
		: graph(g), sqt(sqt), pqt(pqt) {
	}

	template <detail::EMTraits EMT>
	void EdgeMoves<EMT>::initialize(bool initSQT, bool initPQT) {

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
		if (initSQT) {
			sqt.clear();
			for (Edge_handle e : graph.edges()) {
				sqt.insert(e);
			}
		}

		if (initPQT) {
			pqt.clear();
			for (Vertex_handle v : graph.vertices()) {
				if (v->degree() == 0) {
					pqt.insert(v);
				}
			}
		}

		// initialize move data
		for (Edge_handle e : graph.edges()) {
			update(e);
		}
	}

	template <detail::EMTraits EMT>
	std::optional<std::variant<detail::ComboMove<typename EMT::Graph>*, std::pair<detail::SingleMove<typename EMT::Graph>*, detail::SingleMove<typename EMT::Graph>*>>>
		EdgeMoves<EMT>::findNextStep() {

		assert(graph.can_perform_operation());
		while (!queue.empty()) {
			Move* m = queue.peek();

			if (Single* sm = dynamic_cast<Single*>(m)) {

				// test if its blocked
				Rectangle<Kernel> rect = utils::boxOf(sm->swept);

				sm->blocked_by_degzero = false;
				pqt.findContained(rect, [&sm](Vertex_handle b) {
					if (!sm->swept.has_on_unbounded_side(b->point())) {
						sm->blocked_by_degzero = true;
					}
					});

				if (!sm->blocked_by_degzero) {

					sqt.findOverlapped(rect, [this, &sm](Edge_handle b) {
						if (blocks(b, *sm)) {
							EMT::data(b).blocking.push_back(sm);
							sm->blocked_by.push_back(b);
						}
						});

					if (sm->blocked_by.empty()) {
						// safe operation
						return Operation(PairedSingles(sm, nullptr));
					}
				}

				// remove element from queue, it's blocked
				queue.pop();
			}
			else {
				Combo* combo = dynamic_cast<Combo*>(m);
				return Operation(combo);
			}
		}

		// no move exists
		return std::nullopt;
	}

	template <detail::EMTraits EMT>
	void EdgeMoves<EMT>::performStep(Single& contract, Single& compensate) {

		assert(graph.can_perform_operation());

		std::cout << "PERFORMING" << std::endl;
		std::cout << "  " << contract.edge->curve() << " && " << compensate.edge->curve() << std::endl;

		graph.start_operation_group();

		// TODO track changed edges
		using namespace detail;

		switch (contract.src_type) {
		case DEG_TWO_SUPPORT:
		case DEG_TWO_NO_SUPPORT: {
			// skip
			break;
		}
		case DEG_THREE_SMOOTH: {
			break;
		}
		case DEG_THREE_SUPPORT: {
			Edge_handle other_prev = contract.edge->prev(!contract.left);
			graph.subdivide_edge(other_prev, contract.edge->source()->point());
			break;
		}
		case DEG_THREE_ALIGNED:
		case DEG_THREE_NO_SUPPORT: {
			graph.subdivide_edge(contract.edge, contract.edge->source()->point());
			break;
		}
		default:
			assert(false); // unsupported vertex-moving type
		}

		switch (contract.tar_type) {
		case DEG_TWO_SUPPORT:
		case DEG_TWO_NO_SUPPORT: {
			// skip
			break;
		}
		case DEG_THREE_SMOOTH: {
			break;
		}
		case DEG_THREE_SUPPORT: {
			Edge_handle other_next = contract.edge->next(contract.left);
			graph.subdivide_edge(other_next, contract.edge->target()->point());
			break;
		}
		case DEG_THREE_ALIGNED:
		case DEG_THREE_NO_SUPPORT: {
			graph.subdivide_edge(contract.edge, contract.edge->target()->point());
			break;
		}
		default:
			assert(false); // unsupported vertex-moving type
		}

		// TODO: track changed
		if (contract.remove_self) {
			Vertex_handle v = graph.collapse_edge(contract.edge, contract.swept[0]);
			if (contract.remove_previous) {
				Edge_handle e = graph.merge_vertex(v);
				if (contract.merge_previous) {
					graph.merge_vertex(e->source());
				}
			}
			else if (contract.remove_next) {
				Edge_handle e = graph.merge_vertex(v);
				if (contract.merge_next) {
					graph.merge_vertex(e->target());
				}
			}
		}
		else {
			if (contract.remove_previous) {
				Vertex_handle v = contract.edge->source();
				if (v->degree() == 2) {
					Edge_handle e = graph.merge_vertex(v);
					if (contract.merge_previous) {
						graph.merge_vertex(e->source());
					}
				}
				else {
					Vertex_handle pv = v->neighboring_incident_edge(contract.edge, contract.left)->other(v);
					assert(pv->degree() != 2);
					graph.merge_vertex(pv);
					graph.move_vertex(v, contract.swept[0]);

					assert(!contract.merge_previous);
				}
			}
			else {
				graph.move_vertex(contract.edge->source(), contract.swept[0]);
			}

			if (contract.remove_next) {
				Vertex_handle v = contract.edge->target();
				if (v->degree() == 2) {
					Edge_handle e = graph.merge_vertex(v);
					if (contract.merge_next) {
						graph.merge_vertex(e->target());
					}
				}
				else {
					Vertex_handle nv = v->neighboring_incident_edge(contract.edge, !contract.left)->other(v);
					assert(nv->degree() != 2);
					graph.merge_vertex(nv);
					graph.move_vertex(v, contract.swept[3]);

					assert(!contract.merge_next);
				}

			}
			else {
				graph.move_vertex(contract.edge->target(), contract.swept[3]);
			}
		}

		graph.end_operation_group();
	}

	template <detail::EMTraits EMT>
	void EdgeMoves<EMT>::performStep(Combo& combo) {

	}

	template <detail::EMTraits EMT>
	bool EdgeMoves<EMT>::run(std::optional<std::function<bool(int, Number<Kernel>)>> stop) {

		assert(graph.can_perform_operation());

		while (true) {

			std::optional<Operation> next = findNextStep();
			if (!next) {
				return false;
			}

			Operation op = next.value();

			if (std::holds_alternative<Combo*>(op)) {
				Combo* combo = std::get<Combo*>(op);
				if (!stop.has_value() || (*stop)(graph.number_of_edges(), combo->cost)) {
					return true;
				}
				performStep(*combo);
			}
			else {
				PairedSingles pair = std::get<PairedSingles>(op);
				if (!stop.has_value() || (*stop)(graph.number_of_edges(), pair.first->cost)) {
					return true;
				}
				performStep(*pair.first, *pair.second);
			}
		}
	}

	template <detail::EMTraits EMT>
	bool EdgeMoves<EMT>::step() {
		assert(graph.can_perform_operation());

		std::optional<Operation> next = findNextStep();
		if (!next) {
			return false;
		}

		Operation op = next.value();

		if (std::holds_alternative<Combo*>(op)) {
			Combo* combo = std::get<Combo*>(op);
			performStep(*combo);
		}
		else {
			PairedSingles pair = std::get<PairedSingles>(op);
			performStep(*pair.first, *pair.second);
		}
		return true;
	}

	template <typename G>
	detail::EMData<typename G::Kernel, G::Graph_traits::historic> data(typename G::Edge_handle e) {
		return e->data();
	}
	template <typename G>
	Number<typename G::Kernel> BuchinEtAlTraits<G>::determineSingleCost(detail::SingleMove<G>& sm) {
		return CGAL::abs(sm.swept.area());
	}

	template <typename G>
	Number<typename G::Kernel> BuchinEtAlTraits<G>::determineComboCost(detail::ComboMove<G>& cm) {
		return CGAL::abs(cm.prev_swept.area());
	}

} // namespace cartocrow::simplification