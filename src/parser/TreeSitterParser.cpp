#include "parser/TreeSitterParser.h"

#include <tree_sitter/api.h>

// Declaration of the tree-sitter-cpp language function
extern "C" const TSLanguage* tree_sitter_cpp();

namespace cg {

TreeSitterParser::TreeSitterParser() {
    m_parser = ts_parser_new();
    ts_parser_set_language(m_parser, tree_sitter_cpp());
}

TreeSitterParser::~TreeSitterParser() {
    if (m_tree)   ts_tree_delete(m_tree);
    if (m_parser) ts_parser_delete(m_parser);
}

bool TreeSitterParser::parse(const char* source, std::size_t length) {
    if (m_tree) {
        ts_tree_delete(m_tree);
        m_tree = nullptr;
    }
    m_hasErrors = false;

    m_tree = ts_parser_parse_string(m_parser, nullptr, source,
                                    static_cast<uint32_t>(length));
    if (!m_tree) return false;

    checkErrors();
    return true;
}

void TreeSitterParser::checkErrors() {
    if (!m_tree) return;
    TSNode root = ts_tree_root_node(m_tree);
    m_hasErrors = ts_node_has_error(root);
}

} // namespace cg
