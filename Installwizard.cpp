#include "Installwizard.h"
#include "installerworker.h"
#include "systemworker.h"
#include "ui_Installwizard.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QProcess>
#include <QNetworkAccessManager>
#include <QRegularExpression>
#include <QNetworkReply>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <unistd.h>
#include <QStandardPaths>
#include <QThread>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <algorithm>

Installwizard::Installwizard(QWidget *parent) :
    QWizard(parent),
    ui(new Ui::Installwizard) {
    ui->setupUi(this);
    setWindowTitle("Arch Linux Installer");

    // Initially disable navigation buttons until each page completes its work
    //setWizardButtonEnabled(QWizard::NextButton, false);
    setWizardButtonEnabled(QWizard::FinishButton, false);

    // Connect refreshButton to populate drives
    connect(ui->partRefreshButton, &QPushButton::clicked, this, &Installwizard::populateDrives);

    // Connect prepareButton to handle drive preparation
    connect(ui->prepareButton, &QPushButton::clicked, this, [=]() {
        QString selectedDrive = ui->driveDropdown->currentText();
        if (selectedDrive.isEmpty() || selectedDrive == "No drives found") {
            QMessageBox::warning(this, "Error", "Please select a valid drive.");
            return;
        }

        QMessageBox::StandardButton confirm = QMessageBox::question(
            this,
            tr("Confirm Drive"),
            tr("You are about to destroy all data on %1!!! Are you absolutely "
               "sure this is correct?")
                .arg(selectedDrive),
            QMessageBox::Yes | QMessageBox::No);

        if (confirm != QMessageBox::Yes)
            return;

        efiInstall = false; // legacy mode

        // Disable advance until drive prep is done
     setWizardButtonEnabled(QWizard::NextButton, true);

        // Remove "/dev/" prefix for internal processing
        prepareDrive(selectedDrive.mid(5));
    });

    // Populate drives when the wizard starts
    populateDrives();

    // Inside Installwizard constructor
    connect(ui->downloadButton, &QPushButton::clicked, this, [=]() {
        setWizardButtonEnabled(QWizard::NextButton, true);

        downloadISO(ui->progressBar);  // Pass the progress bar to show download progress
    });

    connect(ui->installButton, &QPushButton::clicked,
            this, &Installwizard::on_installButton_clicked);

    connect(this, &QWizard::currentIdChanged, this, [this](int id) {
        if (id == 0) {
            setWizardButtonEnabled(QWizard::NextButton, true);
        } else if (id == 1) {
            setWizardButtonEnabled(QWizard::NextButton, false);

            QString drive = ui->driveDropdown->currentText().mid(5);
            if (!drive.isEmpty())
                populatePartitionTable(drive);
        } else if (id == 2) {
            setWizardButtonEnabled(QWizard::FinishButton, false);

            if (ui->comboDesktopEnvironment->count() == 0) {
                ui->comboDesktopEnvironment->addItems({
                    "GNOME", "KDE Plasma", "XFCE", "LXQt", "Cinnamon", "MATE", "i3"
                });
            }
        }
    });

    connect(ui->partRefreshButton, &QPushButton::clicked, this, [this]() {
        QString drive = ui->driveDropdown->currentText().mid(5);
        populatePartitionTable(drive);
    });

    connect(ui->createPartButton, &QPushButton::clicked, this, [this]() {
        QString drive = ui->driveDropdown->currentText().mid(5);
        if (!drive.isEmpty()) {
            setWizardButtonEnabled(QWizard::NextButton, false);
            prepareForEfi(drive);
        }
    });

    connect(ui->driveDropdown, &QComboBox::currentTextChanged, this,
            [this](const QString &text) {
                if (currentId() == 1 && !text.isEmpty() && text != "No drives found")
                    populatePartitionTable(text.mid(5));
            });
}

QString Installwizard::getUserHome() {
    QString userHome;

    // Use HOME env variable if not root
    if (getuid() != 0) {
        userHome = QDir::homePath();
    } else {
        QByteArray userEnv = qgetenv("SUDO_USER");
        if (!userEnv.isEmpty()) {
            QString sudoUser = QString(userEnv);
            QProcess proc;
            proc.start("getent", QStringList() << "passwd" << sudoUser);
            proc.waitForFinished();
            QString output = proc.readAllStandardOutput();
            QStringList fields = output.split(':');
            if (fields.size() >= 6)
                userHome = fields[5]; // Home directory from /etc/passwd
        }
    }

    // Fallback
    if (userHome.isEmpty())
        userHome = QDir::homePath();

    return userHome;
}

void Installwizard::downloadISO(QProgressBar *progressBar) {
    networkManager = new QNetworkAccessManager(this);
    QUrl url("https://mirror.arizona.edu/archlinux/iso/latest/archlinux-x86_64.iso");
    QNetworkRequest request(url);
    QNetworkReply *reply = networkManager->get(request);

    QString finalIsoPath = QDir::tempPath() + "/archlinux.iso";
    QFile *file = new QFile(finalIsoPath);

    if (!file->open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, "Error", "Unable to open file for writing: " + finalIsoPath);
        reply->abort();
        reply->deleteLater();
        delete file;
        return;
    }
    appendLog("Downloading ISO...");

    connect(reply, &QNetworkReply::downloadProgress, this, [progressBar](qint64 bytesReceived, qint64 bytesTotal) {
        if (bytesTotal > 0) {
            progressBar->setValue(static_cast<int>((bytesReceived * 100) / bytesTotal));
        }
    });

    connect(reply, &QNetworkReply::readyRead, this, [file, reply]() {
        if (file->isOpen()) {
            file->write(reply->readAll());
        }
    });

    connect(reply, &QNetworkReply::finished, this, [this, file, reply, finalIsoPath]() {
        file->close();

        if (reply->error() == QNetworkReply::NoError) {
            // Set file permissions: readable by everyone
            QFile::setPermissions(finalIsoPath, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);

            QMessageBox::information(this, "Success", "Arch Linux ISO downloaded successfully\nto: " + finalIsoPath + " \nNext is Installing dependencies and extracting ISO...");
            installDependencies();

        } else {
            QFile::remove(finalIsoPath);
            QMessageBox::critical(this, "Error", "Failed to download ISO: " + reply->errorString());
        }

        reply->deleteLater();
        file->deleteLater();
    });

    connect(reply, &QNetworkReply::errorOccurred, this, [this, file, reply, finalIsoPath](QNetworkReply::NetworkError) {
        QFile::remove(finalIsoPath);
        QMessageBox::critical(this, "Error", "Network error while downloading ISO: " + reply->errorString());

        reply->deleteLater();
        file->deleteLater();
    });
}

Installwizard::~Installwizard() {
    delete ui;
}

void Installwizard::setWizardButtonEnabled(QWizard::WizardButton which, bool enabled) {
    if (QAbstractButton *btn = button(which))
        btn->setEnabled(enabled);
}

void Installwizard::installDependencies() {


    QProcess process;
    QStringList packages = {
        "arch-install-scripts",  // includes arch-chroot, pacstrap
        "parted",
        "dosfstools",            // mkfs.vfat
        "e2fsprogs",             // mkfs.ext4
        "squashfs-tools",
        "os-prober",
        "wget"     // for downloading bootstrap if needed
    };

    // Detect distribution by reading /etc/os-release
    QString distro;
    QFile osRelease("/etc/os-release");
    if (osRelease.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&osRelease);
        while (!in.atEnd()) {
            QString line = in.readLine();
            if (line.startsWith("ID=")) {
                distro = line.section('=', 1).remove('"').trimmed();
                break;
            }
        }
    }

    QString installCmd;
    if (distro == "fedora") {
        installCmd = "pkexec dnf install -y " + packages.join(" ");
    } else if (distro == "arch" || distro == "archlinux") {
        installCmd = "pkexec pacman -S --noconfirm " + packages.join(" ");
    } else {
        installCmd = "pkexec apt install -y " + packages.join(" ");
    }

    qDebug() << "Installing dependencies:" << installCmd;
    appendLog("Installing dependencies:...");

    process.start("/bin/bash", QStringList() << "-c" << installCmd);
    process.waitForFinished(-1);

    QString output = process.readAllStandardOutput();
    QString error = process.readAllStandardError();

    qDebug() << "Dependency Install Output:" << output;
    qDebug() << "Dependency Install Errors:" << error;

    if (process.exitCode() != 0) {
        QMessageBox::critical(this, "Error", "Failed to install required dependencies:\n" + error);
        return;
    }

    appendLog("Dependencies installed, click next to proceed.");

    // Allow user to advance to partitioning page
    setWizardButtonEnabled(QWizard::NextButton, true);

    getAvailableDrives();
}

QStringList Installwizard::getAvailableDrives() {
    QProcess process;

    // Use lsblk from PATH for better portability
    process.start("lsblk", QStringList() << "-o" << "NAME,SIZE,TYPE" << "-d" << "-n");
    process.waitForFinished();

    QString output = process.readAllStandardOutput();

    QStringList drives;

    // Split output into lines and process each line
    for (const QString &line : output.split('\n', Qt::SkipEmptyParts)) {
        QStringList  tokens = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);  // Split by whitespace

        if (tokens.size() >= 3 && tokens[2] == "disk") {  // Ensure it’s a disk
            QString deviceName = tokens[0];
            if (!deviceName.startsWith("loop")) {  // Skip loop devices
                drives << deviceName;  // Add the drive name (e.g., "sdb")
            }
        }
    }

    return drives;
}

void Installwizard::populateDrives() {
    ui->driveDropdown->clear();  // Clear existing items
    QStringList drives = getAvailableDrives();  // Get available drives

    if (drives.isEmpty()) {
        ui->driveDropdown->addItem("No drives found");
    } else {
        for (const QString &drive : std::as_const(drives)) {
            ui->driveDropdown->addItem(QString("/dev/%1").arg(drive));  // Add "/dev/" prefix
        }
    }

    qDebug() << "Drives added to ComboBox:" << drives;  // Debug: Confirm drives in ComboBox
}

void Installwizard::forceUnmount(const QString &mountPoint) {
    QProcess process;

    process.start("/bin/bash", QStringList() << "-c" << " sudo umount -Rfl /mnt");

    process.waitForFinished();
    if (process.exitCode() == 0) {
        qDebug() << "Recursive unmount succeeded.";
    } else {
        qDebug() << "Recursive unmount failed.";
        qDebug() << "stderr:" << process.readAllStandardError();
    }

    // Find and kill processes using the mount point
    process.start("sudo", QStringList{"fuser", "-vk", mountPoint});
    process.waitForFinished();
    qDebug() << "Killed processes using" << mountPoint;

    // Try unmounting normally
    process.start("sudo", QStringList{"umount", mountPoint});
    process.waitForFinished();
    if (process.exitCode() == 0) {
        qDebug() << "Unmounted successfully: " << mountPoint;
        return;
    }

    // If normal unmount failed, try lazy unmount
    process.start("sudo", QStringList{"umount", "-l", mountPoint});
    process.waitForFinished();
    if (process.exitCode() == 0) {
        qDebug() << "Lazy unmounted: " << mountPoint;
        return;
    }

    // If still fails, force unmount
    process.start("sudo", QStringList{"umount", "-f", mountPoint});
    process.waitForFinished();
    if (process.exitCode() != 0) {
        //  QMessageBox::critical(nullptr, "Error", "Failed to unmount " + mountPoint);
    } else {
        qDebug() << "Force unmounted: " << mountPoint;
    }
}

void Installwizard::unmountDrive(const QString &drive) {
    QProcess process;
    process.start("lsblk",
                  QStringList() << "-nr" << "-o" << "MOUNTPOINT" << QString("/dev/%1").arg(drive));
    process.waitForFinished();
    QStringList points = QString(process.readAllStandardOutput()).split('\n', Qt::SkipEmptyParts);
    for (const QString &pt : points) {
        QString trimmed = pt.trimmed();
        if (!trimmed.isEmpty() && trimmed != "[SWAP]") {
            QProcess::execute("sudo", {"umount", "-f", trimmed});
        }
    }
}

void Installwizard::appendLog(const QString &message) {
    if (ui->logWidget3)
        ui->logWidget3->appendPlainText(message);
    if (ui->logView1)
        ui->logView1->appendPlainText(message);
    if (ui->logView2)
        ui->logView2->appendPlainText(message);
}

void Installwizard::prepareDrive(const QString &drive) {
    selectedDrive = drive;

    InstallerWorker *worker = new InstallerWorker;
    worker->setDrive(drive);

    QThread *thread = new QThread;
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, &InstallerWorker::run);
    connect(worker, &InstallerWorker::logMessage, this, [this](const QString &msg) { appendLog(msg); });
    connect(worker, &InstallerWorker::errorOccurred, this, [this](const QString &msg) {
        QMessageBox::critical(this, "Error", msg);
    });
  
    connect(worker, &InstallerWorker::installComplete, thread, &QThread::quit);
    connect(worker, &InstallerWorker::installComplete, this, [this]() {
        appendLog("\xE2\x9C\x85 Drive preparation complete.");
        setWizardButtonEnabled(QWizard::NextButton, true);

    });

    connect(worker, &InstallerWorker::installComplete, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    thread->start();
}

void Installwizard::populatePartitionTable(const QString &drive) {
    if (drive.isEmpty())
        return;

    QProcess process;
    QString device = QString("/dev/%1").arg(drive);
    process.start("lsblk",
                  QStringList() << "-r" << "-n" << "-o" <<
                                "NAME,SIZE,TYPE,MOUNTPOINT" << device);
    process.waitForFinished();
    QString output = process.readAllStandardOutput();

    ui->treePartitions->clear();
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines.mid(1)) { // skip header
        QStringList cols = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (cols.size() >= 4) {
            QTreeWidgetItem *item = new QTreeWidgetItem(ui->treePartitions);
            item->setText(0, cols.at(0));
            item->setText(1, cols.at(1));
            item->setText(2, cols.at(2));
            item->setText(3, cols.at(3));
        }
    }
}

void Installwizard::prepareForEfi(const QString &drive) {
    efiInstall = true; // remember choice for grub
    unmountDrive(drive);

    QString device = QString("/dev/%1").arg(drive);

    QString partedBin = QStandardPaths::findExecutable("parted");
    if (partedBin.isEmpty()) {
        QMessageBox::critical(this, "Partition Error", "parted not found in PATH");
        return;
    }

    // Create partitions similar to legacy routine but using GPT and FAT32 ESP
    QStringList cmd1{partedBin, device, "--script", "mklabel", "gpt"};
    QStringList cmd2{partedBin, device, "--script", "mkpart", "primary", "fat32", "1MiB", "513MiB"};
    QStringList cmd3{partedBin, device, "--script", "name", "1", "ESP"};
    QStringList cmd4{partedBin, device, "--script", "set", "1", "esp", "on"};
    QStringList cmd5{partedBin, device, "--script", "mkpart", "primary", "ext4", "513MiB", "100%"};

    for (const QStringList &args : {cmd1, cmd2, cmd3, cmd4, cmd5}) {
        if (QProcess::execute("sudo", args) != 0) {

        return;
    }

    // Ensure the disk uses GPT so the ESP can be named
    QProcess p;
    p.start("lsblk", {"-no", "PTTYPE", device});
    p.waitForFinished();
    QString type = QString(p.readAllStandardOutput()).trimmed();
    if (type != "gpt") {
        int ret = QProcess::execute("sudo", {partedBin, device, "--script", "mklabel", "gpt"});
        if (ret != 0) {
            QMessageBox::critical(this, "Partition Error", tr("Failed to create GPT label."));

    if (type != "gpt") {
        int ret = QProcess::execute("sudo", {partedBin, device, "--script", "mklabel", "gpt"});
        if (ret != 0) {
            QMessageBox::critical(this, "Partition Error", tr("Failed to create GPT label."));

    process.start("lsblk", QStringList() << "-no" << "PTTYPE" << device);
    process.waitForFinished();
    QString type = QString(process.readAllStandardOutput()).trimmed();
    if (type != "gpt") {
        process.start("/bin/bash", QStringList()
                                       << "-c"
                                       << QString("sudo %1 %2 --script mklabel gpt")
                                              .arg(partedBin, device));
        process.waitForFinished();
        if (process.exitCode() != 0) {
            QMessageBox::critical(this, "Partition Error",
                                  tr("Failed to create GPT label:\n%1")
                                      .arg(QString(process.readAllStandardError())));

            return;
        }
    }

    // Determine next free region using parted
    p.start(partedBin, {"-sm", device, "unit", "MiB", "print", "free"});
    p.waitForFinished();
    QString out = p.readAllStandardOutput();
    process.start(partedBin, QStringList() << "-sm" << device << "unit" << "MiB" << "print" << "free");

    process.start("parted", QStringList() << "-sm" << device << "unit" << "MiB" << "print" << "free");
✅ Partitions ready for EFI install.    process.waitForFinished();
    QString out = process.readAllStandardOutput();


    double freeStart = -1;
    int maxPart = 0;
    for (const QString &line : out.split('\n', Qt::SkipEmptyParts)) {
        QString trimmed = line.trimmed();
        QStringList cols = trimmed.split(':');
        if (cols.size() < 2)
            continue;

        bool ok = false;
        int num = cols[0].toInt(&ok);
        if (ok && !trimmed.contains("free", Qt::CaseInsensitive))
            maxPart = std::max(maxPart, num);

        if (trimmed.contains("free", Qt::CaseInsensitive) && cols.size() >= 3) {
            double start = cols[1].remove(QRegularExpression("[^0-9.]")).toDouble();
            double end = cols[2].remove(QRegularExpression("[^0-9.]")).toDouble();
            if (end - start > 1000) { // choose space >1GiB
                freeStart = start;
                break;
            }
        }
    }

    if (freeStart < 0) {
        QMessageBox::critical(this, "Partition Error", "No suitable free space found.");
        return;
    }

    double bootStart = freeStart + 1;  // align
    double bootEnd = bootStart + 512;  // 512MiB ESP
    double rootStart = bootEnd;

    QStringList cmd1{partedBin, device, "--script", "mkpart", "primary", "fat32", QString("%1MiB").arg(bootStart), QString("%1MiB").arg(bootEnd)};
    QStringList cmd2{partedBin, device, "--script", "name", QString::number(maxPart + 1), "ESP"};
    QStringList cmd3{partedBin, device, "--script", "set", QString::number(maxPart + 1), "esp", "on"};
    QStringList cmd4{partedBin, device, "--script", "mkpart", "primary", "ext4", QString("%1MiB").arg(rootStart), "100%"};

    QStringList cmds = {
        QString("sudo %1 %2 --script mkpart primary fat32 %3MiB %4MiB")
            .arg(partedBin, device)
            .arg(bootStart)
            .arg(bootEnd),
        QString("sudo %1 %2 --script name %3 ESP").arg(partedBin, device).arg(maxPart + 1),
        QString("sudo %1 %2 --script set %3 esp on").arg(partedBin, device).arg(maxPart + 1),
        QString("sudo %1 %2 --script mkpart primary ext4 %3MiB 100%")
            .arg(partedBin, device)
            .arg(rootStart)
    };

    for (const QStringList &args : {cmd1, cmd2, cmd3, cmd4}) {
        int ret = QProcess::execute("sudo", args);
        if (ret != 0) {
            QMessageBox::critical(this, "Partition Error", tr("Failed to run parted."));
            return;
        }
    }

    QProcess::execute("sudo", {"partprobe", device});
    QProcess::execute("sudo", {"udevadm", "settle"});

    populatePartitionTable(drive);
    appendLog("\xE2\x9C\x85 Partitions ready for EFI install.");
    setWizardButtonEnabled(QWizard::NextButton, true);
}

void Installwizard::mountPartitions(const QString &drive) {
    QProcess process;
    QString suffix = (drive.startsWith("nvme") || drive.startsWith("mmc")) ? "p" : "";
    QString bootPart = QString("/dev/%1%2%3").arg(drive, suffix, "1");
    QString rootPart = QString("/dev/%1%2%3").arg(drive, suffix, "2");

    // 1. Mount root
    process.start("/bin/bash", { "-c",
                                QString("sudo mount %1 /mnt").arg(rootPart) });
    process.waitForFinished(-1);

    // 2. Ensure /mnt/boot exists and mount boot
    process.start("/bin/bash", { "-c",
                                "sudo mkdir -p /mnt/boot" });
    process.waitForFinished(-1);
    // 3. Copy ISO for later installation

    process.start("/bin/bash", { "-c",
                                QString("sudo mount %1 /mnt/boot").arg(bootPart) });
    process.waitForFinished(-1);


    process.start("/bin/bash", { "-c",
                                "sudo cp /tmp/archlinux.iso /mnt/archlinux.iso" });
    process.waitForFinished(-1);
}

void Installwizard::mountISO() {
    QProcess process;

    QString isoPath = "/mnt/archlinux.iso";

    // ✅ Check if ISO exists before mounting. If not, try to copy from /tmp
    if (!QFile::exists(isoPath)) {
        QString tmpIso = QDir::tempPath() + "/archlinux.iso";
        if (QFile::exists(tmpIso)) {
            if (QProcess::execute("sudo", {"cp", tmpIso, isoPath}) != 0) {
                QMessageBox::critical(nullptr, "Error",
                                      "Failed to copy ISO from " + tmpIso +
                                          " to: " + isoPath);
                return;
            }
        }
    }

    if (!QFile::exists(isoPath)) {
        QMessageBox::critical(nullptr, "Error", "Arch Linux ISO not found at: " + isoPath);
        return;
    }

    // ✅ Ensure mountpoint directories exist
    QDir().mkdir("/mnt/archiso");
    QDir().mkdir("/mnt/rootfs");  // Writable directory for extraction

    // ✅ Step 1: Mount the ISO
    QString mountCommand = QString("sudo mount -o loop %1 /mnt/archiso").arg(isoPath);
    qDebug() << "Executing Mount Command:" << mountCommand;
    process.start("/bin/bash", QStringList() << "-c" << mountCommand);
    process.waitForFinished();

    QString output = process.readAllStandardOutput();
    QString errors = process.readAllStandardError();
    qDebug() << "Mount Command Output:" << output;
    qDebug() << "Mount Command Errors:" << errors;

    if (process.exitCode() != 0) {
        QMessageBox::critical(nullptr, "Error", QString("Failed to mount Arch ISO:\n%1").arg(errors));
        return;
    }

    QMessageBox::information(nullptr, "Success", "Arch Linux ISO mounted successfully,\nand dependencies installed\nWill start extracting...");

    // ✅ Step 2: Extract airootfs.sfs to /mnt/rootfs
    QString squashfsPath = "/mnt/archiso/arch/x86_64/airootfs.sfs";  // Adjust if necessary

    QString extractCommand = QString("sudo unsquashfs -f -d /mnt %1").arg(squashfsPath);

    qDebug() << "Executing Extract Command:" << extractCommand;
    process.start("/bin/bash", QStringList() << "-c" << extractCommand);
    process.waitForFinished();

    output = process.readAllStandardOutput();
    errors = process.readAllStandardError();
    qDebug() << "Extract Command Output:" << output;
    qDebug() << "Extract Command Errors:" << errors;

    if (process.exitCode() != 0) {
        QMessageBox::critical(nullptr, "Error", QString("Failed to extract Arch Linux root filesystem:\n%1").arg(errors));
        return;
    }

    QMessageBox::information(
        nullptr,
        "Success",
        "Arch Linux root filesystem extracted successfully!\nNext we Install keys and base system.\nThis could take a few...");


    process.start("/bin/bash", QStringList() << "-c" << " sudo umount -Rfl /mnt/archiso");


    installArchBase(selectedDrive);
}

void Installwizard::installArchBase(const QString &selectedDrive) {

    QProcess process;

    // Ensure /etc/resolv.conf exists in chroot
    QProcess::execute("sudo", {"rm", "-f", "/mnt/etc/resolv.conf"});
    QProcess::execute("sudo", {"cp", "/etc/resolv.conf", "/mnt/etc/resolv.conf"});

    // Step 2: Check & extract bootstrap if needed
    if (!QFile::exists("/mnt/usr/bin/pacman")) {
        QString bootstrapUrl = "https://mirrors.edge.kernel.org/archlinux/iso/latest/archlinux-bootstrap-x86_64.tar.gz";
        int dlRet = QProcess::execute("sudo", {"wget", "-O", "/tmp/arch-bootstrap.tar.gz", bootstrapUrl});
        if (dlRet != 0) {
            QMessageBox::critical(this, "Error", "Failed to download bootstrap.");
            return;
        }
        int extractRet = QProcess::execute("sudo", {"tar", "-xzf", "/tmp/arch-bootstrap.tar.gz", "-C", "/mnt", "--strip-components=1"});
        if (extractRet != 0) {
            QMessageBox::critical(this, "Error", "Failed to extract bootstrap.");
            return;
        }
    }


    // DNS check
    QProcess dnsTest;
    dnsTest.start("sudo", {"arch-chroot", "/mnt", "ping", "-c", "1", "archlinux.org"});
    dnsTest.waitForFinished();

    // Initialize pacman
    QProcess::execute("sudo", {"arch-chroot", "/mnt", "pacman-key", "--init"});
    QProcess::execute("sudo", {"arch-chroot", "/mnt", "pacman-key", "--populate", "archlinux"});
    QProcess::execute("sudo", {"arch-chroot", "/mnt", "pacman", "-Sy", "--noconfirm", "archlinux-keyring"});

    // Install base, kernel, firmware
    appendLog("Installing base, linux, linux-firmware…");

    QProcess baseProc;
    baseProc.setProcessChannelMode(QProcess::MergedChannels);
    baseProc.start("sudo",
                   {"arch-chroot", "/mnt", "pacman", "-Sy", "--noconfirm", "base", "linux",
                    "linux-firmware", "--needed"});
    baseProc.waitForFinished(-1);

    QString baseOut = QString::fromUtf8(baseProc.readAll());
    if (!baseOut.trimmed().isEmpty()) {
        appendLog(baseOut);
    }

    if (baseProc.exitCode() != 0) {
        QMessageBox::critical(this, "Error",
                              QString("Failed to install base system.\n%1")
                                  .arg(QString(baseOut).trimmed()));
        return;
    }

    // Write corrected linux.preset
    QString presetContent =
        "[mkinitcpio preset file for the 'linux' package]\n"
        "ALL_config=\"/etc/mkinitcpio.conf\"\n"
        "ALL_kver=\"/boot/vmlinuz-linux\"\n"
        "\n"
        "PRESETS=(\n"
        "  default\n"
        "  fallback\n"
        ")\n"
        "\n"
        "default_image=\"/boot/initramfs-linux.img\"\n"
        "fallback_image=\"/boot/initramfs-linux-fallback.img\"\n"
        "fallback_options=\"-S autodetect\"\n";

    QString tempPresetPath = "/tmp/linux.preset";
    QFile presetFile(tempPresetPath);
    if (presetFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        presetFile.write(presetContent.toUtf8());
        presetFile.close();
    }
    QProcess::execute("sudo", {"cp", tempPresetPath, "/mnt/etc/mkinitcpio.d/linux.preset"});

    QProcess::execute("sudo", {"arch-chroot", "/mnt", "systemctl", "enable", "systemd-timesyncd.service"});


    // Remove rogue archiso config
    QProcess::execute("sudo", {"arch-chroot", "/mnt", "rm", "-f", "/etc/mkinitcpio.conf.d/archiso.conf"});
    QProcess::execute("sudo", {"arch-chroot", "/mnt", "sed", "-i", "s/archiso[^ ]* *//g", "/etc/mkinitcpio.conf"});

    // ⚠️ Remove old initramfs to ensure fresh build
    QProcess::execute("sudo", {
                                  "arch-chroot", "/mnt",
                                  "rm", "-f", "/boot/initramfs-linux*"
                              });
    appendLog("Regenerating /etc/fstab cleanly…");

    int fstabRet = QProcess::execute("sudo", {
                                                 "arch-chroot", "/mnt",
                                                 "bash", "-c", "genfstab -U / > /etc/fstab"
                                             });
    if (fstabRet != 0) {
        QMessageBox::warning(this, "Warning", "Failed to regenerate /etc/fstab.");
    }    process.waitForFinished();

    QProcess::execute("sudo", {"arch-chroot", "/mnt", "mkinitcpio", "-P"});

    // Set hostname
    QProcess::execute("sudo", {"arch-chroot", "/mnt", "bash", "-c", "echo archlinux > /etc/hostname"});

    // Set locale
    QProcess::execute("sudo", {"arch-chroot", "/mnt", "sed", "-i", "s/^#en_US.UTF-8/en_US.UTF-8/", "/etc/locale.gen"});
    QProcess::execute("sudo", {"arch-chroot", "/mnt", "locale-gen"});
    QProcess::execute("sudo", {"arch-chroot", "/mnt", "bash", "-c", "echo LANG=en_US.UTF-8 > /etc/locale.conf"});

    // Set timezone
    QProcess::execute("sudo", {"arch-chroot", "/mnt", "ln", "-sf", "/usr/share/zoneinfo/UTC", "/etc/localtime"});
    QProcess::execute("sudo", {"arch-chroot", "/mnt", "hwclock", "--systohc"});

    // GRUB placeholder dir (if needed)
    QProcess::execute("sudo", {"arch-chroot", "/mnt", "mkdir", "-p", "/boot/grub"});

    QMessageBox::information(nullptr, "Success",
                             "Base system and setup installed and configured!\nStarting Grub installation and updating next...");

    installGrub(selectedDrive);
}

void Installwizard::installGrub(const QString &drive) {
    QString disk = QString("/dev/%1").arg(drive);

    appendLog("Installing GRUB to target disk from inside chroot…");

    // Install GRUB package into target
    int pkgRet = QProcess::execute("sudo", {
                                               "arch-chroot", "/mnt",
                                               "pacman", "-Sy", "--noconfirm",
                                               "grub", "os-prober", "--needed"
                                           });
    if (pkgRet != 0) {
        QMessageBox::critical(nullptr, "Error", "Failed to install grub+os-prober inside chroot.");
        return;
    }

    // Clean GRUB default file
    QProcess::execute("sudo", {
                                  "arch-chroot", "/mnt",
                                  "sed", "-i", "/2025-05-01-10-09-37-00/d", "/etc/default/grub"
                              });
    QProcess::execute("sudo", {
                                  "arch-chroot", "/mnt",
                                  "bash", "-c", "echo 'GRUB_DISABLE_LINUX_UUID=\"false\"' >> /etc/default/grub"
                              });

    QStringList grubArgs = {"arch-chroot", "/mnt", "grub-install"};
    if (efiInstall) {
        grubArgs << "--target=x86_64-efi" << "--efi-directory=/boot" << "--bootloader-id=GRUB";
    } else {
        grubArgs << "--target=i386-pc" << disk;
    }
    int grubRet = QProcess::execute("sudo", grubArgs);
    if (grubRet != 0) {
        QMessageBox::critical(nullptr, "Error", "grub-install failed.");
        return;
    }

    // Generate grub config
    int cfgRet = QProcess::execute("sudo", {
                                               "arch-chroot", "/mnt",
                                               "grub-mkconfig", "-o", "/boot/grub/grub.cfg"
                                           });
    if (cfgRet != 0) {
        QMessageBox::critical(nullptr, "Error", "grub-mkconfig failed.");
        return;
    }

    appendLog("Updating...");
    int update = QProcess::execute("sudo", {
                                               "arch-chroot", "/mnt",
                                               "pacman", "-Syu", "--noconfirm"
                                           });
    if (update != 0) {
        QMessageBox::critical(this, "Error", "Failed to update base system.");
        return;
    }
}

void Installwizard::on_installButton_clicked() {
    QString username = ui->lineEditUsername->text().trimmed();
    QString password = ui->lineEditPassword->text();
    QString passwordAgain = ui->lineEditPasswordAgain->text();

    QString rootPassword = ui->lineEditRootPassword->text();
    QString rootPasswordAgain = ui->lineEditRootPasswordAgain->text();

    QString desktopEnv = ui->comboDesktopEnvironment->currentText();

    ui->comboDesktopEnvironment->addItems({
        "GNOME", "KDE Plasma", "XFCE", "LXQt", "Cinnamon", "MATE", "i3"
    });

    if (username.isEmpty() || password.isEmpty() || rootPassword.isEmpty()) {
        QMessageBox::warning(this, "Input Error", "Please fill out all fields.");
        return;
    }

    if (password != passwordAgain) {
        QMessageBox::warning(this, "Password Mismatch", "User passwords do not match.");
        return;
    }

    if (rootPassword != rootPasswordAgain) {
        QMessageBox::warning(this, "Password Mismatch", "Root passwords do not match.");
        return;
    }

    SystemWorker *worker = new SystemWorker;
    worker->setParameters(selectedDrive, username, password, rootPassword, desktopEnv, efiInstall);


    // Prevent finishing until the background install completes
    setWizardButtonEnabled(QWizard::FinishButton, false);


    QThread *thread = new QThread;
    worker->moveToThread(thread);
    appendLog("Starting system installation…");


    connect(thread, &QThread::started, worker, &SystemWorker::run);
    connect(worker, &SystemWorker::logMessage, this, [this](const QString &msg) { appendLog(msg); });
    connect(worker, &SystemWorker::errorOccurred, this, [this](const QString &msg) {
        QMessageBox::critical(this, "Error", msg);
    });

    connect(worker, &SystemWorker::finished, this, [this]() {
        appendLog("\xE2\x9C\x85 Installation complete.");
        setWizardButtonEnabled(QWizard::FinishButton, true);
        QMessageBox::information(this, "Complete", "System installation finished.");
    });
    connect(worker, &SystemWorker::finished, thread, &QThread::quit);
    connect(worker, &SystemWorker::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    thread->start();
}
