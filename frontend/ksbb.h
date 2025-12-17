#pragma once

#include "simplification_algorithm.h"

using namespace cartocrow;
using namespace cartocrow::renderer;

class KSBBSimplifier : public SimplificationAlgorithm {
private:
	KSBBSimplifier() {};
public:
	static KSBBSimplifier& getInstance();

	void initialize(InputGraph* graph, const int depth) override;
	void runToComplexity(const int k)  override;
	int getComplexity() override;
	std::shared_ptr<GeometryPainting> getPainting(const VertexMode vmode) override;
	void clear() override;
	bool hasResult() override;

	void smooth(Number<MyKernel> radius) override;
	bool hasSmoothResult() override;
	std::shared_ptr<GeometryPainting> getSmoothPainting() override;

	std::string getName() override {
		return "Kronenfeld et al.";
	}
};
