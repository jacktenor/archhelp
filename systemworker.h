#ifndef SYSTEMWORKER_H
#define SYSTEMWORKER_H

#include <QObject>
#include <QString>
#include <QStringList>

class SystemWorker : public QObject {
    Q_OBJECT
public:
    explicit SystemWorker(QObject *parent = nullptr);

    void setParameters(const QString &drive,
                       const QString &username,
                       const QString &password,
                       const QString &rootPassword,
                       const QString &desktopEnv);

signals:
    void logMessage(const QString &msg);
    void errorOccurred(const QString &msg);
    void finished();

public slots:
    void run();

private:
    QString drive;
    QString username;
    QString password;
    QString rootPassword;
    QString desktopEnv;

    bool runCommand(const QString &cmd);
};

#endif // SYSTEMWORKER_H
