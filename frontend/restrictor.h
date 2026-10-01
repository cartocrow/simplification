#pragma once

#include "simplification_algorithm.h"

void restrict(std::shared_ptr<InputGraph> graph, std::initializer_list<Number<Inexact>> angles);

void restrict(std::shared_ptr<InputGraph> graph, int count, Number<Inexact> initial_angle);