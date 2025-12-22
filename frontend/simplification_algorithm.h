#pragma once

#include <cartocrow/renderer/geometry_painting.h>
#include "library/straight_graph.h"
#include "graph_painter.h"

using namespace cartocrow;
using namespace cartocrow::renderer;
using namespace cartocrow::simplification;

using MyKernel = Exact;
extern template StraightGraph<VoidData, VoidData, MyKernel>;
using InputGraph = StraightGraph<VoidData, VoidData, MyKernel>;
extern template GraphPainting<InputGraph>;

class SimplificationAlgorithm {
public:
	virtual void initialize(InputGraph* graph, const int depth) = 0;
	virtual void runToComplexity(const int k) = 0;
	virtual int getComplexity() = 0;
	virtual std::shared_ptr<GeometryPainting> getPainting(const VertexMode vmode) = 0;
	virtual void clear() = 0;
	virtual bool hasResult() = 0;

	virtual void smooth(Number<Inexact> radius, int edges_on_semicircle) = 0;
	virtual bool hasSmoothResult() = 0;
	virtual std::shared_ptr<GeometryPainting> getSmoothPainting() = 0;

	virtual std::string getName() = 0;
};
