#include "ui/MainController.h"
#include "ui/EdgeListModel.h"
#include "ui/SearchModel.h"

#include <QGuiApplication>
#include <QCommandLineParser>
#include <QLoggingCategory>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    app.setApplicationName("CallGraph");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("CallGraph");

    // Enable categorised logging: export CALLGRAPH_LOG=cg.*=true
    QLoggingCategory::setFilterRules(QStringLiteral("cg.*.info=true\ncg.*.warning=true"));

    QQuickStyle::setStyle("Basic");

    QCommandLineParser parser;
    parser.setApplicationDescription("C++ call-graph analyser");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption sourceRootOpt(
        {"s", "source-root"},
        "Path to the git working tree to analyse.",
        "path");
    parser.addOption(sourceRootOpt);
    parser.process(app);

    // Register QML types
    qmlRegisterUncreatableType<cg::SearchModel>(
        "CallGraph", 1, 0, "SearchModel", "Use controller.searchModel");
    qmlRegisterUncreatableType<cg::EdgeListModel>(
        "CallGraph", 1, 0, "EdgeListModel", "Use controller.callersModel / calleesModel");

    cg::MainController controller;

    // Apply command-line source root override
    if (parser.isSet(sourceRootOpt)) {
        controller.settings()->setSourceRoot(parser.value(sourceRootOpt));
        controller.settings()->save();
    }

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("controller", &controller);

    // loadFromModule is the Qt 6.4+ API for loading a type from a registered
    // QML module. It avoids hardcoding the internal qrc:/ resource path, which
    // varies depending on how qt_add_qml_module places files.
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() {
        qCritical("Failed to create QML root object");
        QCoreApplication::exit(1);
    }, Qt::QueuedConnection);

    engine.loadFromModule("CallGraph", "Main");

    return app.exec();
}
