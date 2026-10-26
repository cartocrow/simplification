#pragma once

#include <QWidget>
#include <cartocrow/utils/persistent_settings.h>
#include "simplification_algorithm.h"

using namespace cartocrow::utils;

class Preprocessor {
protected:
	PersistentSettings& m_settings;
	const std::string m_name;
public:	
	Preprocessor(const std::string& name, PersistentSettings& settings) : m_name(name), m_settings(settings) {}

	std::string name() const { return m_name; }
	virtual QWidget* constructWidget() = 0;
	virtual void applyTo(std::shared_ptr<InputGraph> graph) = 0;
};