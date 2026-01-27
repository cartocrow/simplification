// -----------------------------------------------------------------------------
// IMPLEMENTATION OF TEMPLATE FUNCTIONS
// Do not include this file, but the .h file instead
// -----------------------------------------------------------------------------

namespace cartocrow::simplification {

	namespace detail {

		template<class Graph>
		struct AngleRestriction {

			using Vertex = Graph::Vertex;
			using Edge = Graph::Edge;
			using Kernel = Graph::Kernel;
			using Dir = Vector<Inexact>;
			using Num = Number<Inexact>;

			CGAL::Cartesian_converter<Kernel, Inexact> k_to_inex;
			CGAL::Cartesian_converter<Inexact, Kernel> inex_to_k;

			inline Num to_inexact(const Number<Kernel>& v) {
				if constexpr (std::is_same<Kernel, Inexact>::value) {
					return v;
				}
				else {
					return k_to_inex(v);
				}
			}

			inline Point<Inexact> to_inexact(const Point<Kernel>& v) {
				if constexpr (std::is_same<Kernel, Inexact>::value) {
					return v;
				}
				else {
					return k_to_inex(v);
				}
			}

			inline Vector<Inexact> to_inexact(const Vector<Kernel>& v) {
				if constexpr (std::is_same<Kernel, Inexact>::value) {
					return v;
				}
				else {
					return k_to_inex(v);
				}
			}

			inline Segment<Inexact> to_inexact(const Segment<Kernel>& v) {
				if constexpr (std::is_same<Kernel, Inexact>::value) {
					return v;
				}
				else {
					return k_to_inex(v);
				}
			}

			inline Vector<Kernel> to_kernel(const Vector<Inexact>& v) {
				if constexpr (std::is_same<Kernel, Inexact>::value) {
					return v;
				}
				else {
					return inex_to_k(v);
				}
			}

			inline Dir direction(Vertex* v, Edge* e) {
				auto start = to_inexact(v->getPoint());
				auto end = to_inexact(e->other(v)->getPoint());
				return (end - start) / std::sqrt(CGAL::squared_distance(start, end));
			}

			enum class EdgeType {
				ALIGN,
				UNALIGN,
				EVADING,
				DEV_ALIGN,
				DEV_UNALIGN,
				UNDETERMINED
			};


			struct EdgeData {
				Vertex* significant = nullptr;
				std::pair<int, int> associated = std::pair<int,int>(-1,-1);
				int assigned = -1;
				EdgeType type = EdgeType::UNDETERMINED;
				int stepcount = -1;
				Num stepdist = -1;
				Polygon<Inexact> interference;

				inline int other_direction() {
					return assigned == associated.first ? associated.second : associated.first;
				}
			};

			Graph& graph;
			std::vector<Vector<Inexact>> directions;
			std::vector<bool> significant_vertices;
			std::vector<EdgeData> edge_data;

			AngleRestriction(Graph& graph, std::vector<Dir> dirs) : graph(graph), directions(dirs) {
			};

			bool is_approximately(Dir a, Dir b) {
				const Num eps = 0.000001;
				return std::abs(a.x() - b.x()) < eps
					&& std::abs(a.y() - b.y()) < eps;
			}

			std::pair<int, int> associated_directions(Vertex* v, Edge* e) {

				Dir dir = direction(v, e);

				int i = 0;
				if (is_approximately(directions[i], dir)) {
					return std::pair<int, int>(i, -1);
				}
				int in = 1;
				int ndirs = directions.size();
				while (i < ndirs) {
					if (is_approximately(directions[in], dir)) {
						return std::pair<int, int>(in, -1);
					}
					else if (Direction<Inexact>(dir).counterclockwise_in_between(
						Direction<Inexact>(directions[i]),
						Direction<Inexact>(directions[in])
					)) {
						return std::pair<int, int>(i, in);
					}
					else {
						i++;
						in++;
						if (in >= ndirs) {
							in = 0;
						}
					}
				}

				assert(false);
				return std::pair<int, int>(-1, -1);
			}

			void determine_significant_vertices() {

				std::cout << "determine_significant_vertices\n";
				significant_vertices = std::vector<bool>(graph.getVertexCount(), false);

				for (Vertex* v : graph.getVertices()) {

					int d = v->degree();
					if (d >= 2) {
						std::pair<int, int> assoc_prev = associated_directions(v, v->edge(d - 1));

						for (Edge* e : v->getEdges()) {
							std::pair<int, int> assoc = associated_directions(v, e);

							if (assoc.first == assoc_prev.first
								|| (assoc.second >= 0 && assoc.second == assoc_prev.second)
								|| assoc.second == assoc_prev.first
								|| assoc.first == assoc_prev.second) {
								significant_vertices[v->graphIndex()] = true;
								break;
							}

							assoc_prev = assoc;
						}
					}
				}

			};

			void subdivide_edges(Num lambda) {

				std::cout << "subdivide_edges\n";

				Num max_sqr_len = 0;
				for (Edge* e : graph.getEdges()) {
					Num sqr_len = to_inexact(e->squared_length());
					if (max_sqr_len < sqr_len) {
						max_sqr_len = sqr_len;
					}
				}

				max_sqr_len *= lambda * lambda;

				int cnt = graph.getEdgeCount();
				for (int i = 0; i < cnt; i++) {
					Edge* e = graph.getEdges()[i];

					Num sqr_len = to_inexact(e->squared_length());

					int steps = (int)std::ceil(std::sqrt(sqr_len / max_sqr_len));
					if (steps < 2
						&& significant_vertices[e->getSource()->graphIndex()]
						&& significant_vertices[e->getTarget()->graphIndex()]) {
						steps = 2;
					}

					Vector<Kernel> step = (e->getTarget()->getPoint() - e->getSource()->getPoint()) / steps;
					Point<Kernel> pt = e->getSource()->getPoint() + step;
					while (steps > 1) {
						e = graph.splitEdge(e, pt)->outgoing();

						pt = pt + step;
						steps--;
					}

				}
			};

			void find_best(std::vector<Dir>& out, std::vector<int>& build, Num build_cost, int next, std::vector<int>* best, Num* best_cost) {
				if (build_cost >= *best_cost) {
					// cannot get a better result
					return;
				}
				if (next == out.size()) {
					// handled all, this must outperform best
					*best_cost = build_cost;
					std::copy(build.begin(), build.end(), best->begin());
					return;
				}

				for (int i = 0; i < directions.size(); i++) {
					// test all unused directions for "next"
					int j = 0;
					while (j < next && build[j] != i) {
						j++;
					}
					if (j != next) continue;

					// compute the deviation
					Num dev = std::acos(out[next] * directions[i]);
					dev *= dev;

					// recurse
					build[next] = i;
					find_best(out, build, build_cost + dev, next + 1, best, best_cost);
				}

			}

			bool same_sector(Vertex* v, Edge* e, std::pair<int, int> assoc) {
				if (assoc.second < 0)
					return false;

				Dir dir = direction(v, e);

				return !is_approximately(directions[assoc.first], dir)
					&& !is_approximately(directions[assoc.second], dir)
					&& Direction<Inexact>(dir).counterclockwise_in_between(
						Direction<Inexact>(directions[assoc.first]),
						Direction<Inexact>(directions[assoc.second])
					);
			}

			void assign_directions() {

				std::cout << "assign_directions\n";

				int e_cnt = graph.getEdgeCount();

				edge_data.resize(e_cnt);

				// NB: subdivide may have increased vertex count
				// but the new vertices are insignificant by construction
				int v_cnt = significant_vertices.size();
				for (int i = 0; i < v_cnt; i++) {

					if (!significant_vertices[i]) continue;

					Vertex* v = graph.getVertices()[i];

					int degree = v->degree();
					std::vector<Dir> out;

					for (Edge* e : v->getEdges()) {
						out.push_back(direction(v, e));
					}

					// TODO: optimize this, brute forced currently
					std::vector<int> best(degree, 0);
					Num best_cost = degree * 10;
					std::vector<int> build(degree);
					find_best(out, build, 0, 0, &best, &best_cost);

					for (int i = 0; i < degree; ++i) {

						Edge* e = v->edge(i);
						EdgeData& edata = edge_data[e->graphIndex()];
						edata.significant = v;
						edata.associated = associated_directions(v,e);
						edata.assigned = best[i];

						if (edata.associated.second < 0) {
							if (best[i] == edata.associated.first) {
								edata.type = EdgeType::ALIGN;
							}
							else {
								edata.type = EdgeType::DEV_ALIGN;
							}
						}
						else if (best[i] == edata.associated.first || best[i] == edata.associated.second) {

							bool has_same = false;
							for (int j = 0; j < degree; ++j) {
								if (i != j && same_sector(v, v->getEdges()[j], edata.associated)) {
									has_same = true;
									break;
								}
							}

							if (has_same) {
								edata.type = EdgeType::EVADING;
							}
							else {
								edata.type = EdgeType::UNALIGN;
							}
						}
						else {
							edata.type = EdgeType::DEV_UNALIGN;
						}
					}

				}

			};

			void assign_double_insignificant() {

				std::cout << "assign_double_insignificant\n";

				int e_cnt = graph.getEdgeCount();

				for (int i = 0; i < e_cnt; i++) {

					EdgeData& edata = edge_data[i];
					if (edata.type != EdgeType::UNDETERMINED) {
						continue;
					}

					Edge* e = graph.getEdges()[i];
					edata.significant = e->getSource();
					edata.associated = associated_directions(e->getSource(), e);
					if (edata.associated.second < 0) {
						edata.type = EdgeType::ALIGN;
						edata.assigned = edata.associated.first;
					}
					else {
						edata.type = EdgeType::UNALIGN;
						Dir dir = to_inexact(e->getTarget()->getPoint() - e->getSource()->getPoint());
						Num det1 = CGAL::determinant(directions[edata.associated.first], dir);
						Num det2 = CGAL::determinant(dir, directions[edata.associated.second]);
						edata.assigned = det1 < det2 ? edata.associated.first : edata.associated.second;
					}
				}
			};

			template<typename K>
			std::pair<Number<K>, Number<K>> solve_vector_addition(Vector<K> d1, Vector<K> d2, Vector<K> s) {
				// find to factors, f and g, such that
				// f * d1 + g * d2 = s

				Number<K> sx = s.x();
				Number<K> sy = s.y();
				Number<K> d1x = d1.x();
				Number<K> d1y = d1.y();
				Number<K> d2x = d2.x();
				Number<K> d2y = d2.y();

				// e.g. find f and g such that:
				// sdx = f * d1x + g * d2x
				// sdy = f * d1y + g * d2y
				Number<K> f, g;

				// g * d2y = sy - f * d1y
				// NB division by 0!
				if (-0.000001 < d2y && d2y < 0.000001) {
					// d2y == 0
					// f * d1y = sy
					// NB: d1y cant be zero, as that would result in two parallel directions

					f = sy / d1y;

					// now, solve for g, using f, knowing that d2x != 0
					// sx = f * d1x + g * d2x hence yields:
					g = (sx - f * d1x) / d2x;
				}
				else {
					// g = (sy - f * d1y) / d2y
					// subsituting in first equation:
					// sx = f * d1x + ((sy - f * d1y) / d2y) * d2x
					// rewriting:
					// f (d1x - d1y * (d2x / d2y)) = sx - (sy / d2y) * d2x

					// can d1x - d1y * (d2x / d2y) = 0 hold?
					// == d2x / d2y = d1x / d1y
					// hence, this will only be zero if the directions d1 and d2 are
					// equal (which cannot occur)
					f = (sx - (sy / d2y) * d2x) / (d1x - d1y * (d2x / d2y));

					// now, solve for g, using f, knowing that d2y != 0
					// sy = f * d1y + g * d2y hence yields:
					g = (sy - f * d1y) / d2y;
				}

				return std::pair(f, g);
			}

			Polygon<Inexact> find_interference_region(Edge* e, Num eps) {

				EdgeData& edata = edge_data[e->graphIndex()];

			}

			void determine_interference_regions(Num eps) {
				int e_cnt = graph.getEdgeCount();

				for (int i = 0; i < e_cnt; i++) {

					Edge* e = graph.getEdges()[i];
					EdgeData& edata = edge_data[i];


					switch (edata.type) {
					default: {
						assert(false); // undetermined should not occur
					}
					case EdgeType::ALIGN: {
						edata.interference.push_back(to_inexact(edata.significant->getPoint()));
						edata.interference.push_back(to_inexact(e->other(edata.significant)->getPoint()));
						break;
					}
					case EdgeType::UNALIGN:
					case EdgeType::EVADING: {
						Dir dir_1 = directions[edata.assigned];
						Dir dir_2 = directions[edata.other_direction()];
						Dir edir = direction(edata.significant, e);

						std::pair<Num, Num> fg = solve_vector_addition(dir_1, dir_2, edir);

						auto v_pt = to_inexact(edata.significant->getPoint());
						auto o_pt = to_inexact(e->other(edata.significant)->getPoint());
						edata.interference.push_back(v_pt);
						edata.interference.push_back(v_pt + dir_1 * fg.first);
						edata.interference.push_back(o_pt);
						edata.interference.push_back(v_pt + dir_2 * fg.second);
						break;
					}
					case EdgeType::DEV_UNALIGN: {

						// TODO
						break;
					}
					case EdgeType::DEV_ALIGN: {

						auto v_pt = to_inexact(edata.significant->getPoint());
						auto o_pt = to_inexact(e->other(edata.significant)->getPoint());

						auto len = std::sqrt(CGAL::squared_distance(v_pt, o_pt));

						Dir dir_1 = directions[edata.assigned];
						Dir dir_2 = directions[edata.other_direction()];

						auto step_dist = len * eps;
						auto step_len = (1 - eps) * len / 2.0;

						auto sidestep = step_dist * dir_1;
						auto lengthstep = step_len * dir_2;

						// TODO better var names
						auto a = v_pt + sidestep;
						auto b = a + lengthstep;
						auto c = b - 2 * sidestep;
						auto d = c + lengthstep;

						edata.interference.push_back(v_pt);
						edata.interference.push_back(a);
						edata.interference.push_back(b);
						edata.interference.push_back(o_pt);
						edata.interference.push_back(d); // yes d first, then c (order of polyine vs CH order)		
						edata.interference.push_back(c);
						break;
					}
					}

				}
			}

			void assign_step_counts() {

				std::cout << "assign_step_counts\n";

				int e_cnt = graph.getEdgeCount();

				for (int i = 0; i < e_cnt; i++) {

					Edge* e = graph.getEdges()[i];
					EdgeData& edata = edge_data[i];

					switch (edata.type) {
					default: {
						assert(false); // shouldn't happen, undetermined edge somewhere?
						break;
					}
					case EdgeType::ALIGN: {
						// nothing to do
						break;
					}
					case EdgeType::UNALIGN: {
						Num min_dist_sqr = std::numeric_limits<Num>::infinity();
						Segment<Inexact> seg = to_inexact(e->getSegment());
						// TODO: use quad tree to speed things up
						for (Edge* other : graph.getEdges()) {
							if (other == e) {
								continue;
							}

							Vertex* other_v = edge_data[other->graphIndex()].significant;

							if (other->getSource() == e->getSource() || other->getTarget() == e->getSource()
								|| other->getSource() == e->getTarget() || other->getTarget() == e->getTarget()) {
								// TODO: must be a deviating edge
							}
							else {
								// TODO: test intersection

								Segment<Inexact> seg_other = to_inexact(other->getSegment());
								Num dist_sqr = CGAL::squared_distance(seg, seg_other);
								min_dist_sqr = std::min(min_dist_sqr, dist_sqr);
							}
						}

						for (Vertex* vv : graph.getVertices()) {
							if (vv->degree() == 0) {
								min_dist_sqr = std::min(min_dist_sqr, CGAL::squared_distance(to_inexact(vv->getPoint()), seg));
							}
						}

						if (!std::isfinite(min_dist_sqr)) {
							edata.stepcount = 2;
						}
						else {
							Num min_dist = std::sqrt(min_dist_sqr);
							Dir dir_1 = directions[edata.assigned];
							Dir dir_2 = directions[edata.other_direction()];
							Num elen = std::sqrt(to_inexact(e->squared_length()));
							Dir edir = direction(edata.significant, e);
							Num alpha_1 = std::abs(std::acos(edir * dir_1));
							Num alpha_2 = std::abs(std::acos(edir * dir_2));
							Num lmax = (1 / std::tan(alpha_1) + 1 / std::tan(alpha_2)) * min_dist / 2;
							int k = (int)std::ceil(elen / lmax);
							k += 2 - (k % 2); // make it the next even number
							edata.stepcount = k;
						}
						break;
					}
					case EdgeType::EVADING: {
						Num min_dist_sqr = std::numeric_limits<Num>::infinity();
						Segment<Inexact> seg(to_inexact(edata.significant->getPoint()), to_inexact(e->other(edata.significant)->getPoint()));
						Segment<Inexact> seg_ev(seg.start() + (seg.end() - seg.start()) / 2, seg.end());
						// TODO: more efficient to just iterate over the two faces
						for (Edge* other : graph.getEdges()) {

							if (other == e) {
								continue;
							}

							Vertex* other_v = edge_data[other->graphIndex()].significant;

							if (other->getSource() == e->getSource() || other->getTarget() == e->getSource()
								|| other->getSource() == e->getTarget() || other->getTarget() == e->getTarget()) {
								// must be a deviating edge, or evading edge

								// NB: must share their significant vertex

								if (edata.type == EdgeType::EVADING) {

									Segment<Inexact> seg_other = to_inexact(other->getSegment());
									Num dist_sqr = CGAL::squared_distance(seg_ev, seg_other);
									min_dist_sqr = std::min(min_dist_sqr, dist_sqr);
								}
								else {
									// TODO: deviating case
								}
							}
							else {
								// TODO: test intersection

								Segment<Inexact> seg_other = to_inexact(other->getSegment());
								Num dist_sqr = CGAL::squared_distance(seg, seg_other);
								min_dist_sqr = std::min(min_dist_sqr, dist_sqr);
							}
						}

						for (Vertex* vv : graph.getVertices()) {
							if (vv->degree() == 0) {
								min_dist_sqr = std::min(min_dist_sqr, CGAL::squared_distance(to_inexact(vv->getPoint()), seg));
							}
						}

						if (!std::isfinite(min_dist_sqr)) {
							edata.stepcount = 2;
						}
						else {
							Num min_dist = std::sqrt(min_dist_sqr);
							Dir dir_1 = directions[edata.assigned];
							Dir dir_2 = directions[edata.other_direction()];
							Num elen = std::sqrt(to_inexact(e->squared_length()));
							Dir edir = direction(edata.significant, e);
							Num alpha_1 = std::abs(std::acos(edir * dir_1));
							Num alpha_2 = std::abs(std::acos(edir * dir_2));
							Num lmax = (1 / std::tan(alpha_1) + 1 / std::tan(alpha_2)) * min_dist / 2;
							int k = (int)std::ceil(elen / lmax);
							k += 2 - (k % 2); // make it the next even number
							edata.stepcount = k;
						}
						break;
					}
					case EdgeType::DEV_UNALIGN: {
						// TODO 
						break;
					}
					case EdgeType::DEV_ALIGN: {
						// TODO step distnace
						break;
					}

					} // switch
				} // loop
			};

			void create_staircases(Num eps) {

				std::cout << "create_staircases\n";

				int e_cnt = graph.getEdgeCount();

				for (int i = 0; i < e_cnt; i++) {

					EdgeData& edata = edge_data[i];

					switch (edata.type) {
					case EdgeType::ALIGN: {
						// dont need to do anything
						break;
					}
					case EdgeType::UNALIGN: {
						int k = edata.stepcount;

						assert(k > 1 && k % 2 == 0);

						Edge* e = graph.getEdges()[i];
						Vertex* v = edata.significant;
						Vector<Kernel> d1 = to_kernel(directions[edata.assigned]);
						Vector<Kernel> d2 = to_kernel(directions[edata.other_direction()]);

						if (e->getSource() != v) {
							d1 *= -1;
							d2 *= -1;
						}

						Point<Kernel>& s = e->getSource()->getPoint();
						Point<Kernel>& t = e->getTarget()->getPoint();
						Vector<Kernel> step = (t - s) / k;

						std::pair<Number<Kernel>, Number<Kernel>> fg = solve_vector_addition(d1, d2, step);

						d1 = fg.first * d1;
						d2 = fg.second * d2;

						Point<Kernel> pt = s + d1;
						e = graph.splitEdge(e, pt)->outgoing();
						pt += 2 * d2;
						e = graph.splitEdge(e, pt)->outgoing();
						k -= 2;
						while (k > 0) {
							pt += 2 * d1;
							e = graph.splitEdge(e, pt)->outgoing();
							pt += 2 * d2;
							e = graph.splitEdge(e, pt)->outgoing();
							k -= 2;
						}

						break;
					}
					case EdgeType::EVADING: {
						int k = edata.stepcount;

						assert(k > 1 && k % 2 == 0);

						Edge* e = graph.getEdges()[i];
						Vertex* v = edata.significant;

						Vector<Kernel> d1 = to_kernel(directions[edata.assigned]);
						Vector<Kernel> d2 = to_kernel(directions[edata.other_direction()]);

						if (e->getSource() != v) {
							d1 *= -1;
							d2 *= -1;
						}

						Point<Kernel>& s = e->getSource()->getPoint();
						Point<Kernel>& t = e->getTarget()->getPoint();
						Vector<Kernel> step = (t - s) / k;

						std::pair<Number<Kernel>, Number<Kernel>> fg = solve_vector_addition(d1, d2, step);

						d1 = fg.first * d1;
						d2 = fg.second * d2;

						// outward
						Point<Kernel> pt_s = s + d1;
						e = graph.splitEdge(e, pt_s)->outgoing();
						Point<Kernel> pt_t = t - d1;
						e = graph.splitEdge(e, pt_t)->incoming();
						k -= 2;
						while (k > 0) {
							// back to central line
							pt_s += d2;
							e = graph.splitEdge(e, pt_s)->outgoing();
							pt_t -= d2;
							e = graph.splitEdge(e, pt_t)->incoming();

							// step outward
							pt_s += d1;
							e = graph.splitEdge(e, pt_s)->outgoing();
							pt_t -= d1;
							e = graph.splitEdge(e, pt_t)->incoming();
							k -= 2;
						}
						break;
					}
					case EdgeType::DEV_UNALIGN: {
						break;
					}
					case EdgeType::DEV_ALIGN: {

						Edge* e = graph.getEdges()[i];
						Vertex* v = edata.significant;

						auto v_pt = to_inexact(v->getPoint());
						auto o_pt = to_inexact(e->other(v)->getPoint());

						// TODO actual step distance...
						auto len = std::sqrt(CGAL::squared_distance(v_pt, o_pt));

						Dir dir_1 = directions[edata.assigned];
						Dir dir_2 = directions[edata.other_direction()];

						auto step_dist =len * eps;
						auto step_len = (1 - eps) * len / 2.0; // NB: this uses eps still, not step dist

						auto sidestep = to_kernel(step_dist * dir_1);
						auto lengthstep = to_kernel(step_len * dir_2);

						// TODO better var names
						auto a = v->getPoint() + sidestep;
						auto b = a + lengthstep;
						auto c = b - 2 * sidestep;
						auto d = c + lengthstep;
						auto f = d + sidestep;

						if (v == e->getSource()) {
							e = graph.splitEdge(e, a)->outgoing();
							e = graph.splitEdge(e, b)->outgoing();
							e = graph.splitEdge(e, c)->outgoing();
							e = graph.splitEdge(e, d)->outgoing();
							graph.splitEdge(e, f);
						}
						else {
							e = graph.splitEdge(e, a)->incoming();
							e = graph.splitEdge(e, b)->incoming();
							e = graph.splitEdge(e, c)->incoming();
							e = graph.splitEdge(e, d)->incoming();
							graph.splitEdge(e, f);
						}

						break;
					}
					default:
						assert(false);// undetermined should not occur
						break;
					}

				}
			};
		};

	} // namespace detail

	template<class Graph>
	void restrict_directions(Graph& graph, std::vector<Vector<Inexact>> dirs, Number<Inexact> lambda, Number<Inexact> eps) {

		assert(graph.isSorted());
		assert(graph.isOriented());

		// we assume directions are sorted by construction
		// and we assume they are normalized

		detail::AngleRestriction<Graph> ar(graph, dirs);

		ar.determine_significant_vertices();
		ar.subdivide_edges(lambda);
		ar.assign_directions();
		ar.assign_double_insignificant();
		ar.determine_interference_regions(eps);
		ar.assign_step_counts();
		ar.create_staircases(eps);
	}

	template<class Graph>
	void restrict_directions(Graph& graph, int count, Number<Inexact> initial_angle, Number<Inexact> lambda, Number<Inexact> eps) {
		using Dir = Vector<Inexact>;
		using Transform = CGAL::Aff_transformation_2<Inexact>;

		assert(count >= 4);

		std::vector<Dir> dirs;

		// first direction
		Dir dir(1, 0);
		dir = dir.transform(Transform(CGAL::ROTATION, std::sin(initial_angle), std::cos(initial_angle)));
		dirs.push_back(dir);

		// the other directions
		Number<Inexact> a = std::numbers::pi * 2.0 / count;
		int i = 1;
		while (i < count) {

			Transform t(CGAL::ROTATION, std::sin(i * a), std::cos(i * a));
			dirs.push_back(dir.transform(t));
			i++;
		}

		restrict_directions(graph, dirs, lambda, eps);
	}

	template<class Graph>
	void restrict_directions(Graph& graph, std::initializer_list<Number<Inexact>> angles, Number<Inexact> lambda, Number<Inexact> eps) {
		using Dir = Vector<Inexact>;
		using Transform = CGAL::Aff_transformation_2<Inexact>;

		// right
		Dir dir(1, 0);

		std::vector<Dir> dirs;

		for (Number<Inexact> a : angles) {
			Transform t(CGAL::ROTATION, std::sin(a), std::cos(a));
			dirs.push_back(dir.transform(t));
		}

		restrict_directions(graph, dirs, lambda, eps);
	}
}