#pragma once

#include "library/straight_graph.h"

using namespace cartocrow;
using namespace cartocrow::simplification;

extern template StraightGraph<std::monostate, std::monostate, Inexact>;

using SmoothGraph = StraightGraph<std::monostate, std::monostate, Inexact>;

void smooth(SmoothGraph* graph, const Number<Inexact> radius, const int edges_on_semicircle, std::optional<std::function<void(std::string, int, int)>> progress);

template<class Graph>
SmoothGraph* smoothGraph(Graph* graph, const Number<Inexact> radiusfrac, const int edges_on_semicircle, std::optional<std::function<void(std::string,int,int)>> progress) {
	SmoothGraph* result;
	copy(graph, result);
	smooth(result, radiusfrac, edges_on_semicircle, progress);
	return result;
}