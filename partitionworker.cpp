#include "partitionworker.h"
#include <QProcess>

PartitionWorker::PartitionWorker(QObject *parent) : QObject(parent) {}

void PartitionWorker::setDrive(const QString &drv) {
    drive = drv;
}

void PartitionWorker::run() {
    QProcess proc;
    emit logMessage(QStringLiteral("Creating default partitions on %1...").arg(drive));

    QString device = QString("/dev/%1").arg(drive);
    QStringList cmds = {
        QString("sudo parted %1 --script mklabel msdos").arg(device),
        QString("sudo parted %1 --script mkpart primary ext4 1MiB 513MiB").arg(device),
        QString("sudo parted %1 --script set 1 boot on").arg(device),
        QString("sudo parted %1 --script mkpart primary ext4 513MiB 100%").arg(device)
    };
    for (const QString &cmd : cmds) {
        proc.start("/bin/bash", {"-c", cmd});
        proc.waitForFinished(-1);
        QString err = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        if (proc.exitCode() != 0) {
            emit errorOccurred(QString("Partition error: %1\n%2").arg(cmd, err));
            return;
        }
    }

    proc.start("/bin/bash", {"-c", QString("sudo partprobe %1 && sudo udevadm settle").arg(device)});
    proc.waitForFinished(-1);

    emit finished(drive);
}
