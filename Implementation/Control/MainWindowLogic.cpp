#include "MainWindowLogic.h"

MainWindowLogic::MainWindowLogic(DBLogger* logger, QSqlDatabase* db
                                 , SqlEditor* queries)
 : logger(logger)
 , db(db)
 , queries(queries)
{ }

void MainWindowLogic::deleteDbObject(const QString& toDelete, const QString& parent)
{
    std::function<bool(QString&)> toExecute = [&](QString& failMessage)
    {
        bool deleted = false;
        if (queries::TABLES == parent)
        {
            deleted = db->exec(QString("DROP TABLE %1").arg(toDelete)).isActive();
        }
        else if (queries::INDEXES == parent)
        {
            deleted = db->exec(QString("DROP INDEX %1").arg(toDelete)).isActive();
        }
        else if (queries::VIEWS == parent)
        {
            deleted = db->exec(QString("DROP VIEW %1").arg(toDelete)).isActive();
        }
        else if (queries::FUNCTIONS == parent)
        {
            deleted = db->exec(QString("DROP FUNCTION %1").arg(toDelete)).isActive();
        }
        failMessage = db->lastError().text();
        return deleted;
    };

    logger->logWithTime("Adatbázisban található objektum sikeresen törölve."
                       , "Hiba történt a törlés végrehajtása közben!"
                        , toExecute);
}

QTableView* MainWindowLogic::viewDbObject(const QString& parent
                                          , const QString& child
                                          , QTabWidget* editor)
{
    std::function<bool(QString&)> toExecute;
    QSqlQuery* q = new QSqlQuery;
    QTableView* view = new QTableView(editor);

    toExecute = [&](QString& failMessage)
    {
        bool success = false;
        if (queries::TABLES == parent)
        {
            success = q->exec(queries::SELECT_TABLES(child));
        }
        else if (queries::INDEXES == parent)
        {
            success = q->exec(queries::SELECT_INDEXES(child));
        }
        else if (queries::VIEWS == parent)
        {
            success = q->exec(queries::SELECT_VIEWS(child));
        }
        else if (queries::FUNCTIONS == parent)
        {
            success = q->exec(queries::SELECT_FUNCTIONS(child));
        }
        failMessage = q->lastError().text();
        if (success)
        {
            QSqlQueryModel* model = new QSqlQueryModel;
            model->setQuery(*q);
            view->setModel(model);
            const int colCount = model->columnCount();
            const int width = editor->width() - tables::SIDEBAR_SIZE - 20;
            for (int i = 0; i < colCount; ++i)
            {
                view->setColumnWidth(i, width / colCount);
            }
            view->show();
            editor->addTab(view, child);
            editor->setCurrentWidget(view);
        }
        return success;
    };

    boxes->setCurrentWidget(logger);
    logger->logWithTime("Adatbázisban található objektum sikeresen lekérdezve."
                       , "Hiba történt a lekérdezés végrehajtása közben!"
                       , toExecute);

    delete q;
    return view;
}

void MainWindowLogic::dbObjectClicked(std::function<bool (QString&)>& toExecute)
{
    boxes->setCurrentWidget(logger);
    logger->logWithTime("Adatbázisban található objektumok sikeresen lekérdezve."
                       , "Hiba történt a lekérdezés végrehajtása közben!"
                       , toExecute);
}

bool MainWindowLogic::createList(const QString& queryToExecute
                                 , QList<QTreeWidgetItem *>& list)
{
    boxes->setCurrentWidget(logger);
    QSqlQuery query;
    query.setForwardOnly(true);
    query.exec(queryToExecute);

    if (!query.isActive())
        return false;

    while (query.next())
    {
        const QString item = query.value(0).toString();
        list.append(new QTreeWidgetItem({item}));
    }

    return true;
}

QTreeWidget* MainWindowLogic::createExecutionPlan(const int width)
{
    boxes->setCurrentWidget(logger);
    QTreeWidget* tree = new QTreeWidget();

    std::function<bool(QString&)> toExecute = [&] (QString& failMessage)
    {
        QSqlQuery* q = new QSqlQuery(*db);
        QString query;
        QString selectedText = queries->textCursor().selectedText();
        if (!selectedText.isEmpty())
        {
            selectedText = selectedText.remove(';');
            query = selectedText.simplified();
        }
        else
        {
            query = queries->extractQuery();
        }
        QString explainPlan = QString(
                    "EXPLAIN PLAN SET STATEMENT_ID = 'temp1' FOR %1"
                    ).arg(query);
        q->exec("DELETE FROM PLAN_TABLE WHERE statement_id = 'temp1'");
        bool success = q->exec(explainPlan);
        if (!success)
        {
            failMessage = q->lastError().text();
            return success;
        }
        success = q->exec("select cardinality, \
                            operation || ' ' || options || ' ' || object_name, \
                            cost, \
                            cpu_cost, parent_id from plan_table WHERE \
                            statement_id = 'temp1' order by id");

        if (success)
        {
            QVector<QTreeWidgetItem*> plans;

            while (q->next())
            {
                const QString row = q->value(0).isNull() ?
                    "" : q->value(0).toString();
                const QString plan = q->value(1).toString().trimmed();
                const QString ioCost = q->value(2).isNull() ?
                    "" : q->value(2).toString();
                const QString cpuCost = q->value(3).isNull() ?
                    "" : q->value(3).toString();
                const int parent_id = q->value(4).isNull() ?
                    -1 : q->value(4).toInt();
                QTreeWidgetItem* temp = new QTreeWidgetItem(
                    QStringList({ plan, row, ioCost, cpuCost })
                    );

                plans.append(temp);
                if (parent_id >= 0 && parent_id < plans.size())
                {
                    plans[parent_id]->addChild(temp);
                }
            }

            tree->setColumnCount(4);
            tree->setHeaderLabels({ "Lekérdezési terv", "Sorok"
                        , "IO költség", "CPU költség" });
            QList<QTreeWidgetItem*> temp = plans.toList();
            tree->addTopLevelItem(temp.front());

            const int colCount = tree->columnCount() - 1;
            const int bWidth = width - 20 - 500;
            tree->setColumnWidth(0, 500);
            for (int i = 1; i < colCount; ++i)
            {
                tree->setColumnWidth(i, bWidth / colCount);
            }

            success = q->exec("DELETE FROM PLAN_TABLE WHERE statement_id = 'temp1'");
        }
        failMessage = q->lastError().text();
        delete q;
        if (!success)
            tree->setColumnCount(1);
        return success;
    };

    logger->logWithTime("Lekérdezési terv sikeresen elkészült!"
                       , "Lekérdezési terv létrehozása sikertelen."
                       , toExecute);

    return tree;
}

void MainWindowLogic::executeString(const QString& query, QTabWidget* editor)
{
    boxes->setCurrentWidget(logger);
    if (query.isEmpty())
    {
        logger->log("Hiba: üres lekérdezést próbált végrehajtani!");
        return;
    }

    QSqlQuery* q = new QSqlQuery(*db);
    std::function<bool (QString&)> toExecute = [&] (QString& failMessage)
    {
            q->exec(query);
            failMessage = q->lastError().text();
            bool success = q->isActive();
            if (success && q->isSelect())
            {
                QSqlQueryModel* model = new QSqlQueryModel;
                QTableView* view = new QTableView(boxes);
                model->setQuery(*q);
                view->setModel(model);
                view->show();
                boxes->addTab(view, "Lekérdezés eredménye");
                boxes->setCurrentWidget(view);
            }
            return success;
    };

    QStringList words = QStringList(query.split(' '));
    for (auto& i : words)
    {
        i = i.toUpper();
    }

    if (words.size() >= 1 && "SELECT" == words.at(0))
    {
        logger->logWithTime("Lekérdezés sikeresen végrehajtva!"
                          , "Lekérdezés sikertelen."
                          , toExecute);
    }
    else if (words.size() >= 2 && "MAKE" == words.at(0))
    {
        if ("CHART" == words.at(1))
        {
            QT_CHARTS_USE_NAMESPACE

            QString message;
            QChart* chart = new QChart();
            bool success;

            toExecute = [&] (QString& msg)
            {
                success = queries->makeChart(msg, chart, &words, query, q);
                message = msg;
                if (success)
                {
                    QChartView* chartView = new QChartView(chart);
                    chartView->setRenderHint(QPainter::Antialiasing);
                    editor->addTab(chartView, "Diagram eredménye");
                    editor->setCurrentWidget(chartView);
                }
                return success;
            };

            logger->logWithTime("Diagram sikeresen létrehozva!"
                               , "Diagram létrehozása közben hiba történt."
                               , toExecute);
        }
    }
    else
    {
        logger->logWithTime("Művelet sikeresen végrehajtva."
                           , "Művelet sikertelen.", toExecute);
    }

    delete q;
}
