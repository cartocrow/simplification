#pragma once

#include "library/vertex_removal.h"
#include "simplification_algorithm.h"

using namespace cartocrow;
using namespace cartocrow::renderer;
using namespace cartocrow::simplification;

using VWGraph = HistoricVertexRemovalGraph<MyKernel>;
using VWPQT = PointQuadTree<VWGraph::Vertex, MyKernel>;
using VW = VisvalingamWhyatt<VWGraph>;

class VWSimplifier : public SimplificationAlgorithm {
private:
	VWGraph::BaseGraph* m_base = nullptr;
	VWGraph* m_graph = nullptr;
	VWPQT* m_pqt = nullptr;
	VW* m_alg = nullptr;
public:
	void initialize(InputGraph* graph, const int depth) override;
	void runToComplexity(const int k)  override;
	int getComplexity() override;
	std::shared_ptr<GeometryPainting> getPainting(const VertexMode vmode) override;
	void clear() override;
	bool hasResult() override;
	std::string getName() override {
		return "Visvalingam-Whyatt";
	}
};
