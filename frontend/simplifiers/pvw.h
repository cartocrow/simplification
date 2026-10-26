#pragma once

#include <memory>
#include "frontend/simplifier.h"

using namespace cartocrow;
using namespace cartocrow::renderer;

class PVWSimplifier : public Simplifier {
public:
	struct State;

	PVWSimplifier(PersistentSettings& settings);
	~PVWSimplifier();

	QWidget* constructWidget() override;

	void initialize(std::shared_ptr<InputGraph> graph, const int depth) override;
	void runToComplexity(const int k, std::optional<std::function<void(int)>> progress = std::nullopt,
		std::optional<std::function<bool()>> cancelled = std::nullopt) override;
	int getComplexity() override;
	int getMaximumComplexity() override;
	std::shared_ptr<GeometryPainting> getPainting(const VertexSelection vmode) override;
	void clear() override;
	bool hasResult() override;

	std::shared_ptr<InputGraph> simplifiedToInputGraph() override;
	std::shared_ptr<OutputGraph> simplifiedToOutputGraph() override;

private:
	Color m_color;
	bool m_exact; // NB: GUI setting != state, necessarily
	int m_preservation_weight;
	std::unique_ptr<State> m_state;
};