#pragma once

#include "export/Exporter.h"
#include <ostream>

namespace cg {

class MermaidExporter {
public:
    void exportSlice(const GraphSlice& slice,
                     const Index& index,
                     const ExportHeader& header,
                     std::ostream& out);
};

} // namespace cg
