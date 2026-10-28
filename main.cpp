#include "calculator.h"

#include <QApplication>
#include <QFile>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // 统一加载样式表：所有配色和圆角都写在 style.qss 里，改界面只需要改这一个文件
    QFile styleFile(QStringLiteral(":/style.qss"));
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        app.setStyleSheet(QString::fromUtf8(styleFile.readAll()));
        styleFile.close();
    }

    Calculator window;
    window.show();

    return app.exec();
}
