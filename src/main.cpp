#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "Audio/AudioInput.h"
#include "Audio/AudioOutput.h"
#include "Network/WebRTC.h"

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;

    qmlRegisterType<AudioInput>("Audio", 1, 0, "AudioInput");
    qmlRegisterType<AudioOutput>("Audio", 1, 0, "AudioOutput");
    qmlRegisterType<WebRTC>("WebRTCModule", 1, 0, "WebRTC");

    const QUrl url(QStringLiteral("qrc:/src/UI/main.qml"));
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreated,
        &app,
        [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
