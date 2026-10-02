#pragma once

#include <memory>
#include <cartocrow/renderer/graph_painting.h>
#include <cartocrow/data_structures/straight_graph_2.h>

using namespace cartocrow;
using namespace cartocrow::renderer;

using InputGraph = Straight_graph_2<std::monostate, std::monostate, Exact, CustomGraphTraits<false, true, true, true, std::monostate>>;
extern template GraphPainting<InputGraph>;

class SimplificationAlgorithm {
public:
	virtual void initialize(std::shared_ptr<InputGraph> graph, const int depth) = 0;
	virtual void runToComplexity(const int k, std::optional<std::function<void(int)>> progress = std::nullopt,
		std::optional<std::function<bool()>> cancelled = std::nullopt) = 0;
	virtual int getComplexity() = 0;
	virtual int getMaximumComplexity() = 0;
	virtual std::shared_ptr<GeometryPainting> getPainting(const VertexSelection vmode) = 0;
	virtual void clear() = 0;
	virtual bool hasResult() = 0;

	virtual void smooth(Number<Inexact> radius, int edges_on_semicircle, std::optional<std::function<void(std::string, int, int)>> progress = std::nullopt) = 0;
	virtual bool hasSmoothResult() = 0;
	virtual std::shared_ptr<GeometryPainting> getSmoothPainting() = 0;
	virtual void clearSmoothResult() = 0;

	virtual std::string getName() = 0;

	virtual std::shared_ptr<InputGraph> resultToGraph() = 0;

	virtual std::vector<std::pair<std::shared_ptr<GeometryPainting>, std::string>> getDebugPaintings() {
		return std::vector<std::pair<std::shared_ptr<GeometryPainting>, std::string>>();
	}
};
