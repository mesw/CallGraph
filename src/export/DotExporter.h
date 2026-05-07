#pragma once

#include "export/Exporter.h"
#include <ostream>

namespace cg {

class DotExporter {
public:
    void exportSlice(const GraphSlice& slice,
                     const Index& index,
                     const ExportHeader& header,
                     std::ostream& out);
};

} // namespace cg
