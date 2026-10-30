#pragma once

#include <cartocrow/renderer/graph_painting.h>
#include <cartocrow/data_structures/straight_graph_2.h>
#include <cartocrow/data_structures/graph_traits_2.h>

using namespace cartocrow;

extern template Graph_2<std::monostate, std::monostate, Graph_segment_curve_traits_2<Exact>, CustomGraphTraits<false, true, true, true, std::monostate>>;
using InputGraph = Straight_graph_2<std::monostate, std::monostate, Exact, CustomGraphTraits<false, true, true, true, std::monostate>>;

extern template Graph_2<std::monostate, std::monostate, Graph_segment_curve_traits_2<Inexact>, CustomGraphTraits<false, true, true, true, std::monostate>>;
using OutputGraph = Straight_graph_2<std::monostate, std::monostate, Inexact, CustomGraphTraits<false, true, true, true, std::monostate>>;

extern template GraphPainting<InputGraph>;
extern template GraphPainting<OutputGraph>;