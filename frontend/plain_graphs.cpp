#include "plain_graphs.h"

template class Graph_2<Graph_segment_curve_traits_2<Exact>, MyGraphTraits>;
template class Graph_2<Graph_segment_curve_traits_2<Inexact>, MyGraphTraits>;
template class GraphPainting<InputGraph>;
template class GraphPainting<OutputGraph>;