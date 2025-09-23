#pragma once

#include <cartocrow/core/core.h>

namespace cartocrow::simplification {

	// for not storing anything in a vertex or edge
	struct VoidData {};

	template <class VD, class ED, typename K> class StraightVertex;
	template <class VD, class ED, typename K> class StraightEdge;

	template <class VD, class ED, typename K> class StraightGraph {

		template <class VD, class ED, typename K> friend class StraightVertex;
		template <class VD, class ED, typename K> friend class StraightEdge;

	public:
		using Kernel = K;
		using Vertex = StraightVertex<VD, ED, K>;
		using Edge = StraightEdge<VD, ED, K>;

	private:
		std::vector<Vertex*> vertices;
		std::vector<Edge*> edges;
		bool oriented;
		bool sorted;

		bool verifyOriented();
		bool verifySorted();

	public:
		StraightGraph();

		~StraightGraph();

		bool isOriented();
		bool isSorted();
		int getVertexCount();
		int getEdgeCount();
		std::vector<Vertex*>& getVertices();
		std::vector<Edge*>& getEdges();

		Vertex* addVertex(Point<K> pt);
		void removeVertex(Vertex* vtx);
		Edge* addEdge(Vertex* source, Vertex* target);
		void removeEdge(Edge* edge);

		/// <summary>
		/// Ensures that all degree-2 vertices v have edge(0) = (u,v) and edge(1) = (v,w).
		/// </summary>
		void orient();

		/// <summary>
		/// Sorts the edges at each vertex with degree > 2 in counter-clockwise order.		/// 
		/// </summary>
		void sortIncidentEdges();

		Vertex* splitEdge(Edge* edge, Point<K> pt);
		Edge* mergeVertex(Vertex* v);
		void shiftVertex(Vertex* v, Point<K> pt);
	};

	template <class VD, class ED, typename K> class StraightVertex {

		template <class VD, class ED, typename K> friend class StraightGraph;
		template <class VD, class ED, typename K> friend class StraightEdge;

	public:
		using Kernel = K;
		using Vertex = StraightVertex<VD, ED, K>;
		using Edge = StraightEdge<VD, ED, K>;
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
		bool isNeighborOf(Vertex* v);

		void setPoint(Point<K>& pt);
		Point<K>& getPoint();

		Edge* incoming();
		Edge* outgoing();
		Vertex* next();
		Vertex* previous();
	};

	template <class VD, class ED, typename K> class StraightEdge {

		template <class VD, class ED, typename K> friend class StraightGraph;
		template <class VD, class ED, typename K> friend class StraightVertex;

	public:
		using Kernel = K;
		using Vertex = StraightVertex<VD, ED, K>;
		using Edge = StraightEdge<VD, ED, K>;
		using Data = ED;

	private:
		int index;
		Vertex* source;
		Vertex* target;
		ED d;

	public:
		int graphIndex();
		ED& data();

		Vertex* getSource();
		Vertex* getTarget();
		void reverse();

		Segment<K> getSegment();

		Vertex* other(Vertex* v);
		Vertex* commonVertex(Edge* e);
		Edge* sourceWalk();
		Edge* targetWalk();
		Vertex* sourceWalkNeighbor();
		Vertex* targetWalkNeighbor();
	};

} // namespace cartocrow::simplification

#include "straight_graph.hpp"