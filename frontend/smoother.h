#pragma once

#include "library/straight_graph.h"

using namespace cartocrow;
using namespace cartocrow::simplification;

extern template StraightGraph<VoidData, VoidData, Inexact>;

using SmoothGraph = StraightGraph<VoidData, VoidData, Inexact>;

void smooth(SmoothGraph* graph, const Number<Inexact> radius, const int edges_on_semicircle, std::optional<std::function<void(std::string, int, int)>> progress);

template<class Graph>
SmoothGraph* smoothGraph(Graph* graph, const Number<Inexact> radiusfrac, const int edges_on_semicircle, std::optional<std::function<void(std::string,int,int)>> progress) {
	SmoothGraph* result = copyApproximateGraph<Graph, SmoothGraph>(graph);
	smooth(result, radiusfrac, edges_on_semicircle, progress);
	return result;
}