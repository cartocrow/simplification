#pragma once

#include <cartocrow/renderer/geometry_painting.h>
#include "library/straight_graph.h"

using namespace cartocrow;
using namespace cartocrow::renderer;
using namespace cartocrow::simplification;

using MyKernel = Inexact;
using InputGraph = StraightGraph<VoidData, VoidData, MyKernel>;

class SimplificationAlgorithm {
public:
	virtual void initialize(InputGraph* graph) = 0;
	virtual void runToComplexity(const int k) = 0;
	virtual int getComplexity() = 0;
	virtual std::shared_ptr<GeometryPainting> getPainting() = 0;
	virtual void clear() = 0;
	virtual bool hasResult() = 0;
	virtual std::string getName() = 0;
};