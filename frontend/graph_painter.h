#pragma once

#include <cartocrow/renderer/geometry_painting.h>

using namespace cartocrow;
using namespace cartocrow::renderer;
using namespace cartocrow::simplification;

enum VertexMode {
	DEG0_ONLY = 0,
	NO_DEG2 = 1,
	ALL = 2
};

template<class Graph>
class GraphPainting : public GeometryPainting {
public:
	GraphPainting(Graph& graph, const Color color, const double linewidth, const VertexMode vmode)
		: m_graph(graph), m_color(color), m_linewidth(linewidth), m_vmode(vmode) {
	}

protected:
	void paint(GeometryRenderer& renderer) const override {
		renderer.setMode(GeometryRenderer::stroke);

		renderer.setStroke(m_color, m_linewidth);

		for (typename Graph::Edge* e : m_graph.getEdges()) {
			renderer.draw(e->getSegment());
		}

		for (typename Graph::Vertex* v : m_graph.getVertices()) {
			bool render = false;
			switch (m_vmode) {
			case DEG0_ONLY:
				render = v->degree() == 0;
				break;
			case NO_DEG2:
				render = v->degree() != 2;
				break;
			case ALL:
				render = true;
				break;
			}

			if (render) {
				renderer.draw(v->getPoint());
			}
		}
	}

private:
	Graph& m_graph;
	const Color m_color;
	const double m_linewidth;
	const VertexMode m_vmode;
};
