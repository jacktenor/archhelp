#include "installerworker.h"
#include <QProcess>
#include <QThread>
#include <QFile>

InstallerWorker::InstallerWorker(QObject *parent) : QObject(parent) {}

void InstallerWorker::setDrive(const QString &drive) {
    selectedDrive = drive;
}

void InstallerWorker::run() {
    QProcess process;
    QString bootPart = QString("/dev/%1").arg(selectedDrive + "1");
    QString rootPart = QString("/dev/%1").arg(selectedDrive + "2");

    emit logMessage("🧙 Starting disk preparation in thread...");

    // Unmount
    emit logMessage("Unmounting existing /mnt...");
    process.start("sudo", {"umount", "-l", "/mnt/*"});
    process.waitForFinished();
    process.start("sudo", {"umount", "-l", "/mnt"});
    process.waitForFinished();
    process.start("/bin/bash", {"-c",
                                QString("lsblk -nr -o MOUNTPOINT /dev/%1").arg(selectedDrive)});
    process.waitForFinished();
    QStringList mps = QString(process.readAllStandardOutput()).split('\n', Qt::SkipEmptyParts);
    for (const QString &mp : mps) {
        process.start("sudo", {"umount", "-f", mp.trimmed()});
        process.waitForFinished();
    }

    // Partition
    emit logMessage("Creating new partition table...");
    QStringList cmds = {
        // Legacy BIOS layout: 512MiB boot partition + remainder root
        QString("sudo parted /dev/%1 --script mklabel msdos").arg(selectedDrive),
        QString("sudo parted /dev/%1 --script mkpart primary ext4 1MiB 513MiB").arg(selectedDrive),
        QString("sudo parted /dev/%1 --script set 1 boot on").arg(selectedDrive),
        QString("sudo parted /dev/%1 --script mkpart primary ext4 513MiB 100%").arg(selectedDrive)
    };
    for (const QString &cmd : cmds) {
        process.start("/bin/bash", {"-c", cmd});
        process.waitForFinished(-1);
        QString stdOut = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
        QString errOut = QString::fromUtf8(process.readAllStandardError()).trimmed();
        if (!stdOut.isEmpty())
            emit logMessage(stdOut);
        if (process.exitCode() != 0) {
            emit errorOccurred(QString("Partition error: %1\n%2").arg(cmd, errOut));
            return;
        }
    }

    // Refresh table
    emit logMessage("Refreshing partition table...");
    process.start("/bin/bash", {
        "-c",
        QString("sudo partprobe /dev/%1 && sudo udevadm settle").arg(selectedDrive)
    });
    process.waitForFinished();

    // Format partitions
    emit logMessage("Formatting boot partition " + bootPart + " as ext4...");
    process.start("/bin/bash", {
        "-c", QString("sudo mkfs.ext4 -F %1").arg(bootPart)
    });
    process.waitForFinished();
    if (process.exitCode() != 0) {
        emit errorOccurred("Format failed.");
        return;
    }

    emit logMessage("Formatting partition " + rootPart + " as ext4...");
    process.start("/bin/bash", {
        "-c", QString("sudo mkfs.ext4 -F %1").arg(rootPart)
    });
    process.waitForFinished();
    if (process.exitCode() != 0) {
        emit errorOccurred("Format failed.");
        return;
    }

    // Mount
    emit logMessage("Mounting partitions...");
    process.start("sudo", {"mount", rootPart, "/mnt"});
    process.waitForFinished();
    process.start("sudo", {"mkdir", "-p", "/mnt/boot"});
    process.waitForFinished();


    process.start("sudo", {"mount", bootPart, "/mnt/boot"});
    process.waitForFinished();

    // Copy ISO for later installation if available
    process.start("/bin/bash", {"-c", "if [ -f /tmp/archlinux.iso ]; then sudo cp /tmp/archlinux.iso /mnt/archlinux.iso; fi"});
    process.waitForFinished();

    emit logMessage("✅ Drive is ready.");
    emit installComplete();
}
