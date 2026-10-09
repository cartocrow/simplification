#pragma once

#include "simplification_algorithm.h"

void restrict(std::shared_ptr<InputGraph> graph, std::vector<Number<Inexact>> angles, std::optional<std::function<void(std::string, int, int)>> progress = std::nullopt);

void restrict(std::shared_ptr<InputGraph> graph, std::initializer_list<Number<Inexact>> angles, std::optional<std::function<void(std::string, int, int)>> progress = std::nullopt);

void restrict(std::shared_ptr<InputGraph> graph, int count, Number<Inexact> initial_angle, std::optional<std::function<void(std::string, int, int)>> progress = std::nullopt);