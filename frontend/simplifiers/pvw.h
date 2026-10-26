#pragma once

#include "frontend/simplification_algorithm.h"

using namespace cartocrow;
using namespace cartocrow::renderer;

class PVWSimplifier : public SimplificationAlgorithm {
private:
	PVWSimplifier() {};
public:
	static PVWSimplifier& getInstance();

	void initialize(std::shared_ptr<InputGraph> graph, const int depth) override;
	void runToComplexity(const int k, std::optional<std::function<void(int)>> progress = std::nullopt,
		std::optional<std::function<bool()>> cancelled = std::nullopt)  override;
	int getComplexity() override;
	int getMaximumComplexity() override;
	std::shared_ptr<GeometryPainting> getPainting(const VertexSelection vmode) override;
	void clear() override;
	bool hasResult() override;

	void smooth(Number<Inexact> radius, int edges_on_semicircle, std::optional<std::function<void(std::string, int, int)>> progress = std::nullopt) override;
	bool hasSmoothResult() override;
	std::shared_ptr<GeometryPainting> getSmoothPainting() override;
	void clearSmoothResult() override;

	std::string getName() override {
		return "Preserving Visvalingam-Whyatt";
	}

	std::shared_ptr<InputGraph> resultToGraph() override;
};

class PVWInexactSimplifier : public SimplificationAlgorithm {
private:
	PVWInexactSimplifier() {};
public:
	static PVWInexactSimplifier& getInstance();

	void initialize(std::shared_ptr<InputGraph> graph, const int depth) override;
	void runToComplexity(const int k, std::optional<std::function<void(int)>> progress = std::nullopt,
		std::optional<std::function<bool()>> cancelled = std::nullopt)  override;
	int getComplexity() override;
	int getMaximumComplexity() override;
	std::shared_ptr<GeometryPainting> getPainting(const VertexSelection vmode) override;
	void clear() override;
	bool hasResult() override;

	void smooth(Number<Inexact> radius, int edges_on_semicircle, std::optional<std::function<void(std::string, int, int)>> progress = std::nullopt) override;
	bool hasSmoothResult() override;
	std::shared_ptr<GeometryPainting> getSmoothPainting() override;
	void clearSmoothResult() override;

	std::string getName() override {
		return "Preserving Visvalingam-Whyatt (inexact)";
	}

	std::shared_ptr<InputGraph> resultToGraph() override;
};
