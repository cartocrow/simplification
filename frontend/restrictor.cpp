#include "restrictor.h"

#include "library/angle_restriction.h"

using namespace cartocrow::simplification;

void restrict(InputGraph* graph, std::initializer_list<Number<Inexact>> angles){
	restrict_directions(*graph, angles);
}