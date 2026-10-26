#pragma once

#include <memory>
#include <QWidget>
#include <cartocrow/core/core.h>
#include <cartocrow/utils/persistent_settings.h>
#include <cartocrow/data_structures/straight_graph_2.h>

using namespace cartocrow;
using namespace cartocrow::utils;

using OutputGraph = Straight_graph_2<std::monostate, std::monostate, Inexact, DecomposedGraph<false, std::monostate>>;

class Postprocessor {
protected:
	PersistentSettings& m_settings;
	const std::string m_name;
public:
	Postprocessor(const std::string& name, PersistentSettings& settings) : m_name(name), m_settings(settings) {}

	std::string name() const { return m_name; }
	virtual QWidget* constructWidget(std::function<void()> autoapply) = 0;
	virtual void applyTo(std::shared_ptr<OutputGraph> graph) = 0;
};