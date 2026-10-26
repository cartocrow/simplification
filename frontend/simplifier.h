#pragma once

#include <QWidget>
#include <memory>
#include <cartocrow/renderer/graph_painting.h>
#include <cartocrow/data_structures/straight_graph_2.h>
#include <cartocrow/utils/persistent_settings.h>

#include "postprocessor.h"

using namespace cartocrow;
using namespace cartocrow::renderer;
using namespace cartocrow::utils;

using InputGraph = Straight_graph_2<std::monostate, std::monostate, Exact, CustomGraphTraits<false, true, true, true, std::monostate>>;
extern template GraphPainting<InputGraph>;

class Simplifier {
protected:
	PersistentSettings& m_settings;
	const std::string m_name;
	std::shared_ptr<OutputGraph> m_post;
	std::string m_post_name;
	Color m_post_color;
public:
	Simplifier(const std::string& name, PersistentSettings& settings, Color post_color);
	std::string name() const;

	virtual QWidget* constructWidget() = 0;
	virtual void initialize(std::shared_ptr<InputGraph> graph, const int depth) = 0;
	virtual void runToComplexity(const int k, std::optional<std::function<void(int)>> progress = std::nullopt,
		std::optional<std::function<bool()>> cancelled = std::nullopt) = 0;
	virtual int getComplexity() = 0;
	virtual int getMaximumComplexity() = 0;
	virtual std::shared_ptr<GeometryPainting> getPainting(const VertexSelection vmode) = 0;
	virtual void clear() = 0;
	virtual bool hasResult() = 0;
	virtual std::shared_ptr<InputGraph> simplifiedToInputGraph() = 0;
	virtual std::shared_ptr<OutputGraph> simplifiedToOutputGraph() = 0;

	std::shared_ptr<InputGraph> resultToInputGraph();
	std::shared_ptr<OutputGraph> resultToOutputGraph();

	std::string postprocessedName();
	void applyPostprocessor(Postprocessor* post);
	bool hasPostprocessed();
	std::shared_ptr<OutputGraph> getPostprocessed();
	void clearPostprocessed();
	std::shared_ptr<GraphPainting<OutputGraph>> getPostprocessedPainting();
};