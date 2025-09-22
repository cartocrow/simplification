#pragma once

#include "cartocrow/core/core.h"

namespace cartocrow::simplification {

template <class G> class ObliviousGraph {
  public:
	using Graph = G;
	using Vertex = G::Vertex;
	using Edge = G::Edge;
	using Kernel = G::Kernel;

  private:
	Graph& graph;

  public:
	ObliviousGraph(Graph& graph) : graph(graph) {}
	~ObliviousGraph() {}

	Graph& getGraph() {
		return graph;
	}

	Edge* erase(Vertex* v) {
		assert(v->degree() == 2);

		Vertex* u;
		Vertex* w;
		if (v->edge(0)->getTarget() == v) {
			u = v->neighbor(0);
			w = v->neighbor(1);
		} else {
			u = v->neighbor(1);
			w = v->neighbor(0);
		}

		graph.removeVertex(v);

		return graph.addEdge(u, w);
	}

	Vertex* split(Edge* e, Point<Kernel> p) {

		Vertex* s = e->getSource();
		Vertex* t = e->getTarget();

		graph.removeEdge(e);
		Vertex* v = graph.addVertex(p);
		graph.addEdge(s, v);
		graph.addEdge(v, t);

		return v;
	}

	void shiftVertex(Vertex* v, Point<Kernel> p) {
		v->setPoint(p);
	}
};

} // namespace cartocrow::simplification