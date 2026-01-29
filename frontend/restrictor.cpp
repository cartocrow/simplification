#include "restrictor.h"

#include "library/orientation_restriction.h"

using namespace cartocrow::simplification;

void restrict(InputGraph* graph, std::initializer_list<Number<Inexact>> angles){
	restrict_orientations(*graph, angles);
}

void restrict(InputGraph* graph, int count, Number<Inexact> initial_angle){
	restrict_orientations(*graph, count, initial_angle);
}