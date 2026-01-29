#pragma once

#include <cartocrow/core/core.h>

namespace cartocrow::simplification {

	template<class Graph> 
	void restrict_orientations(Graph& graph, std::vector<Direction<Inexact>> dirs, Number<Inexact> lambda = 1, Number<Inexact> eps = 0.1);

	template<class Graph>
	void restrict_orientations(Graph& graph, int count, Number<Inexact> initial_angle = 0, Number<Inexact> lambda = 1, Number<Inexact> eps = 0.1);

	template<class Graph>
	void restrict_orientations(Graph& graph, std::initializer_list<Number<Inexact>> angles, Number<Inexact> lambda = 1, Number<Inexact> eps = 0.1);

}

#include "orientation_restriction.hpp"