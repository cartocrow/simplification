#include "restrictor.h"

#include "library/orientation_restriction.h"

using namespace cartocrow::simplification;

void restrict(std::shared_ptr<InputGraph> graph, std::initializer_list<Number<Inexact>> angles, std::optional<std::function<void(std::string, int, int)>> progress){
	restrict_orientations(*graph, angles, 1, 0.1, progress);
}

void restrict(std::shared_ptr<InputGraph> graph, int count, Number<Inexact> initial_angle, std::optional<std::function<void(std::string, int, int)>> progress){
	restrict_orientations(*graph, count, initial_angle, 1, 0.1, progress);
}