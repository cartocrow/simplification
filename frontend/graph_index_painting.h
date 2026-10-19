#pragma once

#include <memory>
#include <cartocrow/renderer/geometry_painting.h>

using namespace cartocrow;
using namespace cartocrow::renderer;

template<class Graph>
class GraphIndexPainting : public GeometryPainting {
public:
	GraphIndexPainting(std::shared_ptr<Graph> graph, const bool vertices, const bool edges)
		: m_graph(std::move(graph)), m_vertices(vertices), m_edges(edges) {
	}

protected:
	void paint(GeometryRenderer& renderer) const override {
		renderer.setMode(GeometryRenderer::stroke);
		renderer.setStroke({ 0, 0, 0 }, 1);
		renderer.setHorizontalTextAlignment(cartocrow::renderer::GeometryRenderer::HorizontalTextAlignment::AlignHCenter);
		renderer.setVerticalTextAlignment(cartocrow::renderer::GeometryRenderer::VerticalTextAlignment::AlignVCenter);

		if (m_edges)
			for (typename Graph::Edge_const_handle e : m_graph->edges()) {
				renderer.drawText(CGAL::midpoint(e->source()->point(), e->target()->point()), std::to_string(e->graph_index()));
			}

		if (m_vertices)
			for (typename Graph::Vertex_const_handle v : m_graph->vertices()) {
				renderer.drawText(v->point(), std::to_string(v->graph_index()));
			}
	}

private:
	std::shared_ptr<Graph> m_graph;
	const bool m_vertices;
	const bool m_edges;
};