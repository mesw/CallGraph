#pragma once

#include <QObject>
#include <QSettings>
#include <QString>

namespace cg {

class AppSettings : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString sourceRoot READ sourceRoot WRITE setSourceRoot NOTIFY sourceRootChanged)
    Q_PROPERTY(QString editorScheme READ editorScheme WRITE setEditorScheme NOTIFY editorSchemeChanged)
    Q_PROPERTY(int exportDepth READ exportDepth WRITE setExportDepth NOTIFY exportDepthChanged)
    Q_PROPERTY(bool virtualExpansion READ virtualExpansion WRITE setVirtualExpansion NOTIFY virtualExpansionChanged)

public:
    explicit AppSettings(QObject* parent = nullptr);

    QString sourceRoot() const;
    void setSourceRoot(const QString& root);

    QString editorScheme() const;
    void setEditorScheme(const QString& scheme);

    int exportDepth() const;
    void setExportDepth(int depth);

    bool virtualExpansion() const;
    void setVirtualExpansion(bool enabled);

    Q_INVOKABLE void save();
    Q_INVOKABLE void load();

signals:
    void sourceRootChanged();
    void editorSchemeChanged();
    void exportDepthChanged();
    void virtualExpansionChanged();

private:
    QSettings m_settings;
    QString   m_sourceRoot;
    QString   m_editorScheme = "vscode://file/%1:%2";
    int       m_exportDepth  = 3;
    bool      m_virtualExpansion = false;
};

} // namespace cg
