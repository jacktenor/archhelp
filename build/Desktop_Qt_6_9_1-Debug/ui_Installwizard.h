/********************************************************************************
** Form generated from reading UI file 'Installwizard.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_INSTALLWIZARD_H
#define UI_INSTALLWIZARD_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include <QtWidgets/QWizard>
#include <QtWidgets/QWizardPage>

QT_BEGIN_NAMESPACE

class Ui_Installwizard
{
public:
    QWizardPage *wizardPage;
    QLabel *label;
    QPushButton *downloadButton;
    QProgressBar *progressBar;
    QWizardPage *wizardPage_2;
    QLabel *label_2;
    QComboBox *driveDropdown;
    QPushButton *refreshButton;
    QPushButton *prepareButton;
    QPlainTextEdit *logWidget;
    QWizardPage *partitionPage;
    QLabel *driveLabel;
    QTreeWidget *treePartitions;
    QPushButton *partRefreshButton;
    QPushButton *createPartButton;
    QLabel *label_5;
    QWizardPage *wizardPage_3;
    QWidget *verticalLayoutWidget;
    QVBoxLayout *verticalLayout;
    QLabel *label_3;
    QWidget *verticalLayoutWidget_2;
    QVBoxLayout *verticalLayout_2;
    QLabel *label_4;
    QWidget *verticalLayoutWidget_3;
    QVBoxLayout *verticalLayout_3;
    QLabel *labelUsername;
    QLabel *labelPassword;
    QLabel *labelAgain;
    QLabel *labelRootPassword;
    QLabel *labelRootPasswordAgain;
    QWidget *verticalLayoutWidget_4;
    QVBoxLayout *verticalLayout_4;
    QLineEdit *lineEditUsername;
    QLineEdit *lineEditPassword;
    QLineEdit *lineEditPasswordAgain;
    QLineEdit *lineEditRootPassword;
    QLineEdit *lineEditRootPasswordAgain;
    QWidget *verticalLayoutWidget_5;
    QVBoxLayout *verticalLayout_6;
    QPushButton *installButton;
    QWidget *horizontalLayoutWidget;
    QHBoxLayout *horizontalLayout;
    QComboBox *comboDesktopEnvironment;
    QWidget *verticalLayoutWidget_6;
    QVBoxLayout *verticalLayout_5;
    QPlainTextEdit *logWidget_2;

    void setupUi(QWizard *Installwizard)
    {
        if (Installwizard->objectName().isEmpty())
            Installwizard->setObjectName("Installwizard");
        Installwizard->resize(500, 375);
        Installwizard->setMinimumSize(QSize(500, 375));
        Installwizard->setMaximumSize(QSize(500, 375));
        Installwizard->setWizardStyle(QWizard::WizardStyle::ClassicStyle);
        wizardPage = new QWizardPage();
        wizardPage->setObjectName("wizardPage");
        label = new QLabel(wizardPage);
        label->setObjectName("label");
        label->setGeometry(QRect(90, 38, 301, 25));
        QFont font;
        font.setFamilies({QString::fromUtf8("Noto Sans")});
        font.setPointSize(12);
        font.setWeight(QFont::DemiBold);
        font.setItalic(false);
        label->setFont(font);
        label->setStyleSheet(QString::fromUtf8("font: 600 12pt \"Noto Sans\";"));
        label->setAlignment(Qt::AlignmentFlag::AlignCenter);
        downloadButton = new QPushButton(wizardPage);
        downloadButton->setObjectName("downloadButton");
        downloadButton->setGeometry(QRect(188, 95, 100, 30));
        progressBar = new QProgressBar(wizardPage);
        progressBar->setObjectName("progressBar");
        progressBar->setGeometry(QRect(86, 155, 300, 25));
        progressBar->setValue(0);
        Installwizard->addPage(wizardPage);
        wizardPage_2 = new QWizardPage();
        wizardPage_2->setObjectName("wizardPage_2");
        label_2 = new QLabel(wizardPage_2);
        label_2->setObjectName("label_2");
        label_2->setGeometry(QRect(116, 40, 251, 22));
        label_2->setStyleSheet(QString::fromUtf8("font: 600 12pt \"Noto Sans\";"));
        label_2->setAlignment(Qt::AlignmentFlag::AlignCenter);
        driveDropdown = new QComboBox(wizardPage_2);
        driveDropdown->setObjectName("driveDropdown");
        driveDropdown->setGeometry(QRect(176, 132, 125, 30));
        refreshButton = new QPushButton(wizardPage_2);
        refreshButton->setObjectName("refreshButton");
        refreshButton->setGeometry(QRect(112, 88, 125, 30));
        prepareButton = new QPushButton(wizardPage_2);
        prepareButton->setObjectName("prepareButton");
        prepareButton->setGeometry(QRect(242, 88, 125, 30));
        logWidget = new QPlainTextEdit(wizardPage_2);
        logWidget->setObjectName("logWidget");
        logWidget->setGeometry(QRect(88, 190, 300, 125));
        Installwizard->addPage(wizardPage_2);
        partitionPage = new QWizardPage();
        partitionPage->setObjectName("partitionPage");
        driveLabel = new QLabel(partitionPage);
        driveLabel->setObjectName("driveLabel");
        driveLabel->setGeometry(QRect(32, 62, 420, 15));
        driveLabel->setStyleSheet(QString::fromUtf8("font: 600 10pt \"Noto Sans\";"));
        driveLabel->setAlignment(Qt::AlignmentFlag::AlignLeading|Qt::AlignmentFlag::AlignLeft|Qt::AlignmentFlag::AlignVCenter);
        treePartitions = new QTreeWidget(partitionPage);
        treePartitions->setObjectName("treePartitions");
        treePartitions->setGeometry(QRect(30, 84, 420, 175));
        partRefreshButton = new QPushButton(partitionPage);
        partRefreshButton->setObjectName("partRefreshButton");
        partRefreshButton->setGeometry(QRect(228, 276, 80, 30));
        createPartButton = new QPushButton(partitionPage);
        createPartButton->setObjectName("createPartButton");
        createPartButton->setGeometry(QRect(314, 276, 160, 30));
        label_5 = new QLabel(partitionPage);
        label_5->setObjectName("label_5");
        label_5->setGeometry(QRect(188, 20, 100, 30));
        label_5->setFont(font);
        label_5->setStyleSheet(QString::fromUtf8("font: 600 12pt \"Noto Sans\";"));
        label_5->setAlignment(Qt::AlignmentFlag::AlignCenter);
        Installwizard->addPage(partitionPage);
        wizardPage_3 = new QWizardPage();
        wizardPage_3->setObjectName("wizardPage_3");
        verticalLayoutWidget = new QWidget(wizardPage_3);
        verticalLayoutWidget->setObjectName("verticalLayoutWidget");
        verticalLayoutWidget->setGeometry(QRect(44, 30, 221, 29));
        verticalLayout = new QVBoxLayout(verticalLayoutWidget);
        verticalLayout->setObjectName("verticalLayout");
        verticalLayout->setContentsMargins(0, 0, 0, 0);
        label_3 = new QLabel(verticalLayoutWidget);
        label_3->setObjectName("label_3");
        QFont font1;
        font1.setFamilies({QString::fromUtf8("Noto Serif")});
        font1.setPointSize(11);
        font1.setBold(true);
        label_3->setFont(font1);

        verticalLayout->addWidget(label_3);

        verticalLayoutWidget_2 = new QWidget(wizardPage_3);
        verticalLayoutWidget_2->setObjectName("verticalLayoutWidget_2");
        verticalLayoutWidget_2->setGeometry(QRect(286, 30, 141, 31));
        verticalLayoutWidget_2->setFont(font1);
        verticalLayout_2 = new QVBoxLayout(verticalLayoutWidget_2);
        verticalLayout_2->setObjectName("verticalLayout_2");
        verticalLayout_2->setContentsMargins(0, 0, 0, 0);
        label_4 = new QLabel(verticalLayoutWidget_2);
        label_4->setObjectName("label_4");
        label_4->setFont(font1);

        verticalLayout_2->addWidget(label_4, 0, Qt::AlignmentFlag::AlignHCenter|Qt::AlignmentFlag::AlignVCenter);

        verticalLayoutWidget_3 = new QWidget(wizardPage_3);
        verticalLayoutWidget_3->setObjectName("verticalLayoutWidget_3");
        verticalLayoutWidget_3->setGeometry(QRect(48, 74, 91, 139));
        verticalLayout_3 = new QVBoxLayout(verticalLayoutWidget_3);
        verticalLayout_3->setObjectName("verticalLayout_3");
        verticalLayout_3->setContentsMargins(0, 0, 0, 0);
        labelUsername = new QLabel(verticalLayoutWidget_3);
        labelUsername->setObjectName("labelUsername");

        verticalLayout_3->addWidget(labelUsername, 0, Qt::AlignmentFlag::AlignRight|Qt::AlignmentFlag::AlignBottom);

        labelPassword = new QLabel(verticalLayoutWidget_3);
        labelPassword->setObjectName("labelPassword");

        verticalLayout_3->addWidget(labelPassword, 0, Qt::AlignmentFlag::AlignRight|Qt::AlignmentFlag::AlignBottom);

        labelAgain = new QLabel(verticalLayoutWidget_3);
        labelAgain->setObjectName("labelAgain");

        verticalLayout_3->addWidget(labelAgain, 0, Qt::AlignmentFlag::AlignRight);

        labelRootPassword = new QLabel(verticalLayoutWidget_3);
        labelRootPassword->setObjectName("labelRootPassword");

        verticalLayout_3->addWidget(labelRootPassword, 0, Qt::AlignmentFlag::AlignRight);

        labelRootPasswordAgain = new QLabel(verticalLayoutWidget_3);
        labelRootPasswordAgain->setObjectName("labelRootPasswordAgain");

        verticalLayout_3->addWidget(labelRootPasswordAgain, 0, Qt::AlignmentFlag::AlignRight|Qt::AlignmentFlag::AlignTop);

        verticalLayoutWidget_4 = new QWidget(wizardPage_3);
        verticalLayoutWidget_4->setObjectName("verticalLayoutWidget_4");
        verticalLayoutWidget_4->setGeometry(QRect(146, 74, 107, 143));
        verticalLayout_4 = new QVBoxLayout(verticalLayoutWidget_4);
        verticalLayout_4->setSpacing(0);
        verticalLayout_4->setObjectName("verticalLayout_4");
        verticalLayout_4->setContentsMargins(0, 0, 0, 0);
        lineEditUsername = new QLineEdit(verticalLayoutWidget_4);
        lineEditUsername->setObjectName("lineEditUsername");

        verticalLayout_4->addWidget(lineEditUsername);

        lineEditPassword = new QLineEdit(verticalLayoutWidget_4);
        lineEditPassword->setObjectName("lineEditPassword");

        verticalLayout_4->addWidget(lineEditPassword);

        lineEditPasswordAgain = new QLineEdit(verticalLayoutWidget_4);
        lineEditPasswordAgain->setObjectName("lineEditPasswordAgain");

        verticalLayout_4->addWidget(lineEditPasswordAgain, 0, Qt::AlignmentFlag::AlignVCenter);

        lineEditRootPassword = new QLineEdit(verticalLayoutWidget_4);
        lineEditRootPassword->setObjectName("lineEditRootPassword");

        verticalLayout_4->addWidget(lineEditRootPassword, 0, Qt::AlignmentFlag::AlignVCenter);

        lineEditRootPasswordAgain = new QLineEdit(verticalLayoutWidget_4);
        lineEditRootPasswordAgain->setObjectName("lineEditRootPasswordAgain");

        verticalLayout_4->addWidget(lineEditRootPasswordAgain, 0, Qt::AlignmentFlag::AlignVCenter);

        verticalLayoutWidget_5 = new QWidget(wizardPage_3);
        verticalLayoutWidget_5->setObjectName("verticalLayoutWidget_5");
        verticalLayoutWidget_5->setGeometry(QRect(202, 220, 82, 29));
        verticalLayout_6 = new QVBoxLayout(verticalLayoutWidget_5);
        verticalLayout_6->setObjectName("verticalLayout_6");
        verticalLayout_6->setContentsMargins(0, 0, 0, 0);
        installButton = new QPushButton(verticalLayoutWidget_5);
        installButton->setObjectName("installButton");

        verticalLayout_6->addWidget(installButton);

        horizontalLayoutWidget = new QWidget(wizardPage_3);
        horizontalLayoutWidget->setObjectName("horizontalLayoutWidget");
        horizontalLayoutWidget->setGeometry(QRect(296, 74, 119, 137));
        horizontalLayout = new QHBoxLayout(horizontalLayoutWidget);
        horizontalLayout->setObjectName("horizontalLayout");
        horizontalLayout->setContentsMargins(0, 0, 0, 0);
        comboDesktopEnvironment = new QComboBox(horizontalLayoutWidget);
        comboDesktopEnvironment->setObjectName("comboDesktopEnvironment");

        horizontalLayout->addWidget(comboDesktopEnvironment, 0, Qt::AlignmentFlag::AlignTop);

        verticalLayoutWidget_6 = new QWidget(wizardPage_3);
        verticalLayoutWidget_6->setObjectName("verticalLayoutWidget_6");
        verticalLayoutWidget_6->setGeometry(QRect(128, 256, 229, 72));
        verticalLayout_5 = new QVBoxLayout(verticalLayoutWidget_6);
        verticalLayout_5->setObjectName("verticalLayout_5");
        verticalLayout_5->setContentsMargins(0, 0, 0, 0);
        logWidget_2 = new QPlainTextEdit(verticalLayoutWidget_6);
        logWidget_2->setObjectName("logWidget_2");

        verticalLayout_5->addWidget(logWidget_2);

        Installwizard->addPage(wizardPage_3);

        retranslateUi(Installwizard);

        QMetaObject::connectSlotsByName(Installwizard);
    } // setupUi

    void retranslateUi(QWizard *Installwizard)
    {
        Installwizard->setWindowTitle(QCoreApplication::translate("Installwizard", "Installwizard", nullptr));
        label->setText(QCoreApplication::translate("Installwizard", "Download the latest Arch Linux ISO.", nullptr));
        downloadButton->setText(QCoreApplication::translate("Installwizard", "Download", nullptr));
        label_2->setText(QCoreApplication::translate("Installwizard", "Select a drive for installation", nullptr));
        refreshButton->setText(QCoreApplication::translate("Installwizard", "Refresh", nullptr));
        prepareButton->setText(QCoreApplication::translate("Installwizard", "Prepare Drive", nullptr));
        driveLabel->setText(QCoreApplication::translate("Installwizard", "Drive:", nullptr));
        QTreeWidgetItem *___qtreewidgetitem = treePartitions->headerItem();
        ___qtreewidgetitem->setText(3, QCoreApplication::translate("Installwizard", "Mount", nullptr));
        ___qtreewidgetitem->setText(2, QCoreApplication::translate("Installwizard", "Type", nullptr));
        ___qtreewidgetitem->setText(1, QCoreApplication::translate("Installwizard", "Size", nullptr));
        ___qtreewidgetitem->setText(0, QCoreApplication::translate("Installwizard", "Name", nullptr));
        partRefreshButton->setText(QCoreApplication::translate("Installwizard", "Refresh", nullptr));
        createPartButton->setText(QCoreApplication::translate("Installwizard", "Create Default Partitions", nullptr));
        label_5->setText(QCoreApplication::translate("Installwizard", "Partitioning", nullptr));
        label_3->setText(QCoreApplication::translate("Installwizard", "Add user and set passwords", nullptr));
        label_4->setText(QCoreApplication::translate("Installwizard", "Choose a desktop", nullptr));
        labelUsername->setText(QCoreApplication::translate("Installwizard", "Username:", nullptr));
        labelPassword->setText(QCoreApplication::translate("Installwizard", "Password:", nullptr));
        labelAgain->setText(QCoreApplication::translate("Installwizard", "Again:", nullptr));
        labelRootPassword->setText(QCoreApplication::translate("Installwizard", "Root Password:", nullptr));
        labelRootPasswordAgain->setText(QCoreApplication::translate("Installwizard", "Again:", nullptr));
        installButton->setText(QCoreApplication::translate("Installwizard", "Submit", nullptr));
    } // retranslateUi

};

namespace Ui {
    class Installwizard: public Ui_Installwizard {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_INSTALLWIZARD_H
