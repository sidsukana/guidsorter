#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QPair>

namespace Ui {
class MainWindow;
}

typedef QMap<QString, QString> MasterTables;
typedef QMap<QString, QString> MasterColumns;
typedef QMap<QString, QStringList> Tables;
typedef QPair<QString, QString> StringPair;
typedef QMap<StringPair, QStringList> Columns;
typedef QMap<StringPair, QString> Conditions;

class MainWindow : public QMainWindow
{
    Q_OBJECT

    public:
        explicit MainWindow(QWidget *parent = 0);
        ~MainWindow();

        void reorderMaster();
        void reorderSlaves(QString str = QString());
        void flushMasterFields();
        void flushSlavesFields(QString str = QString());

    public slots:
        void slotDoAll();
        void slotAll();
        void slotFlushAll();
        void slotMaster();
        void slotCurrentSlave();
        void slotFlushMaster();
        void slotFlushCurrentSlave();
        void slotAllSlaves();
        void slotFlushAllSlaves();
        void slotChangeTablesGroup(int index);

    private:
        Ui::MainWindow *ui;
        QJsonDocument m_json;

        Tables m_tables;
        MasterTables m_masterTables;
        Columns m_columns;
        MasterColumns m_masterColumns;
        Conditions m_conditions;
};

#endif // MAINWINDOW_H
