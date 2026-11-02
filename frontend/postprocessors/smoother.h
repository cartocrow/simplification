#pragma once

#include <cartocrow/core/core.h>
#include <cartocrow/data_structures/straight_graph_2.h>

#include "frontend/postprocessor.h"

using namespace cartocrow;

class Smoother : public Postprocessor {
public:
	Smoother(PersistentSettings& settings);

	QWidget* constructWidget(std::function<void()> autoapply) override;
	void applyTo(std::shared_ptr<OutputGraph> graph) override;
private:
	int m_radiuspct;
	int m_samples;
};