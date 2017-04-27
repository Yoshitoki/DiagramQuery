#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <QString>
#include <QRegExp>

namespace connections
{
    const QString CONFIGFOLDER = "Connections";
}

namespace queries
{

    const QString TABLES = QString::fromUtf8("Táblák");
    const QString GET_TABLES = "SELECT object_name FROM dba_objects \
    WHERE object_type = 'TABLE' AND object_name NOT LIKE '%$%'";

    const QString INDEXES = QString::fromUtf8("Indexek");
    const QString GET_INDEXES = "SELECT object_name FROM user_objects \
    WHERE object_type = 'INDEX' AND object_name NOT LIKE '%$%'";

    const QString VIEWS = QString::fromUtf8("Nézetek");
    const QString GET_VIEWS = "SELECT object_name FROM user_objects \
    WHERE object_type = 'VIEW' AND object_name NOT LIKE '%$%'";

    const QString FUNCTIONS = QString::fromUtf8("Függvények");
    const QString GET_FUNCTIONS = "SELECT object_name FROM user_objects \
    WHERE object_type = 'FUNCTION' AND object_name NOT LIKE '%$%'";

    const QString SELECT_TABLES = "SELECT column_name, data_type, data_length, \
            data_precision, nullable FROM user_tab_columns\
            WHERE table_name = '%1'";

    const auto SELECT_INDEXES = [](QString name) { return QString("SELECT index_type, table_owner, table_name \
            , uniqueness FROM user_indexes WHERE index_name = '%1'").arg(name); };

    const QString SELECT_VIEWS = "SELECT text, view_type, read_only \
                                    FROM user_views WHERE view_name = '%1'";

    const QString SELECT_FUNCTIONS = "SELECT object_name, object_id, created \
            FROM user_objects WHERE UPPER(OBJECT_TYPE) = 'FUNCTION' AND \
            object_name = '%1'";

}

namespace tokens
{
    const QRegExp EMPTY_LINE = QRegExp("\u2029\\s*\u2029");
}

namespace errors
{
    const QString EMPTY_NAME = QString::fromUtf8(
    "Kérem adjon meg egy nevet, amivel később majd el lehet érni a kapcsolatot!"
    );

    const QString ALREADY_EXISTS = QString::fromUtf8(
    "Már létezik kapcsolat ilyen névvel! Kérem adjon meg más nevet.");

    const QString DIR_NOT_CREATED = QString::fromUtf8(
    "Kérem ellenőrizze jogosultságát a program könyvtárához.");
}

namespace logger
{
    const QString LOG_FOLDER = "Logs";
}

#endif // !CONSTANTS_H
