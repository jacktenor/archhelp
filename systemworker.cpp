#include "systemworker.h"
#include <QProcess>
#include <QFile>
#include <QDir>
#include <QMap>
#include <QStringList>

SystemWorker::SystemWorker(QObject *parent) : QObject(parent) {}

void SystemWorker::setParameters(const QString &drv,
                                 const QString &user,
                                 const QString &pass,
                                 const QString &rootPass,
                                 const QString &de) {
    drive = drv;
    username = user;
    password = pass;
    rootPassword = rootPass;
    desktopEnv = de;
}

bool SystemWorker::runCommand(const QString &cmd) {
    QProcess proc;
    proc.start("/bin/bash", {"-c", cmd});
    proc.waitForFinished(-1);
    QString out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    QString err = QString::fromUtf8(proc.readAllStandardError()).trimmed();
    if (!out.isEmpty())
        emit logMessage(out);
    if (proc.exitCode() != 0) {
        emit errorOccurred(err.isEmpty() ? QString("Failed: %1").arg(cmd)
                                         : QString("%1\n%2").arg(cmd, err));
        return false;
    }
    return true;
}

void SystemWorker::run() {
    emit logMessage("\xF0\x9F\x9A\x80 Starting system installation...");

    QString isoPath = "/mnt/archlinux.iso";
    if (!QFile::exists(isoPath)) {
        QString tmpIso = QDir::tempPath() + "/archlinux.iso";
        if (QFile::exists(tmpIso)) {
            if (!runCommand(QString("sudo cp %1 %2").arg(tmpIso, isoPath)))
                return;
        } else {
            emit errorOccurred("Arch Linux ISO not found");
            return;
        }
    }

    QDir().mkdir("/mnt/archiso");
    QDir().mkdir("/mnt/rootfs");

    if (!runCommand(QString("sudo mount -o loop %1 /mnt/archiso").arg(isoPath)))
        return;

    QString squashfsPath = "/mnt/archiso/arch/x86_64/airootfs.sfs";
    if (!runCommand(QString("sudo unsquashfs -f -d /mnt %1").arg(squashfsPath)))
        return;

    emit logMessage("ISO mounted and rootfs extracted");

    runCommand("sudo rm -f /mnt/etc/resolv.conf");
    runCommand("sudo cp /etc/resolv.conf /mnt/etc/resolv.conf");

    if (!QFile::exists("/mnt/usr/bin/pacman")) {
        QString bootstrapUrl = "https://mirrors.edge.kernel.org/archlinux/iso/latest/archlinux-bootstrap-x86_64.tar.gz";
        if (!runCommand(QString("sudo wget -O /tmp/arch-bootstrap.tar.gz %1").arg(bootstrapUrl)))
            return;
        if (!runCommand("sudo tar -xzf /tmp/arch-bootstrap.tar.gz -C /mnt --strip-components=1"))
            return;
    }

    runCommand("sudo arch-chroot /mnt pacman-key --init");
    runCommand("sudo arch-chroot /mnt pacman-key --populate archlinux");
    runCommand("sudo arch-chroot /mnt pacman -Sy --noconfirm archlinux-keyring");

    emit logMessage("Installing base, linux, linux-firmware…");
    if (!runCommand("sudo arch-chroot /mnt pacman -Sy --noconfirm base linux linux-firmware --needed"))
        return;

    runCommand("sudo arch-chroot /mnt systemctl enable systemd-timesyncd.service");
    runCommand("sudo arch-chroot /mnt rm -f /etc/mkinitcpio.conf.d/archiso.conf");
    runCommand("sudo arch-chroot /mnt sed -i 's/archiso[^ ]* *//g' /etc/mkinitcpio.conf");
    runCommand("sudo arch-chroot /mnt rm -f /boot/initramfs-linux*");
    runCommand("sudo arch-chroot /mnt mkinitcpio -P");

    runCommand("sudo arch-chroot /mnt bash -c 'echo archlinux > /etc/hostname'");
    runCommand("sudo arch-chroot /mnt sed -i 's/^#en_US.UTF-8/en_US.UTF-8/' /etc/locale.gen");
    runCommand("sudo arch-chroot /mnt locale-gen");
    runCommand("sudo arch-chroot /mnt bash -c 'echo LANG=en_US.UTF-8 > /etc/locale.conf'");
    runCommand("sudo arch-chroot /mnt ln -sf /usr/share/zoneinfo/UTC /etc/localtime");
    runCommand("sudo arch-chroot /mnt hwclock --systohc");
    runCommand("sudo arch-chroot /mnt mkdir -p /boot/grub");

    emit logMessage("Installing GRUB…");
    if (!runCommand("sudo arch-chroot /mnt pacman -Sy --noconfirm grub os-prober --needed"))
        return;
    runCommand("sudo arch-chroot /mnt sed -i '/2025-05-01-10-09-37-00/d' /etc/default/grub");
    runCommand("sudo arch-chroot /mnt bash -c \"echo 'GRUB_DISABLE_LINUX_UUID="false"' >> /etc/default/grub\"");
    if (!runCommand(QString("sudo arch-chroot /mnt grub-install --target=i386-pc /dev/%1").arg(drive)))
        return;
    if (!runCommand("sudo arch-chroot /mnt grub-mkconfig -o /boot/grub/grub.cfg"))
        return;
    if (!runCommand("sudo arch-chroot /mnt pacman -Syu --noconfirm"))
        return;

    emit logMessage("Adding user and configuring system…");
    runCommand(QString("sudo arch-chroot /mnt useradd -m -G wheel %1").arg(username));
    runCommand(QString("sudo arch-chroot /mnt bash -c \"echo '%1:%2' | chpasswd\"" ).arg(username, password));
    runCommand(QString("sudo arch-chroot /mnt bash -c \"echo 'root:%1' | chpasswd\"" ).arg(rootPassword));
    runCommand("sudo arch-chroot /mnt sed -i 's/^# %wheel ALL=(ALL:ALL) ALL/%wheel ALL=(ALL:ALL) ALL/' /etc/sudoers");

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
        emit errorOccurred("Unknown desktop environment");
        return;
    }

    QString pkgCmd = QString("sudo arch-chroot /mnt pacman -Sy --noconfirm %1").arg(desktopPackages.value(desktopEnv).join(' '));
    if (!runCommand(pkgCmd))
        return;

    QString dmService;
    if (desktopEnv == "GNOME") dmService = "gdm.service";
    else if (desktopEnv == "KDE Plasma" || desktopEnv == "LXQt") dmService = "sddm.service";
    else dmService = "lightdm.service";

    runCommand(QString("sudo arch-chroot /mnt systemctl enable %1").arg(dmService));
    runCommand("sudo arch-chroot /mnt rm -f /boot/initramfs-linux*");
    runCommand("sudo arch-chroot /mnt mkinitcpio -P");

    runCommand("sudo arch-chroot /mnt bash -c 'rm -f /etc/fstab'");
    runCommand("sudo bash -c 'genfstab -U /mnt > /mnt/etc/fstab'");
    runCommand("sudo bash -c \"awk '!/^#|^$/{print; exit} 1' /mnt/etc/fstab > /mnt/etc/fstab.clean && mv /mnt/etc/fstab.clean /mnt/etc/fstab\"");

    emit finished();
}

