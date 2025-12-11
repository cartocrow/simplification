// -----------------------------------------------------------------------------
// IMPLEMENTATION OF TEMPLATE FUNCTIONS
// Do not include this file, but the .h file instead
// -----------------------------------------------------------------------------

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

		template<ModifiableGraph MG> struct BaseMove {

			MG::Edge* edge;

			Number<typename MG::Kernel> cost;
			int qid;

			bool blocked_by_degzero;
			std::vector<typename MG::Edge*> blocked_by;
		};

		template<ModifiableGraph MG> struct SingleMove : public BaseMove<MG> {

			using K = MG::Kernel;
			using Vertex = MG::Vertex;
			using Edge = MG::Edge;

			bool left;
			enum VertexType src_type, tar_type;
			bool rem_self, rem_previous, rem_next;
			bool merge_prev, merge_next;
			Polygon<K> swept;
			bool contract_merges_highdegrees;

			bool movable() {
				return src_type != UNMOVABLE && tar_type != UNMOVABLE;
			}

			void update() {
				//swept.clear();
				//cost = std::numeric_limits::infinity<Number<K>>;
				//contract_merges_highdegrees = false;


				//BMRSVertex a = null, b, c, d = null;
				//BMRSVertex a_alt = null, d_alt = null;

				//b = edge.getStart();
				//c = edge.getEnd();

				//Vector edgevector = Vector.subtract(c, b);
				//Vector movevector = makeMoveDir(edgevector, left);
				//Line baseline = Line.byThroughpoints(b, c);

				//// determine start type
				//switch (b.getDegree()) {
				//case 2: {
				//    a = edge.walkStart().getStart();
				//    if (Vector.dotProduct(Vector.subtract(a, b), movevector) > 0) {
				//        start_type = VertexType.DEG_TWO_SUPPORT;
				//    }
				//    else {
				//        start_type = VertexType.DEG_TWO_NO_SUPPORT;
				//    }
				//    break;
				//}
				//case 3: {
				//    a = b.getAdjacent(edge, left).getOtherVertex(b);
				//    a_alt = b.getAdjacent(edge, !left).getOtherVertex(b);
				//    if (baseline.onBoundary(a)) {
				//        start_type = VertexType.DEG_THREE_ALIGNED;
				//    }
				//    else if (Vector.dotProduct(Vector.subtract(a, b), movevector) > 0) {
				//        if (Line.byThroughpoints(a, b).onBoundary(a_alt)) {
				//            start_type = VertexType.DEG_THREE_SMOOTH;
				//        }
				//        else {
				//            start_type = VertexType.DEG_THREE_SUPPORT;
				//        }
				//    }
				//    else {
				//        start_type = VertexType.DEG_THREE_NO_SUPPORT;
				//    }
				//    break;
				//}
				//default:
				//    start_type = VertexType.UNMOVABLE;
				//    break;
				//}

				//// determine end type
				//switch (c.getDegree()) {
				//case 2: {
				//    d = edge.walkEnd().getEnd();
				//    if (Vector.dotProduct(Vector.subtract(d, c), movevector) > 0) {
				//        end_type = VertexType.DEG_TWO_SUPPORT;
				//    }
				//    else {
				//        end_type = VertexType.DEG_TWO_NO_SUPPORT;
				//    }
				//    break;
				//}
				//case 3: {
				//    d = c.getAdjacent(edge, !left).getOtherVertex(c);
				//    d_alt = c.getAdjacent(edge, left).getOtherVertex(c);
				//    if (baseline.onBoundary(d)) {
				//        end_type = VertexType.DEG_THREE_ALIGNED;
				//    }
				//    else if (Vector.dotProduct(Vector.subtract(d, c), movevector) > 0) {
				//        if (Line.byThroughpoints(c, d).onBoundary(d_alt)) {
				//            end_type = VertexType.DEG_THREE_SMOOTH;
				//        }
				//        else {
				//            end_type = VertexType.DEG_THREE_SUPPORT;
				//        }
				//    }
				//    else {
				//        end_type = VertexType.DEG_THREE_NO_SUPPORT;
				//    }
				//    break;
				//}
				//default:
				//    end_type = VertexType.UNMOVABLE;
				//    break;
				//}

				//if (!movable()) {
				//    return;
				//}

				//// how far can we move along the previous track before the previous edge vanishes
				//double prev_dist;
				//switch (start_type) {
				//case DEG_TWO_SUPPORT:
				//case DEG_THREE_SMOOTH:
				//case DEG_THREE_SUPPORT:
				//    prev_dist = baseline.distanceTo(a);
				//    break;
				//default:
				//    prev_dist = Double.POSITIVE_INFINITY;
				//    break;
				//}

				//// how far can we move along the next track before the next edge vanishes
				//double next_dist;
				//switch (end_type) {
				//case DEG_TWO_SUPPORT:
				//case DEG_THREE_SMOOTH:
				//case DEG_THREE_SUPPORT:
				//    next_dist = baseline.distanceTo(d);
				//    break;
				//default:
				//    next_dist = Double.POSITIVE_INFINITY;
				//    break;
				//}

				//// how far can we move until the edge itself vanishes
				//Line prev_track = Line.byThroughpoints(b, start_type == VertexType.DEG_THREE_ALIGNED ? a_alt : a);
				//Line next_track = Line.byThroughpoints(c, end_type == VertexType.DEG_THREE_ALIGNED ? d_alt : d);

				//double edge_dist;
				//Vector edge_lim = null;

				////        System.out.println("start "+start_type);
				////        System.out.println("end   "+end_type);
				//{
				//    List<BaseGeometry> is = prev_track.intersect(next_track);
				//    if (!is.isEmpty() && is.get(0) instanceof Vector) {
				//        edge_lim = (Vector)is.get(0);
				//        if (baseline.onBoundary(edge_lim) || left == baseline.isLeftOf(edge_lim)) {
				//            edge_dist = baseline.distanceTo(edge_lim);
				//            //                    System.out.println("limit "+edge_lim);
				//        }
				//        else {
				//            edge_dist = Double.POSITIVE_INFINITY;
				//            //                    System.out.println("wrong side");
				//        }
				//    }
				//    else {
				//        edge_dist = Double.POSITIVE_INFINITY;
				//        //                System.out.println("no int");
				//    }
				//}

				//// which is the limiting factor? and which edges then get removed?
				//double min_dist = DoubleUtil.min(edge_dist, prev_dist, next_dist);

				//if (Double.isNaN(edge_dist) || Double.isNaN(prev_dist) || Double.isNaN(next_dist)) {
				//    Algorithm.debug("edge " + edge);
				//    Algorithm.debug("  src " + edge.getStart().clone());
				//    Algorithm.debug("  tar " + edge.getEnd().clone());
				//    Algorithm.debug("edge_dist " + edge_dist);
				//    Algorithm.debug("prev_dist " + prev_dist);
				//    Algorithm.debug("next_dist " + next_dist);
				//    Algorithm.debug("prev track dir " + prev_track.getDirection());
				//    Algorithm.debug("next track dir " + next_track.getDirection());
				//    Algorithm.debug("  is: " + prev_track.intersect(next_track).get(0));
				//    Algorithm.debug("det: " + Vector.crossProduct(prev_track.getDirection(), next_track.getDirection()));
				//    throw new UnsupportedOperationException("??");
				//}

				//if (Double.isInfinite(min_dist)) {
				//    // don't allow moving this edge: diverging tracks without support
				//    start_type = VertexType.UNMOVABLE;
				//    end_type = VertexType.UNMOVABLE;
				//    return;
				//}
				//else if (Double.isInfinite(prev_dist) && Double.isInfinite(next_dist)) {
				//    // don't allow moving this edge: converging tracks without support
				//    start_type = VertexType.UNMOVABLE;
				//    end_type = VertexType.UNMOVABLE;
				//    return;
				//}

				//remove_self = DoubleUtil.close(min_dist, edge_dist);
				//remove_prev = DoubleUtil.close(min_dist, prev_dist);
				//remove_next = DoubleUtil.close(min_dist, next_dist);

				//// determine if the previous edge allows merging
				//if (remove_prev && b.getDegree() == 2 && a.getDegree() == 2) {
				//    Vector prevprevvector = Vector.subtract(a, edge.walkStart().walkStart().getStart());
				//    prevprevvector.normalize();

				//    Vector mergevector;
				//    if (remove_self) {
				//        // check if double-prev can merge with next
				//        mergevector = Vector.subtract(d, c);
				//    }
				//    else {
				//        // check alignment with self
				//        mergevector = edgevector;
				//    }
				//    merge_prev = DoubleUtil.close(Vector.crossProduct(prevprevvector, mergevector), 0);
				//}
				//else {
				//    merge_prev = false;
				//}

				//// determine if the next edge allows merging
				//if (remove_next && c.getDegree() == 2 && d.getDegree() == 2) {
				//    Vector nextnextvector = Vector.subtract(edge.walkEnd().walkEnd().getEnd(), d);
				//    nextnextvector.normalize();

				//    Vector mergevector;
				//    if (remove_self) {
				//        // check if double-next can merge with prev
				//        mergevector = Vector.subtract(b, a);
				//    }
				//    else {
				//        // check alignment with self
				//        mergevector = edgevector;
				//    }
				//    merge_next = DoubleUtil.close(Vector.crossProduct(nextnextvector, mergevector), 0);
				//}
				//else {
				//    merge_next = false;
				//}

				//// construct the swept area
				//swept = new Polygon();
				//if (remove_self) {
				//    swept.addVertex(edge_lim);
				//}
				//else if (remove_prev) {
				//    swept.addVertex(a.clone());
				//}
				//else {
				//    // remove_next must be true
				//    Line end_line = new Line(d, edgevector);
				//    List<BaseGeometry> is = end_line.intersect(prev_track, 0); // force using the actual intersection, we (should) have eliminated exactly equal directions
				//    swept.addVertex((Vector)is.get(0));
				//}
				//swept.addVertex(b.clone());
				//swept.addVertex(c.clone());
				//if (remove_self) {
				//    // do nothing, triangular area
				//}
				//else if (remove_next) {
				//    swept.addVertex(d.clone());
				//}
				//else {
				//    // remove_prev must be true!
				//    Line end_line = new Line(a, edgevector);
				//    List<BaseGeometry> is = end_line.intersect(next_track, 0); // force using the actual intersection, we (should) have eliminated exactly equal directions
				//    swept.addVertex((Vector)is.get(0));
				//}
				//cost = swept.areaUnsigned();

				//// precision, be sure we dont want to remove self...        
				//if (!remove_self && swept.vertex(0).isApproximately(swept.vertex(3))) {
				//    remove_self = true;
				//    swept.removeVertex(3);
				//}

				//if (remove_self && b.getDegree() != 2 && c.getDegree() != 2) {
				//    contractmergeshighdegrees = true;
				//}
				//else if (remove_prev && a.getDegree() != 2 && b.getDegree() != 2) {
				//    contractmergeshighdegrees = true;
				//}
				//else if (remove_next && c.getDegree() != 2 && d.getDegree() != 2) {
				//    contractmergeshighdegrees = true;
				//}
				//else {
				//    contractmergeshighdegrees = false;
				//}
			}
		};

		template<ModifiableGraph MG> struct ComboMove : public BaseMove<MG> {

			using K = MG::Kernel;
			using Vertex = MG::Vertex;
			using Edge = MG::Edge;

			Polygon<K> swept_prev;
			Polygon<K> swept_next;
		};


		template<ModifiableGraph MG> struct EMBase {

			SingleMove<MG> left, right;
			ComboMove<MG> combo;
			std::vector<BaseMove<MG>*> blocking;
		};

		template <typename K> struct EMData : public EMBase<typename EdgeMoveGraph<K>> {

		};

		template <typename K> struct HEMData : public EMBase<typename HEMGraph<K>> {
			Operation<HEMGraph<K>>* hist = nullptr;
		};

		template<ModifiableGraph MG>
		struct MoveQueueTraits {

			using Element = BaseMove<MG>;

			static void setIndex(Element* m, int id) {
				m->qid = id;
			}

			static int getIndex(Element* m) {
				return m->qid;
			}

			static int compare(Element* a, Element* b) {
				Number<MG::Kernel> ac = a->cost;
				Number<MG::Kernel> bc = b->cost;
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

	template <class MG, class EMT> requires detail::EMSetup<MG, EMT>
	void EdgeMove<MG, EMT>::update(Edge* e) {
		{// left
			Single& left = e->data().left;
			left.edge = e;
			left.blocked_by_degzero = false;
			left.blocked_by.clear();
			left.left = true;


			left.update();

			if (left.movable()) {
				EMT::determineSingleCost(left);

				if (queue.contains(left)) {
					queue.update(left);
				}
				else {
					queue.push(left);
				}
			}
			else {
				queue.remove(left);
			}
		}
		{// TODO: right
			int a;
		}
		{// TODO: combo
			int b;
		}
	}

	template <class MG, class EMT> requires detail::EMSetup<MG, EMT>
	bool EdgeMove<MG, EMT>::blocks(Edge& e, Move& move) {
		return false;
	}

	template <class MG, class EMT> requires detail::EMSetup<MG, EMT>
	EdgeMove<MG, EMT>::EdgeMove(MG& g, SegmentQuadTree<Edge, Kernel>& sqt, PointQuadTree<Vertex, Kernel>& pqt)
		: graph(g), sqt(sqt), pqt(pqt) {
	}

	template <class MG, class EMT> requires detail::EMSetup<MG, EMT>
	EdgeMove<MG, EMT>::~EdgeMove() {}

	template <class MG, class EMT> requires detail::EMSetup<MG, EMT>
	void EdgeMove<MG, EMT>::initialize(bool initSQT, bool initPQT) {
		// TODO: erase deg-2 aligned vertices

		if (initSQT) {
			sqt.clear();
			for (Edge* e : graph.getEdges()) {
				sqt.insert(*e);
			}
		}

		if (initPQT) {
			pqt.clear();
			for (Vertex* v : graph.getVertices()) {
				if (v->degree() == 0) {
					pqt.insert(*v);
				}
			}
		}

		for (Edge* e : graph.getEdges()) {
			update(e);
		}
	}

	template <class MG, class EMT> requires detail::EMSetup<MG, EMT>
	bool EdgeMove<MG, EMT>::runToComplexity(int k) {
		while (graph.getEdgeCount() > k) {
			if (!step()) {
				return false;
			}
		}
		return true;
	}

	template <class MG, class EMT> requires detail::EMSetup<MG, EMT>
	bool EdgeMove<MG, EMT>::step() {
		if constexpr (ModifiableGraphWithHistory<MG>) {
			assert(graph.atPresent());
		}

		while (!queue.empty()) {
			Move* m = queue.pop();

			std::cout << "trying ";

			// TODO: test topology

			if (!m->blocked_by_degzero && m->blocked_by.empty()) {

				// TODO: find pairing

				std::cout << " -> move!\n";
				// not blocked, executing!

				// TODO: uncheck

				if constexpr (ModifiableGraphWithHistory<MG>) {
					graph.startBatch(m.cost);
				}

				// execute

				if constexpr (ModifiableGraphWithHistory<MG>) {
					graph.endBatch();
				}

				// TODO: update

				return true;
			}
			else {
				std::cout << " -> blocked by " << m->blocked_by.size()
					<< " edges and by degzero: " << m->blocked_by_degzero << " \n ";
			}
		}

		return false;
	}

	template <typename G>
	void BuchinEtAlTraits<G>::determineSingleCost(detail::SingleMove<G>& sm) {
		sm.cost = CGAL::abs(CGAL::area(sm.swept));
	}

	template <typename G>
	void BuchinEtAlTraits<G>::determineComboCost(detail::ComboMove<G>& cm) {
		cm.cost = CGAL::abs(CGAL::area(cm.swept_prev));
	}

} // namespace cartocrow::simplification