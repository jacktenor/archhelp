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
#include <QDir>
#include <unistd.h>
#include <QStandardPaths>
#include <QThread>
#include <QTreeWidget>
#include <QTreeWidgetItem>

Installwizard::Installwizard(QWidget *parent) :
    QWizard(parent),
    ui(new Ui::Installwizard) {
    ui->setupUi(this);
    setWindowTitle("Arch Linux Installer");


    // Connect refreshButton to populate drives
    connect(ui->partRefreshButton, &QPushButton::clicked, this, &Installwizard::populateDrives);

    // Connect prepareButton to handle drive preparation
    connect(ui->prepareButton, &QPushButton::clicked, this, [=]() {
        QString selectedDrive = ui->driveDropdown->currentText();
        if (selectedDrive.isEmpty() || selectedDrive == "No drives found") {
            QMessageBox::warning(this, "Error", "Please select a valid drive.");
            return;
        }
        // Remove "/dev/" prefix for internal processing
        prepareDrive(selectedDrive.mid(5));
    });

    // Populate drives when the wizard starts
    populateDrives();

    // Inside Installwizard constructor
    connect(ui->downloadButton, &QPushButton::clicked, this, [=]() {
        downloadISO(ui->progressBar);  // Pass the progress bar to show download progress
    });

    connect(ui->installButton, &QPushButton::clicked,
        this, &Installwizard::on_installButton_clicked);

    connect(this, &QWizard::currentIdChanged, this, [this](int id) {
        if (id == 1) { // partition page
            QString drive = ui->driveDropdown->currentText().mid(5);
            if (!drive.isEmpty())
                populatePartitionTable(drive);
        }
        if (id == 2) { // final install page

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
        if (!drive.isEmpty())
            createDefaultPartitions(drive);
    });

    connect(ui->driveDropdown, &QComboBox::currentTextChanged, this, [this](const QString &text) {
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

    QString installCmd = "pkexec apt install -y " + packages.join(" ");
    
    qDebug() << "Installing dependencies:" << installCmd;

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

    // QMessageBox::information(this, "Dependencies Ready", "All required packages are installed.");

    getAvailableDrives();
}

QStringList Installwizard::getAvailableDrives() {
    QProcess process;

    // Use full path to lsblk
    process.start("/usr/bin/lsblk", QStringList() << "-o" << "NAME,SIZE,TYPE" << "-d" << "-n");
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
    process.start("sudo fuser -vk " + mountPoint);
    process.waitForFinished();
    qDebug() << "Killed processes using" << mountPoint;

    // Try unmounting normally
    process.start("sudo umount " + mountPoint);
    process.waitForFinished();
    if (process.exitCode() == 0) {
        qDebug() << "Unmounted successfully: " << mountPoint;
        return;
    }

    // If normal unmount failed, try lazy unmount
    process.start("sudo umount -l " + mountPoint);
    process.waitForFinished();
    if (process.exitCode() == 0) {
        qDebug() << "Lazy unmounted: " << mountPoint;
        return;
    }

    // If still fails, force unmount
    process.start("sudo umount -f " + mountPoint);
    process.waitForFinished();
    if (process.exitCode() != 0) {
        //  QMessageBox::critical(nullptr, "Error", "Failed to unmount " + mountPoint);
    } else {
        qDebug() << "Force unmounted: " << mountPoint;
    }
}

void Installwizard::unmountDrive(const QString &drive) {
    QProcess process;
    process.start("/usr/bin/lsblk",
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
    if (ui->logWidget)
        ui->logWidget->appendPlainText(message);
    if (ui->logView2)
        ui->logView2->appendPlainText(message);
    if (ui->logView3)
        ui->logView3->appendPlainText(message);
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
    connect(worker, &InstallerWorker::installComplete, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);



    thread->start();
}

void Installwizard::populatePartitionTable(const QString &drive) {
    if (drive.isEmpty())
        return;

    ui->driveLabel->setText(tr("Drive: /dev/%1").arg(drive));

    QProcess process;
    QString device = QString("/dev/%1").arg(drive);
    process.start("/usr/bin/lsblk",
                  QStringList() << "-r" << "-n" << "-o"
                                << "NAME,SIZE,TYPE,MOUNTPOINT" << device);
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

void Installwizard::createDefaultPartitions(const QString &drive) {
    unmountDrive(drive);
    QProcess process;
    QString device = QString("/dev/%1").arg(drive);
    QStringList cmds = {
        // Legacy BIOS layout: boot partition + root partition
        QString("sudo parted %1 --script mklabel msdos").arg(device),
        QString("sudo parted %1 --script mkpart primary ext4 1MiB 513MiB").arg(device),
        QString("sudo parted %1 --script set 1 boot on").arg(device),
        QString("sudo parted %1 --script mkpart primary ext4 513MiB 100%").arg(device)
    };

    for (const QString &cmd : cmds) {
        process.start("/bin/bash", QStringList() << "-c" << cmd);
        process.waitForFinished();
        if (process.exitCode() != 0) {
            QMessageBox::critical(this, "Partition Error",
                                  tr("Failed to run: %1\n%2")
                                      .arg(cmd, process.readAllStandardError()));
            return;
        }
    }

    // Ensure kernel sees new table
    process.start("/bin/bash", QStringList()
                                << "-c"
                                << QString("sudo partprobe %1 && sudo udevadm settle")
                                       .arg(device));
    process.waitForFinished();

    populatePartitionTable(drive);
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
    worker->setParameters(selectedDrive, username, password, rootPassword, desktopEnv);

    QThread *thread = new QThread;
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, &SystemWorker::run);
    connect(worker, &SystemWorker::logMessage, this, [this](const QString &msg) { appendLog(msg); });
    connect(worker, &SystemWorker::errorOccurred, this, [this](const QString &msg) {
        QMessageBox::critical(this, "Error", msg);
    });
    connect(worker, &SystemWorker::finished, thread, &QThread::quit);
    connect(worker, &SystemWorker::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    thread->start();
}
