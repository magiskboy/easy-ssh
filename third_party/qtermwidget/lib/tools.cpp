#include "tools.h"

#include <QCoreApplication>
#include <QDir>
#include <QtDebug>

Q_LOGGING_CATEGORY(qtermwidgetLogger, "qtermwidget", QtWarningMsg)

/*! Helper function to get possible location of layout files.
By default the KB_LAYOUT_DIR is used (linux/BSD/macports).
But in some cases (apple bundle / portable app layouts) there can be more locations.
*/
QString get_kb_layout_dir()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QLatin1String(KB_LAYOUT_DIR),
        // Easy SSH portable / build-tree layout: <prefix>/bin + <prefix>/share/easy-ssh
        appDir + QLatin1String("/../share/easy-ssh/kb-layouts"),
        appDir + QLatin1String("/kb-layouts"),
#ifdef Q_OS_MAC
        appDir + QLatin1String("/../Resources/kb-layouts"),
#endif
    };

    for (const QString &candidate : candidates) {
        const QString path = QDir::cleanPath(candidate);
        if (QDir(path).exists()) {
            return path + QLatin1Char('/');
        }
    }
    return QString();
}

/*! Helper function to add custom location of color schemes.
 */
namespace
{
QStringList custom_color_schemes_dirs;
}
void add_custom_color_scheme_dir(const QString &custom_dir)
{
    if (!custom_color_schemes_dirs.contains(custom_dir))
        custom_color_schemes_dirs << custom_dir;
}

/*! Helper function to get possible locations of color schemes.
By default the COLORSCHEMES_DIR is used (linux/BSD/macports).
But in some cases (apple bundle / portable app layouts) there can be more locations.
*/
const QStringList get_color_schemes_dirs()
{
    //    qDebug() << __FILE__ << __FUNCTION__;

    QStringList rval;
    QString k(QLatin1String(COLORSCHEMES_DIR));
    QDir d(k);

    //    qDebug() << "default COLORSCHEMES_DIR: " << k;

    if (d.exists())
        rval << k.append(QLatin1Char('/'));

    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList portable = {
        appDir + QLatin1String("/../share/easy-ssh/color-schemes"),
        appDir + QLatin1String("/color-schemes"),
#ifdef Q_OS_MAC
        appDir + QLatin1String("/../Resources/color-schemes"),
#endif
    };
    for (const QString &candidate : portable) {
        const QString path = QDir::cleanPath(candidate);
        d.setPath(path);
        if (d.exists() && !rval.contains(path) && !rval.contains(path + QLatin1Char('/'))) {
            rval << path;
        }
    }

    for (const QString &custom_dir : std::as_const(custom_color_schemes_dirs)) {
        d.setPath(custom_dir);
        if (d.exists())
            rval << custom_dir;
    }
#ifdef QT_DEBUG
    if (!rval.isEmpty()) {
        qDebug() << "Using color-schemes: " << rval;
    } else {
        qDebug() << "Cannot find color-schemes in any location!";
    }
#endif
    return rval;
}
