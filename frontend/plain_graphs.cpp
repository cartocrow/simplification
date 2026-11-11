#include "plain_graphs.h"

template class cartocrow::Graph_2<Graph_segment_curve_traits_2<Exact>, MyGraphTraits>;
template class cartocrow::Graph_2<Graph_segment_curve_traits_2<Inexact>, MyGraphTraits>;
template class cartocrow::GraphPainting<InputGraph>;
template class cartocrow::GraphPainting<OutputGraph>;