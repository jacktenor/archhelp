#ifndef PARTITIONWORKER_H
#define PARTITIONWORKER_H

#include <QObject>
#include <QString>

class PartitionWorker : public QObject {
    Q_OBJECT
public:
    explicit PartitionWorker(QObject *parent = nullptr);
    void setDrive(const QString &drive);

signals:
    void logMessage(const QString &msg);
    void errorOccurred(const QString &msg);
    void finished(const QString &drive);

public slots:
    void run();

private:
    QString drive;
};

#endif // PARTITIONWORKER_H
