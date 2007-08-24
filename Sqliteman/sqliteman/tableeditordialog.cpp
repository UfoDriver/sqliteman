/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QTableWidget>
#include <QCheckBox>
#include <QtDebug>

#include "tableeditordialog.h"
#include "sqleditor.h"


TableEditorDialog::TableEditorDialog(QWidget * parent)//, Mode mode, const QString & tableName): QDialog(parent)
{
	ui.setupUi(this);
	new SqlEditorTools::SqlHighlighter(ui.textEdit->document());
	ui.databaseCombo->addItems(Database::getDatabases().keys());

	connect(ui.nameEdit, SIGNAL(textChanged(const QString&)),
			this, SLOT(nameEdit_textChanged(const QString&)));
	connect(ui.columnTable, SIGNAL(itemSelectionChanged()), this, SLOT(fieldSelected()));
	connect(ui.addButton, SIGNAL(clicked()), this, SLOT(addField()));
	connect(ui.removeButton, SIGNAL(clicked()), this, SLOT(removeField()));
	connect(ui.tabWidget, SIGNAL(currentChanged(int)), this, SLOT(tabWidget_currentChanged(int)));
	connect(ui.createButton, SIGNAL(clicked()), this, SLOT(createButton_clicked()));
}

void TableEditorDialog::createButton_clicked()
{
	qDebug() << "createButton_clicked() not implemented";
}

QComboBox * TableEditorDialog::makeTypeBox()
{
	QComboBox * box;
	QStringList types;
	 
	types << "Text" << "Primary Key" << "Integer" << "Real" << "Blob" << "Null";
	
	box = new QComboBox();
	box->addItems(types);
	
	return box;
}

// void TableEditorDialog::addField(DatabaseTableField * column)
void TableEditorDialog::addField()
{
	int rc = ui.columnTable->rowCount();
	ui.columnTable->setRowCount(rc + 1);
	ui.columnTable->setCellWidget(rc, 1, makeTypeBox());
	ui.columnTable->setCellWidget(rc, 2, new QCheckBox(this));
}

void TableEditorDialog::removeField()
{
	ui.columnTable->removeRow(ui.columnTable->currentRow());
	if (ui.columnTable->rowCount() == 0)
		addField();
}

void TableEditorDialog::fieldSelected()
{
	ui.removeButton->setEnabled(ui.columnTable->selectedRanges().count() != 0);
}

void TableEditorDialog::nameEdit_textChanged(const QString& text)
{
	ui.createButton->setDisabled(text.simplified().isEmpty());
}

void TableEditorDialog::tabWidget_currentChanged(int index)
{
	if (index == 1)
		ui.createButton->setEnabled(true);
	else
		nameEdit_textChanged(ui.nameEdit->text());
}

QString TableEditorDialog::getFullName(const QString & objName)
{
	return QString("\"%1\".\"%2\"").arg(ui.databaseCombo->currentText()).arg(objName);
}

DatabaseTableField TableEditorDialog::getColumn(int row)
{
	DatabaseTableField field;
	QTableWidgetItem * nameItem = ui.columnTable->item(row, 0);

	// skip rows with no column name
	if (nameItem == 0)
	{
		field.cid = -1;
		return field;
	}

	QString type;
	if (ui.columnTable->cellWidget(row, 1) == 0)
		type = "";
	else
	{
		QComboBox * typeBox = qobject_cast<QComboBox *>(ui.columnTable->cellWidget(row, 1));
		type = typeBox->currentText();
	}
	bool nn = qobject_cast<QCheckBox*>(ui.columnTable->cellWidget(row, 2))->checkState() == Qt::Checked;

	// For user convinence reasons, the type "INTEGER PRIMARY KEY" is presented to the user
	// as "Primary Key" alone. Therefor, untill there is a more robust solution (which will
	// support translation of type names as well) the primary key type needs to be corrected
	// at update time.
	bool pk = false;
	if (type == "Primary Key")
	{
		type = "Integer Primary Key";
		pk = true;
	}

	field.cid = 0;
	field.name = nameItem->text();
	field.type = type.toUpper();
	field.notnull = nn;
	field.defval = (ui.columnTable->item(row, 3) == 0) ? "" : ui.columnTable->item(row, 3)->text();
	field.pk = pk;
	field.comment = "";

	return field;
}

QString TableEditorDialog::getDefaultClause(const QString & defVal)
{
	if (defVal.isNull() || defVal.isEmpty())
		return "";
	bool ok;
	defVal.toDouble(&ok);
	if (ok)
		return QString(" DEFAULT (%1)").arg(defVal);
	else if (defVal.simplified().left(1) == "'")
		return QString(" DEFAULT (%1)").arg(defVal);
	else
		return QString(" DEFAULT ('%1')").arg(defVal);
}