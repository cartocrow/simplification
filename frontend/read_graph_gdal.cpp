#include "read_graph_gdal.h"

#include <cartocrow/reader/gdal_conversion.h>
#include <cartocrow/core/transform_helpers.h>

std::pair<RegionSet<Exact>*, OGRSpatialReference*> readRegionSetUsingGDAL(const std::filesystem::path& path) {
	GDALAllRegister();
	GDALDataset* poDS;

	poDS = (GDALDataset*)GDALOpenEx(path.string().c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr);
	if (poDS == nullptr) {
		printf("GDAL open failed.\n");
		exit(1);
	}
	OGRLayer* poLayer;
	poLayer = poDS->GetLayer(0);

	poLayer->ResetReading();

	RegionSet<Exact>* regionSet = new RegionSet<Exact>();

	for (auto& poFeature : *poLayer) {
		OGRGeometry* poGeometry;

		poGeometry = poFeature->GetGeometryRef();

		Region<Exact> region;

		auto addRing = [&](auto& ring) {
			Polygon<Exact> polygon;
			for (auto& pt : ring) {
				polygon.push_back({ pt.getX(), pt.getY() });
			}
			// if the begin and end vertices are equal, remove one of them
			if (polygon.container().front() == polygon.container().back()) {
				polygon.container().pop_back();
			}
			region.rings.push_back(polygon);
			};
		auto addPoly = [&](auto& poly) {
			int cnt = 0;
			for (auto& ring : poly) {
				addRing(ring);
				cnt++;
			}
			region.ringcounts.push_back(cnt);
			};
		auto addMultiPoly = [&](auto& mpoly) {
			for (auto& poly : mpoly) {
				addPoly(poly);
			}
			};

		switch (wkbFlatten(poGeometry->getGeometryType())) {
		case wkbMultiPolygon: {
			OGRMultiPolygon* poMultiPolygon = poGeometry->toMultiPolygon();
			addMultiPoly(poMultiPolygon);
			break;
		}
		case wkbPolygon: {
			OGRPolygon* poly = poGeometry->toPolygon();
			addPoly(poly);
			break;
		}
		default: std::cout << "Did not handle this type of geometry: " << poGeometry->getGeometryName() << std::endl;
		}


		int i = 0;
		for (auto&& oField : *poFeature) {
			std::string name = poFeature->GetDefnRef()->GetFieldDefn(i)->GetNameRef();
			switch (oField.GetType()) {
			case OFTInteger:
				region.attributes[name] = static_cast<int>(oField.GetInteger());
				break;
			case OFTReal:
				region.attributes[name] = oField.GetDouble();
				break;
			case OFTInteger64:
				region.attributes[name] = static_cast<int64_t>(oField.GetInteger64());
				break;
			default:
				std::cout << "Did not handle this type of attribute: " << oField.GetType() << std::endl;
				break;
			}
			++i;
		}

		regionSet->push_back(region);
	}

	return { regionSet, poLayer->GetSpatialRef() };
}
