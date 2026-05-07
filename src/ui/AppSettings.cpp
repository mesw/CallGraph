#include "ui/AppSettings.h"

namespace cg {

AppSettings::AppSettings(QObject* parent)
    : QObject(parent)
    , m_settings("CallGraph", "CallGraph")
{
    load();
}

void AppSettings::load() {
    m_sourceRoot      = m_settings.value("sourceRoot").toString();
    m_editorScheme    = m_settings.value("editorScheme",
                            "vscode://file/%1:%2").toString();
    m_exportDepth     = m_settings.value("exportDepth", 3).toInt();
    m_virtualExpansion= m_settings.value("virtualExpansion", false).toBool();
}

void AppSettings::save() {
    m_settings.setValue("sourceRoot",       m_sourceRoot);
    m_settings.setValue("editorScheme",     m_editorScheme);
    m_settings.setValue("exportDepth",      m_exportDepth);
    m_settings.setValue("virtualExpansion", m_virtualExpansion);
}

QString AppSettings::sourceRoot() const { return m_sourceRoot; }
void AppSettings::setSourceRoot(const QString& root) {
    if (m_sourceRoot == root) return;
    m_sourceRoot = root;
    emit sourceRootChanged();
}

QString AppSettings::editorScheme() const { return m_editorScheme; }
void AppSettings::setEditorScheme(const QString& scheme) {
    if (m_editorScheme == scheme) return;
    m_editorScheme = scheme;
    emit editorSchemeChanged();
}

int AppSettings::exportDepth() const { return m_exportDepth; }
void AppSettings::setExportDepth(int depth) {
    if (m_exportDepth == depth) return;
    m_exportDepth = depth;
    emit exportDepthChanged();
}

bool AppSettings::virtualExpansion() const { return m_virtualExpansion; }
void AppSettings::setVirtualExpansion(bool enabled) {
    if (m_virtualExpansion == enabled) return;
    m_virtualExpansion = enabled;
    emit virtualExpansionChanged();
}

} // namespace cg
