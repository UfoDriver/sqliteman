/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
FIXME if table name contains non-alphanumeric characters, no rows are displayed,
although they are actually still there as proved by renaming it back again.
*/

#include <QColor>
#include <QSqlField>

#include "sqlmodels.h"
#include "database.h"
#include "preferences.h"
#include "utils.h"


SqlTableModel::SqlTableModel(QObject * parent, QSqlDatabase db)
	: QSqlTableModel(parent, db),
	m_pending(false),
	m_schema("")
{
	m_deleteCache.clear();
	Preferences * prefs = Preferences::instance();
	m_readRowsCount = prefs->readRowsCount();

	connect(this, SIGNAL(primeInsert(int, QSqlRecord &)),
			this, SLOT(doPrimeInsert(int, QSqlRecord &)));
}

QVariant SqlTableModel::data(const QModelIndex & item, int role) const
{
	QString curr(QSqlTableModel::data(item, Qt::DisplayRole).toString());
	// numbers
	if (role == Qt::TextAlignmentRole)
	{
		bool ok;
		curr.toDouble(&ok);
		if (ok)
			return QVariant(Qt::AlignRight | Qt::AlignTop);
		return QVariant(Qt::AlignTop);
	}

	if (role == Qt::BackgroundColorRole)
    {
    	// mark rows prepared for a deletion in this trasnaction
        if (m_deleteCache.contains(item.row()))
            return QVariant(Qt::red);
        for (int i = 0; i < columnCount(); ++i)
        {
            if (isDirty(index(item.row(), i)))
                return QVariant(Qt::cyan);
        }
    }

	Preferences * prefs = Preferences::instance();
	bool useNull = prefs->nullHighlight();
	QColor nullColor = prefs->nullHighlightColor();
	QString nullText = prefs->nullHighlightText();
	bool useBlob = prefs->blobHighlight();
	QColor blobColor = prefs->blobHighlightColor();
	QString blobText = prefs->blobHighlightText();
	bool cropColumns = prefs->cropColumns();

	// nulls
	if (useNull && curr.isNull())
	{
		if (role == Qt::BackgroundColorRole)
			return QVariant(nullColor);
		if (role == Qt::ToolTipRole)
			return QVariant(tr("NULL value"));
		if (role == Qt::DisplayRole)
			return QVariant(nullText);
	}
	// BLOBs
	// any others handling with blobs - e.g. converting to images etc.
	// are followed with serious performance issues.
	// Users can see it through edit dialog.
	if (/*f.type.toUpper() == "BLOB" || */
		useBlob /*&&
		   record().field(item.column()).type() == QVariant::ByteArray*/
		   && QSqlTableModel::data(item, Qt::DisplayRole).type() == QVariant::ByteArray)
	{
		if (role == Qt::BackgroundColorRole)
			return QVariant(blobColor);
		if (role == Qt::ToolTipRole)
			return QVariant(tr("BLOB value"));
		if (role == Qt::DisplayRole)
			return QVariant(blobText);
		if (role == Qt::EditRole)
// 			return Database::hex(QSqlTableModel::data(item, Qt::DisplayRole).toByteArray());
			return QSqlTableModel::data(item, Qt::DisplayRole);
	}

	// advanced tooltips
	if (role == Qt::ToolTipRole)
		return QVariant("<qt>" + curr + "</qt>");

	if (role == Qt::DisplayRole && cropColumns)
		return QVariant(curr.length() > 20 ? curr.left(20)+"..." : curr);

	return QSqlTableModel::data(item, role);
}

bool SqlTableModel::setData ( const QModelIndex & ix, const QVariant & value, int role)
{
    if (! ix.isValid())
        return false;

	if (role == Qt::EditRole)
		m_pending = true;

    int r = ix.row();
//	emit dataChanged( index(r, 0), index(r, columnCount()-1) );
	int c = ix.column();
	emit dataChanged( index(r, c), index(r, c) );

	return QSqlTableModel::setData(ix, value, role);
}

QVariant SqlTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal)
	{
		switch (role)
		{
			case (Qt::DecorationRole):
				switch (m_header[section])
				{
					case (SqlTableModel::PK):
						return Utils::getIcon("key.png");
						break;
					case (SqlTableModel::Auto):
						return Utils::getIcon("index.png");
						break;
					case (SqlTableModel::Default):
						return Utils::getIcon("column.png");
						break;
					default:
						return 0;
				}
				break;
			case (Qt::ToolTipRole):
				switch (m_header[section])
				{
					case (SqlTableModel::PK):
						return tr("Primary Key");
						break;
					case (SqlTableModel::Auto):
						return tr("Autoincrement");
						break;
					case (SqlTableModel::Default):
						return tr("Has default value");
						break;
					default:
						return "";
				}
				break;
		}
	}
	return QSqlTableModel::headerData(section, orientation, role);
}

void SqlTableModel::doPrimeInsert(int row, QSqlRecord & record)
{
	FieldList fl = Database::tableFields(tableName(), m_schema);
	bool ok;
	QString defval;
	// guess what type is the default value.
	foreach (DatabaseTableField column, fl)
	{
		if (column.defval.isNull())
			continue;
		defval = column.defval;
		defval.toInt(&ok);
		if (!ok)
		{
			defval.toDouble(&ok);
			if (!ok)
			{
				if (defval.left(1) == "'" || defval.left(1) == "\"")
					defval = defval.mid(1, defval.length()-2);
			}
		}
		record.setValue(column.name, QVariant(defval));
	}
}

bool SqlTableModel::insertRows ( int row, int count, const QModelIndex & parent)
{
	m_pending = true;
	return QSqlTableModel::insertRows(row, count, parent);
}

bool SqlTableModel::removeRows ( int row, int count, const QModelIndex & parent)
{
	m_pending = true;
	// this is a workaround to allow mark heading as deletion
	// (as it's propably a bug in Qt QSqlTableModel ManualSubmit handling
	bool ret = QSqlTableModel::removeRows(row, count, parent);
	emit dataChanged( index(row, 0), index(row+count-1, columnCount()-1) );
	emit headerDataChanged(Qt::Vertical, row, row+count-1);
	for (int i = 0; i < count; ++i)
		m_deleteCache.append(row+i);

	return ret;
}

void SqlTableModel::setTable(const QString &tableName)
{
	m_header.clear();
	QStringList indexes = Database::getSysIndexes(tableName, m_schema);
	FieldList columns = Database::tableFields(tableName, m_schema);

	foreach (DatabaseTableField c, columns)
	{
		if (c.pk)
		{
			m_header[c.cid] = (c.type == "INTEGER PRIMARY KEY AUTOINCREMENT") ? SqlTableModel::Auto : SqlTableModel::PK;
			continue;
		}
		// show has default icon
		if (!c.defval.isEmpty())
		{
			m_header[c.cid] = SqlTableModel::Default;
			continue;
		}
		m_header[c.cid] = SqlTableModel::None;
	}

	QSqlTableModel::setTable(tableName);
}

bool SqlTableModel::select()
{
	bool result = QSqlTableModel::select();
	while (   result &&
			  canFetchMore(QModelIndex())
		   && (   (m_readRowsCount == 0)
			   || (rowCount() < m_readRowsCount)))
	{
		fetchMore();
	}
	return result;
}

void SqlTableModel::setPendingTransaction(bool pending)
{
	m_pending = pending;

	// TODO: examine the better way to get only shown/changed lines.
	// If there is one...
	if (!pending)
	{
		for (int i = 0; i < m_deleteCache.size(); ++i)
			emit headerDataChanged(Qt::Vertical, m_deleteCache[i], m_deleteCache[i]);
	}
	m_deleteCache.clear();
}




SqlQueryModel::SqlQueryModel( QObject * parent)
	: QSqlQueryModel(parent)
{
	Preferences * prefs = Preferences::instance();
	m_readRowsCount = prefs->readRowsCount();
}

QVariant SqlQueryModel::data(const QModelIndex & item, int role) const
{
	QString curr(QSqlQueryModel::data(item, Qt::DisplayRole).toString());

	// numbers
	if (role == Qt::TextAlignmentRole)
	{
		bool ok;
		curr.toDouble(&ok);
		if (ok)
			return QVariant(Qt::AlignRight | Qt::AlignTop);
		return QVariant(Qt::AlignTop);
	}

	Preferences * prefs = Preferences::instance();
	bool useNull = prefs->nullHighlight();
	QColor nullColor = prefs->nullHighlightColor();
	QString nullText = prefs->nullHighlightText();
	bool useBlob = prefs->blobHighlight();
	QColor blobColor = prefs->blobHighlightColor();
	QString blobText = prefs->blobHighlightText();
	bool cropColumns = prefs->cropColumns();

	if (useNull && curr.isNull())
	{
		if (role == Qt::BackgroundColorRole)
			return QVariant(nullColor);
		if (role == Qt::ToolTipRole)
			return QVariant(tr("NULL value"));
		if (role == Qt::DisplayRole)
			return QVariant(nullText);
	}

	if (/*f.type.toUpper() == "BLOB" || */
		useBlob /*&&
		   record().field(item.column()).type() == QVariant::ByteArray*/
		   && QSqlQueryModel::data(item, Qt::DisplayRole).type() == QVariant::ByteArray)
	{
		if (role == Qt::BackgroundColorRole)
			return QVariant(blobColor);
		if (role == Qt::ToolTipRole)
			return QVariant(tr("BLOB value"));
		if (role == Qt::DisplayRole)
			return QVariant(blobText);
	}

	// advanced tooltips
	if (role == Qt::ToolTipRole)
		return QVariant("<qt>" + curr + "</qt>");

	if (role == Qt::DisplayRole && cropColumns)
		return QVariant(curr.length() > 20 ? curr.left(20)+"..." : curr);

	return QSqlQueryModel::data(item, role);
}

void SqlQueryModel::setQuery ( const QSqlQuery & query )
{
	QSqlQueryModel::setQuery(query);
	info = record();
	while (   canFetchMore(QModelIndex())
		   && (   (m_readRowsCount == 0)
			   || (rowCount() < m_readRowsCount)))
	{
		fetchMore();
	}
}

void SqlQueryModel::setQuery ( const QString & query, const QSqlDatabase & db)
{
	QSqlQueryModel::setQuery(query, db);
	info = record();
	while (   canFetchMore(QModelIndex())
		   && (   (m_readRowsCount == 0)
			   || (rowCount() < m_readRowsCount)))
	{
		fetchMore();
	}
}
