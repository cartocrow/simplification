#pragma once

#include <cartocrow/core/core.h>
#include "library/point_quad_tree.h"
#include "simplification_algorithm.h"

namespace cartocrow {
	using RegionAttribute = std::variant<int, std::vector<int>, double, std::vector<double>, std::string, std::vector<std::string>, int64_t>;
	using RegionAttributes = std::unordered_map<std::string, RegionAttribute>;
	// Match the types in GDAL?
	//    /** Single signed 32bit integer */ OFTInteger = 0,
	//    /** List of signed 32bit integers */ OFTIntegerList = 1,
	//    /** Double Precision floating point */ OFTReal = 2,
	//    /** List of doubles */ OFTRealList = 3,
	//    /** String of ASCII chars */ OFTString = 4,
	//    /** Array of strings */ OFTStringList = 5,
	//    /** deprecated */ OFTWideString = 6,
	//    /** deprecated */ OFTWideStringList = 7,
	//    /** Raw Binary data */ OFTBinary = 8,
	//    /** Date */ OFTDate = 9,
	//    /** Time */ OFTTime = 10,
	//    /** Date and Time */ OFTDateTime = 11,
	//    /** Single signed 64bit integer */ OFTInteger64 = 12,
	//    /** List of signed 64bit integers */ OFTInteger64List = 13,

	struct Arc {
		int boundary;
		bool reverse;

		Arc(int bd, bool rev) : boundary(bd), reverse(rev) {}
	};

	struct ArcRegistration : public std::vector<Arc> {

		bool validate(InputGraph* graph);
	};

	template <class K>
	struct Region {
		RegionAttributes attributes;
		std::vector<int> ringcounts;
		std::vector<Polygon<Exact>> rings;
		std::vector<ArcRegistration> arcs;
	};

	template <class K>
	using RegionSet = std::vector<Region<K>>;

	InputGraph* constructGraphAndRegisterBoundaries(RegionSet<Exact>& rs, const int depth);

}