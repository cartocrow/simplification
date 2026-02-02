#pragma once

#include <ipepath.h>
#include <cartocrow/reader/ipe_reader.h>

#include "library/vertex_quad_tree.h"
#include "library/utils.h"

template<class Graph>
Graph* readIpeFile(const std::filesystem::path& file, const int depth) {
	using Vertex = Graph::Vertex;
	using Kernel = Graph::Kernel;
	std::shared_ptr<ipe::Document> document = IpeReader::loadIpeFile(file);

	if (document->countPages() == 0) {
		std::cout << "Warning: No pages found in IPE file\n";
		return nullptr;
	}
	else if (document->countPages() > 1) {
		std::cout << "Warning: Multiple pages found in IPE file; reading only the first\n";
	}

	ipe::Page* page = document->page(0);

	Graph* graph = new Graph();

	// compute a bounding box
	std::vector<Point<Kernel>> points;

	for (int i = 0; i < page->count(); i++) {
		auto object = page->object(i);
		if (object->type() != ipe::Object::Type::EPath) continue;
		auto path = object->asPath();
		auto matrix = object->matrix();
		auto shape = path->shape();
		for (int j = 0; j < shape.countSubPaths(); j++) {
			auto subpath = shape.subPath(j);
			if (subpath->type() != ipe::SubPath::Type::ECurve) continue;
			auto curve = subpath->asCurve();

			for (int k = 0; k < curve->countSegments(); k++) {
				auto segment = curve->segment(k);
				auto pt = matrix * segment.cp(0);
				Point<Kernel> point(pt.x, pt.y);
				points.push_back(point);
			}

			auto pt = matrix * curve->segment(curve->countSegments() - 1).last();
			Point<Kernel> point(pt.x, pt.y);
			points.push_back(point);
		}
	}

	Rectangle<Kernel> box = utils::boxOf<Kernel>(points);

	// construct the graph
	VertexQuadTree<Graph> pqt(box, depth);

	for (int i = 0; i < page->count(); i++) {
		auto object = page->object(i);
		if (object->type() != ipe::Object::Type::EPath) continue;
		auto path = object->asPath();
		auto matrix = object->matrix();
		auto shape = path->shape();
		for (int j = 0; j < shape.countSubPaths(); j++) {
			auto subpath = shape.subPath(j);
			if (subpath->type() != ipe::SubPath::Type::ECurve) continue;
			auto curve = subpath->asCurve();

			Vertex* prev = nullptr;
			for (int k = 0; k < curve->countSegmentsClosing(); k++) {
				auto segment = curve->segment(k);
				auto pt = matrix * segment.cp(0);

				Point<Kernel> point(pt.x, pt.y);

				Vertex* next = pqt.findElement(point, 0.00001);
				if (next == nullptr) {
					next = graph->addVertex(point);
					pqt.insert(*next);
				}
				if (prev != nullptr && !next->isNeighborOf(prev) && next != prev) {
					graph->addEdge(prev, next);
				}
				prev = next;
			}

			auto pt = matrix * curve->segment(curve->countSegmentsClosing() - 1).last();

			Point<Kernel> point(pt.x, pt.y);

			Vertex* next = pqt.findElement(point, 0.00001);
			if (next == nullptr) {
				next = graph->addVertex(point);
				pqt.insert(*next);
			}
			if (prev != nullptr && !next->isNeighborOf(prev) && next != prev) {
				graph->addEdge(prev, next);
			}
			prev = next;
		}
	}

	return graph;
}