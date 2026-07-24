#pragma once

//#include <ipepath.h>

#include <sstream>
#include <string>
#include <fstream>
#include <cartocrow/reader/ipe_reader.h>

#include "library/vertex_quad_tree.h"
#include "library/utils.h"

using namespace cartocrow;
using namespace cartocrow::simplification;

template<class Graph>
VertexQuadTree<Graph>* readIpeFile(Graph& graph, const std::filesystem::path& file, const int depth, const Number<typename Graph::Kernel> prec) {
	using Vertex = Graph::Vertex;
	using Kernel = Graph::Kernel;

	std::ifstream filestream(file);

	bool onpage = false;
	bool inpath = false;

	std::vector<std::vector<Point<Kernel>>> lines;
	std::vector<bool> closed;

	std::string line;
	std::vector<Point<Kernel>>* building = nullptr;
	while (std::getline(filestream, line))
	{
		if (!onpage) {
			onpage = line.starts_with("<page");
		}
		else if (line.starts_with("</page")) {
			onpage = false;
		}
		else if (!inpath) {
			inpath = line.starts_with("<path");
		}
		else if (line.starts_with("</path")) {
			if (building != nullptr) {
				closed.push_back(false);
				building = nullptr;
			}
			inpath = false;
		}
		else if (inpath) {
			if (line.ends_with("m")) {
				if (building != nullptr) {
					closed.push_back(false);
				}
				lines.emplace_back();
				building = &lines[lines.size() - 1];

				std::istringstream iss(line);
				double x, y;
				if (!(iss >> x >> y)) {
					std::cout << "Unexpected path command: " << line << std::endl;
				}
				else {
					building->push_back(Point<Kernel>(x, y));				
				}

			}
			else if (line.ends_with("l")) {
				std::istringstream iss(line);
				double x, y;
				if (!(iss >> x >> y)) {
					std::cout << "Unexpected path command: " << line << std::endl;
				}
				else {
					building->push_back(Point<Kernel>(x, y));
				}
			}
			else if (line.ends_with("h")) {
				if (building != nullptr) {
					closed.push_back(true);
					building = nullptr;
				}
			}
			else {
				std::cout << "Unexpected path command: " << line << std::endl;
			}
		}
	}

	if (lines.empty()) {
		return nullptr;
	}

	Rectangle<Kernel> box = utils::boxOf<Kernel>(lines[0]);
	for (int i = 1; i < lines.size(); ++i) {
		box = utils::boxOf(box, utils::boxOf<Kernel>(lines[i]));
	}

	// construct the graph
	VertexQuadTree<Graph>* pqt = new VertexQuadTree<Graph>(box, depth);

	for (int i = 0; i < lines.size(); i++) {

		Vertex* prev = nullptr;
		Vertex* first = nullptr;
		for (int k = 0; k < lines[i].size(); k++) {
			Point<Kernel>& point = lines[i][k];

			Vertex* next = pqt->findElement(point, prec);
			if (next == nullptr) {
				next = graph.add_vertex(point);
				pqt->insert(next);
			}
			if (first == nullptr) {
				first = next;
			}
			if (prev != nullptr && !next->is_neighbor_of(prev) && next != prev) {
				graph.add_edge(prev, next);
			}
			prev = next;
		}

		if (closed[i] && first != prev && !first->is_neighbor_of(prev)) {
			graph.add_edge(prev, first);
		}
	}
	return pqt;
}

//template<class Graph>
//VertexQuadTree<Graph>* readIpeFile(Graph& graph, const std::filesystem::path& file, const int depth, const Number<typename Graph::Kernel> prec) {
//	using Vertex = Graph::Vertex;
//	using Kernel = Graph::Kernel;
//	std::cout << "Starting IPE doc" << std::endl;
//	std::shared_ptr<ipe::Document> document = IpeReader::loadIpeFile(file);
//
//	if (document->countPages() == 0) {
//		std::cout << "Warning: No pages found in IPE file\n";
//		return nullptr;
//	}
//	else if (document->countPages() > 1) {
//		std::cout << "Warning: Multiple pages found in IPE file; reading only the first\n";
//	}
//
//	ipe::Page* page = document->page(0);
//
//	// compute a bounding box
//	std::cout << "Making bbox" << std::endl;
//	std::vector<Point<Kernel>> points;
//
//	for (int i = 0; i < page->count(); i++) {
//		auto object = page->object(i);
//		if (object->type() != ipe::Object::Type::EPath) continue;
//		auto path = object->asPath();
//		auto matrix = object->matrix();
//		auto shape = path->shape();
//		for (int j = 0; j < shape.countSubPaths(); j++) {
//			auto subpath = shape.subPath(j);
//			if (subpath->type() != ipe::SubPath::Type::ECurve) continue;
//			auto curve = subpath->asCurve();
//
//			for (int k = 0; k < curve->countSegments(); k++) {
//				auto segment = curve->segment(k);
//				auto pt = matrix * segment.cp(0);
//				Point<Kernel> point(pt.x, pt.y);
//				points.push_back(point);
//			}
//
//			auto pt = matrix * curve->segment(curve->countSegments() - 1).last();
//			Point<Kernel> point(pt.x, pt.y);
//			points.push_back(point);
//		}
//	}
//
//	Rectangle<Kernel> box = utils::boxOf<Kernel>(points);
//
//	// construct the graph
//	std::cout << "Making graph" << std::endl;
//	VertexQuadTree<Graph>* pqt = new VertexQuadTree<Graph>(box, depth);
//
//	for (int i = 0; i < page->count(); i++) {
//		auto object = page->object(i);
//		if (object->type() != ipe::Object::Type::EPath) continue;
//		auto path = object->asPath();
//		auto matrix = object->matrix();
//		auto shape = path->shape();
//		for (int j = 0; j < shape.countSubPaths(); j++) {
//			auto subpath = shape.subPath(j);
//			if (subpath->type() != ipe::SubPath::Type::ECurve) continue;
//			auto curve = subpath->asCurve();
//
//			Vertex* prev = nullptr;
//			for (int k = 0; k < curve->countSegmentsClosing(); k++) {
//				auto segment = curve->segment(k);
//				auto pt = matrix * segment.cp(0);
//
//				Point<Kernel> point(pt.x, pt.y);
//
//				Vertex* next = pqt->findElement(point, prec);
//				if (next == nullptr) {
//					next = graph.add_vertex(point);
//					pqt->insert(next);
//				}
//				if (prev != nullptr && !next->is_neighbor_of(prev) && next != prev) {
//					graph.add_edge(prev, next);
//				}
//				prev = next;
//			}
//
//			auto pt = matrix * curve->segment(curve->countSegmentsClosing() - 1).last();
//
//			Point<Kernel> point(pt.x, pt.y);
//
//			Vertex* next = pqt->findElement(point, prec);
//			if (next == nullptr) {
//				next = graph.add_vertex(point);
//				pqt->insert(next);
//			}
//			if (prev != nullptr && !next->is_neighbor_of(prev) && next != prev) {
//				graph.add_edge(prev, next);
//			}
//			prev = next;
//		}
//	}
//	std::cout << "Done" << std::endl;
//	return pqt;
//}

template<class Graph>
Graph* readIpeFile(const std::filesystem::path& file, const int depth) {
	Graph* graph = new Graph();
	auto pqt = readIpeFile(*graph, file, depth, 0.00001);
	if (pqt == nullptr) {
		delete graph;
		return nullptr;
	}
	delete pqt;
	return graph;
}