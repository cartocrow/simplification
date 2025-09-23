#pragma once

#include <cartocrow/core/core.h>

namespace cartocrow::simplification {

template <class G> struct Operation;

// edges store the last operation that has been performed on them

template <class Graph> struct Operation {

	using Edge = Graph::Edge;

	Edge* edge;

	Operation(Edge* e) : edge(e) {}

	virtual void undo(Graph& g) = 0;
	virtual void redo(Graph& g) = 0;
};

template <class Graph> struct SplitOperation : public Operation<Graph> {

	using Edge = Graph::Edge;
	using Vertex = Graph::Vertex;
	using Op = Operation<Graph>;

	SplitOperation(Edge* e, Point<typename Graph::Kernel> pt) : Operation<Graph>(e), loc(pt) {
		// store the past
		past = e->data().hist;
		// we don't know the future yet
		future_inc = nullptr;
		future_out = nullptr;
	}

	// if not yet performed / undone:
	//   edge == the edge to split
	// if performed / redone:
	//   edge == incoming edge of the created vertex

	Point<typename Graph::Kernel> loc;
	Op* past;
	Op* future_inc;
	Op* future_out;

	virtual void undo(Graph& g) {

		// store the future
		future_inc = this->edge->data().hist;
		future_out = this->edge->getTarget()->outgoing()->data().hist;

		// perform and update this edge
		this->edge = g.mergeVertex(this->edge->getTarget());

		// update the past
		this->edge->data().hist = past;
		if (past != nullptr) {
			past->edge = this->edge;
		}
	}

	virtual void redo(Graph& g) {
		perform(g);
	}

	Vertex* perform(Graph& g) {

		// perform and update this edge
		Vertex* v = g.splitEdge(this->edge, loc);
		this->edge = v->incoming();

		// update the future
		v->incoming()->data().hist = future_inc;
		v->outgoing()->data().hist = future_out;
		if (future_inc != nullptr) {
			future_inc->edge = v->incoming();
		}
		if (future_out != nullptr) {
			future_out->edge = v->outgoing();
		}

		return v;
	}
};

template <class Graph> struct EraseOperation : public Operation<Graph> {

	using Edge = Graph::Edge;
	using Vertex = Graph::Vertex;
	using Op = Operation<Graph>;

	EraseOperation(Vertex* v) : Operation<Graph>(v->incoming()), loc(v->getPoint()) {
		// store the past
		past_inc = v->incoming()->data().hist;
		past_out = v->outgoing()->data().hist;
		// we don't know the future yet
		future = nullptr;
	}

	// if not yet performed / undone:
	//   edge == incoming edge of the vertex to erase
	// if performed / redone:
	//   edge == the resulting edge

	Point<typename Graph::Kernel> loc;
	Op* past_inc;
	Op* past_out;
	Op* future;

	virtual void undo(Graph& g) {

		// store the future
		future = this->edge->data().hist;

		// perform and update this edge
		Vertex* v = g.splitEdge(this->edge, loc);
		this->edge = v->incoming();

		// update the past
		v->incoming()->data().hist = past_inc;
		v->outgoing()->data().hist = past_out;
		if (past_inc != nullptr) {
			past_inc->edge = this->edge;
		}
		if (past_out != nullptr) {
			past_out->edge = this->edge;
		}
	}

	virtual void redo(Graph& g) {
		perform(g);
	}

	Edge* perform(Graph& g) {

		// perform and update this edge
		this->edge = g.mergeVertex(this->edge->getTarget());

		// update the future
		this->edge->data().hist = future;
		if (future != nullptr) {
			future->edge = this->edge;
		}

		return this->edge;
	}
};

template <class Graph> struct ShiftOperation : public Operation<Graph> {

	using Edge = Graph::Edge;
	using Vertex = Graph::Vertex;
	using Op = Operation<Graph>;

	ShiftOperation(Vertex* v, Point<typename Graph::Kernel> pt) : Operation<Graph>(v.incoming()), pre_loc(v->getPoint()), post_loc(pt) {	
		// store the past
		past = this->edge->data().hist;
		// we don't know the future yet
		future = nullptr;
	}

	// edge is always the incoming edge of v
	Op* past;
	Op* future;

	Point<typename Graph::Kernel> pre_loc;
	Point<typename Graph::Kernel> post_loc;

	virtual void undo(Graph& g) {

		// store the future
		future = this->edge->data().hist;

		// perform (no need to update edge...)
		this->edge->getTarget()->setPoint(pre_loc);

		// update the past
		this->edge->data().hist = past;
		if (past != nullptr) {
			past->edge = this->edge; // NB: we should keep doing this as the edge may have changed in the future
		}
	}

	virtual void redo(Graph& g) {

		// perform (no need to update edge)
		this->edge->getTarget()->setPoint(post_loc);

		// update the future
		this->edge->data().hist = future;
		if (future != nullptr) {
			future->edge = this->edge; // NB: we should keep doing this as the edge may have changed in the future
		}
	}
};

template <class Graph> struct OperationBatch {

	using Op = Operation<Graph>;

	~OperationBatch() {
		for (Op* op : operations) {
			delete op;
		}
	}

	std::vector<Op*> operations;

	/// Number of edges in the map after this operation
	int post_complexity;
	/// Maximum cost of operations up to this one
	Number<typename Graph::Kernel> post_maxcost;

	void undo(Graph& g) {
		// NB: reverse
		for (auto it = operations.rbegin(); it != operations.rend(); it++) {

			Op* op = *it;
			op->undo(g);
		}
	}
	void redo(Graph& g) {
		for (Op* op : operations) {
			op->redo(g);
		}
	}
};

template <class Graph> 
concept EdgeStoredOperations = requires(typename Graph::Edge::Data d) {
	
	{
		d.hist
	} -> std::same_as<Operation<Graph>*&>; // c++ shenanigans: the expression is still a handle, even if it's declared as a nonhandle.
};

template <class Graph> 
	requires EdgeStoredOperations<Graph>
class HistoricGraph {
  public:
	using Vertex = Graph::Vertex;
	using Edge = Graph::Edge;
	using Kernel = Graph::Kernel;
	using BaseGraph = Graph;

  private:
	using Batch = OperationBatch<Graph>;

	Graph& graph;

	Number<Kernel> max_cost;
	int in_complexity;
	Batch* building_batch = nullptr;

	std::vector<Batch*> history;
	std::vector<Batch*> undone;

  public:
	HistoricGraph(Graph& graph) : graph(graph), max_cost(0) {
		assert(graph.isOriented());

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

	Graph& getBaseGraph() {
		return graph;
	}

	std::vector<Vertex*>& getVertices() {
		return graph.getVertices();
	}

	std::vector<Edge*>& getEdges() {
		return graph.getEdges();
	}

	int getEdgeCount() {
		return graph.getEdgeCount();
	}

	Edge* mergeVertex(Vertex* v) {

		assert(v->degree() == 2);
		assert(building_batch != nullptr);

		EraseOperation<Graph>* op = new EraseOperation<Graph>(v);
		Edge* e = op->perform(graph);
		building_batch->operations.push_back(op);

		return e;
	}

	Vertex* splitEdge(Edge* e, Point<Kernel> p) {

		SplitOperation<Graph>* op = new SplitOperation<Graph>(e, p);
		Vertex* v = op->perform(graph);
		building_batch->operations.push_back(op);

		return v;
	}

	void shiftVertex(Vertex* v, Point<Kernel> p) {

		assert(v->degree() > 0);
		assert(building_batch != nullptr);

		ShiftOperation<Graph>* op = new ShiftOperation<Graph>(v, p);
		op->redo(graph);
		building_batch->operations.push_back(op);
	}

	void recallComplexity(int c) {

		assert(building_batch == nullptr);

		while ( // if history is a single element, check input complexity
		    (history.size() >= 1 && in_complexity <= c)
		    // if history is more than a single element, check the previous operation
		    || (history.size() > 1 && history[history.size() - 2]->post_complexity <= c)) {
			backInTime();
		}
		while (!undone.empty() && graph.getEdgeCount() > c) {
			forwardInTime();
		}
	}

	void recallThreshold(Number<Kernel> t) {

		assert(building_batch == nullptr);

		while (history.size() >= 1 && history.last()->post_maxcost > t) {
			backInTime();
		}
		while (!undone.empty() && undone.last()->post_maxcost <= t) {
			forwardInTime();
		}
	}

	void backInTime() {

		assert(building_batch == nullptr);

		Batch* batch = history.back();
		history.pop_back();
		undone.push_back(batch);

		batch->undo(graph);
	}

	void forwardInTime() {

		assert(building_batch == nullptr);

		Batch* batch = undone.back();
		undone.pop_back();
		history.push_back(batch);

		batch->redo(graph);
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
		assert(building_batch == nullptr);
		assert(atPresent());

		building_batch = new Batch();
		history.push_back(building_batch);

		if (max_cost < c) {
			max_cost = c;
		}

		building_batch->post_maxcost = max_cost;
	}

	void endBatch() {
		assert(building_batch != nullptr);

		building_batch->post_complexity = graph.getEdgeCount();
		building_batch = nullptr;
	}
};

} // namespace cartocrow::simplification