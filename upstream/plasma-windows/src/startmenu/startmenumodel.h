/*
    SPDX-FileCopyrightText: 2026 Plasma Windows contributors
    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <QAbstractListModel>
#include <QJsonArray>
#include <QStringList>
#include <QVector>

#include <QtQml/qqml.h>

class StartMenuModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString directory READ directory WRITE setDirectory NOTIFY directoryChanged)
    Q_PROPERTY(QString directoryName READ directoryName NOTIFY directoryChanged)
    Q_PROPERTY(bool isRoot READ isRoot NOTIFY directoryChanged)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    Q_PROPERTY(int iconPixels READ iconPixels NOTIFY appearanceChanged)
    Q_PROPERTY(int columns READ columns NOTIFY appearanceChanged)
    Q_PROPERTY(QString rootPath READ rootPath CONSTANT)
    Q_PROPERTY(QVariantList categories READ categories NOTIFY directoryChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        IconRole,
        ExecRole,
        IsDirRole,
        PathRole,
        LinkPathRole,
    };

    explicit StartMenuModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString directory() const;
    QString directoryName() const;
    void setDirectory(const QString& path);
    bool isRoot() const;
    QString rootPath() const;
    QVariantList categories() const;
    QString searchText() const { return m_searchText; }
    void setSearchText(const QString& text);
    int iconPixels() const { return m_iconPixels; }
    int columns() const { return m_columns; }

    Q_INVOKABLE void goParent();
    Q_INVOKABLE void launch(int row) const;
    Q_INVOKABLE void reload();
    Q_INVOKABLE void reveal(int row) const;
    Q_INVOKABLE bool activate(int row);

Q_SIGNALS:
    void directoryChanged();
    void searchTextChanged();
    void appearanceChanged();

private:
    void scan();
    void scanUwpApps();
    void loadCategories();
    QString resolveLnk(const QString& lnkPath, QString* targetOut) const;
    void launchUwp(const QString& aumid) const;

    struct Entry {
        QString name;
        QString exec;
        bool isDir = false;
        QString path;
        QString linkPath;
        QString icon;
    };

    QString m_directory;
    bool m_isRoot = true;
    QVector<Entry> m_entries;
    QVector<Entry> m_allEntries, m_uwpEntries;
    QString m_searchText;
    int m_iconPixels = 40, m_columns = 3;
    QStringList m_stack;
    QStringList m_categoryDirs; // active custom-category directories ("cat:" view)
    QJsonArray m_categories;
    bool m_uwpLoaded = false;
};
