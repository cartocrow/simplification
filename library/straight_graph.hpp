// -----------------------------------------------------------------------------
// IMPLEMENTATION OF TEMPLATE FUNCTIONS
// Do not include this file, but the .h file instead
// -----------------------------------------------------------------------------

#include "utils.h"

namespace cartocrow::simplification {


	template <class VD, class ED, typename K>
	bool StraightGraph<VD, ED, K>::verifyOriented() {
		for (Vertex* v : vertices) {
			if (v->degree() != 2) continue; // irrelevant for orientation

			Edge* bwd = v->incident[0];
			Edge* fwd = v->incident[1];

			if (bwd->target == v && fwd->source == v) continue; // already satisfies orientation

			std::cout << "edge assumptions not met" << std::endl;
			return false;
		}

		for (Edge* e : edges) {
			if (e->boundary == nullptr) {
				std::cout << "edge without boundary pointer" << std::endl;
				return false;
			}
		}

		for (Boundary* bd : boundaries) {
			if (bd->first->index < 0) {
				std::cout << "first edge of boundary not in graph" << std::endl;
				return false;
			}
			if (bd->last->index < 0) {
				std::cout << "last edge of boundary not in graph" << std::endl;
				return false;
			}
		}

		return true;
	}

	template <class VD, class ED, typename K>
	bool StraightGraph<VD, ED, K>::verifySorted() {
		for (Vertex* v : vertices) {
			if (v->degree() > 2) {
				CGAL::Direction_2<Kernel> dir_prev = CGAL::Direction_2<Kernel>(v->neighbor(0)->getPoint() - v->getPoint());
				for (int i = 1; i < v->degree(); i++) {
					CGAL::Direction_2<Kernel> dir = CGAL::Direction_2<Kernel>(v->neighbor(i)->getPoint() - v->getPoint());
					if (dir < dir_prev) {
						return false;
					}
					dir_prev = dir;
				}
			}
		}
		return true;
	}


	template <class VD, class ED, typename K>
	void StraightGraph<VD, ED, K>::clearBoundaries() {
		if (oriented) {
			for (Edge* e : edges) {
				e->boundary = nullptr;
			}
			for (Boundary* b : boundaries) {
				delete b;
			}
			boundaries.clear();

			oriented = false;
		}
	}


	template <class VD, class ED, typename K>
	StraightGraph<VD, ED, K>::StraightGraph() {
		oriented = false;
		sorted = false;
	}

	template <class VD, class ED, typename K>
	StraightGraph<VD, ED, K>::~StraightGraph() {
		for (Boundary* b : boundaries) {
			delete b;
		}
		for (Edge* e : edges) {
			delete e;
		}
		for (Vertex* v : vertices) {
			delete v;
		}
	}

	template <class VD, class ED, typename K>
	bool StraightGraph<VD, ED, K>::isOriented() {
		assert(!oriented || verifyOriented());
		return oriented;
	}

	template <class VD, class ED, typename K>
	bool StraightGraph<VD, ED, K>::isSorted() {
		assert(!sorted || verifySorted());
		return sorted;
	}

	template <class VD, class ED, typename K>
	int StraightGraph<VD, ED, K>::getVertexCount() {
		return vertices.size();
	}

	template <class VD, class ED, typename K>
	int StraightGraph<VD, ED, K>::getEdgeCount() {
		return edges.size();
	}


	template <class VD, class ED, typename K>
	int StraightGraph<VD, ED, K>::getBoundaryCount() {
		return boundaries.size();
	}

	template <class VD, class ED, typename K>
	std::vector<StraightVertex<VD, ED, K>*>& StraightGraph<VD, ED, K>::getVertices() {
		return vertices;
	}

	template <class VD, class ED, typename K>
	std::vector<StraightEdge<VD, ED, K>*>& StraightGraph<VD, ED, K>::getEdges() {
		return edges;
	}

	template <class VD, class ED, typename K>
	std::vector<StraightBoundary<VD, ED, K>*>& StraightGraph<VD, ED, K>::getBoundaries() {
		return boundaries;
	}

	template <class VD, class ED, typename K>
	StraightVertex<VD, ED, K>* StraightGraph<VD, ED, K>::addVertex(Point<K> pt) {
		clearBoundaries();
		sorted = false;

		Vertex* v = new Vertex();
		v->index = vertices.size();
		v->point = Point<K>(pt.x(), pt.y());
		vertices.push_back(v);
		return v;
	}

	template <class VD, class ED, typename K>
	void StraightGraph<VD, ED, K>::removeVertex(Vertex* vtx) {

		assert(vertices[vtx->index] == vtx);
		
		clearBoundaries();
		sorted = false;

		for (Edge* e : vtx->incident) {
			Vertex* other = e->other(vtx);

			utils::listRemove(e, other->incident);

			Edge* swp = utils::swapRemove(e->index, edges);
			if (swp != nullptr) {
				swp->index = e->index;
			}

			delete e;
		}


		Vertex* swp = utils::swapRemove(vtx->index, vertices);
		if (swp != nullptr) {
			swp->index = vtx->index;
		}
		delete vtx;
	}

	template <class VD, class ED, typename K>
	StraightEdge<VD, ED, K>* StraightGraph<VD, ED, K>::addEdge(Vertex* source, Vertex* target) {

		assert(!source->isNeighborOf(target));
		
		clearBoundaries();
		sorted = false;

		Edge* e = new Edge();
		e->index = edges.size();
		e->source = source;
		e->target = target;
		edges.push_back(e);
		source->incident.push_back(e);
		target->incident.push_back(e);

		return e;
	}
	template <class VD, class ED, typename K>
	void StraightGraph<VD, ED, K>::removeEdge(Edge* edge) {

		assert(edges[edge->index] == edge);

		clearBoundaries();
		sorted = false;

		utils::listRemove(edge, edge->source->incident);
		utils::listRemove(edge, edge->target->incident);

		Edge* swp = utils::swapRemove(edge->index, edges);
		if (swp != nullptr) {
			swp->index = edge->index;
		}

		delete edge;
	}

	template <class VD, class ED, typename K>
	void StraightGraph<VD, ED, K>::orientWithoutBoundaries() {
		if (oriented) {
			return;
		}

		clearBoundaries();

		for (Vertex* v : vertices) {
			if (v->degree() != 2) continue; // irrelevant for orientation

			Edge* bwd = v->incident[0];
			Edge* fwd = v->incident[1];

			if (bwd->target == v && fwd->source == v) continue; // already satisfies orientation
			if (bwd->source == v && fwd->target == v) {
				// just swap (necessary to ensure direction between graph copies remains the same)
				v->incident[0] = fwd;
				v->incident[1] = bwd;
				continue;
			}

			if (bwd->target != v) {
				bwd->reverse();
			}

			if (fwd->source != v) {
				fwd->reverse();
			}

			Vertex* fv = fwd->target;
			while (fv->degree() == 2 && fv != v) {
				if (fv->incident[0] != fwd) {
					fv->incident[1] = fv->incident[0];
					fv->incident[0] = fwd;
				}

				fwd = fv->incident[1];
				if (fwd->source != fv) {
					fwd->reverse();
				}
				fv = fwd->target;
			}

			if (fv != v) {
				Vertex* bv = bwd->source;
				while (bv->degree() == 2) {
					if (bv->incident[1] != bwd) {
						bv->incident[0] = bv->incident[1];
						bv->incident[1] = bwd;
					}

					bwd = bv->incident[0];
					if (bwd->target != bv) {
						bwd->reverse();
					}
					bv = bwd->source;
				}
			}
		}

		oriented = true;
	}

	template <class VD, class ED, typename K>
	void StraightGraph<VD, ED, K>::orient() {
		orientWithoutBoundaries();

		// construct boundaries
		for (Edge* e : edges) {
			if (e->boundary != nullptr) continue; // already handled

			Boundary* b = new Boundary();
			b->index = boundaries.size();
			boundaries.push_back(b);

			e->boundary = b;

			b->cyclic = false;
			b->first = e;
			b->last = nullptr;

			if (b->first->source->degree() == 2) {

				do {
					b->first = b->first->previous();
					b->first->boundary = b;
				} while (b->first->source->degree() == 2 && b->first != e);

				if (b->first == e) {
					b->cyclic = true;
					b->last = b->first->previous();
				}
			}

			if (b->last == nullptr) {
				b->last = e;
				while (b->last->target->degree() == 2) {
					b->last = b->last->next();
					b->last->boundary = b;
				}
			}
		}

		assert(verifyOriented());
	}

	template <class VD, class ED, typename K>
	void StraightGraph<VD, ED, K>::sortIncidentEdges() {
		for (Vertex* v : vertices) {
			if (v->degree() > 2) {
				std::ranges::sort(v->incident, [&v](Edge* e, Edge* f) {
					using Dir = Direction<K>;
					Dir dir_e = Dir(e->other(v)->getPoint() - v->getPoint());
					Dir dir_f = Dir(f->other(v)->getPoint() - v->getPoint());
					return dir_e < dir_f;
					});
			}
		}
		sorted = true;
		assert(verifySorted());
	}

	template <class VD, class ED, typename K>
	StraightVertex<VD, ED, K>* StraightGraph<VD, ED, K>::splitEdge(Edge* edge, Point<K> pt) {
		assert(oriented && verifyOriented());

		Vertex* v = new Vertex();
		v->index = vertices.size();
		v->point = Point<K>(pt.x(), pt.y());
		vertices.push_back(v);

		Vertex* w = edge->target;

		edge->target = v;
		v->incident.push_back(edge);

		Edge* newedge = new Edge();
		newedge->index = edges.size();
		newedge->source = v;
		newedge->target = w;
		newedge->boundary = edge->boundary;
		edges.push_back(newedge);

		if (edge->boundary->last == edge) {
			edge->boundary->last = newedge;
		}

		v->incident.push_back(newedge);
		utils::listReplace(edge, newedge, w->incident);

		assert(oriented && verifyOriented());

		return v;
	}

	template <class VD, class ED, typename K>
	StraightEdge<VD, ED, K>* StraightGraph<VD, ED, K>::mergeVertex(Vertex* v) {
		assert(oriented && verifyOriented());

		assert(v->degree() == 2);

		Edge* edge = v->incident[0]; // (u,v)
		Edge* other = v->incident[1]; // (v,w)

		assert(edge->boundary == other->boundary);

		assert(edge->boundary->first != edge->boundary->last);

		// "other" can be the first edge for cyclic boundaries (cycle of purely deg-2 vertices)
		if (other->boundary->first == other) {
			other->boundary->first = other->next();
		} else if (other->boundary->last == other) {
			other->boundary->last = edge;
		}

		// reroute edge and take its place in the other's target
		edge->target = other->target;
		utils::listReplace(other, edge, edge->target->incident);

		// delete other edge: no need to remove from incident lists...
		Edge* eswp = utils::swapRemove(other->index, edges);
		if (eswp != nullptr) {
			eswp->index = other->index;
		}
		delete other;

		// delete the vertex: no need to clear edges anymore
		Vertex* vswp = utils::swapRemove(v->index, vertices);
		if (vswp != nullptr) {
			vswp->index = v->index;
		}
		delete v;

		assert(oriented && verifyOriented());

		return edge;

	}
	template <class VD, class ED, typename K>
	void StraightGraph<VD, ED, K>::shiftVertex(Vertex* v, Point<K> pt) {
		v->setPoint(pt);
	}

	template <class VD, class ED, typename K>
	int StraightVertex<VD, ED, K>::graphIndex() {
		return index;
	}
	template <class VD, class ED, typename K>
	int StraightVertex<VD, ED, K>::degree() {
		return incident.size();
	}

	template <class VD, class ED, typename K>
	std::vector<StraightEdge<VD, ED, K>*>& StraightVertex<VD, ED, K>::getEdges() {
		return incident;
	}

	template <class VD, class ED, typename K>
	StraightEdge<VD, ED, K>* StraightVertex<VD, ED, K>::edge(int i) {
		return incident[i];
	}

	template <class VD, class ED, typename K>
	StraightVertex<VD, ED, K>* StraightVertex<VD, ED, K>::neighbor(int i) {
		return incident[i]->other(this);
	}

	template <class VD, class ED, typename K>
	StraightEdge<VD,ED,K>* StraightVertex<VD, ED, K>::edgeTo(Vertex* v) {
		for (Edge* e : incident) {
			if (e->other(this) == v) {
				return e;
			}
		}
		return nullptr;
	}

	template <class VD, class ED, typename K>
	bool StraightVertex<VD, ED, K>::isNeighborOf(Vertex* v) {
		return edgeTo(v) != nullptr;
		for (Edge* e : incident) {
			if (e->other(this) == v) {
				return true;
			}
		}
		return false;
	}

	template <class VD, class ED, typename K>
	VD& StraightVertex<VD, ED, K>::data() {
		return d;
	}

	template <class VD, class ED, typename K>
	void StraightVertex<VD, ED, K>::setPoint(Point<K>& pt) {
		point = Point<K>(pt.x(), pt.y());
	}

	template <class VD, class ED, typename K>
	Point<K>& StraightVertex<VD, ED, K>::getPoint() {
		return point;
	}

	template <class VD, class ED, typename K>
	StraightEdge<VD, ED, K>* StraightVertex<VD, ED, K>::incoming() {
		assert(degree() == 2);
		// assume oriented...
		return incident[0];
	}

	template <class VD, class ED, typename K>
	StraightEdge<VD, ED, K>* StraightVertex<VD, ED, K>::outgoing() {
		assert(degree() == 2);
		// assume oriented...
		return incident[1];
	}

	template <class VD, class ED, typename K>
	StraightVertex<VD, ED, K>* StraightVertex<VD, ED, K>::next() {
		assert(degree() == 2);
		// assume oriented...
		return incident[1]->target;
	}

	template <class VD, class ED, typename K>
	StraightVertex<VD, ED, K>* StraightVertex<VD, ED, K>::previous() {
		assert(degree() == 2);
		// assume oriented...
		return incident[0]->source;
	}



	template <class VD, class ED, typename K>
	int StraightEdge<VD, ED, K>::graphIndex() {
		return index;
	}

	template <class VD, class ED, typename K>
	StraightVertex<VD, ED, K>* StraightEdge<VD, ED, K>::getSource() {
		return source;
	}

	template <class VD, class ED, typename K>
	StraightVertex<VD, ED, K>* StraightEdge<VD, ED, K>::getTarget() {
		return target;
	}

	template <class VD, class ED, typename K>
	void StraightEdge<VD, ED, K>::reverse() {
		Vertex* t = source;
		source = target;
		target = t;
	}

	template <class VD, class ED, typename K>
	ED& StraightEdge<VD, ED, K>::data() {
		return d;
	}

	template <class VD, class ED, typename K>
	Number<K> StraightEdge<VD, ED, K>::squared_length() {
		return CGAL::squared_distance(source->point, target->point);
	}

	template <class VD, class ED, typename K>
	Segment<K> StraightEdge<VD, ED, K>::getSegment() {
		return Segment<K>(source->point, target->point);
	}

	template <class VD, class ED, typename K>
	StraightVertex<VD, ED, K>* StraightEdge<VD, ED, K>::other(Vertex* v) {
		assert(v == source || v == target);

		return v == source ? target : source;
	}

	template <class VD, class ED, typename K>
	StraightVertex<VD, ED, K>* StraightEdge<VD, ED, K>::commonVertex(Edge* e) {
		if (source == e->target || source == e->source) {
			return source;
		}
		else if (target == e->target || target == e->source) {
			return target;
		}
		else {
			return nullptr;
		}
	}

	template <class VD, class ED, typename K>
	StraightEdge<VD, ED, K>* StraightEdge<VD, ED, K>::sourceWalk() {
		assert(source->degree() == 2);
		Edge* w = source->incident[0];
		if (w == this) {
			return source->incident[1];
		}
		else {
			return w;
		}
	}

	template <class VD, class ED, typename K>
	StraightEdge<VD, ED, K>* StraightEdge<VD, ED, K>::targetWalk() {
		assert(target->degree() == 2);
		Edge* w = target->incident[0];
		if (w == this) {
			return target->incident[1];
		}
		else {
			return w;
		}
	}

	template <class VD, class ED, typename K>
	StraightVertex<VD, ED, K>* StraightEdge<VD, ED, K>::sourceWalkNeighbor() {
		Edge* e = sourceWalk();
		return e->other(source);
	}

	template <class VD, class ED, typename K>
	StraightVertex<VD, ED, K>* StraightEdge<VD, ED, K>::targetWalkNeighbor() {
		Edge* e = targetWalk();
		return e->other(target);
	}

	template <class VD, class ED, typename K>
	StraightBoundary<VD, ED, K>* StraightEdge<VD, ED, K>::getBoundary() {
		return boundary;
	}

	template <class VD, class ED, typename K>
	StraightEdge<VD, ED, K>* StraightEdge<VD, ED, K>::previous() {
		return source->incoming();
	}

	template <class VD, class ED, typename K>
	StraightEdge<VD, ED, K>* StraightEdge<VD, ED, K>::next() {
		return target->outgoing();
	}


	template<class InputGraph, class OutputGraph>
	OutputGraph* copy(InputGraph* input) {

		using InVertex = InputGraph::Vertex;
		using InEdge = InputGraph::Edge;
		using InBoundary = InputGraph::Boundary;
		using InKernel = InputGraph::Kernel;

		using OutVertex = OutputGraph::Vertex;
		using OutEdge = OutputGraph::Edge;
		using OutBoundary = OutputGraph::Boundary;
		using OutKernel = OutputGraph::Kernel;

		OutputGraph* output = new OutputGraph();

		if constexpr (std::is_same<InKernel, OutKernel>::value) {
			for (InVertex* v : input->vertices) {
				output->addVertex(v->point);
			}
		}
		else {
			CGAL::Cartesian_converter<InKernel, OutKernel> convert;
			for (InVertex* v : input->vertices) {
				output->addVertex(convert(v->point));
			}
		}

		for (InEdge* e : input->edges) {
			OutVertex* out_src = output->vertices[e->source->index];
			OutVertex* out_tar = output->vertices[e->target->index];
			output->addEdge(out_src, out_tar);
		}

		if (input->oriented) {
			// we oriented without constructing boundaries
			// to then copy the boundaries explicitly
			// -- this is done to ensure that indices match afterwards
			output->orientWithoutBoundaries();

			for (InBoundary* bd : input->boundaries) {
				OutBoundary* tarbd = new OutBoundary();
				tarbd->index = bd->index;
				tarbd->cyclic = bd->cyclic;
				tarbd->first = output->edges[bd->first->index];
				tarbd->last = output->edges[bd->last->index];
				output->boundaries.push_back(tarbd);

				OutEdge* e = tarbd->first;
				e->boundary = tarbd;
				while (e != tarbd->last) {
					e = e->next();
					e->boundary = tarbd;
				}
			}
		}

		if (input->sorted) {
			output->sortIncidentEdges();
		}

		return output;
	}

	template<class InputGraph, class OutputGraph>
	void copy(InputGraph* input, OutputGraph*& output) {
		output = copy<InputGraph, OutputGraph>(input);
	}

} // namespace cartocrow::simplification