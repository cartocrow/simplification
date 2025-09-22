#pragma once

#include <cartocrow/core/core.h>

namespace cartocrow {

	// for not storing anything in a vertex or edge
	struct VoidData {

	};

	template <class VD, class ED, typename K> class StraightVertex;
	template <class VD, class ED, typename K> class StraightEdge;

	template <class VD, class ED, typename K> class StraightGraph {

		template <class VD, class ED, typename K> friend class StraightVertex;
		template <class VD, class ED, typename K> friend class StraightEdge;

	public:
		using Kernel = K;
		using Vertex = StraightVertex<VD, ED, K>;
		using Edge = StraightEdge<VD, ED, K>;

	private:
		std::vector<Vertex*> vertices;
		std::vector<Edge*> edges;

	public:
		StraightGraph() {}

		~StraightGraph() {
			for (Edge* e : edges) {
				delete e;
			}
			for (Vertex* v : vertices) {
				delete v;
			}
		}

		int getVertexCount() {
			return vertices.size();
		}

		int getEdgeCount() {
			return edges.size();
		}

		std::vector<Vertex*>& getVertices() {
			return vertices;
		}

		std::vector<Edge*>& getEdges() {
			return edges;
		}

		Vertex* addVertex(Point<K> pt) {
			Vertex* v = new Vertex();
			v->index = vertices.size();
			v->point = Point<K>(pt.x(), pt.y());
			vertices.push_back(v);
			return v;
		}

		void removeVertex(Vertex* vtx) {
			for (Edge* e : vtx->incident) {
				Vertex* other = e->other(vtx);
				other->incident.erase(std::find(other->incident.begin(), other->incident.end(), e));

				if (e->index == edges.size() - 1) {
					edges.pop_back();
				}
				else {
					edges[e->index] = edges[edges.size() - 1];
					edges[e->index]->index = e->index;
					edges.pop_back();
				}
				delete e;
			}

			if (vtx->index == vertices.size() - 1) {
				vertices.pop_back();
			}
			else {
				vertices[vtx->index] = vertices[vertices.size() - 1];
				vertices[vtx->index]->index = vtx->index;
				vertices.pop_back();
			}
			delete vtx;
		}

		Edge* addEdge(Vertex* source, Vertex* target) {
			Edge* e = new Edge();
			e->index = edges.size();
			e->source = source;
			e->target = target;
			edges.push_back(e);
			source->incident.push_back(e);
			target->incident.push_back(e);
			return e;
		}

		void removeEdge(Edge* edge) {
			edge->source->incident.erase(std::find(edge->source->incident.begin(), edge->source->incident.end(), edge));
			edge->target->incident.erase(std::find(edge->target->incident.begin(), edge->target->incident.end(), edge));

			if (edge->index == edges.size() - 1) {
				edges.pop_back();
			}
			else {
				edges[edge->index] = edges[edges.size() - 1];
				edges[edge->index]->index = edge->index;
				edges.pop_back();
			}

			delete edge;
		}

		void orientChains() {
			for (Vertex* v : vertices) {
				if (v->degree() != 2) continue;
				Edge* e0 = v->edge(0);
				Edge* e1 = v->edge(1);
				if (e0->target == v && e1->source == v) continue;
				if (e1->target == v && e0->source == v) continue;

				e1.reverse();

				Edge* fwd;
				Edge* bwd;
				if (e1->target == v) {
					bwd = e1;
					fwd = e0;
				}
				else {
					bwd = e0;
					fwd = e1;
				}

				Vertex* fv = fwd->target;
				while (fv->degree() != 2) {
					fwd = fwd->walkTarget();
					if (fwd->source != fv) {
						fwd->reverse();
					}
					fv = fwd->target();
				}


				Vertex* bv = bwd->source;
				while (bv->degree() != 2) {
					bwd = bwd->walkSource();
					if (bwd->target != bv) {
						bwd->reverse();
					}
					bv = bwd->target();
				}
			}
		}

		Vertex* splitEdge(Edge* edge, Point<K> pt) {
			Vertex* v = addVertex(pt);

			Vertex* w = edge->target;
			w->incident.erase(std::find(w->incident.begin(), w->incident.end(), edge));

			edge->target = v;
			v->incident.push_back(edge);

			addEdge(v, w);

			return v;
		}

		Edge* mergeVertex(Vertex* v) {
			assert(v->degree() == 2);

			Vertex* u;
			Vertex* w;
			Edge* e = v->edge(0);
			// ensure we keep the same orientations:
			// if the edges were (u,v) and (v,w), create (u,w)
			if (e->getTarget() == v) {
				// at least its (u,v)
				u = v->neighbor(0);
				w = v->neighbor(1);

				removeEdge(v->edge(1)); // delete the other edge
			}
			else {
				// use the other edge, it could be (u,v) or (v,u)
				u = v->neighbor(1);
				w = v->neighbor(0);
				e = v->edge(1);

				removeEdge(v->edge(0)); // delete the other edge
			}

			v->incident.clear();
			removeVertex(v); // get rid of the vertex (but not the edge, ew're reusing it)

			// set both to be sure (due to second case above)
			e->source = u;
			e->target = w;
			w->incident.push_back(e);

			return e;

		}

		void shiftVertex(Vertex* v, Point<K> pt) {
			v->setPoint(pt);
		}
	};

	template <class VD, class ED, typename K> class StraightVertex {

		template <class VD, class ED, typename K> friend class StraightGraph;
		template <class VD, class ED, typename K> friend class StraightEdge;

	public:
		using Kernel = K;
		using Vertex = StraightVertex<VD, ED, K>;
		using Edge = StraightEdge<VD, ED, K>;
		using Data = VD;

	private:
		int index;
		Point<K> point;
		std::vector<Edge*> incident;
		VD d;

	public:
		int graphIndex() {
			return index;
		}

		int degree() {
			return incident.size();
		}

		Edge* edge(int i) {
			return incident[i];
		}

		Vertex* neighbor(int i) {
			return incident[i]->other(this);
		}

		bool isNeighborOf(Vertex* v) {
			for (Edge* e : incident) {
				if (e->other(this) == v) {
					return true;
				}
			}
			return false;
		}

		VD& data() {
			return d;
		}

		void setPoint(Point<K>& pt) {
			point = Point<K>(pt.x(), pt.y());
		}

		Point<K>& getPoint() {
			return point;
		}
	};

	template <class VD, class ED, typename K> class StraightEdge {

		template <class VD, class ED, typename K> friend class StraightGraph;
		template <class VD, class ED, typename K> friend class StraightVertex;

	public:
		using Kernel = K;
		using Vertex = StraightVertex<VD, ED, K>;
		using Edge = StraightEdge<VD, ED, K>;
		using Data = ED;

	private:
		int index;
		Vertex* source;
		Vertex* target;
		ED d;

	public:
		int graphIndex() {
			return index;
		}

		Vertex* getSource() {
			return source;
		}

		Vertex* getTarget() {
			return target;
		}

		void reverse() {
			Vertex* t = source;
			source = target;
			target = t;
		}

		ED& data() {
			return d;
		}

		Segment<K> getSegment() {
			return Segment<K>(source->point, target->point);
		}

		Vertex* other(Vertex* v) {
			return v == source ? target : source;
		}

		Vertex* commonVertex(Edge* e) {
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

		Edge* sourceWalk() {
			assert(source->degree() == 2);
			Edge* w = source->incident[0];
			if (w == this) {
				return source->incident[1];
			}
			else {
				return w;
			}
		}

		Edge* targetWalk() {
			assert(target->degree() == 2);
			Edge* w = target->incident[0];
			if (w == this) {
				return target->incident[1];
			}
			else {
				return w;
			}
		}

		Vertex* sourceWalkNeighbor() {
			Edge* e = sourceWalk();
			return e->other(source);
		}

		Vertex* targetWalkNeighbor() {
			Edge* e = targetWalk();
			return e->other(target);
		}
	};

} // namespace cartocrow