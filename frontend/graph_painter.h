#pragma once

#include <cartocrow/renderer/geometry_painting.h>

using namespace cartocrow;
using namespace cartocrow::renderer;
using namespace cartocrow::simplification;

template<class Graph>
class GraphPainting : public GeometryPainting {
public:
	GraphPainting(Graph& graph, const Color color, const double linewidth)
		: m_graph(graph), m_color(color), m_linewidth(linewidth) {
	}

protected:
	void paint(GeometryRenderer& renderer) const override {
		renderer.setMode(GeometryRenderer::stroke);

		renderer.setStroke(m_color, m_linewidth);

		for (typename Graph::Edge* e : m_graph.getEdges()) {
			renderer.draw(e->getSegment());
		}

		for (typename Graph::Vertex* v : m_graph.getVertices()) {
			if (v->degree() != 2) {
				renderer.draw(v->getPoint());
			}
		}
	}

private:
	Graph& m_graph;
	const Color m_color;
	const double m_linewidth;
};
