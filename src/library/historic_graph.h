#pragma once

#include "cartocrow/core/core.h"

namespace cartocrow::simplification {

template <class G> struct Operation;

// edges store the last operation that has been performed on them

template <class G> concept EdgeStoredHistory = requires(typename G::Edge e, Operation<G>* h) {

	/// Sets the data for a \ref HistoricArrangement
	{e::setHistory(h)};
	/// Retrieves the data for a \ref HistoricArrangement
	{
		e->getHistory()
	} -> std::same_as<Operation<G>*>;
};

template <class G> struct Operation {

	using Graph = G;
	using Edge = G::Edge;
	using Vertex = G::Vertex;
	using HistGraph = HistoricGraph<G>;
	using Op = Operation<G>;

	Edge* pre_edge;
	Op* past;

	Operation(Edge* e) : pre_edge(e) {
		past = e->getHistory();
		e->setHistory(this);
	}

	virtual void undo(Graph& hg) = 0;
	virtual void redo(Graph& hg) = 0;
};

template <class G> SplitOperation : public Operation<G> {

	SplitOperation(Edge * e, Point<typename G::Kernel> pt) : Operation<G>(e), post_loc(pt) {}

	// Result of the split
	Edge* post_edge_1 = nullptr;
	Edge* post_edge_2 = nullptr;
	Point<typename G::Kernel> post_loc;

	// edge histories of the input
	Op* past = nullptr;
	// edge histories of the result
	Op* future_1 = nullptr;
	Op* future_2 = nullptr;

	virtual void undo(Graph& g) {

		future_1 = post_edge_1->getHistory();
		future_2 = post_edge_2->getHistory();

		Vertex* v = post_edge_1->commonVertex(post_edge_2);

		Vertex* u;
		Vertex* w;
		if (v->edge(0)->getTarget() == v) {
			u = v->neighbor(0);
			w = v->neighbor(1);
		} else {
			u = v->neighbor(1);
			w = v->neighbor(0);
		}

		g.removeVertex(v);
		pre_edge = g.addEdge(u, w);

		if (past != nullptr) {
			pre_edge.setHistory(past);
		}
	}

	virtual void redo(Graph& hg) {}
}

template <class G> EraseOperation : public Operation<G> {

	EraseOperation(Vertex* v) : Operation<G>(v->edge(0)) {
		target = v == pre_edge->getTarget();
		pre_loc = v->getPoint();
		target_2 = v == v->edge(1)->getTarget();
		past_2 = v->edge(1)->getHistory();
	}

	bool target;
	bool target_2;
	Point<typename G::Kernel> pre_loc;

	// edge histories of the input
	Op* past_2 = nullptr;
	// edge histories of the result
	Op* future = nullptr;

	virtual void undo(Graph& g) {

		Vertex* u = edge->getSource();
		Vertex* w = edge->getTarget();

		g.removeEdge(edge);

		Vertex* v = g.addVertex(pre_loc);
		if (target) {
			edge = g.addEdge(u, v);
		} else {
			edge = g.addEdge(v, u);
		}

		Edge* edge_2;
		if (target_2) {
			edge_2 = g.addEdge(w, v);
		} else {
			edge_2 = g.addEdge(v, w);
		}

		if (past != null) {
			past->edge = edge;
		}

		if (past_2 != null) {
			past->edge = edge_2;
		}
	}

	virtual void redo(Graph& g) {
	
		Vertex* u = target ? edge->getSource() : edge->getTarget();
		Vertex* w = target ? edge->walkTargetNeighbor() : edge->walkSourceNeighbor();

		g.removeVertex(v);
		edge = g.addEdge(u, w);
	}
}

template <class G> ShiftOperation : public Operation<G> {

	ShiftOperation(Vertex * v, Point<typename G::Kernel> pt) : Operation<G>(v.edge(0)) {
		target = v == edge->getTarget();
		pre_loc = v->getPoint();
		post_loc = pt;
	}

	// Input to the split
	bool target;
	Point<typename G::Kernel> pre_loc;
	// Result of the split
	Point<typename G::Kernel> post_loc;

	virtual void undo(Graph& g) {

		Vertex* v = target ? pre_edge->target() : pre_edge->source();
		v->setPoint(pre_loc);

		if (past != null) {
			past->pre_edge = pre_edge;
		}
	}

	virtual void redo(Graph& g) {

		Vertex* v = target ? pre_edge->target() : pre_edge->source();
		v->setPoint(post_loc);
	}
}

template <class G> struct OperationBatch {

	using Op = Operation<G>;
	using HistGraph = HistoricGraph<G>;

	~OperationBatch() {
		for (Op* op : operations) {
			delete op;
		}
	}

	std::vector<Op*> operations;

	/// Number of edges in the map after this operation
	int post_complexity;
	/// Maximum cost of operations up to this one
	Number<typename G::Kernel> post_maxcost;

	void undo(HistGraph& hg) {
		// NB: reverse
		for (auto it = operations.rbegin(); it != operations.rend(); it++) {

			Op* op = *it;
			op->undo(hg);
		}
	}
	void redo(HistGraph& hg) {
		for (Op* op : operations) {
			op->redo(hg);
		}
	}
};

template <class G> class HistoricGraph {
  public:
	using Graph = G;
	using Vertex = G::Vertex;
	using Edge = G::Edge;
	using Kernel = G::Kernel;

  private:
	using Batch = OperationBatch<G>;

	Graph& graph;

	Number<Kernel> max_cost;
	int in_complexity;
	Batch* building_batch = nullptr;

	std::vector<Batch*> history;
	std::vector<Batch*> undone;

  public:
	HistoricGraph(Graph& graph) : graph(graph), max_cost(0) {
		in_complexity = graph.getEdgeCount();
	}

	~HistoricGraph() {
		for (Batch* op : history) {
			delete op;
		}

		for (Batch* op : undone) {
			delete op;
		}
	}

	Graph& getGraph() {
		return graph;
	}

	Edge* erase(Vertex* v) {

		assert(v->degree() == 2);
		assert(building_batch != null_ptr);

		EraseOperation<G>* op = new EraseOperation<G>(v);
		op->redo(map);
		building_batch->operations.push_back(op);

		return op->post_edge;
	}

	Vertex* split(Edge* e, Point<Kernel> p) {

		ShiftOperation<G>* op = new ShiftOperation<G>(v, p);
		op->redo(map);
		building_batch->operations.push_back(op);

		return op->post_edge_1->commonVertex(op->post_edge_2);
	}

	void shiftVertex(Vertex* v, Point<Kernel> p) {

		assert(v->degree() > 0);
		assert(building_batch != null_ptr);

		ShiftOperation<G>* op = new ShiftOperation<G>(v, p);
		op->redo(map);
		building_batch->operations.push_back(op);
	}

	void recallComplexity(int c) {

		assert(building_batch == null_ptr);

		while ( // if history is a single element, check input complexity
		    (history.size() >= 1 && in_complexity <= c)
		    // if history is more than a single element, check the previous operation
		    || (history.size() > 1 && history[history.size() - 2]->post_complexity <= c)) {
			backInTime();
		}
		while (!undone.empty() && map.number_of_edges() > c) {
			forwardInTime();
		}
	}

	void recallThreshold(Number<Kernel> t) {

		assert(building_batch == null_ptr);

		while (history.size() >= 1 && history.last()->post_maxcost > t) {
			backInTime();
		}
		while (!undone.empty() && undone.last()->post_maxcost <= t) {
			forwardInTime();
		}
	}

	void backInTime() {

		assert(building_batch == null_ptr);

		Batch* batch = history.back();
		history.pop_back();
		undone.push_back(batch);

		batch->undo(map);
	}

	void forwardInTime() {

		assert(building_batch == null_ptr);

		Batch* batch = undone.back();
		undone.pop_back();
		history.push_back(batch);

		batch->redo(map);
	}

	void goToPresent() {
		while (!atPresent()) {
			forwardInTime();
		}
	}

	bool atPresent() {
		return undone.empty();
	}

	void startBatch(Number<Kernel> c) {
		assert(building_batch == null_ptr);
		assert(atPresent());

		building_batch = new Batch();
		history.push_back(building_batch);

		if (max_cost < cost) {
			max_cost = cost;
		}

		building_batch->post_maxcost = max_cost;
	}

	void endBatch() {
		assert(building_batch != null_ptr);

		building_batch->post_complexity = graph.getEdgeCount();
		building_batch = nullptr;
	}
};

} // namespace cartocrow::simplification