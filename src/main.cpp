#include <QApplication>
#include "mainwindow.h"
#include "styles.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Zelvex");
    app.setOrganizationName("Zelvex");
    app.setApplicationVersion("2.0.0");

    app.setStyleSheet(ZelvexStyles::darkTheme());

    MainWindow window;
    window.show();

    return app.exec();
}
