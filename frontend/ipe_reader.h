#pragma once

#include <sstream>
#include <string>
#include <fstream>

#include <cartocrow/reader/ipe_reader.h>
#include <cartocrow/data_structures/graph_map_2.h>

#include "library/vertex_quad_tree.h"
#include "library/utils.h"

using namespace cartocrow;
using namespace cartocrow::simplification;

template<class Graph>
VertexQuadTree<Graph>* readIpeFile(Graph& graph, const std::filesystem::path& file, const int depth, const Number<typename Graph::Kernel> prec) {
	using Vertex_handle = Graph::Vertex_handle;
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

		Vertex_handle prev = nullptr;
		Vertex_handle first = nullptr;
		for (int k = 0; k < lines[i].size(); k++) {
			Point<Kernel>& point = lines[i][k];

			Vertex_handle next = pqt->findElement(point, prec);
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

template<class Graph>
Graph* readIpeFile(const std::filesystem::path& file, const int depth, const Number<typename Graph::Kernel> prec) {
	Graph* graph = new Graph();
	auto pqt = readIpeFile(*graph, file, depth, prec);
	if (pqt == nullptr) {
		delete graph;
		return nullptr;
	}
	delete pqt;
	return graph;
}

template<class Graph>
void writeIpeFile(Graph& graph, const std::filesystem::path& file) {
	using Vertex_handle = Graph::Vertex_handle;
	using Edge_handle = Graph::Edge_handle;
	using Kernel = Graph::Kernel;

	std::ofstream filestream(file);

	filestream << "<?xml version=\"1.0\"?>" << std::endl;
	filestream << "<!DOCTYPE ipe SYSTEM \"ipe.dtd\">" << std::endl;
	filestream << "<ipe version=\"70010\" creator=\"Ipe 7.0.10\">" << std::endl;
	filestream << "<info created=\"D:20100909134504\" modified=\"D:20100909150018\"/>" << std::endl;
	filestream << "<ipestyle name=\"export\">" << std::endl;
	filestream << "<layout paper=\"595 842\" origin=\"0 0\" frame=\"595 842\"/>" << std::endl;
	filestream << "<color name=\"black\" value=\"0.0 0.0 0.0\"/>" << std::endl;
	filestream << "<symbolsize name=\"normal\" value=\"3.0\"/>" << std::endl;
	filestream << "<pen name=\"normal\" value=\"0.4\"/>" << std::endl;
	filestream << "</ipestyle>" << std::endl;
	filestream << "<page>" << std::endl;
	filestream << "<layer name=\"default\"/>" << std::endl;
	filestream << "<view layers=\"default\" active=\"default\"/>" << std::endl;

	Graph_static_edge_map<Graph, Edge_handle> handled(graph, nullptr); // workaround because bool doesnt work yet

	for (Edge_handle e : graph.edges()) {
		if (handled[e] != nullptr) {
			continue;
		}

		Edge_handle walk = e;
		while (walk->source()->degree() == 2) {
			walk = walk->prev();
			if (walk == e) {
				break;
			}
		}

		filestream << "<path cap=\"1\" layer=\"default\" stroke=\"black\" pen=\"normal\">" << std::endl;
		if (walk->source()->degree() == 2) {
			// cycle
			assert(walk == e);
			filestream << walk->target()->point().x() << " " << walk->target()->point().y() << " m" << std::endl;
			handled[walk] = e;
			walk = walk->next();
			while (walk != e) {
				filestream << walk->target()->point().x() << " " << walk->target()->point().y() << " l" << std::endl;
				handled[walk] = e;
				walk = walk->next();
			}
			filestream << "h" << std::endl;
		}
		else {
			// polyline
			filestream << walk->source()->point().x() << " " << walk->source()->point().y() << " m" << std::endl;
			filestream << walk->target()->point().x() << " " << walk->target()->point().y() << " l" << std::endl;
			handled[walk] = e;
			while (walk->target()->degree() == 2) {
				walk = walk->next();
				filestream << walk->target()->point().x() << " " << walk->target()->point().y() << " l" << std::endl;
				handled[walk] = e;
			}
		}
		filestream << "</path>" << std::endl;
	}

	filestream << "</page>" << std::endl;
	filestream << "</ipe>" << std::endl;
}