#pragma once

#include <QObject>

namespace app {

/// 加载文件列表完成的信号。
class LoadListSignals : public QObject {
    Q_OBJECT
public:
    explicit LoadListSignals(QObject* parent = nullptr) : QObject(parent) {}

signals:
    void finished(const QList<QMap<QString, QVariant>>& file_items, const QString& error);
};

/// 操作完成的信号（创建、删除、重命名等）。
class OpFinishedSignals : public QObject {
    Q_OBJECT
public:
    explicit OpFinishedSignals(QObject* parent = nullptr) : QObject(parent) {}

signals:
    void finished(bool success, const QString& name1, const QString& name2,
                  const QString& error,
                  const QList<QMap<QString, QVariant>>& file_items,
                  const QList<QMap<QString, QVariant>>& folder_items);
};

/// 存储统计完成的信号。
class StorageSignals : public QObject {
    Q_OBJECT
public:
    explicit StorageSignals(QObject* parent = nullptr) : QObject(parent) {}

signals:
    void finished(const QString& used_text);
};

}  // namespace app
