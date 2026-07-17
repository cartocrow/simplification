#pragma once

#include <cartocrow/core/core.h>
#include <cartocrow/data_structures/straight_graph_2.h>

using namespace cartocrow;

// extern template?
using SmoothGraph = Straight_graph_2<std::monostate, std::monostate, Inexact, DecomposedGraph<false, std::monostate>>;;

void smooth(SmoothGraph* graph, const Number<Inexact> radius, const int edges_on_semicircle, std::optional<std::function<void(std::string, int, int)>> progress);

template<class Graph>
SmoothGraph* smoothGraph(Graph* graph, const Number<Inexact> radiusfrac, const int edges_on_semicircle, std::optional<std::function<void(std::string,int,int)>> progress) {
	SmoothGraph* result = new SmoothGraph();
	graph_2_copy(*graph, *result);
	smooth(result, radiusfrac, edges_on_semicircle, progress);
	return result;
}