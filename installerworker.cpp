#include "installerworker.h"
#include <QProcess>
#include <QThread>
#include <QFile>
#include <QStandardPaths>
#include <QFileInfo>

static QString locatePartedBinary()
{
    QString p = QStandardPaths::findExecutable("parted");
    if (!p.isEmpty())
        return p;
    const QStringList fallbacks{"/usr/sbin/parted", "/sbin/parted"};
    for (const QString &path : fallbacks)
        if (QFileInfo::exists(path))
            return path;
    return QString();
}

InstallerWorker::InstallerWorker(QObject *parent) : QObject(parent) {}

void InstallerWorker::setDrive(const QString &drive) {
    selectedDrive = drive;
}

void InstallerWorker::setMode(InstallMode m) { mode = m; }

void InstallerWorker::setTargetPartition(const QString &part) {
    targetPartition = part;
}


void InstallerWorker::run() {
    QProcess process;
    QString suffix = (selectedDrive.startsWith("nvme") || selectedDrive.startsWith("mmc")) ? "p" : "";
    QString bootPart = QString("/dev/%1%2%3").arg(selectedDrive, suffix, "1");
    QString rootPart = QString("/dev/%1%2%3").arg(selectedDrive, suffix, "2");

    emit logMessage("🧙 Starting disk preparation in thread...");

    // Unmount any existing mounts
    emit logMessage("Unmounting existing /mnt...");
    process.start("sudo", {"umount", "-l", "/mnt/*"});
    process.waitForFinished();
    process.start("sudo", {"umount", "-l", "/mnt"});
    process.waitForFinished();

    QString queryTarget;
    if (mode == InstallMode::UsePartition)
        queryTarget = targetPartition;
    else
        queryTarget = QString("/dev/%1").arg(selectedDrive);

    process.start("/bin/bash", {"-c", QString("lsblk -nr -o MOUNTPOINT %1").arg(queryTarget)});
    process.waitForFinished();
    QStringList mps = QString(process.readAllStandardOutput()).split('\n', Qt::SkipEmptyParts);
    for (const QString &mp : mps) {
        process.start("sudo", {"umount", "-f", mp.trimmed()});
        process.waitForFinished();
    }

    QString partedBin;

    if (mode == InstallMode::WipeDrive) {
        partedBin = locatePartedBinary();
        if (partedBin.isEmpty()) {
            emit errorOccurred("parted not found");
            return;
        }
        emit logMessage("Creating new partition table...");
        QStringList args{partedBin, QString("/dev/%1").arg(selectedDrive), "--script",
                         "mklabel", "msdos",
                         "mkpart", "primary", "ext4", "1MiB", "513MiB",
                         "set", "1", "boot", "on",
                         "mkpart", "primary", "ext4", "513MiB", "100%"};
        if (QProcess::execute("sudo", args) != 0) {
            emit errorOccurred("Partition command failed");
            return;
        }

        emit logMessage("Refreshing partition table...");
        QProcess::execute("sudo", {"partprobe", QString("/dev/%1").arg(selectedDrive)});
        QProcess::execute("sudo", {"udevadm", "settle"});

        emit logMessage("Formatting boot partition " + bootPart + " as ext4...");
        if (QProcess::execute("sudo", {"mkfs.ext4", "-F", bootPart}) != 0) {
            emit errorOccurred("Format failed.");
            return;
        }

        emit logMessage("Formatting partition " + rootPart + " as ext4...");
        if (QProcess::execute("sudo", {"mkfs.ext4", "-F", rootPart}) != 0) {
            emit errorOccurred("Format failed.");
            return;
        }

        emit logMessage("Mounting partitions...");
        process.start("sudo", {"mount", rootPart, "/mnt"});
        process.waitForFinished();
        process.start("sudo", {"mkdir", "-p", "/mnt/boot"});
        process.waitForFinished();
        process.start("sudo", {"mount", bootPart, "/mnt/boot"});
        process.waitForFinished();
    } else if (mode == InstallMode::UsePartition) {
        rootPart = targetPartition;
        emit logMessage("Formatting target partition " + rootPart + " as ext4...");
        if (QProcess::execute("sudo", {"mkfs.ext4", "-F", rootPart}) != 0) {
            emit errorOccurred("Format failed.");
            return;
        }
        emit logMessage("Mounting partition...");
        process.start("sudo", {"mount", rootPart, "/mnt"});
        process.waitForFinished();
    } else if (mode == InstallMode::UseFreeSpace) {
        partedBin = locatePartedBinary();
        if (partedBin.isEmpty()) {
            emit errorOccurred("parted not found");
            return;
        }
        emit logMessage("Searching for free space...");
        process.start("sudo", {partedBin, QString("/dev/%1").arg(selectedDrive), "-m", "unit", "MiB", "print", "free"});
        process.waitForFinished();
        QString out = process.readAllStandardOutput();
        QStringList lines = out.split('\n', Qt::SkipEmptyParts);
        double bestSize = 0.0; QString bestStart, bestEnd;
        for (const QString &l : lines) {
            if (!l.contains("free")) continue;
            QStringList cols = l.split(':');
            if (cols.size() >= 4) {
                QString sizeStr = cols.at(3);
                sizeStr.remove("MiB");
                double sz = sizeStr.toDouble();
                if (sz > bestSize) { bestSize = sz; bestStart = cols.at(1); bestEnd = cols.at(2); }
            }
        }
        if (bestSize <= 0.0) {
            emit errorOccurred("No free space found");
            return;
        }
        QStringList args{partedBin, QString("/dev/%1").arg(selectedDrive), "--script",
                         "mkpart", "primary", "ext4", bestStart, bestEnd};
        if (QProcess::execute("sudo", args) != 0) {
            emit errorOccurred("Failed to create partition in free space");
            return;
        }
        emit logMessage("Refreshing partition table...");
        QProcess::execute("sudo", {"partprobe", QString("/dev/%1").arg(selectedDrive)});
        QProcess::execute("sudo", {"udevadm", "settle"});

        process.start("/bin/bash", {"-c", QString("lsblk -nr -o NAME,TYPE /dev/%1 | awk '$2==\"part\"{print $1}' | tail -n 1").arg(selectedDrive)});
        process.waitForFinished();
        rootPart = "/dev/" + QString(process.readAllStandardOutput()).trimmed();
        emit logMessage("Formatting partition " + rootPart + " as ext4...");
        if (QProcess::execute("sudo", {"mkfs.ext4", "-F", rootPart}) != 0) {
            emit errorOccurred("Format failed.");
            return;
        }
        emit logMessage("Mounting partition...");
        process.start("sudo", {"mount", rootPart, "/mnt"});
        process.waitForFinished();
    }

    process.start("/bin/bash", {"-c", "if [ -f /tmp/archlinux.iso ]; then sudo cp /tmp/archlinux.iso /mnt/archlinux.iso; fi"});
    process.waitForFinished();

    emit logMessage("✅ Drive is ready.");
    emit installComplete();
}
