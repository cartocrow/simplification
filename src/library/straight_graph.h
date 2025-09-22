#pragma once

#include "cartocrow/core/core.h"

namespace cartocrow {

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
			} else {
				edges[e->index] = edges[edges.size() - 1];
				edges[e->index]->index = e->index;
				edges.pop_back();
			}
			delete e;
		}

		if (vtx->index == vertices.size() - 1) {
			vertices.pop_back();
		} else {
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
};

template <class VD, class ED, typename K> class StraightVertex {

	template <class VD, class ED, typename K> friend class StraightGraph;
	template <class VD, class ED, typename K> friend class StraightEdge;

  public:
	using Kernel = K;
	using Vertex = StraightVertex<VD, ED, K>;
	using Edge = StraightEdge<VD, ED, K>;

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
		} else if (target == e->target || target == e->source) {
			return target;
		} else {
			return nullptr;
		}
	}

	Edge* sourceWalk() {
		assert(source->degree() == 2);
		Edge* w = source->incident[0];
		if (w == this) {
			return source->incident[1];
		} else {
			return w;
		}
	}

	Edge* targetWalk() {
		assert(target->degree() == 2);
		Edge* w = target->incident[0];
		if (w == this) {
			return target->incident[1];
		} else {
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