#include "Headers/mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QSqlDatabase& db, QWidget * parent) :
	QMainWindow(parent)
	, ui(new Ui::MainWindow)
	, db(db)
	, logger(*new DBLogger)
{
	ui->setupUi(this);

	progressBar = new QProgressBar(ui->statusBar);
	progressBar->setRange(0, 100);
	progressBar->setValue(0);
	progressBar->setMaximumSize(150, 18);
	logger.setProgressBar(progressBar);

	ui->mainSplitter->setSizes({ 150, 600 });
	ui->splitter->setSizes({ 400, 200 });

	queries = new SqlEditor(ui->tWUpper);
	queries->textCursor().insertText("--aS");
	queries->textCursor().insertBlock();
	queries->textCursor().insertText("SELECT * FROM emp;");
	queries->setFont(QFont("Segoe UI", 11));
	SqlHighlighter * highLighter = new SqlHighlighter();
	highLighter->setDocument(queries->document());
	QString dbName = db.hostName();
	ui->tWUpper->addTab(queries, dbName);

	QTreeWidget * treeWidget = ui->trWLeft;
	treeWidget->setColumnCount(1);
	QList<QTreeWidgetItem *> items;
	QTreeWidgetItem * tables	= new QTreeWidgetItem(QStringList(queries::TABLES));
	QTreeWidgetItem * indexes	= new QTreeWidgetItem(QStringList(queries::INDEXES));
	QTreeWidgetItem * views		= new QTreeWidgetItem(QStringList(queries::VIEWS));
	QTreeWidgetItem * functions = new QTreeWidgetItem(QStringList(queries::FUNCTIONS));
	items.append(tables);
	items.append(indexes);
	items.append(views);
	items.append(functions);
	treeWidget->insertTopLevelItems(0, items);
	treeWidget->setHeaderLabel(dbName);
	ui->statusBar->addPermanentWidget(progressBar, 0);

	ui->tWLower->addTab(&logger, "Adatbázis log");
	logger.appendPlainText("Sikeresen csatlakozva az adatbázishoz!");

	ui->tWUpper->tabBar()->tabButton(0, QTabBar::RightSide)->resize(0, 0);
	ui->tWLower->tabBar()->tabButton(0, QTabBar::RightSide)->resize(0, 0);

	QShortcut * shortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_Return), queries);
	QObject::connect(shortcut, SIGNAL(activated()), this, SLOT(executeQuery()));

	shortcut = new QShortcut(QKeySequence(Qt::Key_F4), queries);
	QObject::connect(shortcut, SIGNAL(activated()), this, SLOT(showExecutionPlan()));
}


bool MainWindow::fillList(QTreeWidgetItem* list, const QString& queryToExecute)
{
	QSqlQuery query = db.exec(queryToExecute);
	if (!query.isActive())
	{
		return false;
	}
	while (query.next())
	{
		list->insertChild(0, new QTreeWidgetItem(
			QStringList(query.value(0).toString()))
		);
	}
	return true;
}

void MainWindow::on_trWLeft_itemDoubleClicked(QTreeWidgetItem* item, int /*column*/)
{
	std::function<bool(QString)> toExecute;
	if (queries::TABLES == item->text(0))
	{
		toExecute = [&] (QString s) { return fillTableList(item); };
	}
	else if (queries::INDEXES == item->text(0))
	{
		toExecute = [&] (QString s) { return fillIndexList(item); };
	}
	else if (queries::VIEWS == item->text(0))
	{
		toExecute = [&] (QString s) { return fillViewList(item); };
	}
	else if (queries::FUNCTIONS == item->text(0))
	{
		toExecute = [&] (QString s) { return fillFunctionList(item); };
	}
	logger.logWithTime("Adatbázisban található objektumok sikeresen lekérdezve.", "Hiba történt a lekérdezés végrehajtása közben!", toExecute);
}

/*
 * Finds and executes query "near" the cursor in the current document.
 * It selects every text until EOL, from previous ;.
 *
 */
void MainWindow::executeQuery()
{
	QSqlQuery * q = new QSqlQuery(db);
	QString query = queries->extractQuery();
    std::function<bool(QString)> toExecute = [&] (QString s) {
			q->exec(query);
			s = q->lastError().text();
			return q->isActive(); };

	QString firstWord = query.split(' ').at(0).toUpper();
	if ("SELECT" == firstWord)
	{
		bool success = logger.logWithTime("Lekérdezés sikeresen végrehajtva!", "Lekérdezés sikertelen.", toExecute);
		if (success)
		{
			QSqlQueryModel* model = new QSqlQueryModel;
			model->setQuery(*q);
			QTableView *view = new QTableView(ui->tWLower);
			view->setModel(model);
			view->show();
			ui->tWLower->addTab(view, "Lekérdezés eredménye");
			ui->tWLower->setCurrentWidget(view);
		}
	}
	else if ("MAKE" == firstWord)
	{
		/// TODO: implement make chart command
	}
	else
	{
		logger.logWithTime("Művelet sikeresen végrehajtva.", "Művelet sikertelen.", toExecute);
	}
}

void MainWindow::showExecutionPlan()
{
    std::function<bool(QString)> toExecute = [&] (QString failMessage) {
		QSqlQuery * q = new QSqlQuery(db);
		QString query = queries->extractQuery();
		QString explainPlan = QString("EXPLAIN PLAN SET STATEMENT_ID = 'temp1' FOR %1").arg(query);
		bool success = q->exec(explainPlan);
		if (success)
		{
			success = q->exec("SELECT cardinality \"Rows\", \
				lpad(' ',level-1)||operation||' '||options||' '||object_name \"Plan\" \
				FROM PLAN_TABLE \
				CONNECT BY prior id = parent_id \
				AND prior statement_id = statement_id \
				START WITH id = 0 \
				AND statement_id = 'temp1' \
				ORDER BY id");
			//      q->exec("SELECT PLAN_TABLE_OUTPUT FROM TABLE(DBMS_XPLAN.DISPLAY())");
			if (success)
			{
				QSqlQueryModel* model = new QSqlQueryModel;
				model->setQuery(*q);
				QTableView *view = new QTableView(ui->tWLower);
				view->setModel(model);
				view->show();
				ui->tWLower->addTab(view, "Lekérdezési terv");
				ui->tWLower->setCurrentWidget(view);
				success = q->exec("DELETE FROM PLAN_TABLE WHERE statement_id = 'temp1'");
			}
		}
		failMessage = db.lastError().text();
		return success;
    };
	logger.logWithTime("Lekérdezési terv sikeresen elkészült!", "Lekérdezési terv létrehozása sikertelen.", toExecute);
}

void MainWindow::on_actionT_rl_s_triggered()
{
	QTreeWidget* tw = ui->trWLeft;
	if (tw->selectedItems().size() > 0)
	{
		QTreeWidgetItem* child = tw->selectedItems().at(0);
		QString parent = child->parent()->text(0);
		std::function<bool(QString)> toExecute;
		if (queries::TABLES == parent)
		{
			toExecute = [&](QString failMessage) {
				bool success = db.exec(QString("DROP TABLE %1").arg(child->text(0))).isActive();
				failMessage = db.lastError().text();
				return success; };
		}
		else if (queries::INDEXES == parent)
		{
			toExecute = [&](QString failMessage) {
				bool success = db.exec(QString("DROP INDEX %1").arg(child->text(0))).isActive();
				failMessage = db.lastError().text();
				return success; };
		}
		else if (queries::VIEWS == parent)
		{
			toExecute = [&](QString failMessage) {
				bool success = db.exec(QString("DROP VIEW %1").arg(child->text(0))).isActive();
				failMessage = db.lastError().text();
				return success; };
		}
		else if (queries::FUNCTIONS == parent)
		{
			toExecute = [&](QString failMessage) {
				bool success = db.exec(QString("DROP FUNCTION %1").arg(child->text(0))).isActive();
				failMessage = db.lastError().text();
				return success; };
		}
		logger.logWithTime("Adatbázisban található objektum sikeresen törölve.", "Hiba történt a törlés végrehajtása közben!", toExecute);
		delete child;
	}
}

inline
MainWindow::~MainWindow()
{
	delete ui;
	delete progressBar;
	delete queries;
	delete &db;
	delete &logger;
}

inline
void MainWindow::on_tWUpper_tabCloseRequested(int index)
{
	delete ui->tWUpper->widget(index);
}

inline
void MainWindow::on_tWLower_tabCloseRequested(int index)
{
	delete ui->tWLower->widget(index);
}