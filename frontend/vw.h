#pragma once

#include "simplification_algorithm.h"

using namespace cartocrow;
using namespace cartocrow::renderer;

class VWSimplifier : public SimplificationAlgorithm {
private:
	VWSimplifier() {};
public:
	static VWSimplifier& getInstance();

	void initialize(InputGraph* graph, const int depth) override;
	void runToComplexity(const int k, std::optional<std::function<void(int)>> progress = std::nullopt,
		std::optional<std::function<bool()>> cancelled = std::nullopt)  override;
	int getComplexity() override;
	std::shared_ptr<GeometryPainting> getPainting(const VertexMode vmode) override;
	void clear() override;
	bool hasResult() override;

	void smooth(Number<Inexact> radius, int edges_on_semicircle, std::optional<std::function<void(std::string, int, int)>> progress = std::nullopt) override;
	bool hasSmoothResult() override;
	std::shared_ptr<GeometryPainting> getSmoothPainting() override;
	void clearSmoothResult() override;

	std::string getName() override {
		return "Visvalingam-Whyatt";
	}

	InputGraph* resultToGraph() override;
};
