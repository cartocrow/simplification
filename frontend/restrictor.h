#pragma once

#include "simplification_algorithm.h"

void restrict(InputGraph* graph, std::initializer_list<Number<Inexact>> angles);

void restrict(InputGraph* graph, int count, Number<Inexact> initial_angle);