#pragma once

#include <cartocrow/core/core.h>

namespace cartocrow::simplification {

	template<class Graph> requires Graph::Graph_traits::sorted && Graph::Graph_traits::oriented
	void restrict_orientations(Graph& graph, std::vector<Direction<Inexact>> dirs, Number<Inexact> lambda = 1, Number<Inexact> eps = 0.1, std::optional<std::function<void(std::string, int, int)>> progress = std::nullopt);

	template<class Graph> requires Graph::Graph_traits::sorted && Graph::Graph_traits::oriented
	void restrict_orientations(Graph& graph, int count, Number<Inexact> initial_angle = 0, Number<Inexact> lambda = 1, Number<Inexact> eps = 0.1, std::optional<std::function<void(std::string, int, int)>> progress = std::nullopt);

	template<class Graph> requires Graph::Graph_traits::sorted&& Graph::Graph_traits::oriented
		void restrict_orientations(Graph& graph, std::vector<Number<Inexact>> angles, Number<Inexact> lambda = 1, Number<Inexact> eps = 0.1, std::optional<std::function<void(std::string, int, int)>> progress = std::nullopt);

	template<class Graph> requires Graph::Graph_traits::sorted && Graph::Graph_traits::oriented
	void restrict_orientations(Graph& graph, std::initializer_list<Number<Inexact>> angles, Number<Inexact> lambda = 1, Number<Inexact> eps = 0.1, std::optional<std::function<void(std::string, int, int)>> progress = std::nullopt);

}

#include "orientation_restriction.hpp"