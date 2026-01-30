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
			using Vec = Vector<Inexact>;
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

			inline Vec vector(Vertex* v, Edge* e) {
				auto start = to_inexact(v->getPoint());
				auto end = to_inexact(e->other(v)->getPoint());
				return end - start;
			}

			inline Vec direction(Vertex* v, Edge* e) {
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
				std::pair<int, int> associated = std::pair<int, int>(-1, -1);
				int assigned = -1;
				EdgeType type = EdgeType::UNDETERMINED;
				int stepcount = -1;
				Num stepdist = -1;
				Polygon<Inexact> interference;

				inline int other_direction() {
					return assigned == associated.first ? associated.second : associated.first;
				}
			};

			inline Segment<Inexact> directed_segment(Edge* e, EdgeData& d) {
				return Segment<Inexact>(to_inexact(d.significant->getPoint()), to_inexact(e->other(d.significant)->getPoint()));
			}

			Graph& graph;
			std::vector<Vector<Inexact>> directions;
			std::vector<bool> significant_vertices;
			std::vector<EdgeData> edge_data;

			AngleRestriction(Graph& graph, std::vector<Vec> dirs) : graph(graph), directions(dirs) {
			};

			bool is_approximately(Vec a, Vec b) {
				const Num eps = 0.000001;
				return std::abs(a.x() - b.x()) < eps
					&& std::abs(a.y() - b.y()) < eps;
			}

			std::pair<int, int> associated_directions(Vertex* v, Edge* e) {

				Vec dir = direction(v, e);

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

			void find_best(std::vector<Vec>& out, std::vector<int>& build, Num build_cost, int next, std::vector<int>* best, Num* best_cost) {
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

				Vec dir = direction(v, e);

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
					std::vector<Vec> out;

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
						int gi = e->graphIndex();
						EdgeData& edata = edge_data[gi];
						edata.significant = v;
						edata.associated = associated_directions(v, e);
						edata.assigned = best[i];

						if (edata.associated.second < 0) {
							if (best[i] == edata.associated.first) {
								edata.type = EdgeType::ALIGN;
								std::cout << gi << " -- align\n";
							}
							else {
								edata.type = EdgeType::DEV_ALIGN;
								std::cout << gi << " -- dev align\n";
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
								std::cout << gi << " -- evading\n";
							}
							else {
								edata.type = EdgeType::UNALIGN;
								std::cout << gi << " -- unalign\n";
							}
						}
						else {
							edata.type = EdgeType::DEV_UNALIGN;
							std::cout << gi << " -- dev unalign\n";
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
						Vec dir = to_inexact(e->getTarget()->getPoint() - e->getSource()->getPoint());
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

			void determine_interference_regions(Num eps) {
				int e_cnt = graph.getEdgeCount();

				for (int i = 0; i < e_cnt; i++) {

					Edge* e = graph.getEdges()[i];
					EdgeData& edata = edge_data[i];

					switch (edata.type) {
					default: {
						assert(false); // undetermined should not occur
						break;
					}
					case EdgeType::ALIGN: {
						edata.interference.push_back(to_inexact(edata.significant->getPoint()));
						edata.interference.push_back(to_inexact(e->other(edata.significant)->getPoint()));
						break;
					}
					case EdgeType::UNALIGN:
					case EdgeType::EVADING: {
						Vec dir_1 = directions[edata.assigned];
						Vec dir_2 = directions[edata.other_direction()];
						Vec evec = vector(edata.significant, e);

						std::pair<Num, Num> fg = solve_vector_addition(dir_1, dir_2, evec);

						auto v_pt = to_inexact(edata.significant->getPoint());
						auto o_pt = to_inexact(e->other(edata.significant)->getPoint());
						edata.interference.push_back(v_pt);
						edata.interference.push_back(v_pt + dir_1 * fg.first);
						edata.interference.push_back(o_pt);
						edata.interference.push_back(v_pt + dir_2 * fg.second);

						if (!edata.interference.is_counterclockwise_oriented()) {
							edata.interference.reverse_orientation();
						}
						break;
					}
					case EdgeType::DEV_UNALIGN: {

						Vec dir_close, dir_far;
						if ((edata.assigned + 1) % directions.size() == edata.associated.first) {
							dir_close = directions[edata.associated.first];
							dir_far = directions[edata.associated.second];
						}
						else {
							dir_close = directions[edata.associated.second];
							dir_far = directions[edata.associated.first];
						}

						Vec step = vector(edata.significant, e) / 3.0;

						std::pair<Num, Num> fg = solve_vector_addition(dir_close, dir_far, step);

						dir_close *= fg.first;
						dir_far *= fg.second;

						Num halfcross = 0.5 * std::abs(CGAL::determinant(dir_close, dir_far));
						Num len = halfcross / std::abs(CGAL::determinant(dir_close, directions[edata.assigned]));
						Vec d = len * directions[edata.assigned];

						auto v_pt = to_inexact(edata.significant->getPoint());
						auto o_pt = to_inexact(e->other(edata.significant)->getPoint());

						auto far_pt = v_pt + d + dir_close;
						auto is = CGAL::intersection(Line<Inexact>(o_pt, o_pt + dir_far), Line<Inexact>(far_pt, far_pt + dir_close));
						auto corner = std::get<Point<Inexact>>(*is);

						edata.interference.push_back(v_pt);
						edata.interference.push_back(v_pt + d);
						if (CGAL::squared_distance(v_pt, far_pt) > CGAL::squared_distance(v_pt, corner)) {
							edata.interference.push_back(far_pt);
						}
						else {
							edata.interference.push_back(corner);
						}
						edata.interference.push_back(o_pt);
						edata.interference.push_back(o_pt - 3 * dir_close);

						if (!edata.interference.is_counterclockwise_oriented()) {
							edata.interference.reverse_orientation();
						}
						break;
					}
					case EdgeType::DEV_ALIGN: {

						auto v_pt = to_inexact(edata.significant->getPoint());
						auto o_pt = to_inexact(e->other(edata.significant)->getPoint());

						auto len = std::sqrt(CGAL::squared_distance(v_pt, o_pt));

						Vec dir_1 = directions[edata.assigned];
						Vec dir_2 = directions[edata.other_direction()];

						auto step_dist = len * eps;
						auto step_len = (1 - eps) * len / 2.0;

						auto sidestep = step_dist * dir_1;
						auto lengthstep = step_len * dir_2;

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

						if (!edata.interference.is_counterclockwise_oriented()) {
							edata.interference.reverse_orientation();
						}
						break;
					}
					}

				}
			}

			void assign_step_counts(Num eps) {

				std::cout << "assign_step_counts\n";

				int e_cnt = graph.getEdgeCount();

				auto even_rounding = [](Num v) {
					int k = (int)std::ceil(v + 0.000001);
					k += k % 2;
					return k;
					};

				auto uncommon_interference = [](EdgeData& a, EdgeData& b) {
					if (a.type == EdgeType::ALIGN) {
						assert(b.type != EdgeType::ALIGN);


						if (!b.interference.has_on_unbounded_side(a.interference[0])) {
							return true;
						}

						Segment<Inexact> a_ls = Segment<Inexact>(a.interference[0], a.interference[1]);

						for (auto ls : b.interference.edges()) {
							if (CGAL::intersection(ls, a_ls).has_value()) {
								return true;
							}
						}
						return false;
					}
					else if (b.type == EdgeType::ALIGN) {
						assert(a.type != EdgeType::ALIGN);

						if (!a.interference.has_on_unbounded_side(b.interference[0])) {
							return true;
						}

						Segment<Inexact> b_ls = Segment<Inexact>(b.interference[0], b.interference[1]);
						for (auto ls : a.interference.edges()) {
							if (CGAL::intersection(ls, b_ls).has_value()) {
								return true;
							}
						}
						return false;
					}
					else {
						// this only works if the interference polygons are not segments (i.e. for aligned edges...)
						
						if (!a.interference.has_on_unbounded_side(b.interference[0])) {
							return true;
						}

						if (!b.interference.has_on_unbounded_side(a.interference[0])) {
							return true;
						}

						for (auto ls_a : a.interference.edges()) {
							for (auto ls_b : b.interference.edges()) {
								if (CGAL::intersection(ls_a, ls_b).has_value()) {
									return true;
								}
							}
						}

						return false;
						// this throws a CGAL error -- possibly because of inexact. Above is fine, our polygons are small...
						/*try {
							return CGAL::do_intersect(a.interference, b.interference);
						}
						catch (...) {
							std::cout << "ERROR\n";
							std::cout << "<ipeselection pos = \"0 0\">\n";
							std::cout << "<path layer=\"alpha\" stroke=\"red\" cap=\"1\" join=\"1\">\n";

							std::cout << a.interference[0] << " m\n";
							for (int i = 1; i < a.interference.size(); i++)
								std::cout << a.interference[i] << " l\n";
							std::cout << "h\n";
							std::cout << "</path>\n";
							std::cout << "<path layer=\"alpha\" stroke=\"blue\" cap=\"1\" join=\"1\">\n";

							std::cout << b.interference[0] << " m\n";
							for (int i = 1; i < b.interference.size(); i++)
								std::cout << b.interference[i] << " l\n";
							std::cout << "h\n";
							std::cout << "</path>\n";
							std::cout << "</ipeselection>";

							assert(false);
						}*/
					}
					};

				auto common_interference = [](EdgeData& a, EdgeData& b) {
					if (a.type == EdgeType::ALIGN || b.type == EdgeType::ALIGN) {
						// aligned cannot interfere with anything else
						return false;
					}

					if ((a.type == EdgeType::DEV_ALIGN || a.type == EdgeType::DEV_UNALIGN)
						&& (a.type == EdgeType::DEV_ALIGN || a.type == EdgeType::DEV_UNALIGN)) {
						// deviating cannot interfere with eachother
						return false;
					}

					if ((a.type == EdgeType::UNALIGN && b.type == EdgeType::UNALIGN)
						|| (a.type == EdgeType::UNALIGN && b.type == EdgeType::EVADING)
						|| (a.type == EdgeType::EVADING && b.type == EdgeType::UNALIGN)) {
						// unaligned cannot interfere with other unaligned or evading
						return false;
					}

					if (a.type == EdgeType::EVADING && b.type == EdgeType::EVADING) {
						// two evading edges only interfere if they lie in the same sector
						return a.associated.first == b.associated.first && a.associated.second == b.associated.second;
					}

					if (a.type == EdgeType::UNALIGN || a.type == EdgeType::EVADING) {
						if (b.type == EdgeType::DEV_UNALIGN) {
							// only interfere if they lie in the same sector
							return a.associated.first == b.associated.first && a.associated.second == b.associated.second;
						}
						else {
							assert(b.type == EdgeType::DEV_ALIGN);
							// count the aligned direction for both sectors
							return a.associated.first == b.associated.first || a.associated.second == b.associated.first;
						}
					}

					if (b.type == EdgeType::UNALIGN || b.type == EdgeType::EVADING) {
						if (a.type == EdgeType::DEV_UNALIGN) {
							// only interfere if they lie in the same sector
							return a.associated.first == b.associated.first && a.associated.second == b.associated.second;
						}
						else {
							assert(a.type == EdgeType::DEV_ALIGN);
							// count the aligned direction for both sectors
							return a.associated.first == b.associated.first || a.associated.first == b.associated.second;
						}
					}

					assert(false); // should not occur, the above cases are exhaustive
					return false;
					};

				auto ignore = [](Segment<Inexact> e, Num frac) {
					return Segment<Inexact>(e.start() + (e.end() - e.start()) * frac, e.end());
					};

				// first we do deviating edges
				for (int i = 0; i < e_cnt; i++) {

					Edge* e = graph.getEdges()[i];
					EdgeData& edata = edge_data[i];

					switch (edata.type) {
					default: {
						assert(false); // shouldn't happen, undetermined edge somewhere?
						break;
					}
					case EdgeType::ALIGN:
					case EdgeType::UNALIGN:
					case EdgeType::EVADING: {
						break;
					}
					case EdgeType::DEV_UNALIGN: {
						Num min_dist_sqr = std::numeric_limits<Num>::infinity();
						Segment<Inexact> seg = directed_segment(e, edata);
						Segment<Inexact> seg_ignore = ignore(seg, 1.0 / 3.0);

						// TODO: use quad tree to speed things up
						for (Edge* other : graph.getEdges()) {

							if (other == e) {
								continue;
							}

							Vertex* common = other->commonVertex(e);

							if (common == nullptr) {
								// no shared vertex, treat normally

								if (!uncommon_interference(edata, edge_data[other->graphIndex()])) {
									continue;
								}

								min_dist_sqr = std::min(min_dist_sqr, CGAL::squared_distance(seg, to_inexact(other->getSegment())));
							}
							else if (common == edata.significant) {
								// the other edge must be also evading or deviating

								EdgeData& odata = edge_data[other->graphIndex()];

								if (!common_interference(edata, odata)) {
									continue;
								}

								Segment<Inexact> seg_other = directed_segment(other, odata);
								min_dist_sqr = std::min(min_dist_sqr, CGAL::squared_distance(seg_ignore, seg_other));

							} // else: sharing an insignficant vertex, these staircases do not interact

						}

						for (Vertex* vv : graph.getVertices()) {
							if (vv->degree() == 0) {
								min_dist_sqr = std::min(min_dist_sqr, CGAL::squared_distance(to_inexact(vv->getPoint()), seg));
							}
						}

						if (!std::isfinite(min_dist_sqr)) {
							edata.stepcount = 4;
						}
						else {
							Num min_dist = std::sqrt(min_dist_sqr);

							Vec dir_close, dir_far;
							if ((edata.assigned + 1) % directions.size() == edata.associated.first) {
								dir_close = directions[edata.associated.first];
								dir_far = directions[edata.associated.second];
							}
							else {
								dir_close = directions[edata.associated.second];
								dir_far = directions[edata.associated.first];
							}

							Num elen = std::sqrt(to_inexact(e->squared_length()));

							// set dir_close/far such that describe a step of unit length
							Vec step = vector(edata.significant, e) / elen;
							std::pair<Num, Num> fg = solve_vector_addition(dir_close, dir_far, step);
							dir_close *= fg.first;
							dir_far *= fg.second;

							// the area of the unit step is the A = |dir_close X dir_far| / 2
							// we need to find a vector side_step = f D, such that |dir_close X side_step| = A
							//    where D is the assigned direction and some f > 0
							// that is, |dir_close X f D| = A
							// which is the same as f |dir_close X D| = A
							// so this reduces to f = A / |dir_close X D| = 0.5 |dir_close X dir_far| / |dir_close X D|

							Vec dir_assigned = directions[edata.assigned];
							Num f = 0.5 * std::abs(CGAL::determinant(dir_close, dir_far)) / std::abs(CGAL::determinant(dir_close, dir_assigned));
							Vec side_step = f * dir_assigned;

							Vec vp = dir_close + side_step;

							Num d_1 = std::sqrt(CGAL::squared_distance(seg.start() + vp, seg));

							edata.stepcount = std::max(4, even_rounding(2 * d_1 * elen / min_dist + 1));
						}
						break;
					}
					case EdgeType::DEV_ALIGN: {
						Num min_dist_sqr = std::numeric_limits<Num>::infinity();
						Segment<Inexact> seg = directed_segment(e, edata);
						Segment<Inexact> seg_ignore = ignore(seg, (1 - eps) / 2.0);

						// TODO: use quad tree to speed things up
						for (Edge* other : graph.getEdges()) {

							if (other == e) {
								continue;
							}

							Vertex* common = other->commonVertex(e);

							if (common == nullptr) {
								// no shared vertex, treat normally

								if (!uncommon_interference(edata, edge_data[other->graphIndex()])) {
									continue;
								}

								min_dist_sqr = std::min(min_dist_sqr, CGAL::squared_distance(seg, to_inexact(other->getSegment())));
							}
							else if (common == edata.significant) {
								// the other edge must be also evading or deviating

								EdgeData& odata = edge_data[other->graphIndex()];

								if (!common_interference(edata, odata)) {
									continue;
								}

								Segment<Inexact> seg_other = directed_segment(other, odata);
								min_dist_sqr = std::min(min_dist_sqr, CGAL::squared_distance(seg_ignore, seg_other));

							} // else: sharing an insignficant vertex, these staircases do not interact

						}

						for (Vertex* vv : graph.getVertices()) {
							if (vv->degree() == 0) {
								min_dist_sqr = std::min(min_dist_sqr, CGAL::squared_distance(to_inexact(vv->getPoint()), seg));
							}
						}

						Num maxdist = eps * std::sqrt(to_inexact(e->squared_length()));
						if (!std::isfinite(min_dist_sqr)) {
							edata.stepcount = maxdist;
						}
						else {
							Num min_dist = std::sqrt(min_dist_sqr);
							edata.stepdist = std::min(min_dist / 2, maxdist);
						}
						break;
					}

					} // switch
				} // loop

				// then the remaining edges
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

							Vertex* common = other->commonVertex(e);

							if (common == nullptr) {
								// no shared vertex, treat normally

								if (!uncommon_interference(edata, edge_data[other->graphIndex()])) {
									continue;
								}

								Segment<Inexact> seg_other = to_inexact(other->getSegment());
								Num dist_sqr = CGAL::squared_distance(seg, seg_other);
								min_dist_sqr = std::min(min_dist_sqr, dist_sqr);
							}
							else if (common == edata.significant) {
								// the other edge must be deviating, either aligned or unaligned

								EdgeData& odata = edge_data[other->graphIndex()];

								if (!common_interference(edata, odata)) {
									continue;
								}

								Segment<Inexact> seg_other = directed_segment(other, odata);

								switch (odata.type) {
								case EdgeType::DEV_ALIGN: {
									min_dist_sqr = std::min(min_dist_sqr, CGAL::squared_distance(seg, ignore(seg_other, (1 - eps) / 2.0)));
									break;
								}
								case EdgeType::DEV_UNALIGN: {
									min_dist_sqr = std::min(min_dist_sqr, CGAL::squared_distance(seg, ignore(seg_other, 1.0 / (odata.stepcount - 1))));
									break;
								}
								default: {
									assert(false); // unexpected edge type
									break;
								}
								}

							} // else: sharing an insignficant vertex, these staircases do not interact
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
							Vec dir_1 = directions[edata.assigned];
							Vec dir_2 = directions[edata.other_direction()];
							Num elen = std::sqrt(to_inexact(e->squared_length()));
							Vec edir = vector(edata.significant, e) / elen;
							Num alpha_1 = std::abs(std::acos(edir * dir_1));
							Num alpha_2 = std::abs(std::acos(edir * dir_2));
							Num lmax = (1 / std::tan(alpha_1) + 1 / std::tan(alpha_2)) * min_dist / 2;
							edata.stepcount = even_rounding(elen / lmax);
						}
						break;
					}
					case EdgeType::EVADING: {
						Num min_dist_sqr = std::numeric_limits<Num>::infinity();
						Segment<Inexact> seg = directed_segment(e, edata);
						Segment<Inexact> seg_ev = ignore(seg, 0.5);

						// TODO: use quad tree to speed things up
						for (Edge* other : graph.getEdges()) {

							if (other == e) {
								continue;
							}

							Vertex* common = other->commonVertex(e);

							if (common == nullptr) {
								// no shared vertex, treat normally

								if (!uncommon_interference(edata, edge_data[other->graphIndex()])) {
									continue;
								}

								Segment<Inexact> seg_other = to_inexact(other->getSegment());
								Num dist_sqr = CGAL::squared_distance(seg, seg_other);
								min_dist_sqr = std::min(min_dist_sqr, dist_sqr);
							}
							else if (common == edata.significant) {
								// the other edge must be also evading or deviating

								EdgeData& odata = edge_data[other->graphIndex()];

								if (!common_interference(edata, odata)) {
									continue;
								}

								Segment<Inexact> seg_other = directed_segment(other, odata);

								switch (odata.type) {
								case EdgeType::EVADING: {
									min_dist_sqr = std::min(min_dist_sqr, CGAL::squared_distance(seg_ev, seg_other));
									break;
								}
								case EdgeType::DEV_ALIGN: {
									min_dist_sqr = std::min(min_dist_sqr, CGAL::squared_distance(seg, ignore(seg_other, (1 - eps) / 2.0)));
									break;
								}
								case EdgeType::DEV_UNALIGN: {
									min_dist_sqr = std::min(min_dist_sqr, CGAL::squared_distance(seg, ignore(seg_other, 1.0 / (odata.stepcount - 1))));
									break;
								}
								default: {
									assert(false); // unexpected edge type
									break;
								}
								}

							} // else: sharing an insignficant vertex, these staircases do not interact

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
							Vec dir_1 = directions[edata.assigned];
							Vec dir_2 = directions[edata.other_direction()];
							Num elen = std::sqrt(to_inexact(e->squared_length()));
							Vec edir = vector(edata.significant, e) / elen;
							Num alpha_1 = std::abs(std::acos(edir * dir_1));
							Num alpha_2 = std::abs(std::acos(edir * dir_2));
							Num lmax = (1 / std::tan(alpha_1) + 1 / std::tan(alpha_2)) * min_dist / 2;
							edata.stepcount = even_rounding(elen / lmax);
						}
						break;
					}
					case EdgeType::DEV_UNALIGN:
					case EdgeType::DEV_ALIGN: {
						// already done in previous loop
						break;
					}

					} // switch
				} // loop

			};

			void create_staircases(Num eps) {

				std::cout << "create_staircases\n";

				int e_cnt = graph.getEdgeCount();

				for (int i = 0; i < e_cnt; i++) {

					Edge* e = graph.getEdges()[i];
					EdgeData& edata = edge_data[i];
					Vertex* v = edata.significant;

					std::function<Edge* (Edge*, Point<Kernel>)> split;
					if (v == e->getSource()) {
						split = [this](Edge* edge, Point<Kernel> point) {
							return graph.splitEdge(edge, point)->outgoing();
							};
					}
					else {
						split = [this](Edge* edge, Point<Kernel> point) {
							return graph.splitEdge(edge, point)->incoming();
							};
					}

					switch (edata.type) {
					case EdgeType::ALIGN: {
						// dont need to do anything
						break;
					}
					case EdgeType::UNALIGN: {
						int k = edata.stepcount;

						assert(k > 1 && k % 2 == 0);

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
						int k = edata.stepcount;

						assert(k >= 4 && k % 2 == 0);

						Vec dir_close, dir_far;
						if ((edata.assigned + 1) % directions.size() == edata.associated.first) {
							dir_close = directions[edata.associated.first];
							dir_far = directions[edata.associated.second];
						}
						else {
							dir_close = directions[edata.associated.second];
							dir_far = directions[edata.associated.first];
						}

						// set dir_close/far such that describe a step of unit length
						Vec step = vector(edata.significant, e) / (k - 1);

						std::pair<Num, Num> fg = solve_vector_addition(dir_close, dir_far, step);
						dir_close *= fg.first;
						dir_far *= fg.second;

						// the area of the unit step is the A = |dir_close X dir_far| / 2
						// we need to find a vector side_step = f D, such that |dir_close X side_step| = A
						//    where D is the assigned direction and some f > 0
						// that is, |dir_close X f D| = A
						// which is the same as f |dir_close X D| = A
						// so this reduces to f = A / |dir_close X D| = 0.5 |dir_close X dir_far| / |dir_close X D|

						Vec dir_assigned = directions[edata.assigned];
						Num f = 0.5 * std::abs(CGAL::determinant(dir_close, dir_far)) / std::abs(CGAL::determinant(dir_close, dir_assigned));

						Vector<Kernel> side_step = to_kernel(f * dir_assigned);
						Vector<Kernel> close_step = to_kernel(dir_close);
						Vector<Kernel> far_step = to_kernel(dir_far);

						int hk = k / 2;

						// side step
						auto pt = v->getPoint() + side_step;
						e = split(e, pt);
						pt += close_step;
						e = split(e, pt);
						pt -= side_step;
						if (!is_approximately(-dir_assigned, dir_far / fg.second)) {
							e = split(e, pt);
						}
						hk -= 2;

						// evading steps
						while (hk > 0) {
							hk--;

							pt += far_step;
							e = split(e, pt);
							pt += close_step;
							e = split(e, pt);
						}

						// other steps
						hk = k / 2;
						pt += 2 * far_step;
						e = split(e, pt);
						hk--;
						while (hk > 0) {
							hk--;

							pt += close_step;
							e = split(e, pt);
							pt += far_step;
							e = split(e, pt);
						}
						break;
					}
					case EdgeType::DEV_ALIGN: {

						auto v_pt = to_inexact(v->getPoint());
						auto o_pt = to_inexact(e->other(v)->getPoint());

						auto len = std::sqrt(CGAL::squared_distance(v_pt, o_pt));

						Vec dir_1 = directions[edata.assigned];
						Vec dir_2 = directions[edata.associated.first];

						auto step_dist = edata.stepdist;
						auto step_len = (1 - eps) * len / 2.0; // NB: this needs to use eps, not stepdist

						auto sidestep = to_kernel(step_dist * dir_1);
						auto lengthstep = to_kernel(step_len * dir_2);

						auto a = v->getPoint() + sidestep;
						auto b = a + lengthstep;
						auto c = b - 2 * sidestep;
						auto d = c + lengthstep;
						auto f = d + sidestep;

						e = split(e, a);
						e = split(e, b);
						e = split(e, c);
						e = split(e, d);
						split(e, f);
						break;
					}
					default:
						assert(false);// undetermined should not occur
						break;
					}

				}
			};
		};

		template<class Graph>
		void restrict_directions(Graph& graph, std::vector<Vector<Inexact>> directions, Number<Inexact> lambda, Number<Inexact> eps) {

			assert(graph.isSorted());
			assert(graph.isOriented());

			// we assume directions are sorted by construction
			// and we assume they are normalized

			detail::AngleRestriction<Graph> ar(graph, directions);

			ar.determine_significant_vertices();
			ar.subdivide_edges(lambda);
			ar.assign_directions();
			ar.assign_double_insignificant();
			ar.determine_interference_regions(eps);
			ar.assign_step_counts(eps);
			ar.create_staircases(eps);
		}

	} // namespace detail

	template<class Graph>
	void restrict_orientations(Graph& graph, std::vector<Vector<Inexact>> orientations, Number<Inexact> lambda, Number<Inexact> eps) {

		assert(graph.isSorted());
		assert(graph.isOriented());

		// convert orientations ("mod 180 degrees")
		// to directions ("mod 360 degrees")
		std::vector<Vector<Inexact>> dirs;

		for (Vector<Inexact> v : orientations) {
			assert(0.999999 <= v.squared_length() && v.squared_length() <= 1.000001);
			dirs.push_back(v);
		}
		for (Vector<Inexact> v : orientations) {
			dirs.push_back(-1 * v);
		}

		detail::restrict_directions(graph, dirs, lambda, eps);
	}

	template<class Graph>
	void restrict_orientations(Graph& graph, int count, Number<Inexact> initial_angle, Number<Inexact> lambda, Number<Inexact> eps) {
		using Vec = Vector<Inexact>;
		using Transform = CGAL::Aff_transformation_2<Inexact>;

		assert(count >= 2);

		std::vector<Vec> dirs;

		// first direction
		Vec dir(1, 0);
		dir = dir.transform(Transform(CGAL::ROTATION, std::sin(initial_angle), std::cos(initial_angle)));
		dirs.push_back(dir);

		// the other directions
		Number<Inexact> a = std::numbers::pi / count;
		int i = 1;
		while (i < 2 * count) {

			Transform t(CGAL::ROTATION, std::sin(i * a), std::cos(i * a));
			dirs.push_back(dir.transform(t));
			i++;
		}

		detail::restrict_directions(graph, dirs, lambda, eps);
	}

	template<class Graph>
	void restrict_orientations(Graph& graph, std::initializer_list<Number<Inexact>> angles, Number<Inexact> lambda, Number<Inexact> eps) {
		using Vec = Vector<Inexact>;
		using Transform = CGAL::Aff_transformation_2<Inexact>;

		// right
		Vec dir(1, 0);

		std::vector<Vec> dirs;

		for (Number<Inexact> a : angles) {
			Transform t(CGAL::ROTATION, std::sin(a), std::cos(a));
			dirs.push_back(dir.transform(t));
		}

		for (int i = 0; i < angles.size(); i++) {
			dirs.push_back(-1 * dirs[i]);
		}

		detail::restrict_directions(graph, dirs, lambda, eps);
	}
}