#pragma once

#include <cartocrow/core/core.h>

namespace cartocrow::simplification {

	namespace detail {
		template <class Graph> struct Operation;
		template <class Graph> struct OperationBatch;

		template <class Graph>
		concept EdgeStoredOperations = requires(typename Graph::Edge::Data d) {

			{
				d.hist
			} -> std::same_as<Operation<Graph>*&>; // c++ shenanigans: the expression is still a handle, even if it's declared as a nonhandle.
		};
	}

	template <class Graph>
		requires detail::EdgeStoredOperations<Graph>
	class HistoricGraph {
	public:
		using Vertex = Graph::Vertex;
		using Edge = Graph::Edge;
		using Kernel = Graph::Kernel;
		using BaseGraph = Graph;

	private:
		using Batch = detail::OperationBatch<Graph>;

		Graph& graph;

		Number<Kernel> max_cost;
		int in_complexity;
		Batch* building_batch = nullptr;

		std::vector<Batch*> history;
		std::vector<Batch*> undone;

	public:
		HistoricGraph(Graph& graph);
		~HistoricGraph();

		Graph& getBaseGraph();
		std::vector<Vertex*>& getVertices();
		std::vector<Edge*>& getEdges();
		int getEdgeCount();

		void recallComplexity(int c);
		void recallThreshold(Number<Kernel> t);
		void backInTime();
		void forwardInTime();
		void goToPresent();
		bool atPresent();

		void startBatch(Number<Kernel> c);
		void endBatch();

		Edge* mergeVertex(Vertex* v);
		Vertex* splitEdge(Edge* e, Point<Kernel> p);
		void shiftVertex(Vertex* v, Point<Kernel> p);
	};

} // namespace cartocrow::simplification

#include "historic_graph.hpp"