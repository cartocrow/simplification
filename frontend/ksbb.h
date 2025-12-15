#pragma once

#include "library/edge_collapse.h"
#include "simplification_algorithm.h"

using namespace cartocrow;
using namespace cartocrow::renderer;
using namespace cartocrow::simplification;

using KSBBGraph = HistoricEdgeCollapseGraph<MyKernel>;
using KSBBPQT = PointQuadTree<KSBBGraph::Vertex, MyKernel>;
using KSBBSQT = SegmentQuadTree<KSBBGraph::Edge, MyKernel>;
using KSBB = KronenfeldEtAl<KSBBGraph>;

class KSBBSimplifier : public SimplificationAlgorithm {
private:
	KSBBGraph::BaseGraph* m_base = nullptr;
	KSBBGraph* m_graph = nullptr;
	KSBBPQT* m_pqt = nullptr;
	KSBBSQT* m_sqt = nullptr;
	KSBB* m_alg = nullptr;
public:
	void initialize(InputGraph* graph) override;
	void runToComplexity(const int k)  override;
	int getComplexity() override;
	std::shared_ptr<GeometryPainting> getPainting() override;
	void clear() override;
	bool hasResult() override;
	std::string getName() override {
		return "Kronenfeld et al.";
	}
};
