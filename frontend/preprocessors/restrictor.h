#pragma once

#include "frontend/preprocessor.h"

class RegularRestriction : public Preprocessor {
private:
	int m_count;
	int m_angle;
public:
	RegularRestriction(PersistentSettings& settings);

	QWidget* constructWidget() override;
	void applyTo(std::shared_ptr<InputGraph> graph) override;
};

class IrregularRestriction : public Preprocessor {
private:
	std::string m_string;

	std::vector<Number<Inexact>> extractAngles();
public:
	IrregularRestriction(PersistentSettings& settings);

	QWidget* constructWidget() override;
	void applyTo(std::shared_ptr<InputGraph> graph) override;
};