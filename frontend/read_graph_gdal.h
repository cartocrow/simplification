#pragma once

#include <filesystem>
#include <cartocrow/core/cubic_bezier.h>
#include "library/straight_graph.h"
#include "region_set.h"
#include "simplification_algorithm.h"

#include <ogrsf_frmts.h>

std::pair<RegionSet<Exact>*, std::optional<std::string>> readRegionSetUsingGDAL(const std::filesystem::path& path);

template<class Graph>
void exportRegionSetUsingGDAL(const std::filesystem::path& path, Graph* graph, const RegionSet<Exact>& regions, std::optional<std::string> spatialReference) {

    const char* pszDriverName = "ESRI Shapefile";
    GDALDriver* poDriver;

    GDALAllRegister();

    poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
    if (poDriver == nullptr)
    {
        std::cout << pszDriverName << " driver not available." << std::endl;
        return;
    }

    GDALDataset* poDS;

    poDS = poDriver->Create(path.parent_path().string().c_str(), 0, 0, 0, GDT_Unknown, nullptr);
    if (poDS == nullptr)
    {
        std::cout << "Creation of output file failed." << std::endl;
        return;
    }

    OGRLayer* poLayer;

    OGRSpatialReference* srs;
    if (spatialReference.has_value()) {
        srs = new OGRSpatialReference();
        srs->importFromWkt((*spatialReference).c_str());
    }
    else {
        srs = nullptr;
    }

    poLayer = poDS->CreateLayer(path.stem().string().c_str(), srs, wkbMultiPolygon, NULL);
    if (poLayer == NULL)
    {
        std::cout << "Layer creation failed." << std::endl;
        GDALClose(poDS);
        return;
    }

    for (const auto& [attribute, value] : regions[0].attributes) {
        OGRFieldDefn oField = [&]() {
            if (std::holds_alternative<double>(value)) {
                return OGRFieldDefn(attribute.c_str(), OFTReal);
            }
            else if (std::holds_alternative<int>(value)) {
                return OGRFieldDefn(attribute.c_str(), OFTInteger);
            }
            else if (std::holds_alternative<int64_t>(value)) {
                return OGRFieldDefn(attribute.c_str(), OFTInteger64);
            }
            else if (std::holds_alternative<std::string>(value)) {
                return OGRFieldDefn(attribute.c_str(), OFTString);
            } 
            else {
                std::cerr << "Did not handle attribute type of field " << attribute << std::endl;
            }
            // todo: other attributes
            }();

        if (poLayer->CreateField(&oField) != OGRERR_NONE) {
            std::cout << "Creating field failed." << std::endl;
            GDALClose(poDS);
            return;
        }
    }

    auto convert = [&graph](ArcRegistration reg) {
        OGRLinearRing* ring = new OGRLinearRing();

        auto addVertexToRing = [&ring](typename Graph::Vertex* v) {
            ring->addPoint(CGAL::to_double(v->getPoint().x()), CGAL::to_double(v->getPoint().y()));
            };

        bool first = true;

        for (Arc& a : reg) {

            typename Graph::Boundary* bd = graph->getBoundaries()[a.boundary];

            if (a.reverse) {
                typename Graph::Edge* e = bd->getLastEdge();
                if (first) {
                    addVertexToRing(e->getTarget());
                    first = false;
                }
                addVertexToRing(e->getSource());
                while (e != bd->getFirstEdge()) {
                    e = e->previous();
                    addVertexToRing(e->getSource());
                }
            }
            else {
                typename Graph::Edge* e = bd->getFirstEdge();
                if (first) {
                    addVertexToRing(e->getSource());
                    first = false;
                }
                addVertexToRing(e->getTarget());
                while (e != bd->getLastEdge()) {
                    e = e->next();
                    addVertexToRing(e->getTarget());
                }
            }
        }

        return ring;
        };

        for (const auto& region : regions) {
            
            OGRMultiPolygon mPgn;

            int index = 0;
            for (int rc : region.ringcounts) {
                OGRPolygon pgn;
                pgn.addRing(convert(region.arcs[index]));
                index++;
                rc--;
                while (rc > 0) {
                    pgn.addRing(convert(region.arcs[index]));
                    index++;
                    rc--;
                }
                mPgn.addGeometry(&pgn);
            }

            OGRFeature* poFeature;

            poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
            for (const auto& [attribute, data] : region.attributes) {
                if (auto vDouble = std::get_if<double>(&data)) {
                    poFeature->SetField(attribute.c_str(), *vDouble);
                }
                else if (auto vInt = std::get_if<int>(&data)) {
                    poFeature->SetField(attribute.c_str(), static_cast<int>(*vInt));
                }
                else if (auto vInt64 = std::get_if<int64_t>(&data)) {
                    poFeature->SetField(attribute.c_str(), static_cast<GIntBig>(*vInt64));
                }
                else if (auto vStr = std::get_if<std::string>(&data)) {
                    poFeature->SetField(attribute.c_str(), (*vStr).c_str());
                }
                else {
                    std::cout << "Did not handle attribute value of field " << attribute << std::endl;
                }
            }

            poFeature->SetGeometry(&mPgn);

            if (poLayer->CreateFeature(poFeature) != OGRERR_NONE) {

                std::cout << "Failed to create feature in shapefile." << std::endl;

                GDALClose(poDS);
                return;
            }

            OGRFeature::DestroyFeature(poFeature);
        }
    
    GDALClose(poDS);

}

