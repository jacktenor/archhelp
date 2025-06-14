#ifndef INSTALLWIZARD_H
#define INSTALLWIZARD_H

#include <QWizard>
#include <QNetworkAccessManager>
#include <QProgressBar>
#include <QStringList>

QT_BEGIN_NAMESPACE
namespace Ui {
class Installwizard;
}
QT_END_NAMESPACE

class Installwizard : public QWizard {
    Q_OBJECT

public:
    explicit Installwizard(QWidget *parent = nullptr);
    ~Installwizard();

private:
    void installDependencies();
    Ui::Installwizard *ui;
    QProgressBar *progressBar;
    QNetworkAccessManager *networkManager;
    QString selectedDrive;  // 🧠 TRACK THE CURRENT DRIVE
    QString getUserHome();
    void populateDrives(); // Populate the dropdown with available drives
    void mountPartitions(const QString &drive);
    // Remove the parameter from installGrubAsync since we won't need one.
    void installGrub(const QString &drive);
    void downloadISO(QProgressBar *progressBar);
    void installArchBase(const QString &selectedDrive);
    void mountISO();
    void on_installButton_clicked();
    void bindSystemDirectories();
    void forceUnmount(const QString &mountPoint);
    void unmountDrive(const QString &drive);
    // Declare the methods that were missing
    QStringList getAvailableDrives();        // Detect available drives
    void prepareDrive(const QString &drive);   // Prepare the selected drive
    void populatePartitionTable(const QString &drive); // new
    void createDefaultPartitions(const QString &drive); // new example
};

#endif // INSTALLWIZARD_H
