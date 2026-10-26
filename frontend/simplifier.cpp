#include "simplifier.h"

Simplifier::Simplifier(const std::string& name, PersistentSettings& settings, Color post_color) : m_name(name), m_settings(settings), m_post_color(post_color) {}

std::string Simplifier::name() const { return m_name; }

std::shared_ptr<InputGraph> Simplifier::resultToInputGraph() {
	if (hasPostprocessed()) {
		std::shared_ptr<InputGraph> res = std::make_shared<InputGraph>();
		graph_2_copy(*m_post, *res);
		return res;
	}
	else {
		return simplifiedToInputGraph();
	}
}
std::shared_ptr<OutputGraph> Simplifier::resultToOutputGraph() {
	if (hasPostprocessed()) {
		std::shared_ptr<OutputGraph> res = std::make_shared<OutputGraph>();
		graph_2_copy(*m_post, *res);
		return res;
	}
	else {
		return simplifiedToOutputGraph();
	}
}

std::string Simplifier::postprocessedName() {
	return m_post_name;
}

void Simplifier::applyPostprocessor(Postprocessor* post) {
	clearPostprocessed();
	m_post_name = post->name();
	m_post = resultToOutputGraph();
	post->applyTo(m_post);
}
bool Simplifier::hasPostprocessed() {
	return m_post != nullptr;
}
std::shared_ptr<OutputGraph> Simplifier::getPostprocessed() {
	return m_post;
}
void Simplifier::clearPostprocessed() {
	m_post = nullptr;
}
std::shared_ptr<GraphPainting<OutputGraph>> Simplifier::getPostprocessedPainting() {
	return std::make_shared<GraphPainting<OutputGraph>>(m_post, VertexStyle{ m_post_color }, EdgeStyle{ m_post_color, 2 }, VertexSelection::DEG0_ONLY);
};