#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMessageBox>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSql>
#include <QFile>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->pushButton_4, SIGNAL(clicked()), this, SLOT(slotAllSlaves()));
    connect(ui->pushButton_3, SIGNAL(clicked()), this, SLOT(slotCurrentSlave()));
    connect(ui->pushButton_2, SIGNAL(clicked()), this, SLOT(slotMaster()));
    connect(ui->pushButton, SIGNAL(clicked()), this, SLOT(slotAll()));
    connect(ui->comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(slotChangeTablesGroup(int)));

    ui->progressBar->setValue(0);

    QFile file("config.json");
    if (file.open(QFile::ReadOnly))
        m_json = QJsonDocument::fromJson(file.readAll());

    if (!m_json.isEmpty())
    {
        QJsonObject config = m_json.object();
        QJsonObject tableGroups = config.value("Tables").toObject();

        for (auto itr = tableGroups.constBegin(); itr != tableGroups.constEnd(); ++itr)
        {
            ui->comboBox->addItem(itr.key());

            QJsonObject tableGroup = itr.value().toObject();

            QStringList tables;
            for (auto itr2 = tableGroup.constBegin(); itr2 != tableGroup.constEnd(); ++itr2)
            {
                QJsonObject table = itr2.value().toObject();

                if (table.value("main").toBool())
                {
                    m_masterTables[itr.key()] = itr2.key();
                    m_masterColumns[itr2.key()] = table.value("column").toString();
                    continue;
                }

                QJsonArray tableColumns = table.value("columns").toArray();
                QStringList columns;
                for (auto itr3 = tableColumns.constBegin(); itr3 != tableColumns.constEnd(); ++itr3)
                    columns << (*itr3).toString();
                m_columns[ColumnPair(itr.key(), itr2.key())] = columns;

                tables << itr2.key();
            }

            m_tables[itr.key()] = tables;
        }

        slotChangeTablesGroup(0);

        QSqlDatabase db = QSqlDatabase::addDatabase("QMYSQL");

        QJsonObject dbConfig = config.value("Database").toObject();
        db.setHostName(dbConfig.value("hostname").toString());
        db.setPort(dbConfig.value("port").toInt());
        db.setUserName(dbConfig.value("username").toString());
        db.setPassword(dbConfig.value("password").toString());
        db.setDatabaseName(dbConfig.value("database").toString());

        if (!db.open())
            QMessageBox::warning(this, tr("Unable to open database"), tr("An error occured while opening the connection: ") + db.lastError().text());
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::slotAll()
{
    reorderMaster();
    reorderSlaves();
}

void MainWindow::slotMaster()
{
    reorderMaster();
}

void MainWindow::slotCurrentSlave()
{
    reorderSlaves(ui->comboBox_2->currentText());
}

void MainWindow::slotAllSlaves()
{
    reorderSlaves();
}

void MainWindow::slotChangeTablesGroup(int index)
{
    ui->comboBox_2->clear();

    QString group = ui->comboBox->itemText(index);
    ui->lineEdit->setText(m_masterTables[group]);
    ui->comboBox_2->addItems(m_tables[group]);
}

void MainWindow::reorderMaster()
{
    QString masterTable = ui->lineEdit->text();
    QString masterColumn = m_masterColumns[masterTable];

    QSqlDatabase db = QSqlDatabase::database();

    QString type;
    QSqlQuery result(QString("SHOW COLUMNS FROM %0").arg(masterTable));
    while (result.next())
    {
        if (masterColumn == result.value(0).toString())
        {
            type = result.value(1).toString();
            break;
        }
    }

    db.transaction();
    QSqlQuery(QString("ALTER TABLE `%0` ADD COLUMN `_%1` %2 DEFAULT 0 NOT NULL AFTER `%1`")
              .arg(masterTable)
              .arg(masterColumn)
              .arg(type));
    QSqlQuery(QString("UPDATE `%0` SET `_%1` = `%1`")
              .arg(masterTable)
              .arg(masterColumn));
    QSqlQuery("SET @i=0;");
    QSqlQuery(QString("UPDATE `%0` SET `_%1` = (@i:= @i + 1);")
              .arg(masterTable)
              .arg(masterColumn));
    db.commit();
}

void MainWindow::reorderSlaves(QString str)
{
    QSqlDatabase db = QSqlDatabase::database();

    QString group = ui->comboBox->currentText();
    QString masterTable = ui->lineEdit->text();
    QString masterColumn = m_masterColumns[masterTable];

    if (str.isEmpty())
    {
        quint8 tablesCount = ui->comboBox_2->count();
        for (quint8 i = 0; i < tablesCount; ++i)
        {
            QString table = ui->comboBox_2->itemText(i);
            QStringList columns = m_columns[ColumnPair(group, table)];

            for (auto& column : columns)
            {
                QString type;
                QSqlQuery result(QString("SHOW COLUMNS FROM %0").arg(table));
                while (result.next())
                {
                    if (column == result.value(0).toString())
                    {
                        type = result.value(1).toString();
                        break;
                    }
                }

                db.transaction();
                QSqlQuery(QString("ALTER TABLE `%0` ADD COLUMN `_%1` %2 DEFAULT 0 NOT NULL AFTER `%1`")
                          .arg(table)
                          .arg(column)
                          .arg(type));
                QSqlQuery(QString("UPDATE `%0` SET `_%1` = `%1`")
                          .arg(table)
                          .arg(column));
                QSqlQuery(QString("UPDATE `%0`, `%1` SET `%0`.`_%2` = `%1`.`_%3` WHERE `%0`.`%2` = `%1`.`%3`")
                          .arg(table)
                          .arg(masterTable)
                          .arg(column)
                          .arg(masterColumn));
                db.commit();
            }
        }
    }
    else
    {
        QStringList columns = m_columns[ColumnPair(group, str)];

        for (auto& column : columns)
        {
            QString type;
            QSqlQuery result(QString("SHOW COLUMNS FROM %0").arg(str));
            while (result.next())
            {
                if (column == result.value(0).toString())
                {
                    type = result.value(1).toString();
                    break;
                }
            }

            db.transaction();
            QSqlQuery(QString("ALTER TABLE `%0` ADD COLUMN `_%1` %2 DEFAULT 0 NOT NULL AFTER `%1`")
                      .arg(str)
                      .arg(column)
                      .arg(type));
            QSqlQuery(QString("UPDATE `%0` SET `_%1` = `%1`")
                      .arg(str)
                      .arg(column));
            QSqlQuery(QString("UPDATE `%0`, `%1` SET `%0`.`_%2` = `%1`.`_%3` WHERE `%0`.`%2` = `%1`.`%3`")
                      .arg(str)
                      .arg(masterTable)
                      .arg(column)
                      .arg(masterColumn));
            db.commit();
        }
    }
}
