#pragma once

#include <cartocrow/core/core.h>

namespace cartocrow::simplification {

	template <class VD, class ED, typename K> class StraightVertex;
	template <class VD, class ED, typename K> class StraightEdge;
	template <class VD, class ED, typename K> class StraightBoundary;

	template <class VD, class ED, typename K> class StraightGraph {

		template <class VD, class ED, typename K> friend class StraightVertex;
		template <class VD, class ED, typename K> friend class StraightEdge;
		template <class VD, class ED, typename K> friend class StraightBoundary;
		template <class InputGraph, class OutputGraph> friend OutputGraph* copy(InputGraph* input);

	public:
		using Kernel = K;
		using Vertex = StraightVertex<VD, ED, K>;
		using Edge = StraightEdge<VD, ED, K>;
		using Boundary = StraightBoundary<VD, ED, K>;

	private:
		std::vector<Vertex*> vertices;
		std::vector<Edge*> edges;
		std::vector<Boundary*> boundaries;
		bool oriented;
		bool sorted;

		bool verifyOriented();
		bool verifySorted();

		void clearBoundaries();
		void orientWithoutBoundaries();

	public:
		StraightGraph();
		~StraightGraph();

		bool isOriented();
		bool isSorted();
		int getVertexCount();
		int getEdgeCount();
		int getBoundaryCount();
		std::vector<Vertex*>& getVertices();
		std::vector<Edge*>& getEdges();
		std::vector<Boundary*>& getBoundaries();

		Vertex* addVertex(Point<K> pt);
		void removeVertex(Vertex* vtx);
		Edge* addEdge(Vertex* source, Vertex* target);
		void removeEdge(Edge* edge);

		/// <summary>
		/// Ensures that all degree-2 vertices v have edge(0) = (u,v) and edge(1) = (v,w).
		/// </summary>
		void orient();

		/// <summary>
		/// Sorts the edges at each vertex with degree > 2 in counter-clockwise order.		
		/// </summary>
		void sortIncidentEdges();

		// functions below are only correct in an oriented graph; they maintain orientation and sortedness
		Vertex* splitEdge(Edge* edge, Point<K> pt);
		Edge* mergeVertex(Vertex* v);
		void shiftVertex(Vertex* v, Point<K> pt);
	};

	template <class VD, class ED, typename K> class StraightVertex {

		template <class VD, class ED, typename K> friend class StraightGraph;
		template <class VD, class ED, typename K> friend class StraightEdge;
		template <class VD, class ED, typename K> friend class StraightBoundary;
		template <class InputGraph, class OutputGraph> friend OutputGraph* copy(InputGraph* input);

	public:
		using Kernel = K;
		using Vertex = StraightVertex<VD, ED, K>;
		using Edge = StraightEdge<VD, ED, K>;
		using Boundary = StraightBoundary<VD, ED, K>;
		using Data = VD;

	private:
		int index;
		Point<K> point;
		std::vector<Edge*> incident;
		VD d;

	public:
		int graphIndex();
		VD& data();

		int degree();
		std::vector<Edge*>& getEdges();

		Edge* edge(int i);
		Vertex* neighbor(int i);
		Edge* edgeTo(Vertex* v);
		bool isNeighborOf(Vertex* v);

		void setPoint(Point<K>& pt);
		Point<K>& getPoint();

		// functions below are only for deg-2 vertices in an oriented graph
		Edge* incoming();
		Edge* outgoing();
		Vertex* next();
		Vertex* previous();

		friend std::ostream& operator<<(std::ostream& os, StraightVertex<VD, ED, K> const* self) {
			return os << "(" << self->index << ": " << self->point << ")";
		}
	};

	template <class VD, class ED, typename K> class StraightEdge {

		template <class VD2, class ED2, typename K2> friend class StraightGraph;
		template <class VD2, class ED2, typename K2> friend class StraightVertex;
		template <class VD2, class ED2, typename K2> friend class StraightBoundary;
		template <class InputGraph, class OutputGraph> friend OutputGraph* copy(InputGraph* input);

	public:
		using Kernel = K;
		using Vertex = StraightVertex<VD, ED, K>;
		using Edge = StraightEdge<VD, ED, K>;
		using Boundary = StraightBoundary<VD, ED, K>;
		using Data = ED;

	private:
		int index;
		Vertex* source;
		Vertex* target;
		Boundary* boundary;
		ED d;

	public:
		int graphIndex();
		ED& data();

		Vertex* getSource();
		Vertex* getTarget();
		void reverse();

		Number<K> squared_length();
		Segment<K> getSegment();

		Vertex* other(Vertex* v);
		Vertex* commonVertex(Edge* e);
		Edge* sourceWalk();
		Edge* targetWalk();
		Vertex* sourceWalkNeighbor();
		Vertex* targetWalkNeighbor();

		Boundary* getBoundary();

		// functions below are only for deg-2 vertices in an oriented graph
		Edge* next();
		Edge* previous();

		friend std::ostream& operator<<(std::ostream& os, StraightEdge<VD, ED, K> const* self) {
			return os << self->source << "--" << self->target;
		}
	};

	template <class VD, class ED, typename K> class StraightBoundary {

		template <class VD2, class ED2, typename K2> friend class StraightGraph;
		template <class VD2, class ED2, typename K2> friend class StraightVertex;
		template <class VD2, class ED2, typename K2> friend class StraightEdge;
		template <class InputGraph, class OutputGraph> friend OutputGraph* copy(InputGraph* input);

	public:
		using Kernel = K;
		using Vertex = StraightVertex<VD, ED, K>;
		using Edge = StraightEdge<VD, ED, K>;
		using Boundary = StraightBoundary<VD, ED, K>;

	private:
		int index;
		Edge* first;
		Edge* last;
		bool cyclic;

	public:
		int graphIndex() {
			return index;
		}

		Edge* getFirstEdge() {
			return first;
		}
		Edge* getLastEdge() {
			return last;
		}

		bool isCyclic() {
			return cyclic;
		}
	};

	template<class InputGraph, class OutputGraph>
	OutputGraph* copy(InputGraph* input);

	template<class InputGraph, class OutputGraph>
	void copy(InputGraph* input, OutputGraph*&);

} // namespace cartocrow::simplification

#include "straight_graph.hpp"