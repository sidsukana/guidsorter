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
typedef QPair<QString, QString> ColumnPair;
typedef QMap<ColumnPair, QStringList> Columns;

class MainWindow : public QMainWindow
{
    Q_OBJECT

    public:
        explicit MainWindow(QWidget *parent = 0);
        ~MainWindow();

        void reorderMaster();
        void reorderSlaves(QString str = QString());

    public slots:
        void slotAll();
        void slotMaster();
        void slotCurrentSlave();
        void slotAllSlaves();
        void slotChangeTablesGroup(int index);

    private:
        Ui::MainWindow *ui;
        QJsonDocument m_json;

        Tables m_tables;
        MasterTables m_masterTables;
        Columns m_columns;
        MasterColumns m_masterColumns;
};

#endif // MAINWINDOW_H
