#include "Installwizard.h"
#include "installerworker.h"
#include "ui_Installwizard.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QProcess>
#include <QNetworkAccessManager>
#include <QRegularExpression>
#include <QNetworkReply>
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
    connect(ui->refreshButton, &QPushButton::clicked, this, &Installwizard::populateDrives);

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

void Installwizard::prepareDrive(const QString &drive) {
    selectedDrive = drive;
    unmountDrive(drive);

    InstallerWorker *worker = new InstallerWorker;
    worker->setDrive(drive);

    QThread *thread = new QThread;
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, &InstallerWorker::run);
    connect(worker, &InstallerWorker::logMessage, ui->logWidget, &QPlainTextEdit::appendPlainText);
    connect(worker, &InstallerWorker::errorOccurred, this, [this](const QString &msg) {
        QMessageBox::critical(this, "Error", msg);
    });
    connect(worker, &InstallerWorker::installComplete, thread, &QThread::quit);
    connect(worker, &InstallerWorker::installComplete, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    connect(worker, &InstallerWorker::installComplete, this, [this]() {
        mountPartitions(selectedDrive);
    });

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
        QString("sudo parted %1 --script mklabel msdos").arg(device),
        QString("sudo parted %1 --script mkpart primary ext4 1MiB 100%").arg(device)
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


void Installwizard::mountPartitions(const QString &drive) {
    QProcess process;
    QString rootPart = QString("/dev/%1").arg(drive + "1");

    // 1. Mount root
    process.start("/bin/bash", { "-c",
                                QString("sudo mount %1 /mnt").arg(rootPart) });
    process.waitForFinished(-1);

    // 2. Ensure /mnt/boot exists
    process.start("/bin/bash", { "-c",
                                "sudo mkdir -p /mnt/boot" });
    process.waitForFinished(-1);
    // 3. Copy ISO for later installation
    process.start("/bin/bash", { "-c",
                                "sudo cp /tmp/archlinux.iso /mnt/archlinux.iso" });
    process.waitForFinished(-1);
}

void Installwizard::mountISO() {
    QProcess process;

    QString isoPath = "/mnt/archlinux.iso";

    // ✅ Check if ISO exists before mounting
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

    QMessageBox::information(nullptr, "Success", "Arch Linux root filesystem extracted successfully!\nNext we Install keys and base system.\nThis could take a few...");
    bindSystemDirectories();
}

void Installwizard::bindSystemDirectories() {
    QProcess process;

    // ✅ Ensure directories exist before binding
    QDir().mkpath("/mnt/proc");
    QDir().mkpath("/mnt/sys");
    QDir().mkpath("/mnt/dev");
    QDir().mkpath("/mnt/run");

    QStringList bindCommands = {
        "sudo mount --bind /proc /mnt/proc",
        "sudo mount --bind /sys /mnt/sys",
        "sudo mount --bind /dev /mnt/dev",
        "sudo mount --bind /run /mnt/run"
    };

    for (const QString &cmd : bindCommands) {
        qDebug() << "Executing Bind Command:" << cmd;
        process.start("/bin/bash", QStringList() << "-c" << cmd);
        process.waitForFinished();

        QString output = process.readAllStandardOutput();
        QString errors = process.readAllStandardError();

        qDebug() << "Bind Command Output:" << output;
        qDebug() << "Bind Command Errors:" << errors;

        if (process.exitCode() != 0) {
            QMessageBox::critical(nullptr, "Error", QString("Failed to bind system directories:\n%1").arg(errors));
            return;
        }
    }

    // QMessageBox::information(nullptr, "Success", "System directories bound successfully!");
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

    // Mount system dirs
    QProcess::execute("sudo", {"mount", "-t", "proc", "/proc", "/mnt/proc"});
    QProcess::execute("sudo", {"mount", "--rbind", "/sys", "/mnt/sys"});
    QProcess::execute("sudo", {"mount", "--rbind", "/dev", "/mnt/dev"});
    QProcess::execute("sudo", {"mount", "--rbind", "/run", "/mnt/run"});

    // DNS check
    QProcess dnsTest;
    dnsTest.start("sudo", {"arch-chroot", "/mnt", "ping", "-c", "1", "archlinux.org"});
    dnsTest.waitForFinished();

    // Initialize pacman
    QProcess::execute("sudo", {"arch-chroot", "/mnt", "pacman-key", "--init"});
    QProcess::execute("sudo", {"arch-chroot", "/mnt", "pacman-key", "--populate", "archlinux"});
    QProcess::execute("sudo", {"arch-chroot", "/mnt", "pacman", "-Sy", "--noconfirm", "archlinux-keyring"});

    // Install base, kernel, firmware
    ui->logWidget->appendPlainText("Installing base, linux, linux-firmware…");
    int baseRet = QProcess::execute("sudo", {
                                                "arch-chroot", "/mnt",
                                                "pacman", "-Sy", "--noconfirm",
                                                "base", "linux", "linux-firmware", "--needed"
                                            });
    if (baseRet != 0) {
        QMessageBox::critical(this, "Error", "Failed to install base system.");
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
    ui->logWidget->appendPlainText("Regenerating /etc/fstab cleanly…");

    int fstabRet = QProcess::execute("sudo", {
                                                 "arch-chroot", "/mnt",
                                                 "bash", "-c", "genfstab -U / > /etc/fstab"
                                             });
    if (fstabRet != 0) {
        QMessageBox::warning(this, "Warning", "Failed to regenerate /etc/fstab.");
    }

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

    ui->logWidget->appendPlainText("Installing GRUB to target disk from inside chroot…");

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

    // Run grub-install inside chroot
    int grubRet = QProcess::execute("sudo", {
                                                "arch-chroot", "/mnt",
                                                "grub-install",
                                                "--target=i386-pc",
                                                disk
                                            });
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

    ui->logWidget->appendPlainText("Updating...");
    int update = QProcess::execute("sudo", {
                                                "arch-chroot", "/mnt",
                                                "pacman", "-Syu", "--noconfirm"
                                            });
    if (update != 0) {
        QMessageBox::critical(this, "Error", "Failed to update base system.");
        return;
    }

    QMessageBox::information(nullptr, "Success",
                             "GRUB successfully installed and configured!");
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

    // Install base system before configuring users
    mountISO();

    // Add user and set password
    ui->logWidget->appendPlainText("Adding user and setting password...");
    QProcess::execute("sudo", {
                                  "arch-chroot", "/mnt",
                                  "useradd", "-m", "-G", "wheel", username
                              });
    QProcess::execute("sudo", {
                                  "arch-chroot", "/mnt",
                                  "bash", "-c", QString("echo '%1:%2' | chpasswd").arg(username, password)
                              });

    // Set root password
    QProcess::execute("sudo", {
                                  "arch-chroot", "/mnt",
                                  "bash", "-c", QString("echo 'root:%1' | chpasswd").arg(rootPassword)
                              });

    // Enable sudo for wheel group
    QProcess::execute("sudo", {
                                  "arch-chroot", "/mnt",
                                  "sed", "-i",
                                  "s/^# %wheel ALL=(ALL:ALL) ALL/%wheel ALL=(ALL:ALL) ALL/",
                                  "/etc/sudoers"
                              });

    // Select desktop package groups
    QMap<QString, QStringList> desktopPackages = {
        {"GNOME", {"xorg", "gnome", "gdm"}},
        {"KDE Plasma", {"xorg", "plasma", "sddm", "kde-applications"}},
        {"XFCE", {"xorg", "xfce4", "xfce4-goodies", "lightdm", "lightdm-gtk-greeter"}},
        {"LXQt", {"xorg", "lxqt", "sddm"}},
        {"Cinnamon", {"xorg", "cinnamon", "lightdm", "lightdm-gtk-greeter"}},
        {"MATE", {"xorg", "mate", "mate-extra", "lightdm", "lightdm-gtk-greeter"}},
        {"i3", {"xorg", "i3", "lightdm", "lightdm-gtk-greeter"}}
    };

    if (!desktopPackages.contains(desktopEnv)) {
        QMessageBox::critical(this, "Error", "Unknown desktop environment selected.");
        return;
    }

    // Install selected desktop packages
    ui->logWidget->appendPlainText("Installing desktop environment: " + desktopEnv);
    QStringList dePkgs = {"arch-chroot", "/mnt", "pacman", "-Sy", "--noconfirm"};
    dePkgs.append(desktopPackages[desktopEnv]);

    if (QProcess::execute("sudo", dePkgs) != 0) {
        QMessageBox::critical(this, "Error", "Failed to install desktop environment.");
        return;
    }

    // Enable corresponding display manager
    QString dmService;
    if (desktopEnv == "GNOME") dmService = "gdm.service";
    else if (desktopEnv == "KDE Plasma" || desktopEnv == "LXQt") dmService = "sddm.service";
    else dmService = "lightdm.service";

    QProcess::execute("sudo", {
                                  "arch-chroot", "/mnt",
                                  "systemctl", "enable", dmService
                              });
    // ⚠️ Remove old initramfs to ensure fresh build
    QProcess::execute("sudo", {
                                  "arch-chroot", "/mnt",
                                  "rm", "-f", "/boot/initramfs-linux*"
                              });

    QProcess::execute("sudo", {"arch-chroot", "/mnt", "mkinitcpio", "-P"});




    ui->logWidget->appendPlainText("Removing and rebuilding /etc/fstab...");

    int fstabRetTwo = QProcess::execute("sudo", {
                                                 "arch-chroot", "/mnt",
                                                 "bash", "-c", "rm -f /etc/fstab"
                                             });

    if (fstabRetTwo != 0) {
        QMessageBox::warning(this, "Warning", "Failed to remove /etc/fstab.");
    }



    ui->logWidget->appendPlainText("Regenerating /etc/fstab from host…");

    int fstabRet = QProcess::execute("sudo", {
                                                 "bash", "-c", "genfstab -U /mnt > /mnt/etc/fstab"
                                             });
    if (fstabRet != 0) {
        QMessageBox::warning(this, "Warning", "Failed to regenerate /etc/fstab.");
    }

    ui->logWidget->appendPlainText("Sanitizing /etc/fstab to remove ghost devices…");

    int cleanRet = QProcess::execute("sudo", {
                                                 "bash", "-c",
                                                 R"(awk '!/^#|^$/{print; exit} 1' /mnt/etc/fstab > /mnt/etc/fstab.clean && mv /mnt/etc/fstab.clean /mnt/etc/fstab)"
                                             });
    if (cleanRet != 0) {
        QMessageBox::warning(this, "Warning", "Failed to sanitize fstab.");
    }


    QMessageBox::information(this, "Success", "User setup and desktop environment installed successfully!\nEnjoy Arch Linux.");
}
