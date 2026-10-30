#include "plain_graphs.h"

template class Graph_2<std::monostate, std::monostate, Graph_segment_curve_traits_2<Exact>, CustomGraphTraits<false, true, true, true, std::monostate>>;
template class Graph_2<std::monostate, std::monostate, Graph_segment_curve_traits_2<Inexact>, CustomGraphTraits<false, true, true, true, std::monostate>>;
template class GraphPainting<InputGraph>;
template class GraphPainting<OutputGraph>;