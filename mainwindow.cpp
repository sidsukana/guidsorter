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

    connect(ui->pushButton_9, SIGNAL(clicked()), this, SLOT(slotDoAll()));
    connect(ui->pushButton_8, SIGNAL(clicked()), this, SLOT(slotFlushAll()));
    connect(ui->pushButton_7, SIGNAL(clicked()), this, SLOT(slotFlushAllSlaves()));
    connect(ui->pushButton_6, SIGNAL(clicked()), this, SLOT(slotFlushCurrentSlave()));
    connect(ui->pushButton_5, SIGNAL(clicked()), this, SLOT(slotFlushMaster()));
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

                if (table.value("master").toBool())
                {
                    m_masterTables[itr.key()] = itr2.key();
                    m_masterColumns[itr2.key()] = table.value("column").toString();
                    continue;
                }

                if (table.value("columns").isObject())
                {
                    QJsonObject tableColumns = table.value("columns").toObject();
                    QStringList columns;
                    for (auto itr3 = tableColumns.constBegin(); itr3 != tableColumns.constEnd(); ++itr3)
                    {
                        columns << itr3.key();
                        QJsonObject objCol = itr3.value().toObject();
                        QJsonValue condition = objCol.value("condition");
                        if (!condition.isUndefined())
                            m_conditions[StringPair(itr2.key(), itr3.key())] = condition.toString();
                        else
                            m_conditions[StringPair(itr2.key(), itr3.key())] = "";
                    }
                    m_columns[StringPair(itr.key(), itr2.key())] = columns;
                }
                else
                {
                    QJsonArray tableColumns = table.value("columns").toArray();
                    QStringList columns;
                    for (auto itr3 = tableColumns.constBegin(); itr3 != tableColumns.constEnd(); ++itr3)
                    {
                        columns << (*itr3).toString();
                        m_conditions[StringPair(itr2.key(), (*itr3).toString())] = "";
                    }
                    m_columns[StringPair(itr.key(), itr2.key())] = columns;
                }

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

void MainWindow::slotDoAll()
{
    slotAll();
    slotFlushAll();
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

void MainWindow::slotFlushMaster()
{
    flushMasterFields();
}

void MainWindow::slotFlushCurrentSlave()
{
    flushSlavesFields(ui->comboBox_2->currentText());
}

void MainWindow::slotFlushAllSlaves()
{
    flushSlavesFields();
}

void MainWindow::slotFlushAll()
{
    flushMasterFields();
    flushSlavesFields();
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
            QStringList columns = m_columns[StringPair(group, table)];

            for (auto& column : columns)
            {
                QString type;
                QSqlQuery result(QString("SHOW COLUMNS FROM `%0`").arg(table));
                while (result.next())
                {
                    if (column == result.value(0).toString())
                    {
                        type = result.value(1).toString();
                        break;
                    }
                }

                QString condition = m_conditions[StringPair(table, column)];

                db.transaction();
                QSqlQuery(QString("ALTER TABLE `%0` ADD COLUMN `_%1` %2 DEFAULT 0 NOT NULL AFTER `%1`")
                          .arg(table)
                          .arg(column)
                          .arg(type));
                QSqlQuery(QString("UPDATE `%0` SET `_%1` = `%1`")
                          .arg(table)
                          .arg(column));
                QSqlQuery(QString("UPDATE `%0` AS `slave`, `%1` AS `master` SET `slave`.`_%2` = `master`.`_%3` "
                                  "WHERE `slave`.`%2` = `master`.`%3` %4")
                          .arg(table)
                          .arg(masterTable)
                          .arg(column)
                          .arg(masterColumn)
                          .arg(condition));
                db.commit();
            }
        }
    }
    else
    {
        QStringList columns = m_columns[StringPair(group, str)];

        for (auto& column : columns)
        {
            QString type;
            QSqlQuery result(QString("SHOW COLUMNS FROM `%0`").arg(str));
            while (result.next())
            {
                if (column == result.value(0).toString())
                {
                    type = result.value(1).toString();
                    break;
                }
            }

            QString condition = m_conditions[StringPair(str, column)];

            db.transaction();
            QSqlQuery(QString("ALTER TABLE `%0` ADD COLUMN `_%1` %2 DEFAULT 0 NOT NULL AFTER `%1`")
                      .arg(str)
                      .arg(column)
                      .arg(type));
            QSqlQuery(QString("UPDATE `%0` SET `_%1` = `%1`")
                      .arg(str)
                      .arg(column));
            QSqlQuery(QString("UPDATE `%0` AS `slave`, `%1` AS `master` SET `slave`.`_%2` = `master`.`_%3` "
                              "WHERE `slave`.`%2` = `master`.`%3` %4")
                      .arg(str)
                      .arg(masterTable)
                      .arg(column)
                      .arg(masterColumn)
                      .arg(condition));
            db.commit();
        }
    }
}

void MainWindow::flushMasterFields()
{
    QString masterTable = ui->lineEdit->text();
    QString masterColumn = m_masterColumns[masterTable];

    QSqlDatabase db = QSqlDatabase::database();

    QMap<QString, QStringList> indexMap;
    QMap<QString, bool> uniques;
    QSqlQuery result(QString("SHOW INDEX FROM `%0`").arg(masterTable));
    while (result.next())
    {
        QString indexName = result.value(2).toString();
        QStringList indexColumns = indexMap[result.value(2).toString()];
        indexColumns << result.value(4).toString();
        indexMap[indexName] = indexColumns;
        uniques[indexName] = !result.value(1).toBool();
    }

    QString query = QString("ALTER TABLE `%0` ").arg(masterTable);

    QSqlQuery res(QString("SHOW COLUMNS FROM `%0` WHERE `Field` = '%1'")
                  .arg(masterTable).arg(masterColumn));

    if (res.next())
    {
        query += QString("DROP COLUMN `%0`")
                .arg(masterColumn);
        query += ", ";
        query += QString("CHANGE COLUMN `_%0` `%0` %1 DEFAULT 0 NOT NULL")
                .arg(masterColumn).arg(res.value(1).toString());
    }

    for (auto itr = indexMap.constBegin(); itr != indexMap.constEnd(); ++itr)
    {
        QString key = itr.key() == "PRIMARY" ? itr.key() + " KEY" : "INDEX `" + itr.key() + "`";
        if (query.contains(key))
            continue;

        QStringList idxColumns = itr.value();
        if (idxColumns.contains(masterColumn))
        {
            query += ", ";

            if (itr.key() == "PRIMARY")
            {
                query += "DROP " + itr.key() + " KEY";
                query += ", ";
                query += "ADD " + itr.key() + " KEY (";

                bool first = true;
                for (auto& idxColumn : idxColumns)
                {
                    if (!first)
                        query += ", ";
                    query += QString("`%0`").arg(idxColumn);
                    first = false;
                }
                query += ")";
            }
            else
            {
                query += "DROP INDEX " + QString("`%0`").arg(itr.key());
                query += ", ";

                if (uniques[itr.key()])
                    query += "ADD UNIQUE INDEX " + QString("`%0`").arg(itr.key()) + " (";
                else
                    query += "ADD INDEX " + QString("`%0`").arg(itr.key()) + " (";

                bool first = true;
                for (auto& idxColumn : idxColumns)
                {
                    if (!first)
                        query += ", ";
                    query += QString("`%0`").arg(idxColumn);
                    first = false;
                }
                query += ")";
            }
        }
    }

    query += ";";

    QSqlQuery(query, db);
}

void MainWindow::flushSlavesFields(QString str)
{
    QSqlDatabase db = QSqlDatabase::database();

    QString group = ui->comboBox->currentText();

    if (str.isEmpty())
    {
        quint8 tablesCount = ui->comboBox_2->count();
        for (quint8 i = 0; i < tablesCount; ++i)
        {
            QString table = ui->comboBox_2->itemText(i);

            QMap<QString, QStringList> indexMap;
            QMap<QString, bool> uniques;
            QSqlQuery result(QString("SHOW INDEX FROM `%0`").arg(table));
            while (result.next())
            {
                QString indexName = result.value(2).toString();
                QStringList indexColumns = indexMap[result.value(2).toString()];
                indexColumns << result.value(4).toString();
                indexMap[indexName] = indexColumns;
                uniques[indexName] = !result.value(1).toBool();
            }

            QStringList columns = m_columns[StringPair(group, table)];

            QString query = QString("ALTER TABLE `%0` ").arg(table);

            bool first = true;
            for (auto& column : columns)
            {
                QSqlQuery res(QString("SHOW COLUMNS FROM `%0` WHERE `Field` = '%1'")
                              .arg(table).arg(column));

                if (res.next())
                {
                    if (!first)
                        query += ", ";

                    query += QString("DROP COLUMN `%0`")
                            .arg(column);
                    query += ", ";
                    query += QString("CHANGE COLUMN `_%0` `%0` %1 DEFAULT 0 NOT NULL")
                            .arg(column).arg(res.value(1).toString());

                    first = false;
                }
            }

            for (auto& column : columns)
            {
                for (auto itr = indexMap.constBegin(); itr != indexMap.constEnd(); ++itr)
                {
                    QString key = itr.key() == "PRIMARY" ? itr.key() + " KEY" : "INDEX `" + itr.key() + "`";
                    if (query.contains(key))
                        continue;

                    QStringList idxColumns = itr.value();
                    if (idxColumns.contains(column))
                    {
                        query += ", ";

                        if (itr.key() == "PRIMARY")
                        {
                            query += "DROP " + itr.key() + " KEY";
                            query += ", ";
                            query += "ADD " + itr.key() + " KEY (";

                            first = true;
                            for (auto& idxColumn : idxColumns)
                            {
                                if (!first)
                                    query += ", ";
                                query += QString("`%0`").arg(idxColumn);
                                first = false;
                            }
                            query += ")";
                        }
                        else
                        {
                            query += "DROP INDEX " + QString("`%0`").arg(itr.key());
                            query += ", ";

                            if (uniques[itr.key()])
                                query += "ADD UNIQUE INDEX " + QString("`%0`").arg(itr.key()) + " (";
                            else
                                query += "ADD INDEX " + QString("`%0`").arg(itr.key()) + " (";

                            first = true;
                            for (auto& idxColumn : idxColumns)
                            {
                                if (!first)
                                    query += ", ";
                                query += QString("`%0`").arg(idxColumn);
                                first = false;
                            }
                            query += ")";
                        }
                    }
                }
            }
            query += ";";

            QSqlQuery(query, db);
        }
    }
    else
    {
        QMap<QString, QStringList> indexMap;
        QMap<QString, bool> uniques;
        QSqlQuery result(QString("SHOW INDEX FROM `%0`").arg(str));
        while (result.next())
        {
            QString indexName = result.value(2).toString();
            QStringList indexColumns = indexMap[result.value(2).toString()];
            indexColumns << result.value(4).toString();
            indexMap[indexName] = indexColumns;
            uniques[indexName] = !result.value(1).toBool();
        }

        QStringList columns = m_columns[StringPair(group, str)];

        QString query = QString("ALTER TABLE `%0` ").arg(str);

        bool first = true;
        for (auto& column : columns)
        {
            QSqlQuery res(QString("SHOW COLUMNS FROM `%0` WHERE `Field` = '%1'")
                          .arg(str).arg(column));

            if (res.next())
            {
                if (!first)
                    query += ", ";

                query += QString("DROP COLUMN `%0`")
                        .arg(column);
                query += ", ";
                query += QString("CHANGE COLUMN `_%0` `%0` %1 DEFAULT 0 NOT NULL")
                        .arg(column).arg(res.value(1).toString());

                first = false;
            }
        }

        for (auto& column : columns)
        {
            for (auto itr = indexMap.constBegin(); itr != indexMap.constEnd(); ++itr)
            {
                QString key = itr.key() == "PRIMARY" ? itr.key() + " KEY" : "INDEX `" + itr.key() + "`";
                if (query.contains(key))
                    continue;

                QStringList idxColumns = itr.value();
                if (idxColumns.contains(column))
                {
                    query += ", ";

                    if (itr.key() == "PRIMARY")
                    {
                        query += "DROP " + itr.key() + " KEY";
                        query += ", ";
                        query += "ADD " + itr.key() + " KEY (";

                        first = true;
                        for (auto& idxColumn : idxColumns)
                        {
                            if (!first)
                                query += ", ";
                            query += QString("`%0`").arg(idxColumn);
                            first = false;
                        }
                        query += ")";
                    }
                    else
                    {
                        query += "DROP INDEX " + QString("`%0`").arg(itr.key());
                        query += ", ";

                        if (uniques[itr.key()])
                            query += "ADD UNIQUE INDEX " + QString("`%0`").arg(itr.key()) + " (";
                        else
                            query += "ADD INDEX " + QString("`%0`").arg(itr.key()) + " (";

                        first = true;
                        for (auto& idxColumn : idxColumns)
                        {
                            if (!first)
                                query += ", ";
                            query += QString("`%0`").arg(idxColumn);
                            first = false;
                        }
                        query += ")";
                    }
                }
            }
        }
        query += ";";

        QSqlQuery(query, db);
    }
}
