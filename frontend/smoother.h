#pragma once

#include <cartocrow/core/core.h>
#include <cartocrow/data_structures/straight_graph_2.h>

using namespace cartocrow;

// extern template?
using SmoothGraph = Straight_graph_2<std::monostate, std::monostate, Inexact, DecomposedGraph<false, std::monostate>>;;

void smooth(SmoothGraph* graph, const Number<Inexact> radius, const int edges_on_semicircle, std::optional<std::function<void(std::string, int, int)>> progress);

template<class Graph>
std::shared_ptr<SmoothGraph> smoothGraph(std::shared_ptr<Graph> graph, const Number<Inexact> radiusfrac, const int edges_on_semicircle, std::optional<std::function<void(std::string,int,int)>> progress) {
	std::shared_ptr<SmoothGraph> result = std::make_shared<SmoothGraph>();
	graph_2_copy(*graph, *result);
	smooth(result.get(), radiusfrac, edges_on_semicircle, progress);
	return result;
}