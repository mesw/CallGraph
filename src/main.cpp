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

    // Load main QML file. With qt_add_qml_module (URI = "CallGraph"),
    // files land at qrc:/qt/qml/CallGraph/<filename>.qml
    const QUrl mainUrl(QStringLiteral("qrc:/qt/qml/CallGraph/Main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [mainUrl](QObject* obj, const QUrl& url) {
        if (!obj && url == mainUrl) {
            qCritical("Failed to load Main.qml");
            QCoreApplication::exit(1);
        }
    }, Qt::QueuedConnection);

    engine.load(mainUrl);

    return app.exec();
}
