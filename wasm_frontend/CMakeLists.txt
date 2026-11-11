set(SOURCES
    simplification.cpp
    ../frontend/plain_graphs.cpp
    ../frontend/region_set.cpp
)

add_executable(simplification_wasm ${SOURCES})

target_link_libraries(
    simplification_wasm
    PRIVATE
    cartocrow::core
    cartocrow::data_structures
    cartocrow::renderer
)

target_link_options(simplification_wasm PRIVATE
    --emit-tsd "$<TARGET_FILE_DIR:simplification_wasm>/simplification_wasm.d.ts"
    -sMODULARIZE -sEXPORT_ES6
    -sALLOW_MEMORY_GROWTH
    # Only debug builds include debug information, a source map and assertions.
    # CartoCrowConfig.cmake adds -g to all targets, so release builds override it with -g0.
    "$<IF:$<CONFIG:Release,MinSizeRel>,-g0,-g;-gsource-map;-sASSERTIONS>"
)
