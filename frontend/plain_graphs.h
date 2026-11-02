#pragma once

#include <cartocrow/renderer/graph_painting.h>
#include <cartocrow/data_structures/straight_graph_2.h>
#include <cartocrow/data_structures/graph_traits_2.h>

using namespace cartocrow;

using MyGraphTraits = CustomGraphTraits<false, true, true, true>;

extern template Graph_2<std::monostate, std::monostate, std::monostate, Graph_segment_curve_traits_2<Exact>, MyGraphTraits>;
using InputGraph = Straight_graph_2<std::monostate, std::monostate, std::monostate, Exact, MyGraphTraits>;

extern template Graph_2<std::monostate, std::monostate, std::monostate, Graph_segment_curve_traits_2<Inexact>, MyGraphTraits>;
using OutputGraph = Straight_graph_2<std::monostate, std::monostate, std::monostate, Inexact, MyGraphTraits>;

extern template GraphPainting<InputGraph>;
extern template GraphPainting<OutputGraph>;